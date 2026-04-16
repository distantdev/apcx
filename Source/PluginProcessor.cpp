// SPDX-License-Identifier: AGPL-3.0-or-later

#include "PluginProcessor.hpp"
#include "PluginEditor.hpp"
#include <pluginterfaces/base/ftypes.h>
#include "StepButton.hpp"
#include "HardwareDisplay.hpp"

APCXAudioProcessor::APCXAudioProcessor()
    : AudioProcessor(BusesProperties() 
        // No audio inputs or outputs, just MIDI
    ), 
    apvts(*this, nullptr, "Parameters", {
        std::make_unique<juce::AudioParameterInt>(juce::ParameterID{"rootNote", 1}, "Root Note", 0, 11, 0),
        std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"scaleType", 1}, "Scale Type", 
            juce::StringArray{"Chromatic", "Major", "Minor", "Harmonic Minor", "Melodic Minor", "Dorian", "Phrygian", "Lydian", "Mixolydian", "Locrian", "Blues", "Pentatonic Major", "Pentatonic Minor"}, 0),
        std::make_unique<juce::AudioParameterInt>(juce::ParameterID{"octave", 1}, "Octave", -4, 4, 0),
        std::make_unique<juce::AudioParameterInt>(juce::ParameterID{"midiChannel", 1}, "MIDI Channel", 1, 16, 1)
    }),
    sampleCounter(0),
    samplesPerStep(0), 
    currentSamplePos(0),
    currentStep(0),
    isPlaying(true),
    tempo(120.0),
    currentSampleRate(44100.0),
    sendAllNotesOff(false),
    hardwareConnected(false),
    selectedNote(-1),
    selectedVelocity(127),
    midiChannel(1) // Default to MIDI channel 1
{
    // Initialize with playing state so it responds to host transport
    
    // Initialize step cache with empty steps
    stepCache.resize(32); // Assuming 32 steps in the sequencer
    
    // Initialize selection state vectors
    noteButtonSelections.resize(16, false);
    velocityButtonSelections.resize(16, false);
    velocityButtonValues.resize(16, 0);

    // Set default velocity button values
    for (int i = 0; i < 16; i++) {
        velocityButtonValues[i] = (i + 1) * 8; // 8, 16, 24... up to 128
    }

    // Initialize music theory settings with defaults
    musicTheory.setRootNote(0);  // C
    musicTheory.setScaleType(MusicTheory::ScaleType::Major);
    musicTheory.setOctaveTransposition(0);

    // Try to connect to the APC hardware
    findAndConnectToAPC();
    
    // Start the timer to check for hardware connection
    startTimer(500); // Check every 500ms
    
    // If hardware is connected, initialize its state
    if (hardwareConnected && midiOutput != nullptr) {
        syncHardwareToCurrentState();
    }
}

APCXAudioProcessor::~APCXAudioProcessor()
{
    // Stop the timer
    stopTimer();
    
    // Clear any references to the editor
    const std::lock_guard<std::mutex> lock(callbackMutex);
    stepEventCallback = nullptr;
    editorStepButtons = nullptr;
    
    // Ensure all notes are off
    pendingNoteOffs.clear();
}

const juce::String APCXAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

void APCXAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    // Use the sample rate for timing calculations
    currentSampleRate = sampleRate;
    
    // Reset timing variables
    sampleCounter = 0;
    currentSamplePos = 0;
    
    // Calculate samples per step based on host tempo
    updateTempoFromHost();
    
    // Clear any pending notes
    pendingNoteOffs.clear();
    
    // Reset sequencer state
    if (!isPlaying)
    {
        currentStep = 0;
        
        // Update UI if available
        const std::lock_guard<std::mutex> lock(callbackMutex);
        if (stepEventCallback)
        {
            stepEventCallback(0);
        }
    }
    
    DBG("Plugin prepared with sample rate: " + juce::String(sampleRate) + 
        " Hz, buffer size: " + juce::String(samplesPerBlock));
}

void APCXAudioProcessor::releaseResources()
{
    // Free any resources that are no longer needed
    
    // Send all-notes-off when plugin is released/deactivated
    sendAllNotesOff = true;
    
    // Clear any pending notes
    pendingNoteOffs.clear();
    
    // Immediately send all-notes-off and all-sound-off to the host
    for (int channel = 1; channel <= 16; ++channel) {
        sendImmediateMidiMessage(juce::MidiMessage::allNotesOff(channel));
        sendImmediateMidiMessage(juce::MidiMessage::allSoundOff(channel));
    }
    
    // Turn off all hardware lights
    if (hardwareConnected && midiOutput != nullptr) {
        for (int i = 0; i < 64; ++i) {
            sendMessage(juce::MidiMessage::noteOn(channelFull, i, static_cast<juce::uint8>(colorOff)));
        }
    }
}

void APCXAudioProcessor::updateTempoFromHost()
{
    // Get tempo from host if available
    auto* localPlayHead = getPlayHead();
    if (localPlayHead != nullptr)
    {
        // Capture previous state
        bool wasPlaying = isPlaying;
        
        // Get updated position information
        juce::AudioPlayHead::CurrentPositionInfo newPos;
        bool gotNewPosition = localPlayHead->getCurrentPosition(newPos);
        
        if (gotNewPosition)
        {
            // Update internal tempo to match host
            setTempo(newPos.bpm);
            
            // Update playback state based on host transport
            bool nowPlaying = newPos.isPlaying;
            
            // Detect transport state changes
            if (wasPlaying && !nowPlaying)
            {
                // Immediately clear notes to avoid hanging
                sendAllNotesOff = true;
                pendingNoteOffs.clear();
            }
            
            // Now update the position info and state
            currentPos = newPos;
            setPlaying(nowPlaying);
        }
    }
    else
    {
        // Use default tempo if host info not available
        // 120 BPM is a standard default tempo
        setTempo(120.0);
    }
}

