#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <functional>
#include <vector>
#include <mutex>
#include "NoteData.hpp"
#include "MusicTheory.hpp"
// Forward declarations
class APCXAudioProcessorEditor;
class StepButton;
class APCXAudioProcessor : public juce::AudioProcessor,
                                  public juce::MidiInputCallback,
                                  public juce::Timer
{
public:
    APCXAudioProcessor();
    ~APCXAudioProcessor() override;
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;
    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    juce::AudioProcessorValueTreeState apvts;
    int getCurrentStep() const { return currentStep.load(std::memory_order_relaxed); }
    
    // Add a handler for the editor to register itself
    void setStepEventCallback(std::function<void(int)> callback) { 
        const std::lock_guard<std::mutex> lock(callbackMutex);
        stepEventCallback = callback; 
    }
    
    // BPM control - now updated from host
    void setTempo(double newTempo);
    double getTempo() const { return tempo; }
    
    // Register step buttons from the editor
    void registerStepButtons(juce::OwnedArray<StepButton>* buttons) {
        const std::lock_guard<std::mutex> lock(callbackMutex);
        editorStepButtons = buttons;
        // Cache update will be called separately
    }
    
    // Set MIDI channel
    void setMidiChannel(int channel);
    
    // Control playback - now updated from host transport
    void setPlaying(bool shouldPlay);
    bool isSequencerPlaying() const { return isPlaying; }
    
    // Get the current MIDI channel
    int getMidiChannel() const { return midiChannel; }
    
    // Get selected note and velocity
    int getSelectedNote() const { return selectedNote; }
    int getSelectedVelocity() const { return selectedVelocity; }
    
    // Set selected note and velocity
    void setSelectedNote(int note) { selectedNote = note; }
    void setSelectedVelocity(int velocity) { selectedVelocity = velocity; }
    
    // Current position info from the host
    juce::AudioPlayHead::CurrentPositionInfo currentPos;
    
    // Structure to cache step data for when editor is closed
    struct StepCache {
        std::vector<NoteData> notes;
    };
    
    // Get step cache data
    const std::vector<StepCache>& getStepCache() const { return stepCache; }
    
    // Update the cached step data from the editor buttons
    void updateStepCache();
    // MIDI hardware control
    void findAndConnectToAPC();
    bool isHardwareConnected() const { return hardwareConnected; }
    bool isShiftPressed() const { return shiftPressed.load(std::memory_order_relaxed); }
    void sendMessage(const juce::MidiMessage& message);
    
    // Send a MIDI message immediately to the output
    void sendImmediateMidiMessage(const juce::MidiMessage& message);
    
    // Add method to get MIDI output
    juce::MidiOutput* getMidiOutput() const { return midiOutput.get(); }
    
    // Handle MIDI input messages
    void handleIncomingMidiMessage(juce::MidiInput* source, const juce::MidiMessage& message) override;
    
    // Hardware LED update methods
    void updateStepButtonOnHardware(int index, bool isCurrentStep);
    void updateNoteButtonOnHardware(int index, bool isSelected);
    void updateModifierButtonOnHardware(int index, bool isSelected, int velocity);
    
    // Update all hardware LEDs
    void syncHardwareToCurrentState();
    
    // Shift-mode hardware LED overlay (scale/key/octave)
    void updateHardwareMenu();
    
    // Update just the note buttons based on current scale/key
    void syncNoteButtonsToCurrentScale();
    
    // Add a callback for handling hardware button presses
    void setHardwareButtonCallback(std::function<void(int, int)> callback) {
        const std::lock_guard<std::mutex> lock(callbackMutex);
        hardwareButtonCallback = callback;
    }
    
    // Register note buttons for hardware updates
    void registerNoteSelectionState(std::vector<bool> noteSelections) {
        const std::lock_guard<std::mutex> lock(callbackMutex);
        noteButtonSelections = noteSelections;
    }
    
    // Register velocity buttons for hardware updates
    void registerVelocitySelectionState(std::vector<bool> velocitySelections, std::vector<int> velocityValues) {
        const std::lock_guard<std::mutex> lock(callbackMutex);
        velocityButtonSelections = velocitySelections;
        velocityButtonValues = velocityValues;
    }
    // Add method to check if MIDI devices are still connected
    bool checkMidiConnectionStatus();
    // Get music theory instance
    const MusicTheory& getMusicTheory() const { return musicTheory; }
    
    /**
     * Set the root note (key) for the musical scale
     * @param rootNote MIDI note number (0-11, where 0 = C, 1 = C#, etc.)
     */
    void setRootNote(int rootNote);
    
    /**
     * Get the current root note
     * @return The current root note (0-11)
     */
    int getRootNote() const { return musicTheory.getRootNote(); }
    
    /**
     * Set the scale type
     * @param scaleType Scale type enum value (see MusicTheory::ScaleType)
     */
    void setScaleType(MusicTheory::ScaleType scaleType);
    
    /**
     * Get the current scale type
     * @return The current scale type
     */
    MusicTheory::ScaleType getScaleType() const { return musicTheory.getScaleType(); }
    
    /**
     * Set the octave transposition
     * @param octaves Number of octaves to transpose (-4 to +4)
     */
    void setOctaveTransposition(int octaves);
    
    /**
     * Get the current octave transposition
     * @return The current octave transposition
     */
    int getOctaveTransposition() const { return musicTheory.getOctaveTransposition(); }
    
    /**
     * Check if a MIDI note is in the current scale
     * @param midiNote The MIDI note number to check
     * @return True if the note is in the scale
     */
    bool isNoteInScale(int midiNote) const { return musicTheory.isNoteInScale(midiNote); }
    // Timer callback
    void timerCallback() override;
private:
    // Structure to track notes that need to be turned off
    struct PendingNoteOff {
        int noteNumber;
        int channel;
        int64_t sendTime;
        bool active;
    };
    // Timing variables
    double tempo = 120.0;  // Default tempo in BPM
    double currentSampleRate = 44100.0;  // Default sample rate
    std::atomic<int> sampleCounter{0};
    std::atomic<int> samplesPerStep{0};
    int64_t currentSamplePos = 0;
    std::atomic<int> currentStep{0};
    
    // Callback to notify the editor when a step change occurs
    std::function<void(int)> stepEventCallback;
    
    // MIDI output parameters
    int midiChannel = 1;
    bool isPlaying = true;
    bool sendAllNotesOff = false;
    
    // Note management
    std::vector<PendingNoteOff> pendingNoteOffs;
    
    // Link to editor's step buttons (to get note data)
    juce::OwnedArray<StepButton>* editorStepButtons = nullptr;
    
    // Mutex to protect callback and step buttons access
    std::mutex callbackMutex;
    // Cache of step data for when editor is closed
    std::vector<StepCache> stepCache;
    // Generate MIDI messages for the current step
    /**
     * Generate MIDI note-on messages for the current step
     * @param buffer The MIDI buffer to add events to
     * @param samplePosition The sample position within the current buffer
     */
    void generateMidiMessagesForStep(juce::MidiBuffer& buffer, int samplePosition);
    
    /**
     * Helper to get the step data for a specific step index
     * @param stepIndex The step index to retrieve
     * @return Pointer to the StepButton, or nullptr if not found
     */
    StepButton* getStepData(int stepIndex);
    
    /**
     * Update tempo from the host's playhead
     */
    void updateTempoFromHost();
    // MIDI device management for hardware
    std::unique_ptr<juce::MidiOutput> midiOutput;
    std::unique_ptr<juce::MidiInput> midiInput;
    bool hardwareConnected = false;
    
    // Hardware grid mapping
    int mapToHardwareIndex(int row, int col);
    
    // Hardware button press callback
    std::function<void(int, int)> hardwareButtonCallback;
    
    // Current selections for hardware updates
    int selectedNote = -1;
    int selectedVelocity = 127;
    std::vector<bool> noteButtonSelections;
    std::vector<bool> velocityButtonSelections;
    std::vector<int> velocityButtonValues;
    
    // APC Mini MK2 color indices (from the official documentation)
    // These are the velocity values to send with Note On messages
    const int colorOff = 0;       // Off (#000000)
    const int colorGreen = 21;    // Green (#00FF00)
    const int colorRed = 5;       // Red (#FF0000)
    const int colorYellow = 13;   // Yellow (#FFFF00)
    const int colorBlue = 45;     // Blue (#0000FF)
    const int colorMagenta = 53;  // Magenta/Purple (#FF00FF)
    const int colorCyan = 37;     // Cyan (#00A9FF)
    const int colorWhite = 3;     // White (#FFFFFF)
    
    // MIDI channels for brightness (from the documentation)
    // Channel numbers are 0-indexed in MIDI messages (channel 1 in MIDI is 0 in code)
    const int channelDimmest = 1;  // Channel 1 - 10% brightness
    const int channelDimmer = 2;   // Channel 2 - 20% brightness
    const int channelDim = 3;      // Channel 3 - 30% brightness
    const int channelMedium = 4;   // Channel 4 - 50% brightness
    const int channelBright = 5;   // Channel 5 - 75% brightness
    const int channelBrighter = 6; // Channel 6 - 90% brightness
    const int channelFull = 7;     // Channel 7 - 100% brightness
    // Music theory handler
    MusicTheory musicTheory;
    std::atomic<bool> shiftPressed{ false };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(APCXAudioProcessor)
};
