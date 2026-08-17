#pragma once

#include <cstddef>
#include <vector>

namespace hhps
{

// A granular (delay-line) pitch shifter in the shape of Logic Pro's Pitch
// Shifter: two read heads run through a circular buffer at the pitch ratio,
// half a grain apart, and are crossfaded so the splice between grains is
// inaudible. Deliberately free of JUCE so the DSP can be tested as a plain
// executable.

constexpr double kMinGrainMs = 3.0;
constexpr double kMaxGrainMs = 100.0;

double semitonesToRatio(double semitones, double cents) noexcept;

// Grain length Logic-style "auto" picks for a given shift: large intervals
// need long grains to keep the pitch stable, small ones stay tight.
double autoGrainMs(double ratio) noexcept;

class DelayLine
{
public:
    void prepare(int maxSamples);
    void reset();

    void write(float x) noexcept;

    // Fractional read `delay` samples back from the write head, 4-point
    // Catmull-Rom so a slowly drifting read head doesn't sound gritty.
    float read(double delay) const noexcept;

    int size() const noexcept { return static_cast<int>(buffer.size()); }

private:
    float at(int back) const noexcept;

    std::vector<float> buffer;
    int writeIndex = 0;
};

// One pitch-shifted tap: the shifter itself plus the fixed pre-delay and gain
// that turn a second voice into a double-tracked guitar.
class PitchVoice
{
public:
    void prepare(double sampleRate, double maxDelayMs);
    void reset();

    void setRatio(double ratio) noexcept { targetRatio = ratio; }
    void setGrainMs(double ms) noexcept;
    void setCrossfade(double amount01) noexcept;   // 0 = short splice, 1 = full Hann
    void setPreDelayMs(double ms) noexcept;

    // Snap smoothed values to their targets (after prepare / a preset load).
    void snap() noexcept;

    float process(float x) noexcept;

private:
    DelayLine line;
    double fs = 44100.0;

    double ratio = 1.0, targetRatio = 1.0;
    double grainSamples = 1024.0, targetGrainSamples = 1024.0;
    double crossfade = 1.0, targetCrossfade = 1.0;
    double preDelay = 0.0, targetPreDelay = 0.0;
    double phase = 0.0;                 // read-head lag inside the grain
    double smoothing = 0.0;             // one-pole coefficient for the above
};

// 12 dB/oct state-variable high-pass, used to keep the octave-down voice from
// muddying up the low end of a guitar track.
class HighPass
{
public:
    void prepare(double sampleRate);
    void reset();
    void setCutoff(double hz) noexcept;
    float process(float x) noexcept;

private:
    double fs = 44100.0, g = 0.0, k = 1.41421356, a1 = 0.0, a2 = 0.0, a3 = 0.0;
    double ic1eq = 0.0, ic2eq = 0.0;
};

class PitchShiftEngine
{
public:
    struct Params
    {
        double semitones = 0.0;      // -24 .. +24
        double cents = 0.0;          // -100 .. +100
        double mix = 100.0;          // % wet
        double grainMs = 0.0;        // <= 0 selects auto
        double crossfade = 50.0;     // % of the grain spent splicing
        double lowCutHz = 20.0;      // high-pass on the wet path
        double outputGainDb = 0.0;

        // Dual tracking: two extra voices hard-ish panned left / right, each
        // detuned and delayed, which is how a doubled guitar is faked.
        bool doubleTrack = false;
        double doubleDetuneCents = 12.0;  // +/- this much on the two sides
        double doubleDelayMs = 22.0;      // right side; left uses ~60% of it
        double doubleWidth = 100.0;       // % pan spread
        double doubleLevelDb = -1.5;
        bool monoInput = false;           // sum L+R before shifting
    };

    void prepare(double sampleRate, int maxBlockSize);
    void reset();

    void setParams(const Params& p) noexcept;

    // In-place stereo processing. `right` may be null for mono hosts.
    void process(float* left, float* right, int numSamples) noexcept;

    double latencySamples() const noexcept { return 0.0; }

private:
    void applyToVoices() noexcept;

    Params params {};
    double fs = 44100.0;

    PitchVoice mainL, mainR;
    PitchVoice doubleL, doubleR;
    HighPass wetHpL, wetHpR;

    double wetGain = 1.0, dryGain = 0.0, outGain = 1.0, doubleGain = 0.0;
    // The doubler voices run whether or not Dual Track is on, and this gain
    // ramps them in: a voice that stopped running would come back holding a
    // stale delay line and splice a click into the middle of a take.
    double doubleGainSmoothed = 0.0, gainSmoothing = 0.001;
    bool prepared = false;
};

} // namespace hhps