void APCXAudioProcessor::setTempo(double newTempo)
{
    if (tempo != newTempo)
    {
        tempo = newTempo;
        
        // Calculate samples per step based on tempo
        // For 4/4 time signature at 16th notes:
        // (60 seconds / BPM) = seconds per quarter note
        // (60 / BPM) / 4 = seconds per 16th note
        // seconds per 16th note * sample rate = samples per step
        double secondsPerBeat = 60.0 / tempo;
        double secondsPerStep = secondsPerBeat / 4.0; // Assuming 16th notes
        samplesPerStep = static_cast<int>(secondsPerStep * getSampleRate());
        
        DBG("Tempo changed to " + juce::String(newTempo) + " BPM, samples per step: " + juce::String(samplesPerStep));
    }
}

void APCXAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    // Capture current playing state before updating
    bool wasPlaying = isPlaying;
    
    // Get latest tempo and transport info from host
    updateTempoFromHost();
    
    // Check if playback just stopped
    if (wasPlaying && !isPlaying)
    {
        // Send all-notes-off to both output and pending notes
        for (int channel = 1; channel <= 16; ++channel) {
            midiMessages.addEvent(juce::MidiMessage::allNotesOff(channel), 0);
            midiMessages.addEvent(juce::MidiMessage::allSoundOff(channel), 0);
        }
        
        // Clear any pending notes immediately
        pendingNoteOffs.clear();
    }
    
    const int numSamples = buffer.getNumSamples();
    
    // Create a new MIDI buffer for our output
    juce::MidiBuffer outputBuffer;
    
    // Process any input MIDI messages - pass them through
    for (const auto metadata : midiMessages)
    {
        // Always add incoming MIDI to the output
        outputBuffer.addEvent(metadata.getMessage(), metadata.samplePosition);
    }
    
    // Process pending note-offs
    for (auto& noteOff : pendingNoteOffs)
    {
        if (noteOff.sendTime <= currentSamplePos)
        {
            outputBuffer.addEvent(juce::MidiMessage::noteOff(noteOff.channel, noteOff.noteNumber), 0);
            noteOff.active = false;
        }
    }
    
    // Remove any note-offs that have been sent
    pendingNoteOffs.erase(
        std::remove_if(pendingNoteOffs.begin(), pendingNoteOffs.end(), 
                      [](const PendingNoteOff& noteOff) { return !noteOff.active; }),
        pendingNoteOffs.end());
    
    // Only process sequencer if playing
    if (isPlaying)
    {
        // Check if we should sync to host position
        if (currentPos.isPlaying && currentPos.ppqPosition >= 0.0)
        {
            // Calculate which step we should be on based on host PPQ position
            // For 16th notes (4 per quarter note)
            double stepsPerQuarter = 4.0;
            double totalSteps = currentPos.ppqPosition * stepsPerQuarter;
            
            // Map to our 32-step grid (looping every 2 bars of 16th notes)
            int hostStep = static_cast<int>(std::fmod(totalSteps, 32.0));
            
            // If the host step is different from our current step, update and trigger notes
            if (hostStep != currentStep.load())
            {
                int prevStep = currentStep.load();
                currentStep = hostStep;
                
                // Generate MIDI for the new step
                generateMidiMessagesForStep(outputBuffer, 0);
                
                // Notify UI of step change
                if (stepEventCallback)
                {
                    stepEventCallback(hostStep);
                }
                
                // Update hardware if connected
                if (hardwareConnected) {
                    updateStepButtonOnHardware(hostStep, true);
                    updateStepButtonOnHardware(prevStep, false);
                }
            }
        }
        else
        {
            // Fallback to our internal timing if host doesn't provide position
            for (int sample = 0; sample < numSamples; ++sample)
            {
                if (sampleCounter >= samplesPerStep)
                {
                    sampleCounter = 0;
                    
                    // Update step first
                    int newStep = (currentStep + 1) % 32; // 32 steps = 2 bars of 16th notes
                    int prevStep = currentStep.load();
                    currentStep = newStep;
                    
                    // Generate MIDI messages for the updated step
                    generateMidiMessagesForStep(outputBuffer, sample);
                    
                    // Trigger step event (UI update)
                    if (stepEventCallback)
                    {
                        stepEventCallback(newStep);
                    }
                    
                    // Always update the hardware representation of the current step
                    if (hardwareConnected) {
                        updateStepButtonOnHardware(newStep, true);
                        updateStepButtonOnHardware(prevStep, false);
                    }
                }
                ++sampleCounter;
            }
        }
    }
    
    // If we need to send all-notes-off
    if (sendAllNotesOff)
    {
        for (int channel = 1; channel <= 16; ++channel) {
            // Send both all-notes-off and all-sound-off messages for maximum compatibility
            outputBuffer.addEvent(juce::MidiMessage::allNotesOff(channel), 0);
            outputBuffer.addEvent(juce::MidiMessage::allSoundOff(channel), 0);
            
            // Also send individual note-offs for all possible MIDI notes
            for (int note = 0; note < 128; ++note) {
                outputBuffer.addEvent(juce::MidiMessage::noteOff(channel, note), 0);
            }
        }
        sendAllNotesOff = false;
        
        // Also clear all pending note-offs
        pendingNoteOffs.clear();
    }
    
    // Replace the input buffer with our processed buffer
    midiMessages.swapWith(outputBuffer);
    
    // Update current sample position
    currentSamplePos += numSamples;
}

