#include "PitchShiftEngine.h"

#include <algorithm>
#include <cmath>

namespace hhps
{

namespace
{
constexpr double kPi = 3.14159265358979323846;
constexpr double kReadMargin = 8.0;      // samples of headroom between heads
constexpr double kMaxPreDelayMs = 80.0;
constexpr double kUnityWindow = 1.0e-3;  // ratio distance where grains fade in

inline double clampd(double v, double lo, double hi) noexcept
{
    return v < lo ? lo : (v > hi ? hi : v);
}

inline float flushDenormal(float v) noexcept
{
    return std::abs(v) < 1.0e-15f ? 0.0f : v;
}

// Raised-cosine step: 0 below the fade, 1 above it.
inline double smoothStep(double x) noexcept
{
    if (x <= 0.0)
        return 0.0;
    if (x >= 1.0)
        return 1.0;
    return 0.5 - 0.5 * std::cos(kPi * x);
}

// Weight of the first read head over the grain cycle `u` in [0,1).
//
// The two heads are half a cycle apart, so head 1 splices at u = 0 and head 2
// at u = 0.5. This window is therefore centred on u = 0.5 and hits zero at the
// cycle edges: whichever head is jumping is the one being faded out, and the
// other head carries the signal alone. `fade` is the crossfade length as a
// fraction of the cycle (0.5 = the two windows overlap everywhere, i.e. Hann;
// short fades keep a single head running, which is cleaner on sustained
// material because two taps of the same note comb-filter each other).
double headOneWeight(double u, double fade) noexcept
{
    u -= std::floor(u);
    const double f = clampd(fade, 0.01, 0.5);
    return smoothStep((u - 0.25 + f * 0.5) / f) - smoothStep((u - 0.75 + f * 0.5) / f);
}
} // namespace

double semitonesToRatio(double semitones, double cents) noexcept
{
    return std::pow(2.0, (semitones + cents / 100.0) / 12.0);
}

double autoGrainMs(double ratio) noexcept
{
    // The further from unity, the further the read head travels per grain, so
    // the grain has to grow or the shift turns into a warble.
    const double semis = std::abs(12.0 * std::log2(std::max(ratio, 1.0e-6)));
    return clampd(18.0 + 2.6 * semis, kMinGrainMs, kMaxGrainMs);
}

// --- DelayLine ---------------------------------------------------------------

void DelayLine::prepare(int maxSamples)
{
    buffer.assign(static_cast<size_t>(std::max(maxSamples, 16)), 0.0f);
    writeIndex = 0;
}

void DelayLine::reset()
{
    std::fill(buffer.begin(), buffer.end(), 0.0f);
    writeIndex = 0;
}

void DelayLine::write(float x) noexcept
{
    buffer[static_cast<size_t>(writeIndex)] = x;
    if (++writeIndex >= size())
        writeIndex = 0;
}

float DelayLine::at(int back) const noexcept
{
    const int n = size();
    int idx = writeIndex - 1 - back;
    while (idx < 0)
        idx += n;
    while (idx >= n)
        idx -= n;
    return buffer[static_cast<size_t>(idx)];
}

float DelayLine::read(double delay) const noexcept
{
    const double maxDelay = static_cast<double>(size()) - 4.0;
    const double d = clampd(delay, 1.0, maxDelay);
    const int i = static_cast<int>(d);
    const double frac = d - static_cast<double>(i);

    const double y0 = at(i - 1);
    const double y1 = at(i);
    const double y2 = at(i + 1);
    const double y3 = at(i + 2);

    // Catmull-Rom: interpolating between y1 and y2 as frac goes 0 -> 1.
    const double a = -0.5 * y0 + 1.5 * y1 - 1.5 * y2 + 0.5 * y3;
    const double b = y0 - 2.5 * y1 + 2.0 * y2 - 0.5 * y3;
    const double c = -0.5 * y0 + 0.5 * y2;
    return static_cast<float>(((a * frac + b) * frac + c) * frac + y1);
}

// --- PitchVoice --------------------------------------------------------------

void PitchVoice::prepare(double sampleRate, double maxDelayMs)
{
    fs = sampleRate > 0.0 ? sampleRate : 44100.0;
    const double maxMs = maxDelayMs + kMaxGrainMs + kMaxPreDelayMs;
    line.prepare(static_cast<int>(std::ceil(maxMs * 0.001 * fs)) + 32);
    smoothing = 1.0 - std::exp(-1.0 / (0.020 * fs));   // ~20 ms
    setGrainMs(30.0);
    snap();
    reset();
}

void PitchVoice::reset()
{
    line.reset();
    phase = 0.0;
}

void PitchVoice::setGrainMs(double ms) noexcept
{
    targetGrainSamples = clampd(ms, kMinGrainMs, kMaxGrainMs) * 0.001 * fs;
}

void PitchVoice::setCrossfade(double amount01) noexcept
{
    targetCrossfade = clampd(amount01, 0.0, 1.0);
}

void PitchVoice::setPreDelayMs(double ms) noexcept
{
    targetPreDelay = clampd(ms, 0.0, kMaxPreDelayMs) * 0.001 * fs;
}

void PitchVoice::snap() noexcept
{
    ratio = targetRatio;
    grainSamples = targetGrainSamples;
    crossfade = targetCrossfade;
    preDelay = targetPreDelay;
    if (phase >= grainSamples)
        phase = 0.0;
}

float PitchVoice::process(float x) noexcept
{
    line.write(flushDenormal(x));

    ratio += smoothing * (targetRatio - ratio);
    grainSamples += smoothing * (targetGrainSamples - grainSamples);
    crossfade += smoothing * (targetCrossfade - crossfade);
    preDelay += smoothing * (targetPreDelay - preDelay);

    const double grain = std::max(grainSamples, 16.0);

    // d'(t) = 1 - ratio: shifting up walks the read head towards the write
    // head, shifting down away from it, and the grain length is how far it is
    // allowed to travel before it snaps back.
    phase += 1.0 - ratio;
    phase -= grain * std::floor(phase / grain);

    const double base = preDelay + kReadMargin;
    const double u1 = phase / grain;
    const double u2 = u1 + 0.5;

    const double w1 = headOneWeight(u1, 0.5 * clampd(crossfade, 0.02, 1.0));
    const double w2 = 1.0 - w1;

    const double d1 = base + phase;
    const double d2 = base + (u2 - std::floor(u2)) * grain;

    const double grains = w1 * line.read(d1) + w2 * line.read(d2);

    // At (or passing through) unity there is nothing to splice, so fade to a
    // single head and avoid the comb filter two taps would give.
    const double blend = clampd(std::abs(ratio - 1.0) / kUnityWindow, 0.0, 1.0);
    const double single = line.read(base);

    return flushDenormal(static_cast<float>(blend * grains + (1.0 - blend) * single));
}

// --- HighPass ---------------------------------------------------------------

void HighPass::prepare(double sampleRate)
{
    fs = sampleRate > 0.0 ? sampleRate : 44100.0;
    setCutoff(20.0);
    reset();
}

void HighPass::reset()
{
    ic1eq = ic2eq = 0.0;
}

void HighPass::setCutoff(double hz) noexcept
{
    const double f = clampd(hz, 10.0, 0.45 * fs);
    g = std::tan(kPi * f / fs);
    a1 = 1.0 / (1.0 + g * (g + k));
    a2 = g * a1;
    a3 = g * a2;
}

float HighPass::process(float x) noexcept
{
    const double v0 = x;
    const double v3 = v0 - ic2eq;
    const double v1 = a1 * ic1eq + a2 * v3;
    const double v2 = ic2eq + a2 * ic1eq + a3 * v3;
    ic1eq = 2.0 * v1 - ic1eq;
    ic2eq = 2.0 * v2 - ic2eq;
    return flushDenormal(static_cast<float>(v0 - k * v1 - v2));
}

// --- PitchShiftEngine -------------------------------------------------------

void PitchShiftEngine::prepare(double sampleRate, int /*maxBlockSize*/)
{
    fs = sampleRate > 0.0 ? sampleRate : 44100.0;
    for (auto* v : { &mainL, &mainR, &doubleL, &doubleR })
        v->prepare(fs, kMaxGrainMs);
    wetHpL.prepare(fs);
    wetHpR.prepare(fs);
    gainSmoothing = 1.0 - std::exp(-1.0 / (0.015 * fs));   // ~15 ms
    prepared = true;
    applyToVoices();
    for (auto* v : { &mainL, &mainR, &doubleL, &doubleR })
        v->snap();
}

void PitchShiftEngine::reset()
{
    for (auto* v : { &mainL, &mainR, &doubleL, &doubleR })
        v->reset();
    wetHpL.reset();
    wetHpR.reset();
    doubleGainSmoothed = doubleGain;
}

void PitchShiftEngine::setParams(const Params& p) noexcept
{
    params = p;
    if (prepared)
        applyToVoices();
}

void PitchShiftEngine::applyToVoices() noexcept
{
    const double baseRatio = semitonesToRatio(params.semitones, params.cents);
    const double grainMs = params.grainMs > 0.0 ? params.grainMs : autoGrainMs(baseRatio);
    const double xf = clampd(params.crossfade / 100.0, 0.0, 1.0);

    for (auto* v : { &mainL, &mainR })
    {
        v->setRatio(baseRatio);
        v->setGrainMs(grainMs);
        v->setCrossfade(xf);
        v->setPreDelayMs(0.0);
    }

    const double detune = clampd(params.doubleDetuneCents, 0.0, 50.0);
    doubleL.setRatio(semitonesToRatio(params.semitones, params.cents + detune));
    doubleR.setRatio(semitonesToRatio(params.semitones, params.cents - detune));
    const double delayR = clampd(params.doubleDelayMs, 0.0, kMaxPreDelayMs);
    doubleL.setPreDelayMs(delayR * 0.6);
    doubleR.setPreDelayMs(delayR);
    for (auto* v : { &doubleL, &doubleR })
    {
        v->setGrainMs(grainMs);
        v->setCrossfade(xf);
    }

    const double mix = clampd(params.mix / 100.0, 0.0, 1.0);
    wetGain = mix;
    dryGain = 1.0 - mix;
    outGain = std::pow(10.0, clampd(params.outputGainDb, -24.0, 12.0) / 20.0);
    doubleGain = params.doubleTrack
        ? std::pow(10.0, clampd(params.doubleLevelDb, -24.0, 6.0) / 20.0)
        : 0.0;

    wetHpL.setCutoff(params.lowCutHz);
    wetHpR.setCutoff(params.lowCutHz);
}

void PitchShiftEngine::process(float* left, float* right, int numSamples) noexcept
{
    if (left == nullptr || numSamples <= 0)
        return;

    const bool stereo = right != nullptr;
    const double width = clampd(params.doubleWidth / 100.0, 0.0, 1.0);
    const double gSame = 0.5 * (1.0 + width);
    const double gCross = 0.5 * (1.0 - width);

    for (int i = 0; i < numSamples; ++i)
    {
        const float dryL = left[i];
        const float dryR = stereo ? right[i] : left[i];

        float inL = dryL, inR = dryR;
        if (params.monoInput)
            inL = inR = 0.5f * (dryL + dryR);

        double wl = wetHpL.process(mainL.process(inL));
        double wr = stereo ? wetHpR.process(mainR.process(inR)) : wl;

        // Always run the doubler voices so their delay lines stay current, and
        // ramp the level instead of gating the voices.
        doubleGainSmoothed += gainSmoothing * (doubleGain - doubleGainSmoothed);
        const float mono = 0.5f * (inL + inR);
        const double dl = doubleGainSmoothed * doubleL.process(mono);
        const double dr = doubleGainSmoothed * doubleR.process(mono);
        wl += gSame * dl + gCross * dr;
        wr += gSame * dr + gCross * dl;

        left[i] = static_cast<float>(outGain * (dryGain * dryL + wetGain * wl));
        if (stereo)
            right[i] = static_cast<float>(outGain * (dryGain * dryR + wetGain * wr));
    }
}

} // namespace hhps
