// Plug-in level tests: the guarantees that need the real processor and its
// parameter tree. Runs headless, with no audio device and no editor window.

#include "../SourcePS/PitchProcessor.h"

#include <JuceHeader.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <set>

namespace
{
int failures = 0;

void check(bool condition, const juce::String& what, const juce::String& detail = {})
{
    std::printf("%s  %s%s\n", condition ? "PASS" : "FAIL", what.toRawUTF8(),
                detail.isEmpty() ? "" : (" \xe2\x80\x94 " + detail).toRawUTF8());
    if (! condition)
        ++failures;
}

float paramValue(hhps::PitchProcessor& proc, const char* id)
{
    auto* p = proc.state().getParameter(id);
    jassert(p != nullptr);
    return proc.state().getParameterRange(id).convertFrom0to1(p->getValue());
}

void setParam(hhps::PitchProcessor& proc, const char* id, float value)
{
    auto* p = proc.state().getParameter(id);
    jassert(p != nullptr);
    p->setValueNotifyingHost(proc.state().getParameterRange(id).convertTo0to1(value));
}

// Fills a buffer with a guitar-ish plucked tone: fundamental plus two
// harmonics, so the shifter is fed something with real structure. `sample` is
// the running position in the take, so the tone stays continuous across blocks
// however small they are.
void fillTone(juce::AudioBuffer<float>& buffer, double sampleRate, double hz, int64_t& sample)
{
    for (int i = 0; i < buffer.getNumSamples(); ++i, ++sample)
    {
        const double t = static_cast<double>(sample) / sampleRate;
        const double env = 0.4 + 0.6 * std::exp(-1.5 * std::fmod(t, 2.0));
        const float s = static_cast<float>(env * (0.6 * std::sin(juce::MathConstants<double>::twoPi * hz * t)
                                                + 0.3 * std::sin(juce::MathConstants<double>::twoPi * 2.0 * hz * t)
                                                + 0.1 * std::sin(juce::MathConstants<double>::twoPi * 3.0 * hz * t)));
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            buffer.setSample(ch, i, s);
    }
}

bool isFinite(const juce::AudioBuffer<float>& buffer)
{
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            if (! std::isfinite(buffer.getSample(ch, i)))
                return false;
    return true;
}

// Runs `blocks` blocks of the tone through the processor and reports the peak.
float runTone(hhps::PitchProcessor& proc, double sampleRate, int blockSize, int blocks,
              bool* finite = nullptr)
{
    juce::AudioBuffer<float> buffer(2, blockSize);
    juce::MidiBuffer midi;
    float peak = 0.0f;
    bool allFinite = true;
    int64_t sample = 0;

    for (int b = 0; b < blocks; ++b)
    {
        fillTone(buffer, sampleRate, 196.0, sample);
        proc.processBlock(buffer, midi);
        allFinite = allFinite && isFinite(buffer);
        peak = std::max(peak, buffer.getMagnitude(0, blockSize));
    }

    if (finite != nullptr)
        *finite = allFinite;
    return peak;
}
} // namespace