void APCXAudioProcessor::generateMidiMessagesForStep(juce::MidiBuffer& buffer, int samplePosition)
{
    // Get the current step index
    int currentStepIndex = currentStep.load();
    
    // Try to get the step data
    StepButton* stepData = nullptr;
    
    {
        const std::lock_guard<std::mutex> lock(callbackMutex);
        if (editorStepButtons && currentStepIndex >= 0 && currentStepIndex < editorStepButtons->size())
            stepData = editorStepButtons->operator[](currentStepIndex);
    }
    
    // If we have step data from the editor, use it
    if (stepData != nullptr) 
    {
        // Generate MIDI notes from the editor's button data
        auto notes = stepData->getNotes(); // Make a copy to avoid threading issues
        
        // Add MIDI note-on messages for each note in this step
        for (const auto& note : notes) {
            buffer.addEvent(juce::MidiMessage::noteOn(midiChannel, note.note, 
                                                      static_cast<juce::uint8>(note.velocity)), 
                            samplePosition);
            
            // Calculate when to turn this note off (16th note duration)
            int noteDurationInSamples = samplesPerStep;
            
            // Add to pending note-offs list
            pendingNoteOffs.push_back({
                note.note,
                midiChannel,
                currentSamplePos + noteDurationInSamples,
                true
            });
        }
    }
    else if (currentStepIndex >= 0 && currentStepIndex < static_cast<int>(stepCache.size())) 
    {
        // Use our cached step data instead if the editor is not available
        std::vector<NoteData> notes;
        
        {
            const std::lock_guard<std::mutex> lock(callbackMutex);
            notes = stepCache[currentStepIndex].notes; // Make a copy to avoid threading issues
        }
        
        // Add MIDI note-on messages for each note in this step
        for (const auto& note : notes) {
            buffer.addEvent(juce::MidiMessage::noteOn(midiChannel, note.note, 
                                                      static_cast<juce::uint8>(note.velocity)), 
                            samplePosition);
            
            // Calculate when to turn this note off (16th note duration)
            int noteDurationInSamples = samplesPerStep;
            
            // Add to pending note-offs list
            pendingNoteOffs.push_back({
                note.note,
                midiChannel,
                currentSamplePos + noteDurationInSamples,
                true
            });
        }
    }
}

StepButton* APCXAudioProcessor::getStepData(int stepIndex)
{
    const std::lock_guard<std::mutex> lock(callbackMutex);
    if (editorStepButtons && stepIndex >= 0 && stepIndex < 32)
        return editorStepButtons->operator[](stepIndex);
    
    return nullptr;
}

void APCXAudioProcessor::setMidiChannel(int channel)
{
    midiChannel = channel;
    // Channel has been updated
}

void APCXAudioProcessor::setPlaying(bool shouldPlay)
{
    if (isPlaying != shouldPlay)
    {
        isPlaying = shouldPlay;
        
        // If stopping, ensure all notes are off
        if (!shouldPlay)
        {
            // Clear pending note-offs
            pendingNoteOffs.clear();
            
            // Set flag to send all-notes-off message on next processBlock call
            sendAllNotesOff = true;
            
            // Reset current step to 0 when stopping playback
            currentStep = 0;
            
            // Update UI if callback is available
            if (stepEventCallback)
            {
                stepEventCallback(0);
            }
        }
    }
}

bool APCXAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    // Explicitly acknowledge unused parameter
    juce::ignoreUnused(layouts);
    
    // We support any layout since this is a MIDI-only plugin
    return true;
}

bool APCXAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* APCXAudioProcessor::createEditor()
{
    return new APCXAudioProcessorEditor(*this);
}

void APCXAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    // Create XML to store plugin state
    juce::XmlElement xml("APCXState");
    
    // Store music theory settings
    xml.setAttribute("rootNote", musicTheory.getRootNote());
    xml.setAttribute("scaleType", static_cast<int>(musicTheory.getScaleType()));
    xml.setAttribute("octaveTransposition", musicTheory.getOctaveTransposition());
    
    // Store MIDI channel
    xml.setAttribute("midiChannel", midiChannel);
    
    // Store step cache data
    juce::XmlElement* stepsXml = xml.createNewChildElement("Steps");
    for (size_t i = 0; i < stepCache.size(); ++i)
    {
        juce::XmlElement* stepXml = stepsXml->createNewChildElement("Step");
        stepXml->setAttribute("index", static_cast<int>(i));
        
        // Store all notes for this step
        for (const auto& note : stepCache[i].notes)
        {
            juce::XmlElement* noteXml = stepXml->createNewChildElement("Note");
            noteXml->setAttribute("note", note.note);
            noteXml->setAttribute("velocity", note.velocity);
            noteXml->setAttribute("panning", note.panning);
            noteXml->setAttribute("probability", note.probability);
        }
    }
    
    // Convert XML to binary and store in destData
    juce::MemoryOutputStream stream(destData, true);
    xml.writeTo(stream);
}

void APCXAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    // Parse the binary data back into XML
    std::unique_ptr<juce::XmlElement> xmlState(juce::XmlDocument::parse(
        juce::String::createStringFromData(data, sizeInBytes)));
        
    if (xmlState != nullptr
        && (xmlState->hasTagName("APCXState") || xmlState->hasTagName("APCSequencerState")))
    {
        // Restore music theory settings
        if (xmlState->hasAttribute("rootNote"))
            musicTheory.setRootNote(xmlState->getIntAttribute("rootNote"));
            
        if (xmlState->hasAttribute("scaleType"))
            musicTheory.setScaleType(static_cast<MusicTheory::ScaleType>(
                xmlState->getIntAttribute("scaleType")));
                
        if (xmlState->hasAttribute("octaveTransposition"))
            musicTheory.setOctaveTransposition(xmlState->getIntAttribute("octaveTransposition"));
            
        // Restore MIDI channel
        if (xmlState->hasAttribute("midiChannel"))
            midiChannel = xmlState->getIntAttribute("midiChannel");
            
        // Restore step cache data
        if (auto* stepsXml = xmlState->getChildByName("Steps"))
        {
            // Clear existing cache
            stepCache.clear();
            stepCache.resize(32); // Reset to default size
            
            // Restore each step
            forEachXmlChildElement(*stepsXml, stepXml)
            {
                int index = stepXml->getIntAttribute("index");
                if (index >= 0 && index < static_cast<int>(stepCache.size()))
                {
                    // Clear existing notes for this step
                    stepCache[index].notes.clear();
                    
                    // Restore all notes for this step
                    forEachXmlChildElement(*stepXml, noteXml)
                    {
                        if (noteXml->hasTagName("Note"))
                        {
                            int note = noteXml->getIntAttribute("note");
                            int velocity = noteXml->getIntAttribute("velocity");
                            float panning = noteXml->getDoubleAttribute("panning", 0.0f);
                            float probability = noteXml->getDoubleAttribute("probability", 1.0f);
                            
                            stepCache[index].notes.push_back(NoteData(note, velocity, panning, probability));
                        }
                    }
                }
            }
        }
        
        // Update hardware if connected
        if (hardwareConnected && midiOutput != nullptr)
        {
            syncHardwareToCurrentState();
        }
    }
}

