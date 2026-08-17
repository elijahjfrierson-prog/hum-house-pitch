#include "PitchProcessor.h"
#include "PitchEditor.h"

namespace hhps
{

const std::vector<Preset>& factoryPresets()
{
    // name                   semi  cents  mix  grain  xfade  lowCut  out   dbl   det  delay width level  mono   auto
    static const std::vector<Preset> presets {
        { "Init",                0,    0, 100,   30,    50,     20,  0.0f, false, 12,  22,  100, -1.5f, false, true },
        { "Guitar Double Track", 0,    0, 100,   26,    50,     70, -1.0f, true,  14,  24,  100, -1.0f, true,  true },
        { "Wide 12-String",     12,    0,  40,   22,    50,    140, -1.5f, true,  18,  30,  100, -3.0f, true,  true },
        { "Octave Down Riff",  -12,    0,  55,   40,    50,     45, -1.0f, false, 12,  22,   80, -2.0f, true,  true },
        { "Octave Up Shimmer",  12,    0,  35,   28,    50,    260, -2.0f, false, 10,  18,  100, -3.0f, false, true },
        { "5th Harmony",         7,    0,  45,   34,    50,     90, -1.5f, false, 10,  20,   70, -2.0f, true,  true },
        { "4th Harmony",         5,    0,  45,   34,    50,     90, -1.5f, false, 10,  20,   70, -2.0f, true,  true },
        { "Detune Chorus",       0,    0,  50,   18,    50,     40, -1.0f, true,  22,  14,  100, -0.5f, false, true },
        { "Fat Rhythm Stack",    0,    0,  65,   24,    50,     80, -2.0f, true,  16,  34,  100,  0.0f, true,  true },
        { "Bass Octaver",      -12,    0,  50,   55,    50,     25, -1.5f, false, 10,  20,   50, -3.0f, true,  true },
        { "Grain Warble",        0,   30,  60,    6,    40,     40, -1.0f, true,  40,   8,  100, -1.0f, false, false },
    };
    return presets;
}

juce::AudioProcessorValueTreeState::ParameterLayout PitchProcessor::makeLayout()
{
    using APF = juce::AudioParameterFloat;
    using APB = juce::AudioParameterBool;
    using Range = juce::NormalisableRange<float>;

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    auto semitoneText = [](float v, int) { return juce::String(juce::roundToInt(v)) + " st"; };
    auto centText = [](float v, int) { return juce::String(v, 1) + " ct"; };
    auto pctText = [](float v, int) { return juce::String(juce::roundToInt(v)) + " %"; };
    auto msText = [](float v, int) { return juce::String(v, 1) + " ms"; };
    auto hzText = [](float v, int) { return juce::String(juce::roundToInt(v)) + " Hz"; };
    auto dbText = [](float v, int) { return juce::String(v, 1) + " dB"; };

    params.push_back(std::make_unique<APF>(
        juce::ParameterID { pid::semitones, 1 }, "Semi Tones",
        Range { -24.0f, 24.0f, 1.0f }, 0.0f, juce::AudioParameterFloatAttributes().withStringFromValueFunction(semitoneText)));
    params.push_back(std::make_unique<APF>(
        juce::ParameterID { pid::cents, 1 }, "Cents",
        Range { -100.0f, 100.0f, 0.1f }, 0.0f, juce::AudioParameterFloatAttributes().withStringFromValueFunction(centText)));
    params.push_back(std::make_unique<APF>(
        juce::ParameterID { pid::mix, 1 }, "Mix",
        Range { 0.0f, 100.0f, 0.1f }, 100.0f, juce::AudioParameterFloatAttributes().withStringFromValueFunction(pctText)));

    params.push_back(std::make_unique<APB>(
        juce::ParameterID { pid::grainAuto, 1 }, "Grain Auto", true));
    params.push_back(std::make_unique<APF>(
        juce::ParameterID { pid::grainMs, 1 }, "Grain Size",
        Range { static_cast<float>(kMinGrainMs), static_cast<float>(kMaxGrainMs), 0.1f, 0.5f }, 30.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(msText)));
    params.push_back(std::make_unique<APF>(
        juce::ParameterID { pid::crossfade, 1 }, "Crossfade",
        Range { 0.0f, 100.0f, 1.0f }, 50.0f, juce::AudioParameterFloatAttributes().withStringFromValueFunction(pctText)));

    params.push_back(std::make_unique<APF>(
        juce::ParameterID { pid::lowCut, 1 }, "Low Cut",
        Range { 20.0f, 800.0f, 1.0f, 0.35f }, 20.0f, juce::AudioParameterFloatAttributes().withStringFromValueFunction(hzText)));
    params.push_back(std::make_unique<APF>(
        juce::ParameterID { pid::output, 1 }, "Output",
        Range { -24.0f, 12.0f, 0.1f }, 0.0f, juce::AudioParameterFloatAttributes().withStringFromValueFunction(dbText)));

    params.push_back(std::make_unique<APB>(
        juce::ParameterID { pid::dblOn, 1 }, "Dual Track", false));
    params.push_back(std::make_unique<APF>(
        juce::ParameterID { pid::dblDetune, 1 }, "Track Detune",
        Range { 0.0f, 50.0f, 0.1f }, 12.0f, juce::AudioParameterFloatAttributes().withStringFromValueFunction(centText)));
    params.push_back(std::make_unique<APF>(
        juce::ParameterID { pid::dblDelay, 1 }, "Track Delay",
        Range { 0.0f, 80.0f, 0.1f }, 22.0f, juce::AudioParameterFloatAttributes().withStringFromValueFunction(msText)));
    params.push_back(std::make_unique<APF>(
        juce::ParameterID { pid::dblWidth, 1 }, "Track Width",
        Range { 0.0f, 100.0f, 1.0f }, 100.0f, juce::AudioParameterFloatAttributes().withStringFromValueFunction(pctText)));
    params.push_back(std::make_unique<APF>(
        juce::ParameterID { pid::dblLevel, 1 }, "Track Level",
        Range { -24.0f, 6.0f, 0.1f }, -1.5f, juce::AudioParameterFloatAttributes().withStringFromValueFunction(dbText)));

    params.push_back(std::make_unique<APB>(
        juce::ParameterID { pid::monoInput, 1 }, "Mono Input", false));

    return { params.begin(), params.end() };
}

PitchProcessor::PitchProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "STATE", makeLayout())
{
}

void PitchProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    engine.prepare(sampleRate, samplesPerBlock);
    pullParameters();
    engine.reset();
}

void PitchProcessor::releaseResources()
{
    engine.reset();
}

bool PitchProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void PitchProcessor::pullParameters() noexcept
{
    const auto value = [this](const char* id)
    {
        return apvts.getRawParameterValue(id)->load();
    };

    PitchShiftEngine::Params p;
    p.semitones = value(pid::semitones);
    p.cents = value(pid::cents);
    p.mix = value(pid::mix);
    p.grainMs = value(pid::grainAuto) > 0.5f ? 0.0 : value(pid::grainMs);
    p.crossfade = value(pid::crossfade);
    p.lowCutHz = value(pid::lowCut);
    p.outputGainDb = value(pid::output);
    p.doubleTrack = value(pid::dblOn) > 0.5f;
    p.doubleDetuneCents = value(pid::dblDetune);
    p.doubleDelayMs = value(pid::dblDelay);
    p.doubleWidth = value(pid::dblWidth);
    p.doubleLevelDb = value(pid::dblLevel);
    p.monoInput = value(pid::monoInput) > 0.5f;
    engine.setParams(p);
}

void PitchProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numChannels = buffer.getNumChannels();
    for (int ch = getTotalNumInputChannels(); ch < numChannels; ++ch)
        buffer.clear(ch, 0, buffer.getNumSamples());

    pullParameters();

    float* left = numChannels > 0 ? buffer.getWritePointer(0) : nullptr;
    float* right = numChannels > 1 ? buffer.getWritePointer(1) : nullptr;
    engine.process(left, right, buffer.getNumSamples());
}

void PitchProcessor::setCurrentProgram(int index)
{
    const auto& presets = factoryPresets();
    if (index < 0 || index >= static_cast<int>(presets.size()))
        return;

    currentProgram = index;
    const auto& p = presets[static_cast<size_t>(index)];

    const auto set = [this](const char* id, float v)
    {
        if (auto* param = apvts.getParameter(id))
            param->setValueNotifyingHost(param->convertTo0to1(v));
    };

    set(pid::semitones, p.semitones);
    set(pid::cents, p.cents);
    set(pid::mix, p.mix);
    set(pid::grainAuto, p.grainAuto ? 1.0f : 0.0f);
    set(pid::grainMs, p.grainMs);
    set(pid::crossfade, p.crossfade);
    set(pid::lowCut, p.lowCut);
    set(pid::output, p.output);
    set(pid::dblOn, p.dblOn ? 1.0f : 0.0f);
    set(pid::dblDetune, p.dblDetune);
    set(pid::dblDelay, p.dblDelay);
    set(pid::dblWidth, p.dblWidth);
    set(pid::dblLevel, p.dblLevel);
    set(pid::monoInput, p.monoInput ? 1.0f : 0.0f);
}

const juce::String PitchProcessor::getProgramName(int index)
{
    const auto& presets = factoryPresets();
    if (index < 0 || index >= static_cast<int>(presets.size()))
        return {};
    return presets[static_cast<size_t>(index)].name;
}

void PitchProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto tree = apvts.copyState();
    tree.setProperty("program", currentProgram, nullptr);
    if (auto xml = tree.createXml())
        copyXmlToBinary(*xml, destData);
}

void PitchProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
    {
        auto tree = juce::ValueTree::fromXml(*xml);
        if (tree.isValid() && tree.hasType(apvts.state.getType()))
        {
            currentProgram = tree.getProperty("program", 0);
            apvts.replaceState(tree);
            pullParameters();
        }
    }
}

juce::AudioProcessorEditor* PitchProcessor::createEditor()
{
    return new PitchEditor(*this);
}

} // namespace hhps

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new hhps::PitchProcessor();
}