int main()
{
    const juce::ScopedJuceInitialiser_GUI juceInit;

    hhps::PitchProcessor proc;
    proc.prepareToPlay(48000.0, 256);

    // 1. Every parameter the editor and the presets reference must exist, and
    //    every ID must be unique.
    {
        const juce::StringArray ids { hhps::pid::semitones, hhps::pid::cents, hhps::pid::mix,
                                      hhps::pid::grainAuto, hhps::pid::grainMs, hhps::pid::crossfade,
                                      hhps::pid::lowCut, hhps::pid::output, hhps::pid::dblOn,
                                      hhps::pid::dblDetune, hhps::pid::dblDelay, hhps::pid::dblWidth,
                                      hhps::pid::dblLevel, hhps::pid::monoInput };
        bool allPresent = true;
        std::set<juce::String> seen;
        for (const auto& id : ids)
        {
            allPresent = allPresent && proc.state().getParameter(id) != nullptr;
            seen.insert(id);
        }
        check(allPresent, "every documented parameter exists on the plug-in");
        check(seen.size() == static_cast<size_t>(ids.size()), "parameter IDs are unique");
        check(proc.getNumPrograms() == static_cast<int>(hhps::factoryPresets().size()),
              "the host sees every factory preset",
              juce::String(proc.getNumPrograms()) + " presets");
    }

    // 2. Recalling a preset has to land the parameters on that preset's values,
    //    otherwise the preset menu lies to the player.
    {
        bool allMatched = true;
        juce::String firstMismatch;
        const auto& presets = hhps::factoryPresets();

        for (int i = 0; i < static_cast<int>(presets.size()); ++i)
        {
            const auto& preset = presets[static_cast<size_t>(i)];
            proc.setCurrentProgram(i);

            const auto matches = [&](const char* id, float expected)
            {
                const bool ok = std::abs(paramValue(proc, id) - expected) < 0.01f;
                if (! ok && firstMismatch.isEmpty())
                    firstMismatch = juce::String(preset.name) + "/" + id;
                return ok;
            };

            allMatched = matches(hhps::pid::semitones, preset.semitones) && allMatched;
            allMatched = matches(hhps::pid::cents, preset.cents) && allMatched;
            allMatched = matches(hhps::pid::mix, preset.mix) && allMatched;
            allMatched = matches(hhps::pid::grainMs, preset.grainMs) && allMatched;
            allMatched = matches(hhps::pid::crossfade, preset.crossfade) && allMatched;
            allMatched = matches(hhps::pid::lowCut, preset.lowCut) && allMatched;
            allMatched = matches(hhps::pid::output, preset.output) && allMatched;
            allMatched = matches(hhps::pid::dblOn, preset.dblOn ? 1.0f : 0.0f) && allMatched;
            allMatched = matches(hhps::pid::dblDetune, preset.dblDetune) && allMatched;
            allMatched = matches(hhps::pid::dblDelay, preset.dblDelay) && allMatched;
            allMatched = matches(hhps::pid::dblWidth, preset.dblWidth) && allMatched;
            allMatched = matches(hhps::pid::dblLevel, preset.dblLevel) && allMatched;
            allMatched = matches(hhps::pid::monoInput, preset.monoInput ? 1.0f : 0.0f) && allMatched;
            allMatched = matches(hhps::pid::grainAuto, preset.grainAuto ? 1.0f : 0.0f) && allMatched;

            check(proc.getProgramName(i) == juce::String(preset.name),
                  "preset " + juce::String(i) + " reports its name");
        }
        check(allMatched, "recalling a preset applies all of its values",
              firstMismatch.isEmpty() ? juce::String() : "first mismatch " + firstMismatch);
    }

    // 3. A session reload must restore the exact sound: save, scramble, restore.
    {
        proc.setCurrentProgram(1);
        setParam(proc, hhps::pid::semitones, -12.0f);
        setParam(proc, hhps::pid::cents, 33.5f);
        setParam(proc, hhps::pid::dblDetune, 27.0f);
        setParam(proc, hhps::pid::monoInput, 0.0f);

        juce::MemoryBlock saved;
        proc.getStateInformation(saved);

        setParam(proc, hhps::pid::semitones, 7.0f);
        setParam(proc, hhps::pid::cents, -80.0f);
        setParam(proc, hhps::pid::dblDetune, 4.0f);
        setParam(proc, hhps::pid::monoInput, 1.0f);

        proc.setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));

        const bool restored = std::abs(paramValue(proc, hhps::pid::semitones) + 12.0f) < 0.01f
                           && std::abs(paramValue(proc, hhps::pid::cents) - 33.5f) < 0.05f
                           && std::abs(paramValue(proc, hhps::pid::dblDetune) - 27.0f) < 0.05f
                           && paramValue(proc, hhps::pid::monoInput) < 0.5f;
        check(restored, "the plug-in state round-trips through the host");
        check(proc.getCurrentProgram() == 1, "the recalled state remembers its preset");

        // Garbage from a corrupt session must not take the plug-in down.
        const juce::String junk("not a valid plug-in state");
        proc.setStateInformation(junk.toRawUTF8(), static_cast<int>(junk.getNumBytesAsUTF8()));
        check(true, "a corrupt state block is ignored without crashing");
        proc.setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
    }

    // 4. Audio: the plug-in has to make finite sound on stereo and mono, and go
    //    quiet when it is fed silence.
    {
        proc.setCurrentProgram(1); // Guitar Double Track
        bool finite = false;
        const float peak = runTone(proc, 48000.0, 256, 200, &finite);
        check(finite, "processing stays finite on the doubler preset");
        check(peak > 0.05f, "the doubler preset makes audible output",
              "peak " + juce::String(peak, 3));

        juce::AudioBuffer<float> silence(2, 256);
        juce::MidiBuffer midi;
        for (int b = 0; b < 400; ++b)
        {
            silence.clear();
            proc.processBlock(silence, midi);
        }
        check(silence.getMagnitude(0, 256) < 1.0e-4f, "silence in, silence out",
              "residual " + juce::String(silence.getMagnitude(0, 256), 8));
    }

    // 5. Hosts hand out odd block sizes and sample rates; none of them may
    //    produce garbage.
    {
        bool allFinite = true;
        float minPeak = 1.0e9f;
        for (double sr : { 44100.0, 48000.0, 96000.0 })
        {
            for (int block : { 1, 17, 64, 1024 })
            {
                proc.prepareToPlay(sr, block);
                bool finite = false;
                minPeak = std::min(minPeak, runTone(proc, sr, block, 4096 / block + 1, &finite));
                allFinite = allFinite && finite;
            }
        }
        check(allFinite, "every sample rate and block size stays finite");
        check(minPeak > 0.0f, "no host configuration renders total silence");
    }

    // 6. Mono tracks are the common guitar case: the plug-in must accept a
    //    1-in/1-out layout and still pass audio.
    {
        juce::AudioProcessor::BusesLayout mono;
        mono.inputBuses.add(juce::AudioChannelSet::mono());
        mono.outputBuses.add(juce::AudioChannelSet::mono());
        check(proc.isBusesLayoutSupported(mono), "a mono in / mono out layout is supported");

        juce::AudioProcessor::BusesLayout stereo;
        stereo.inputBuses.add(juce::AudioChannelSet::stereo());
        stereo.outputBuses.add(juce::AudioChannelSet::stereo());
        check(proc.isBusesLayoutSupported(stereo), "a stereo layout is supported");

        proc.prepareToPlay(48000.0, 256);
        juce::AudioBuffer<float> monoBuffer(1, 256);
        juce::MidiBuffer midi;
        float peak = 0.0f;
        int64_t sample = 0;
        for (int b = 0; b < 100; ++b)
        {
            fillTone(monoBuffer, 48000.0, 196.0, sample);
            proc.processBlock(monoBuffer, midi);
            peak = std::max(peak, monoBuffer.getMagnitude(0, 256));
        }
        check(isFinite(monoBuffer) && peak > 0.05f, "a mono buffer processes to audible output",
              "peak " + juce::String(peak, 3));
    }

    // 7. Toggling the doubler mid-performance must not click: the voices run all
    //    the time and only their gain moves.
    {
        proc.prepareToPlay(48000.0, 256);
        proc.setCurrentProgram(0);
        setParam(proc, hhps::pid::mix, 100.0f);
        setParam(proc, hhps::pid::dblOn, 0.0f);

        juce::AudioBuffer<float> buffer(2, 256);
        juce::MidiBuffer midi;
        float worstStep = 0.0f;
        float previous = 0.0f;
        int64_t sample = 0;

        for (int b = 0; b < 120; ++b)
        {
            if (b == 40)
                setParam(proc, hhps::pid::dblOn, 1.0f);
            if (b == 80)
                setParam(proc, hhps::pid::dblOn, 0.0f);

            fillTone(buffer, 48000.0, 196.0, sample);
            proc.processBlock(buffer, midi);

            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                const float s = buffer.getSample(0, i);
                if (b > 4)
                    worstStep = std::max(worstStep, std::abs(s - previous));
                previous = s;
            }
        }
        check(worstStep < 0.25f, "switching Dual Track on and off does not click",
              "largest sample step " + juce::String(worstStep, 4));
    }

    // 8. The editor has to build and lay out headlessly, which is what a host
    //    does the first time the window opens.
    {
        std::unique_ptr<juce::AudioProcessorEditor> editor(proc.createEditor());
        const bool ok = editor != nullptr && editor->getWidth() > 0 && editor->getHeight() > 0;
        check(ok, "the editor builds with a valid size",
              editor != nullptr ? juce::String(editor->getWidth()) + "x" + juce::String(editor->getHeight())
                                : juce::String("no editor"));
    }

    proc.releaseResources();

    std::printf("\n%s (%d failures)\n", failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED", failures);
    return failures == 0 ? 0 : 1;
}