bool APCXAudioProcessor::acceptsMidi() const
{
    return true;
}

bool APCXAudioProcessor::producesMidi() const
{
    return true;
}

bool APCXAudioProcessor::isMidiEffect() const
{
    return true;
}

double APCXAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int APCXAudioProcessor::getNumPrograms()
{
    return 1;
}

int APCXAudioProcessor::getCurrentProgram()
{
    return 0;
}

void APCXAudioProcessor::setCurrentProgram(int index)
{
    // Explicitly acknowledge unused parameter
    juce::ignoreUnused(index);
}

const juce::String APCXAudioProcessor::getProgramName(int index)
{
    // Explicitly acknowledge unused parameter
    juce::ignoreUnused(index);
    return {};
}

void APCXAudioProcessor::changeProgramName(int index, const juce::String& newName)
{
    // Explicitly acknowledge unused parameters
    juce::ignoreUnused(index, newName);
}

void APCXAudioProcessor::timerCallback()
{
    // Periodically check if the APC is connected
    checkMidiConnectionStatus();
}

void APCXAudioProcessor::updateStepCache()
{
    const std::lock_guard<std::mutex> lock(callbackMutex);
    
    // If no editor buttons, just return
    if (!editorStepButtons)
        return;
    
    // Update the cache from the editor buttons
    for (int i = 0; i < editorStepButtons->size() && i < static_cast<int>(stepCache.size()); ++i)
    {
        StepButton* stepButton = editorStepButtons->operator[](i);
        if (stepButton)
        {
            // Clear existing cache for this step
            stepCache[i].notes.clear();
            
            // Copy all notes from this step button
            for (const auto& note : stepButton->getNotes())
            {
                stepCache[i].notes.push_back(note);
            }
        }
    }
    
    DBG("Step cache updated with " + juce::String(stepCache.size()) + " steps");
}

bool APCXAudioProcessor::checkMidiConnectionStatus()
{
    // If we're already connected, verify the connection is still valid
    if (hardwareConnected)
    {
        bool inputFound = false;
        bool outputFound = false;
        
        // Check if our current devices are still in the available devices lists
        if (midiInput != nullptr)
        {
            juce::String inputId = midiInput->getIdentifier();
            auto availableInputs = juce::MidiInput::getAvailableDevices();
            for (const auto& device : availableInputs)
            {
                if (device.identifier == inputId)
                {
                    inputFound = true;
                    break;
                }
            }
        }
        
        if (midiOutput != nullptr)
        {
            juce::String outputId = midiOutput->getIdentifier();
            auto availableOutputs = juce::MidiOutput::getAvailableDevices();
            for (const auto& device : availableOutputs)
            {
                if (device.identifier == outputId)
                {
                    outputFound = true;
                    break;
                }
            }
        }
        
        // If either device is no longer available, disconnect
        if (!inputFound || !outputFound)
        {
            DBG("APC Mini connection lost - device no longer in available devices list");
            hardwareConnected = false;
            midiInput.reset();
            midiOutput.reset();
            
            // Immediately try to reconnect
            findAndConnectToAPC();
            return hardwareConnected;
        }
        
        return true;
    }
    else
    {
        // Not connected, try to find and connect to the APC
        findAndConnectToAPC();
        return hardwareConnected;
    }
}

void APCXAudioProcessor::findAndConnectToAPC()
{
    // Don't try to connect if we're already connected
    if (hardwareConnected)
        return;
        
    // Look for APC Mini MK2 in available devices
    auto midiInputs = juce::MidiInput::getAvailableDevices();
    auto midiOutputs = juce::MidiOutput::getAvailableDevices();
    
    juce::MidiDeviceInfo foundInputDevice;
    juce::MidiDeviceInfo foundOutputDevice;
    bool foundInput = false;
    bool foundOutput = false;
    
    // Look for input device
    for (const auto& device : midiInputs)
    {
        if (device.name.containsIgnoreCase("APC mini mk2"))
        {
            foundInputDevice = device;
            foundInput = true;
            break;
        }
    }
    
    // Look for output device
    for (const auto& device : midiOutputs)
    {
        if (device.name.containsIgnoreCase("APC mini mk2"))
        {
            foundOutputDevice = device;
            foundOutput = true;
            break;
        }
    }
    
    // If we found both devices, try to connect
    if (foundInput && foundOutput)
    {
        // Open the MIDI devices
        auto newInput = juce::MidiInput::openDevice(foundInputDevice.identifier, this);
        auto newOutput = juce::MidiOutput::openDevice(foundOutputDevice.identifier);
        
        if (newInput != nullptr && newOutput != nullptr)
        {
            // Store the devices
            midiInput.reset(newInput.release());
            midiOutput.reset(newOutput.release());
            
            // Start the input device
            midiInput->start();
            
            hardwareConnected = true;

            DBG("Successfully connected to APC Mini MK2");

            syncHardwareToCurrentState();
        }
        else
        {
            DBG("Failed to open APC Mini MK2 devices");
            midiInput.reset();
            midiOutput.reset();
            hardwareConnected = false;
        }
    }
}

void APCXAudioProcessor::sendMessage(const juce::MidiMessage& message)
{
    if (hardwareConnected && midiOutput != nullptr)
        midiOutput->sendMessageNow(message);
}

