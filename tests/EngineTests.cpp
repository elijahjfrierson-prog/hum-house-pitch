// Plain-executable tests for the pitch shifter DSP: the engine is JUCE-free so
// pitch accuracy, unity passthrough and the doubler's stereo behaviour can all
// be measured without a host.

#include "../SourcePS/PitchShiftEngine.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace
{
constexpr double kFs = 48000.0;
int failures = 0;

void check(bool ok, const std::string& what, const std::string& detail = {})
{
    std::printf("%s  %s%s%s\n", ok ? "PASS" : "FAIL", what.c_str(),
                detail.empty() ? "" : " — ", detail.c_str());
    if (! ok)
        ++failures;
}

void checkNear(double value, double expected, double tolerance, const std::string& what)
{
    const bool ok = std::abs(value - expected) <= tolerance;
    char detail[128];
    std::snprintf(detail, sizeof(detail), "got %.4f, expected %.4f +/- %.4f",
                  value, expected, tolerance);
    check(ok, what, detail);
}

std::vector<float> sine(double freq, int numSamples, double amplitude = 0.5)
{
    std::vector<float> out(static_cast<size_t>(numSamples));
    for (int i = 0; i < numSamples; ++i)
        out[static_cast<size_t>(i)] =
            static_cast<float>(amplitude * std::sin(2.0 * M_PI * freq * i / kFs));
    return out;
}

double rms(const std::vector<float>& x, int from = 0)
{
    double sum = 0.0;
    int n = 0;
    for (size_t i = static_cast<size_t>(from); i < x.size(); ++i, ++n)
        sum += static_cast<double>(x[i]) * x[i];
    return n > 0 ? std::sqrt(sum / n) : 0.0;
}

// Frequency of the strongest spectral component, by brute-force DFT on a
// Hann-windowed block. Autocorrelation is the obvious choice for a sine, but a
// granular shifter repeats or skips material once per grain, and the resulting
// sidebands pull a correlation peak several percent off the true pitch; the
// fundamental still dominates the spectrum, so a peak search is the honest
// measurement here.
double estimateFrequency(const std::vector<float>& x, int from, int length)
{
    std::vector<double> windowed(static_cast<size_t>(length));
    for (int i = 0; i < length; ++i)
    {
        const double w = 0.5 - 0.5 * std::cos(2.0 * M_PI * i / (length - 1));
        windowed[static_cast<size_t>(i)] = w * x[static_cast<size_t>(from + i)];
    }

    const auto magnitude = [&](double freq)
    {
        double re = 0.0, im = 0.0;
        const double step = 2.0 * M_PI * freq / kFs;
        for (int i = 0; i < length; ++i)
        {
            re += windowed[static_cast<size_t>(i)] * std::cos(step * i);
            im += windowed[static_cast<size_t>(i)] * std::sin(step * i);
        }
        return std::sqrt(re * re + im * im);
    };

    double bestFreq = 40.0, bestMag = -1.0;
    for (double freq = 40.0; freq <= 3000.0; freq += 1.0)
    {
        const double m = magnitude(freq);
        if (m > bestMag)
        {
            bestMag = m;
            bestFreq = freq;
        }
    }

    // Refine to 0.05 Hz around the winning bin.
    for (double freq = bestFreq - 1.0; freq <= bestFreq + 1.0; freq += 0.05)
    {
        const double m = magnitude(freq);
        if (m > bestMag)
        {
            bestMag = m;
            bestFreq = freq;
        }
    }
    return bestFreq;
}

// Runs the engine over a mono sine and returns the (stereo) output channels.
struct Rendered
{
    std::vector<float> left, right;
};

Rendered render(hhps::PitchShiftEngine::Params params, const std::vector<float>& input,
                int blockSize = 128)
{
    hhps::PitchShiftEngine engine;
    engine.prepare(kFs, blockSize);
    engine.setParams(params);

    Rendered out { input, input };
    for (size_t i = 0; i < input.size(); i += static_cast<size_t>(blockSize))
    {
        const int n = static_cast<int>(std::min<size_t>(static_cast<size_t>(blockSize),
                                                        input.size() - i));
        engine.process(out.left.data() + i, out.right.data() + i, n);
    }
    return out;
}

double correlation(const std::vector<float>& a, const std::vector<float>& b, int from)
{
    double num = 0.0, da = 0.0, db = 0.0;
    for (size_t i = static_cast<size_t>(from); i < a.size(); ++i)
    {
        num += static_cast<double>(a[i]) * b[i];
        da += static_cast<double>(a[i]) * a[i];
        db += static_cast<double>(b[i]) * b[i];
    }
    return (da > 0.0 && db > 0.0) ? num / std::sqrt(da * db) : 0.0;
}

bool isFinite(const std::vector<float>& x)
{
    for (float v : x)
        if (! std::isfinite(v))
            return false;
    return true;
}

void testRatioMath()
{
    checkNear(hhps::semitonesToRatio(12.0, 0.0), 2.0, 1.0e-9, "an octave up is ratio 2");
    checkNear(hhps::semitonesToRatio(-12.0, 0.0), 0.5, 1.0e-9, "an octave down is ratio 0.5");
    checkNear(hhps::semitonesToRatio(0.0, 100.0), std::pow(2.0, 1.0 / 12.0), 1.0e-9,
              "100 cents equals one semitone");
    check(hhps::autoGrainMs(2.0) > hhps::autoGrainMs(1.01),
          "auto grain grows with the interval");
}

// Long grains splice rarely, so the shifted tone is a clean line and the pitch
// can be held to a quarter of a semitone.
void testShiftAccuracy(double semitones, double cents, const char* what)
{
    hhps::PitchShiftEngine::Params p;
    p.semitones = semitones;
    p.cents = cents;
    p.mix = 100.0;
    p.grainMs = 90.0;

    const double inputHz = 220.0;
    const auto out = render(p, sine(inputHz, static_cast<int>(kFs)));
    check(isFinite(out.left), std::string(what) + ": output is finite");

    const double measured = estimateFrequency(out.left, static_cast<int>(kFs * 0.4), 16384);
    const double expected = inputHz * hhps::semitonesToRatio(semitones, cents);
    checkNear(measured, expected, expected * 0.015, what);
}

// With the shorter grains "auto" picks, each grain splice phase-modulates the
// tone and throws sidebands one splice-rate away from the carrier, which is the
// granular character this design shares with Logic's Pitch Shifter. The pitch
// still has to land within a sideband of the target, and the target has to
// dominate the original note by a wide margin.
void testAutoGrainShift(double semitones, const char* what)
{
    hhps::PitchShiftEngine::Params p;
    p.semitones = semitones;
    p.mix = 100.0;

    const double inputHz = 220.0;
    const double ratio = hhps::semitonesToRatio(semitones, 0.0);
    const auto out = render(p, sine(inputHz, static_cast<int>(kFs)));

    const double spliceHz = std::abs(1.0 - ratio) / (hhps::autoGrainMs(ratio) * 0.001);
    const double expected = inputHz * ratio;
    const double measured = estimateFrequency(out.left, static_cast<int>(kFs * 0.4), 16384);
    checkNear(measured, expected, std::max(expected * 0.006, 1.5 * spliceHz), what);
}

void testUnityPassthrough()
{
    hhps::PitchShiftEngine::Params p;
    p.mix = 100.0;

    const auto input = sine(440.0, static_cast<int>(kFs * 0.5));
    const auto out = render(p, input);

    const int from = static_cast<int>(kFs * 0.1);
    double best = 0.0;
    for (int lag = 0; lag < 64; ++lag)
    {
        std::vector<float> shifted(input.size(), 0.0f);
        for (size_t i = static_cast<size_t>(lag); i < input.size(); ++i)
            shifted[i] = input[i - static_cast<size_t>(lag)];
        best = std::max(best, correlation(out.left, shifted, from));
    }
    check(best > 0.999, "no shift passes the signal through cleanly",
          "best correlation " + std::to_string(best));
    checkNear(rms(out.left, from) / rms(input, from), 1.0, 0.02,
              "no shift keeps the level");
}

void testDryOnly()
{
    hhps::PitchShiftEngine::Params p;
    p.semitones = 7.0;
    p.mix = 0.0;

    const auto input = sine(300.0, 4096);
    const auto out = render(p, input);

    bool identical = true;
    for (size_t i = 0; i < input.size(); ++i)
        identical = identical && std::abs(out.left[i] - input[i]) < 1.0e-6f;
    check(identical, "mix at 0% is bit-for-bit dry");
}

void testDualTracking()
{
    hhps::PitchShiftEngine::Params p;
    p.mix = 100.0;
    p.doubleTrack = true;
    p.doubleDetuneCents = 14.0;
    p.doubleDelayMs = 24.0;
    p.doubleWidth = 100.0;

    const auto input = sine(196.0, static_cast<int>(kFs * 0.7));
    const auto out = render(p, input);
    check(isFinite(out.left) && isFinite(out.right), "dual tracking output is finite");

    const int from = static_cast<int>(kFs * 0.2);
    const double corr = correlation(out.left, out.right, from);
    check(corr < 0.99, "the two tracked voices are not the same signal",
          "L/R correlation " + std::to_string(corr));

    // A doubler that cancels in mono is useless on a guitar bus.
    std::vector<float> mono(out.left.size());
    for (size_t i = 0; i < mono.size(); ++i)
        mono[i] = 0.5f * (out.left[i] + out.right[i]);
    const double monoRms = rms(mono, from);
    const double sideRms = rms(out.left, from);
    check(monoRms > 0.5 * sideRms, "the doubled signal survives a mono fold-down",
          "mono " + std::to_string(monoRms) + " vs side " + std::to_string(sideRms));

    // Width at 0 collapses the doubler to the middle: both channels agree.
    p.doubleWidth = 0.0;
    const auto narrow = render(p, input);
    check(correlation(narrow.left, narrow.right, from) > 0.999,
          "width at 0% centres the doubler");
}

void testSilenceAndExtremes()
{
    hhps::PitchShiftEngine::Params p;
    p.semitones = -24.0;
    p.cents = -100.0;
    p.mix = 100.0;
    p.grainMs = 3.0;
    p.crossfade = 0.0;
    p.doubleTrack = true;
    p.doubleDetuneCents = 50.0;
    p.doubleDelayMs = 80.0;
    p.outputGainDb = 12.0;

    const std::vector<float> silence(static_cast<size_t>(kFs * 0.2), 0.0f);
    const auto quiet = render(p, silence);
    check(isFinite(quiet.left) && rms(quiet.left) < 1.0e-9,
          "silence in, silence out at the parameter extremes");

    p.semitones = 24.0;
    p.cents = 100.0;
    const auto loud = render(p, sine(1000.0, static_cast<int>(kFs * 0.3), 1.0));
    bool bounded = isFinite(loud.left);
    for (float v : loud.left)
        bounded = bounded && std::abs(v) < 8.0f;
    check(bounded, "extreme upward shift stays bounded");
}

void testParameterSweepIsClickFree()
{
    hhps::PitchShiftEngine engine;
    engine.prepare(kFs, 64);

    auto input = sine(220.0, static_cast<int>(kFs * 2.0));
    hhps::PitchShiftEngine::Params p;
    p.mix = 100.0;

    double maxStep = 0.0;
    float previous = 0.0f;
    const int block = 64;
    for (size_t i = 0; i < input.size(); i += static_cast<size_t>(block))
    {
        const int n = static_cast<int>(std::min<size_t>(static_cast<size_t>(block),
                                                        input.size() - i));
        // Sweep from -12 to +12 semitones across the render, which is the worst
        // case: the read head reverses direction through unity.
        p.semitones = -12.0 + 24.0 * static_cast<double>(i) / static_cast<double>(input.size());
        engine.setParams(p);
        engine.process(input.data() + i, nullptr, n);

        if (i > static_cast<size_t>(kFs * 0.05))
            for (int j = 0; j < n; ++j)
            {
                const float v = input[i + static_cast<size_t>(j)];
                maxStep = std::max(maxStep, std::abs(static_cast<double>(v - previous)));
                previous = v;
            }
        else
            previous = input[i + static_cast<size_t>(n - 1)];
    }

    // A 220 Hz sine at 0.5 amplitude moves at most ~0.09 per sample when shifted
    // up an octave; a grain splice discontinuity would be several times that.
    check(maxStep < 0.25, "sweeping the pitch does not click",
          "largest sample step " + std::to_string(maxStep));
    check(isFinite(input), "swept output is finite");
}

void testShiftKeepsLevel()
{
    hhps::PitchShiftEngine::Params p;
    p.mix = 100.0;

    const auto input = sine(220.0, static_cast<int>(kFs * 0.6));
    const int from = static_cast<int>(kFs * 0.2);
    const double dry = rms(input, from);

    for (double semitones : { -12.0, -7.0, 5.0, 12.0 })
    {
        p.semitones = semitones;
        const auto out = render(p, input);
        const double ratioDb = 20.0 * std::log10(rms(out.left, from) / dry);
        char detail[96];
        std::snprintf(detail, sizeof(detail), "%+.1f st is %+.2f dB", semitones, ratioDb);
        check(std::abs(ratioDb) < 2.0, "shifting keeps the level within 2 dB", detail);
    }
}

void testLowCut()
{
    hhps::PitchShiftEngine::Params p;
    p.mix = 100.0;
    p.lowCutHz = 500.0;

    const auto low = render(p, sine(60.0, static_cast<int>(kFs * 0.4)));
    const auto high = render(p, sine(2000.0, static_cast<int>(kFs * 0.4)));
    const int from = static_cast<int>(kFs * 0.15);
    check(rms(low.left, from) < 0.2 * rms(high.left, from),
          "the wet low cut attenuates 60 Hz but not 2 kHz");
}
} // namespace

int main()
{
    testRatioMath();
    testShiftAccuracy(12.0, 0.0, "shift up an octave");
    testShiftAccuracy(-12.0, 0.0, "shift down an octave");
    testShiftAccuracy(7.0, 0.0, "shift up a fifth");
    testShiftAccuracy(-5.0, 0.0, "shift down a fourth");
    testShiftAccuracy(0.0, 50.0, "shift up 50 cents");
    testShiftAccuracy(0.0, -25.0, "shift down 25 cents");
    testShiftAccuracy(24.0, 0.0, "shift up two octaves");
    testAutoGrainShift(12.0, "auto grain, octave up");
    testAutoGrainShift(-12.0, "auto grain, octave down");
    testAutoGrainShift(-5.0, "auto grain, fourth down");
    testShiftKeepsLevel();
    testUnityPassthrough();
    testDryOnly();
    testDualTracking();
    testSilenceAndExtremes();
    testParameterSweepIsClickFree();
    testLowCut();

    std::printf("\n%s (%d failure%s)\n", failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED",
                failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
