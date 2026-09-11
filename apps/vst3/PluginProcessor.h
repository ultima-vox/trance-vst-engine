#pragma once
#include <JuceHeader.h>
#include <array>
#include <memory>
#include "instrument/InstrumentRegistry.h"
#include "midi/GeneratedNoteScheduler.h"
#include "midi/SourceSelector.h"
#include "parts/PartState.h"
#include "preset/PresetManager.h"
#include "rack/Rack.h"
#include "rack/RackState.h"
#include "sequence/Sequence.h"
#include "sequence/RealtimeSequenceBridge.h"

class VstEngineAudioProcessor final : public juce::AudioProcessor {
public:
    // Canonical MIDI source mode (AUTO / PIANO ROLL / GENERATOR / BOTH).
    // The decision rule lives in vstengine::midi::shouldRunGenerator() so the
    // full mode x input-presence matrix is unit-tested in isolation.
    using MidiSourceMode = vstengine::midi::SourceMode;

    VstEngineAudioProcessor();
    ~VstEngineAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override
    {
        return currentSampleRate > 0.0
            ? static_cast<double>(rack.tailSamples()) / currentSampleRate : 0.0;
    }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    juce::AudioProcessorValueTreeState& parameters() noexcept { return apvts; }
    juce::MidiKeyboardState& bassKeyboardState() noexcept { return keyboard; }
    juce::MidiKeyboardState& keyboardState() noexcept { return keyboard; }
    vstengine::sequence::Sequence& partSequence(int partIndex) noexcept
    {
        return parts[static_cast<std::size_t>(partIndex == 0 ? 0 : 1)].sequence;
    }
    vstengine::sequence::Sequence& sequence() noexcept { return partSequence(0); }
    vstengine::PresetManager* presetManager() noexcept { return presetManager_.get(); }
    int getPartPlayHeadStep(int partIndex) const noexcept
    {
        return partIndex == 0 ? scheduler.playHeadStep() : -1;
    }
    int getCurrentPlayHeadStep() const noexcept { return getPartPlayHeadStep(0); }
    void requestPanic() noexcept { panicRequested.store(true); }
    void publishPartSequenceForAudio(int partIndex) noexcept;
    void publishSequenceForAudio() noexcept { publishPartSequenceForAudio(0); }
    juce::File createPartMidiFile(int partIndex);
    juce::File createGeneratedMidiFile() { return createPartMidiFile(0); }
    [[nodiscard]] vstengine::parts::PartRegistry& partRegistry() noexcept
    {
        return parts;
    }
    [[nodiscard]] bool isPartLocked(int partIndex) const noexcept;
    [[nodiscard]] vstengine::rack::Rack& instrumentRack() noexcept { return rack; }
    [[nodiscard]] const vstengine::rack::Rack& instrumentRack() const noexcept { return rack; }
    void selectSlot(std::size_t index) noexcept
    {
        selectedSlot.store(juce::jlimit<std::size_t>(0,
            vstengine::instrument::maxSlots - 1, index));
    }
    [[nodiscard]] std::size_t selectedSlotIndex() const noexcept
    {
        return selectedSlot.load();
    }
    enum class ChannelConflictAction { reject, swap, move, layer };
    [[nodiscard]] std::span<const vstengine::instrument::InstrumentDescriptor* const>
        availableInstruments() const noexcept
    {
        return instrumentRegistry.descriptors();
    }
    bool loadSlotInstrument(std::size_t, std::string_view,
                            juce::String& diagnostic);
    bool assignSlotChannel(std::size_t, int channel,
                           ChannelConflictAction, juce::String& diagnostic);
    [[nodiscard]] int nextFreeChannel() const noexcept;

private:
    // APVTS bridge passed to the preset module (vstengine::PresetStateStore).
    class ApvtsPresetStore final : public vstengine::PresetStateStore {
    public:
        explicit ApvtsPresetStore(juce::AudioProcessorValueTreeState& s)
            : apvts(s) {}
        [[nodiscard]] juce::ValueTree copyState() const override
        {
            return apvts.copyState();
        }
        void replaceState(juce::ValueTree newState) override
        {
            apvts.replaceState(std::move(newState));
        }
        [[nodiscard]] float convertTo0to1(const char* parameterId,
                                          float plainValue) const override
        {
            auto* ranged = apvts.getParameter(parameterId);
            return ranged != nullptr ? ranged->convertTo0to1(plainValue)
                                     : plainValue;
        }