void APCXAudioProcessor::sendImmediateMidiMessage(const juce::MidiMessage& message)
{
    // Create temporary buffer for immediate output
    juce::MidiBuffer tempBuffer;
    tempBuffer.addEvent(message, 0);
    
    // Process the immediate message in a zero-length audio buffer
    juce::AudioBuffer<float> emptyBuffer(getTotalNumInputChannels(), 0);
    processBlock(emptyBuffer, tempBuffer);
    
    if (hardwareConnected && midiOutput != nullptr)
        midiOutput->sendMessageNow(message);
}

int APCXAudioProcessor::mapToHardwareIndex(int row, int col)
{
    // The APC Mini MK2 has rows numbered from bottom to top (0-7), but we're 
    // displaying them top to bottom in our UI. This function flips the row.
    const int flippedRow = 7 - row;
    
    // The final MIDI note number is row * 8 + col
    return (flippedRow * 8) + col;
}

void APCXAudioProcessor::updateStepButtonOnHardware(int index, bool isCurrentStep)
{
    // While shift is held, the grid is dedicated to the shift menu overlay.
    if (shiftPressed.load(std::memory_order_relaxed))
        return;

    if (!hardwareConnected || midiOutput == nullptr || index < 0 || index >= 32)
        return;
    
    // Calculate the hardware grid position
    int row = index / 8;  // Step buttons occupy the first 4 rows (0-3)
    int col = index % 8;
    
    // Map to hardware index (flipping rows to match APC layout)
    int midiNote = mapToHardwareIndex(row, col);
    
    // Set color based on step state - using correct color indices from documentation
    juce::uint8 velocity;
    int channel; // Will set this below based on the state
    
    // Lock while accessing step cache
    bool hasNotes = false;
    bool hasSelectedNote = false;
    
    {
        const std::lock_guard<std::mutex> lock(callbackMutex);
        // Check if this step has any notes
        hasNotes = !stepCache[index].notes.empty();
        
        // Check if it has the selected note
        if (selectedNote != -1) {
            for (const auto& note : stepCache[index].notes) {
                if (note.note == selectedNote) {
                    hasSelectedNote = true;
                    break;
                }
            }
        }
    }
    
    // First check if this is the current step
    if (isCurrentStep) {
        velocity = static_cast<juce::uint8>(colorYellow); // Current step is always yellow
        channel = channelFull;                           // Full brightness
    }
    // Then check if a specific note is selected and this step has it
    else if (selectedNote != -1 && hasSelectedNote) {
        velocity = static_cast<juce::uint8>(colorBlue);  // Step has the selected note, show in blue
        channel = channelFull;                          // Full brightness
    }
    // Then check if the step has any notes at all
    else if (hasNotes) {
        velocity = static_cast<juce::uint8>(colorGreen); // Green for steps with any notes
        channel = channelFull;                          // Full brightness
    }
    // Use dim white for empty steps instead of off
    else {
        velocity = static_cast<juce::uint8>(colorWhite); // White for empty steps
        channel = channelDimmest;                       // Very dim (10% brightness)
    }
    
    // Send the MIDI message to the hardware
    sendMessage(juce::MidiMessage::noteOn(channel, midiNote, velocity));
}

void APCXAudioProcessor::updateNoteButtonOnHardware(int index, bool isSelected)
{
    // While shift is held, the grid is dedicated to the shift menu overlay.
    if (shiftPressed.load(std::memory_order_relaxed))
        return;

    if (!hardwareConnected || midiOutput == nullptr || index < 0 || index >= 16)
        return;
    
    // Calculate the position in the hardware grid
    // Note buttons start at row 4, column 0-3
    int row = 4 + (index / 4);
    int col = index % 4;
    
    // Map to hardware index (flipping rows to match APC layout)
    int midiNote = mapToHardwareIndex(row, col);
    
    // First remap to get bottom-left to right, then up ordering
    int buttonRow = index / 4;
    int buttonCol = index % 4;
    int remappedRow = 3 - buttonRow; // Invert the row
    int remappedIndex = remappedRow * 4 + buttonCol;
    
    // Get the actual MIDI note for this button based on current scale
    int buttonMidiNote = musicTheory.mapButtonToScaleNote(remappedIndex, 3); // baseOctave is typically 3
    
    // Set color based on selection state and note type
    juce::uint8 velocity;
    int channel = channelFull; // Default to full brightness
    
    // Check if this is a root note (only notes that match the root note value)
    bool isRootNote = (buttonMidiNote % 12 == musicTheory.getRootNote());
    
    if (isSelected) {
        // Selected note is blue, regardless of note type
        velocity = static_cast<juce::uint8>(colorBlue);
    }
    else if (isRootNote) {
        // Root notes are red
        velocity = static_cast<juce::uint8>(colorRed);
    }
    else if (musicTheory.isNoteInScale(buttonMidiNote)) {
        // Notes in scale are yellow
        velocity = static_cast<juce::uint8>(colorYellow);
    }
    else {
        // Notes not in the scale are dim yellow
        velocity = static_cast<juce::uint8>(colorYellow);
        channel = channelDimmer; // Dimmer brightness
    }
    
    // Send the MIDI message to the hardware
    sendMessage(juce::MidiMessage::noteOn(channel, midiNote, velocity));
}

