#pragma once

#include <JuceHeader.h>

#include "PitchShiftEngine.h"

namespace hhps
{

// Parameter IDs, kept in one place because the editor, the presets and the
// state round-trip all key off them.
namespace pid
{
inline constexpr const char* semitones   = "semitones";
inline constexpr const char* cents       = "cents";
inline constexpr const char* mix         = "mix";
inline constexpr const char* grainAuto   = "grainAuto";
inline constexpr const char* grainMs     = "grainMs";
inline constexpr const char* crossfade   = "crossfade";
inline constexpr const char* lowCut      = "lowCut";
inline constexpr const char* output      = "output";
inline constexpr const char* dblOn       = "dblOn";
inline constexpr const char* dblDetune   = "dblDetune";
inline constexpr const char* dblDelay    = "dblDelay";
inline constexpr const char* dblWidth    = "dblWidth";
inline constexpr const char* dblLevel    = "dblLevel";
inline constexpr const char* monoInput   = "monoInput";
} // namespace pid

struct Preset
{
    const char* name;
    float semitones, cents, mix, grainMs, crossfade, lowCut, output;
    bool dblOn;
    float dblDetune, dblDelay, dblWidth, dblLevel;
    bool monoInput;
    bool grainAuto;
};

const std::vector<Preset>& factoryPresets();

class PitchProcessor : public juce::AudioProcessor
{
public:
    PitchProcessor();
    ~PitchProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.2; }

    int getNumPrograms() override { return static_cast<int>(factoryPresets().size()); }
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& state() { return apvts; }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout makeLayout();
    void pullParameters() noexcept;

    juce::AudioProcessorValueTreeState apvts;
    PitchShiftEngine engine;
    int currentProgram = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PitchProcessor)
};

} // namespace hhps