    private:
        juce::AudioProcessorValueTreeState& apvts;
    };

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void seedInitialSequence();
    static bool containsNoteEvents(const juce::MidiBuffer& midi) noexcept;
    MidiSourceMode currentMidiMode() const noexcept;
    bool keyboardHasActiveNotes() const noexcept;
    void syncPartControlsFromParameters(
        vstengine::parts::PartRegistry& destination) noexcept;
    void syncParametersFromParts();
    void syncRackControlsFromParameters() noexcept;
    void cacheRackParameterPointers();
    std::size_t convertMidi(const juce::MidiBuffer&,
                            std::array<VoxMidiEventV1,
                                vstengine::rack::RackRouter::eventCapacity>&,
                            std::uint32_t sampleCount) noexcept;

    juce::MidiKeyboardState keyboard;
    juce::AudioProcessorValueTreeState apvts;
    vstengine::instrument::InstrumentRegistry instrumentRegistry;
    vstengine::rack::Rack rack;
    vstengine::parts::PartRegistry parts;
    vstengine::sequence::RealtimeSequenceBridge sequenceBridge;
    vstengine::sequence::Sequence audioSequence { 16 };
    ApvtsPresetStore presetStore;
    std::unique_ptr<vstengine::PresetManager> presetManager_;
    // Generated-playback state machine (audio thread, see libs/midi).
    vstengine::midi::GeneratedNoteScheduler scheduler;
    double currentSampleRate { 44100.0 };
    bool wasTransportPlaying { false };
    std::atomic<bool> panicRequested { false };

    // Preallocated scratch buffer for keyboard MIDI. processBlock() reuses it
    // every block (clear, processNextMidiBuffer, merge) so the realtime
    // callback never constructs a local MidiBuffer or grows one past this
    // reserved capacity. Capacity is reserved once in prepareToPlay.
    juce::MidiBuffer keyboardScratch;
    juce::MidiBuffer generatedScratch;
    std::array<bool, 128> auditionNotes {};
    std::array<VoxMidiEventV1, vstengine::rack::RackRouter::eventCapacity>
        hostEvents {};
    std::array<VoxMidiEventV1, vstengine::rack::RackRouter::eventCapacity>
        auditionEvents {};
    std::array<VoxMidiEventV1, vstengine::rack::RackRouter::eventCapacity>
        generatedEvents {};
    std::atomic<std::size_t> selectedSlot { 0 };
    struct RackParameterRefs {
        std::atomic<float>* midiIn {};
        std::atomic<float>* layer {};
        std::atomic<float>* keyLow {};
        std::atomic<float>* keyHigh {};
        std::atomic<float>* velocityLow {};
        std::atomic<float>* velocityHigh {};
        std::atomic<float>* transpose {};
        std::atomic<float>* enabled {};
        std::atomic<float>* mute {};
        std::atomic<float>* solo {};
        std::atomic<float>* locked {};
        std::atomic<float>* level {};
        std::atomic<float>* pan {};
        std::array<std::atomic<float>*, vstengine::instrument::macrosPerSlot>
            macros {};
    };
    std::array<RackParameterRefs, vstengine::instrument::maxSlots>
        rackParameterRefs {};
    std::array<vstengine::rack::ProcessSlotControls,
               vstengine::instrument::maxSlots> rackControls {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VstEngineAudioProcessor)
};