void APCXAudioProcessor::updateModifierButtonOnHardware(int index, bool isSelected, int velocity)
{
    // While shift is held, the grid is dedicated to the shift menu overlay.
    if (shiftPressed.load(std::memory_order_relaxed))
        return;

    if (!hardwareConnected || midiOutput == nullptr || index < 0 || index >= 16)
        return;
    
    // Calculate the position in the hardware grid
    // Modifier buttons start at row 4, column 4-7
    int row = 4 + (index / 4);
    int col = 4 + (index % 4);
    
    // Map to hardware index (flipping rows to match APC layout)
    int midiNote = mapToHardwareIndex(row, col);
    
    // Set color and brightness based on selection state and velocity value
    juce::uint8 velocityValue;
    int channel;
    
    if (isSelected) {
        // Selected velocity is bright magenta
        velocityValue = static_cast<juce::uint8>(colorMagenta);
        channel = channelFull;   // Full brightness
    }
    else {
        // Always use Cyan for velocity buttons, and adjust brightness with channel
        velocityValue = static_cast<juce::uint8>(colorCyan);
        
        // Map the velocity value (0-127) to different brightness levels
        // Use MIDI channel to control brightness - higher channel = higher brightness
        if (velocity > 110) {
            channel = channelFull;      // 100% brightness
        }
        else if (velocity > 90) {
            channel = channelBrighter;  // 90% brightness
        }
        else if (velocity > 70) {
            channel = channelBright;    // 75% brightness
        }
        else if (velocity > 50) {
            channel = channelMedium;    // 50% brightness
        }
        else if (velocity > 30) {
            channel = channelDim;       // 30% brightness
        }
        else if (velocity > 15) {
            channel = channelDimmer;    // 20% brightness
        }
        else {
            channel = channelDimmest;   // 10% brightness
        }
    }
    
    // Send the MIDI message to the hardware
    sendMessage(juce::MidiMessage::noteOn(channel, midiNote, velocityValue));
}

void APCXAudioProcessor::syncHardwareToCurrentState()
{
    if (!hardwareConnected || midiOutput == nullptr)
        return;
    
    // First update all the step buttons
    for (int i = 0; i < 32; ++i)
    {
        updateStepButtonOnHardware(i, i == currentStep.load());
    }
    
    // Then update all the note buttons with scale-aware logic
    syncNoteButtonsToCurrentScale();
    
    // Finally update all the velocity buttons
    for (int i = 0; i < 16; ++i)
    {
        // We need to know if this button is selected
        bool isSelected = false;
        int velocity = 127; // Default velocity
        
        // Check if we have up-to-date velocity button information
        if (i < velocityButtonSelections.size() && i < velocityButtonValues.size())
        {
            isSelected = velocityButtonSelections[i];
            velocity = velocityButtonValues[i];
        }
        
        updateModifierButtonOnHardware(i, isSelected, velocity);
    }
}

void APCXAudioProcessor::syncNoteButtonsToCurrentScale()
{
    if (!hardwareConnected || midiOutput == nullptr)
        return;
    
    // We need to update all note buttons (indices 0-15) based on the current scale
    for (int i = 0; i < 16; ++i)
    {
        // Calculate the position in the hardware grid
        // Note buttons are in the bottom rows, columns 0-3
        int row = 4 + (i / 4);  // Start at row 4 (0-based)
        int col = i % 4;        // Columns 0-3
        
        // Map to hardware index
        int midiNote = mapToHardwareIndex(row, col);
        
        // First remap to get bottom-left to right, then up ordering
        int buttonRow = i / 4;
        int buttonCol = i % 4;
        int remappedRow = 3 - buttonRow; // Invert the row
        int remappedIndex = remappedRow * 4 + buttonCol;
        
        // Now get the actual MIDI note number
        int buttonMidiNote = musicTheory.mapButtonToScaleNote(remappedIndex, 3); // baseOctave is typically 3
        
        // Now set colors based on the note and selection state
        juce::uint8 velocity;
        int channel = channelFull; // Full brightness
        
        // Check if this button is selected in the UI
        bool isSelected = false;
        {
            const std::lock_guard<std::mutex> lock(callbackMutex);
            if (i < noteButtonSelections.size()) {
                isSelected = noteButtonSelections[i];
            }
        }
        
        if (isSelected) {
            // Selected note is blue
            velocity = static_cast<juce::uint8>(colorBlue);
        }
        else if (buttonMidiNote % 12 == musicTheory.getRootNote()) {
            // Root notes are red
            velocity = static_cast<juce::uint8>(colorRed);
        }
        else if (musicTheory.isNoteInScale(buttonMidiNote)) {
            // Notes in scale are yellow (not green)
            velocity = static_cast<juce::uint8>(colorYellow);
        }
        else {
            // Notes not in scale are dim yellow
            velocity = static_cast<juce::uint8>(colorYellow);
            channel = channelDimmer; // Use a dimmer channel
        }
        
        // Send the MIDI message to update the hardware button
        sendMessage(juce::MidiMessage::noteOn(channel, midiNote, velocity));
    }
}

void APCXAudioProcessor::updateHardwareMenu()
{
    if (!hardwareConnected || midiOutput == nullptr)
        return;

    // Clear Grid
    for (int i = 0; i < 64; ++i) {
        sendMessage(juce::MidiMessage::noteOn(channelDimmest, i, static_cast<juce::uint8>(colorOff)));
    }

    // 1. Scale Picker (Top Row - UI Row 0, Cols 0-6)
    int currentScale = static_cast<int>(musicTheory.getScaleType());
    for (int i = 0; i < 7; ++i) {
        int midiNote = mapToHardwareIndex(0, i);
        int primaryScale = i;
        int overflowScale = i + 7;
        
        int color = colorWhite;
        int channel = channelDimmest;

        if (currentScale == primaryScale) {
            color = colorRed;
            channel = channelFull;
        } else if (currentScale == overflowScale) {
            color = colorYellow;
            channel = channelFull;
        }

        sendMessage(juce::MidiMessage::noteOn(channel, midiNote, static_cast<juce::uint8>(color)));
    }

    // 2. Absolute Octave Selector (Column 7, All Rows)
    int currentOctave = musicTheory.getOctaveTransposition();
    for (int r = 0; r < 8; ++r) {
        int midiNote = mapToHardwareIndex(r, 7);
        int octaveAtThisRow = 4 - r; // Row 0=+4, Row 7=-3
        int color = (octaveAtThisRow == currentOctave) ? colorBlue : colorWhite;
        int channel = (octaveAtThisRow == currentOctave) ? channelFull : channelDimmest;
        sendMessage(juce::MidiMessage::noteOn(channel, midiNote, static_cast<juce::uint8>(color)));
    }

    // 3. Piano Layout (Bottom 2 Rows - UI Rows 6-7, Cols 0-6)
    int currentRoot = musicTheory.getRootNote();
    // White keys (UI Row 7)
    int whiteKeys[] = {0, 2, 4, 5, 7, 9, 11};
    for (int i = 0; i < 7; ++i) {
        int midiNote = mapToHardwareIndex(7, i);
        int color = (whiteKeys[i] == currentRoot) ? colorYellow : colorWhite;
        sendMessage(juce::MidiMessage::noteOn(channelFull, midiNote, static_cast<juce::uint8>(color)));
    }
    // Black keys (UI Row 6)
    int blackKeys[] = {1, 3, -1, 6, 8, 10, -1};
    for (int i = 0; i < 7; ++i) {
        if (blackKeys[i] == -1) continue;
        int midiNote = mapToHardwareIndex(6, i);
        int color = (blackKeys[i] == currentRoot) ? colorYellow : colorCyan;
        sendMessage(juce::MidiMessage::noteOn(channelFull, midiNote, static_cast<juce::uint8>(color)));
    }

    // 4. SCALE ID DISPLAY (Center area - UI Rows 1-5, Cols 0-6)
    juce::String scaleID = HardwareDisplay::getScaleID(currentScale);
    char char1 = scaleID[0];
    char char2 = scaleID[1];
    uint16_t glyph1 = HardwareDisplay::font3x5.at(char1);
    uint16_t glyph2 = HardwareDisplay::font3x5.at(char2);

    for (int y = 0; y < 5; ++y) {
        for (int x = 0; x < 3; ++x) {
            // Char 1 (Cols 0-2)
            if (HardwareDisplay::getPixel3x5(glyph1, x, y))
                sendMessage(juce::MidiMessage::noteOn(channelFull, mapToHardwareIndex(y + 1, x), static_cast<juce::uint8>(colorGreen)));
            // Char 2 (Cols 4-6)
            if (HardwareDisplay::getPixel3x5(glyph2, x, y))
                sendMessage(juce::MidiMessage::noteOn(channelFull, mapToHardwareIndex(y + 1, x + 4), static_cast<juce::uint8>(colorGreen)));
        }
    }
}

void APCXAudioProcessor::handleIncomingMidiMessage(juce::MidiInput* source, const juce::MidiMessage& message)
{
    juce::ignoreUnused(source);
    
    if (message.getNoteNumber() == 122) // SHIFT
    {
        shiftPressed = message.isNoteOn();
        if (!shiftPressed) syncHardwareToCurrentState();
        else updateHardwareMenu();
        return;
    }

    if (message.isNoteOn())
    {
        int noteNumber = message.getNoteNumber();
        int velocity = message.getVelocity();
        
        if (shiftPressed)
        {
            int col = noteNumber % 8;
            int row = 7 - (noteNumber / 8);

            // 1. Octave Selector (Column 7)
            if (col == 7) {
                setOctaveTransposition(4 - row);
                updateHardwareMenu();
                return;
            }

            // 2. Scale Picker (Row 0, Cols 0-6) - Linear Toggle
            if (row == 0 && col < 7) {
                int currentScale = static_cast<int>(musicTheory.getScaleType());
                int primaryScale = col;
                int overflowScale = col + 7;

                // If primary is already selected, toggle to overflow
                if (currentScale == primaryScale && overflowScale < static_cast<int>(MusicTheory::NumScaleTypes)) {
                    setScaleType(static_cast<MusicTheory::ScaleType>(overflowScale));
                } else {
                    // Otherwise, jump to primary
                    setScaleType(static_cast<MusicTheory::ScaleType>(primaryScale));
                }
                
                updateHardwareMenu();
                return;
            }

            // 3. Piano Layout (Rows 6-7, Cols 0-6)
            if (row == 7 && col < 7) {
                int whiteKeys[] = {0, 2, 4, 5, 7, 9, 11};
                setRootNote(whiteKeys[col]);
                updateHardwareMenu();
                return;
            }
            if (row == 6 && col < 7) {
                int blackKeys[] = {1, 3, -1, 6, 8, 10, -1};
                if (blackKeys[col] != -1) {
                    setRootNote(blackKeys[col]);
                    updateHardwareMenu();
                }
                return;
            }
            return;
        }

        // First try to route this message to the editor if a callback is set
        bool handled = false;
        {
            const std::lock_guard<std::mutex> lock(callbackMutex);
            if (hardwareButtonCallback)
            {
                hardwareButtonCallback(noteNumber, velocity);
                handled = true;
            }
        }
        
        // If editor is closed or callback not set, handle it directly
        if (!handled) {
            // We need to reverse map the hardware note to our UI layout
            int col = noteNumber % 8;
            int row = 7 - (noteNumber / 8); // Flip the row to match our UI layout
            
            // Process based on the button location
            if (row < 4) {
                // Step button area (rows 0-3)
                int stepIndex = row * 8 + col;
                
                if (stepIndex >= 0 && stepIndex < 32) {
                    // Check if we have a selected note
                    if (selectedNote != -1) {
                        // Toggle the selected note
                        bool hasSelectedNote = false;
                        {
                            const std::lock_guard<std::mutex> lock(callbackMutex);
                            if (stepIndex < static_cast<int>(stepCache.size())) {
                                for (const auto& note : stepCache[stepIndex].notes) {
                                    if (note.note == selectedNote) {
                                        hasSelectedNote = true;
                                        break;
                                    }
                                }
                            }
                        }
                        
                        // Toggle the note
                        {
                            const std::lock_guard<std::mutex> lock(callbackMutex);
                            if (hasSelectedNote) {
                                // Remove the note
                                auto& notes = stepCache[stepIndex].notes;
                                notes.erase(
                                    std::remove_if(notes.begin(), notes.end(), 
                                                [this](const NoteData& note) { return note.note == selectedNote; }),
                                    notes.end());
                            } else {
                                // Add the note
                                stepCache[stepIndex].notes.push_back(NoteData(selectedNote, selectedVelocity, 0.0f, 1.0f));
                            }
                        }
                    } else {
                        // No note selected - clear all notes from this step
                        const std::lock_guard<std::mutex> lock(callbackMutex);
                        if (stepIndex < static_cast<int>(stepCache.size())) {
                            stepCache[stepIndex].notes.clear();
                        }
                    }
                    
                    // Update the hardware state
                    updateStepButtonOnHardware(stepIndex, stepIndex == currentStep.load());
                }
            }
            else if (row >= 4 && row <= 5 && col < 4) {
                // Note button area (rows 4-5, columns 0-3)
                int noteIndex = (row - 4) * 4 + col;
                
                if (noteIndex >= 0 && noteIndex < 16) {
                    // First remap to get bottom-left to right, then up ordering
                    int buttonRow = noteIndex / 4;
                    int buttonCol = noteIndex % 4;
                    int remappedRow = 3 - buttonRow; // Invert the row
                    int remappedIndex = remappedRow * 4 + buttonCol;
                    
                    // Get the actual MIDI note number for this button
                    int midiNote = musicTheory.mapButtonToScaleNote(remappedIndex, 3); // baseOctave is typically 3
                    
                    // Check if this note is already selected
                    bool wasSelected = false;
                    {
                        const std::lock_guard<std::mutex> lock(callbackMutex);
                        if (noteIndex < noteButtonSelections.size()) {
                            wasSelected = noteButtonSelections[noteIndex];
                        }
                    }
                    
                    // If already selected, deselect it
                    if (wasSelected) {
                        {
                            const std::lock_guard<std::mutex> lock(callbackMutex);
                            // Clear all selections
                            for (int i = 0; i < noteButtonSelections.size(); i++) {
                                noteButtonSelections[i] = false;
                            }
                        }
                        selectedNote = -1; // Clear selection
                        
                        // Update hardware state
                        for (int i = 0; i < 16; i++) {
                            updateNoteButtonOnHardware(i, false);
                        }
                        
                        // Update step buttons to clear highlighting
                        for (int i = 0; i < 32; i++) {
                            updateStepButtonOnHardware(i, i == currentStep.load());
                        }
                    } else {
                        // Select this note and deselect others
                        {
                            const std::lock_guard<std::mutex> lock(callbackMutex);
                            for (int i = 0; i < noteButtonSelections.size(); i++) {
                                noteButtonSelections[i] = (i == noteIndex);
                            }
                        }
                        
                        // Set the selected note to the actual MIDI note number
                        selectedNote = midiNote;
                        
                        // Update hardware state
                        for (int i = 0; i < 16; i++) {
                            updateNoteButtonOnHardware(i, i == noteIndex);
                        }
                        
                        // Update step buttons to show the selected note
                        for (int i = 0; i < 32; i++) {
                            updateStepButtonOnHardware(i, i == currentStep.load());
                        }
                    }
                }
            }
            else if (row >= 4 && row <= 5 && col >= 4) {
                // Modifier button area (rows 4-5, columns 4-7)
                int velocityIndex = (row - 4) * 4 + (col - 4);
                
                if (velocityIndex >= 0 && velocityIndex < 16) {
                    // Get velocity value
                    int velocityValue = 127;
                    {
                        const std::lock_guard<std::mutex> lock(callbackMutex);
                        if (velocityIndex < velocityButtonValues.size()) {
                            velocityValue = velocityButtonValues[velocityIndex];
                        }
                    }
                    
                    // Set the selected velocity
                    selectedVelocity = velocityValue;
                    
                    // Update selection state
                    {
                        const std::lock_guard<std::mutex> lock(callbackMutex);
                        for (int i = 0; i < velocityButtonSelections.size(); i++) {
                            velocityButtonSelections[i] = (i == velocityIndex);
                        }
                    }
                    
                    // Update hardware state
                    for (int i = 0; i < 16; i++) {
                        bool isSelected = (i == velocityIndex);
                        int value = velocityValue;
                        {
                            const std::lock_guard<std::mutex> lock(callbackMutex);
                            if (i < velocityButtonValues.size()) {
                                value = velocityButtonValues[i];
                            }
                        }
                        updateModifierButtonOnHardware(i, isSelected, value);
                    }
                }
            }
        }
    }
}

// Music theory methods implementation
void APCXAudioProcessor::setRootNote(int rootNote)
{
    musicTheory.setRootNote(rootNote);
    
    // Trigger an update for the UI
    if (stepEventCallback)
    {
        stepEventCallback(currentStep.load());
    }
    
    // Update the sequencer state to reflect new notes
    updateStepCache();
    
    // Update hardware lights only if not in Shift mode
    if (hardwareConnected && midiOutput != nullptr && !shiftPressed.load())
    {
        syncHardwareToCurrentState();
    }
}

void APCXAudioProcessor::setScaleType(MusicTheory::ScaleType scaleType)
{
    musicTheory.setScaleType(scaleType);
    
    // Trigger an update for the UI
    if (stepEventCallback)
    {
        stepEventCallback(currentStep.load());
    }
    
    // Update the sequencer state to reflect new notes
    updateStepCache();
    
    // Update hardware lights only if not in Shift mode
    if (hardwareConnected && midiOutput != nullptr && !shiftPressed.load())
    {
        syncHardwareToCurrentState();
    }
}

void APCXAudioProcessor::setOctaveTransposition(int octaves)
{
    musicTheory.setOctaveTransposition(octaves);
    
    // Trigger an update for the UI
    if (stepEventCallback)
    {
        stepEventCallback(currentStep.load());
    }
    
    // Update the sequencer state to reflect new notes
    updateStepCache();
    
    // Update hardware lights only if not in Shift mode
    if (hardwareConnected && midiOutput != nullptr && !shiftPressed.load())
    {
        syncHardwareToCurrentState();
    }
}

// This creates new instances of the plugin
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new APCXAudioProcessor();
}
