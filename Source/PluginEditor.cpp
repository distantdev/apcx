#include "PluginProcessor.hpp"
#include "PluginEditor.hpp"
#include "StepButton.hpp"
#include "NoteButton.hpp"
#include "ModifierButton.hpp"
#include <map>

APCXAudioProcessorEditor::APCXAudioProcessorEditor(APCXAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    // Set up status label for APC connection status
    addAndMakeVisible(connectionStatusLabel);
    connectionStatusLabel.setJustificationType(juce::Justification::centred);
    
    // Update connection status
    if (processor.isHardwareConnected()) {
        connectionStatusLabel.setText("Connected to APC Mini MK2", juce::dontSendNotification);
    } else {
        connectionStatusLabel.setText("Searching for APC Mini MK2...", juce::dontSendNotification);
    }
    
    // Always start the timer to check connection status
    // This will run for the lifetime of the editor
    startTimer(500); // Check every 500ms
    
    // Create sequencer UI components
    createButtons();

    // Restore notes from processor's step cache to step buttons
    const auto& stepCache = processor.getStepCache();
    for (int i = 0; i < stepButtons.size(); ++i) {
        if (i < stepCache.size()) {
            // Clear any default notes
            stepButtons[i]->clearNotes();
            // Add all notes from the cache
            for (const auto& note : stepCache[i].notes) {
                stepButtons[i]->addNote(note);
            }
        }
    }

    // Select the highest velocity button (top right, index 3)
    if (!modifierButtons.isEmpty()) {
        modifierButtons[3]->setIsSelectedVelocity(true);
        selectedVelocity = modifierButtons[3]->getVelocity();
        processor.setSelectedVelocity(selectedVelocity);
    }
    
    // Get the currently selected note from processor
    int processorSelectedNote = processor.getSelectedNote();
    if (processorSelectedNote != -1) {
        // Find and select the corresponding note button
        for (int i = 0; i < noteButtons.size(); i++) {
            if (getMappedMidiNote(i) == processorSelectedNote) {
                noteButtons[i]->setIsSelectedNote(true);
                selectedNoteIndex = processorSelectedNote;
                break;
            }
        }
    }

    // Initialize currentStep from processor to ensure consistency
    currentStep = processor.getCurrentStep();

    // Calculate grid height
    int gridHeight = (((buttonSize / 2) + padding) * 8 + padding);
    
    // Add space for a single row of controls below the grid 
    int controlsHeight = 60;
    
    // Add space for connection status at bottom
    int statusBarHeight = 30;
    
    // Set plugin window size
    setSize(((buttonSize + padding) * 8 + padding), 
            gridHeight + controlsHeight + statusBarHeight);
    
    // Register the callback for step changes from the processor
    processor.setStepEventCallback([this](int newStep) {
        // Use MessageManager to safely call from audio thread to UI thread
        juce::MessageManager::callAsync([this, newStep]() {
            handleStepChange(newStep);
        });
    });
    
    // Register step buttons with the processor so it can access note data
    processor.registerStepButtons(&stepButtons);
    
    // Register for hardware button presses
    processor.setHardwareButtonCallback([this](int noteNumber, int velocity) {
        // Use MessageManager to safely call from audio thread to UI thread
        juce::MessageManager::callAsync([this, noteNumber, velocity]() {
            handleHardwareButtonPress(noteNumber, velocity);
        });
    });
    
    // Update the processor with the current state of all buttons
    updateProcessorHardwareState();
    
    // If hardware is connected, sync all buttons to hardware
    if (processor.isHardwareConnected()) {
        processor.syncHardwareToCurrentState();
    }
    
    // Update button states to show current step
    updateButtonStates();

    // Set up the music theory controls
    setupMusicTheoryControls();
    
    // Create attachments for APVTS parameters
    rootNoteAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.apvts, "rootNote", rootNoteComboBox);
    scaleTypeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.apvts, "scaleType", scaleTypeComboBox);
    
    // Restore music theory settings from processor
    currentOctaveTransposition = processor.getMusicTheory().getOctaveTransposition();
    octaveValueLabel.setText(juce::String(currentOctaveTransposition), juce::dontSendNotification);
    
    // Update note buttons to reflect current scale
    updateNoteButtonsForCurrentScale();
    
    // Update step buttons to show selected notes
    updateStepButtonsForSelectedNote();
}

APCXAudioProcessorEditor::~APCXAudioProcessorEditor()
{
    // Stop the device search timer
    stopTimer();
    
    // Update the processor's step cache one final time before removing callbacks
    processor.updateStepCache();
    
    // Remove the callback registration to prevent access after deletion
    processor.setStepEventCallback(nullptr);
    processor.registerStepButtons(nullptr);
    
    // Unregister the hardware button callback
    processor.setHardwareButtonCallback(nullptr);
}

void APCXAudioProcessorEditor::createButtons()
{
    // Create StepButtons and NoteButtons
    for (int i = 0; i < 32; ++i)
    {
        createStepButton(i);
    }

    for (int i = 0; i < 16; i++)
    {
        createNoteButton(i);
    }

    for (int i = 0; i < 16; i++)
    {
        createModifierButton(i);
    }
}

void APCXAudioProcessorEditor::createStepButton(int index)
{
    // Explicitly acknowledge unused parameter
    juce::ignoreUnused(index);
    
    auto* button = new StepButton();
    button->addListener(this);
    addAndMakeVisible(button);
    stepButtons.add(button);
}

void APCXAudioProcessorEditor::createNoteButton(int index)
{
    auto* button = new NoteButton();
    button->setNote(getMappedNoteName(index));
    button->addListener(this);
    addAndMakeVisible(button);
    noteButtons.add(button);
}

void APCXAudioProcessorEditor::createModifierButton(int index)
{
    auto* button = new ModifierButton();
    
    // Calculate velocity value - we want to map higher velocities to the top rows
    // Map index 0-15 to velocities spanning the MIDI range (approx. 8-127)
    int row = index / 4;
    int col = index % 4;
    
    // Create higher velocities at the top (we have 4 rows, with 4 buttons each)
    int invertedRow = 3 - row;  // Invert the row (top row = highest velocity)
    int velocity = 8 + ((invertedRow * 4 + col) * 8);  // Spread values across MIDI velocity range
    
    button->setVelocity(velocity);
    button->addListener(this);
    addAndMakeVisible(button);
    modifierButtons.add(button);
}

void APCXAudioProcessorEditor::paint(juce::Graphics& g)
{
    // Fill background with dark grey
    g.fillAll(juce::Colour(40, 40, 40));
}

void APCXAudioProcessorEditor::resized()
{
    // Position the APC connection status label
    connectionStatusLabel.setBounds(10, getHeight() - 30, getWidth() - 20, 20);
    
    // Position step buttons in grid
    for (int i = 0; i < 32; ++i)
    {
        int row = i / 8;
        int col = i % 8;
        stepButtons[i]->setBounds(col * (buttonSize + padding) + padding, 
                              row * (buttonSize / 2 + padding) + padding, 
                              buttonSize, buttonSize / 2);
    }
    
    // Position note buttons in grid (bottom half)
    for (int i = 0; i < 16; ++i)
    {
        int row = i / 4;
        int col = i % 4;
        noteButtons[i]->setBounds(col * (buttonSize + padding) + padding, 
                               4 * (buttonSize / 2 + padding) + padding + row * (buttonSize / 2 + padding), 
                               buttonSize, buttonSize / 2);
    }
    
    // Position modifier buttons in grid (bottom half, right side)
    for (int i = 0; i < 16; ++i)
    {
        int row = i / 4;
        int col = i % 4 + 4; // offset to right side
        modifierButtons[i]->setBounds(col * (buttonSize + padding) + padding, 
                                   4 * (buttonSize / 2 + padding) + padding + row * (buttonSize / 2 + padding), 
                                   buttonSize, buttonSize / 2);
    }

    // Calculate the bottom of the grid
    int gridBottom = 8 * (buttonSize / 2 + padding) + padding;
    
    // Position the music theory controls in a single line below the grid
    const int labelWidth = 50; // Slightly wider to fit "Octave" label fully
    const int keyComboWidth = 85; // Width for the key combo box
    const int scaleComboWidth = 140; // Wider scale combo box to fit longer scale names
    const int buttonWidth = 30; // Width for the octave +/- buttons
    const int valueWidth = 30;  // Width for the octave value display
    const int controlHeight = 24;
    const int controlY = gridBottom + 20; // 20px below the grid
    const int controlSpacing = 10;
    
    // Start from 20px from the left edge
    int x = 20;
    
    // Root note controls
    rootNoteLabel.setBounds(x, controlY, labelWidth, controlHeight);
    x += labelWidth;
    rootNoteComboBox.setBounds(x, controlY, keyComboWidth, controlHeight);
    x += keyComboWidth + controlSpacing;
    
    // Scale type controls
    scaleTypeLabel.setBounds(x, controlY, labelWidth, controlHeight);
    x += labelWidth;
    scaleTypeComboBox.setBounds(x, controlY, scaleComboWidth, controlHeight);
    x += scaleComboWidth + controlSpacing;
    
    // Octave controls (all on same line)
    octaveLabel.setBounds(x, controlY, labelWidth, controlHeight);
    x += labelWidth;
    octaveValueLabel.setBounds(x, controlY, valueWidth, controlHeight);
    x += valueWidth;
    octaveDownButton.setBounds(x, controlY, buttonWidth, controlHeight);
    x += buttonWidth;
    octaveUpButton.setBounds(x, controlY, buttonWidth, controlHeight);
}

void APCXAudioProcessorEditor::buttonClicked(juce::Button* button)
{
    // Step, note (pitch), or velocity pad
    if (StepButton* stepButton = dynamic_cast<StepButton*>(button))
    {
        // Get the button index
        int index = stepButtons.indexOf(stepButton);
        
        // if selectedNoteIndex is not -1, add or remove the note at the button's index
        if (selectedNoteIndex != -1)
        {
            // Check if this step already has this note
            bool hasThisNote = stepButton->hasNote(selectedNoteIndex);
            
            if (hasThisNote)
            {
                // Remove the specific note if it exists
                stepButton->removeNote(selectedNoteIndex);
            }
            else
            {
                // Add the note if it doesn't exist
                stepButton->addNote(NoteData(selectedNoteIndex, selectedVelocity, 0.0f, 1.0f));
            }
        }
        else
        {
            // If no note is selected, toggle clearing the step
            if (stepButton->hasNotes())
                stepButton->clearNotes();
        }
        
        // Update step button appearance based on selected note
        if (selectedNoteIndex != -1) {
            // Update the visual state of the button
            stepButton->setHighlightForSelectedNote(stepButton->hasNote(selectedNoteIndex));
        }
        
        // Update hardware through processor
        if (processor.isHardwareConnected() && index >= 0) {
            // First update the processor's step cache to ensure it has the latest data
            processor.updateStepCache();
            
            // Then update the hardware with the correct visual state
            processor.updateStepButtonOnHardware(index, index == currentStep);
        }
        
        // Update the step cache in the processor so it has the latest data
        processor.updateStepCache();
    }
    else if (NoteButton* noteButton = dynamic_cast<NoteButton*>(button))
    {
        int noteButtonIndex = noteButtons.indexOf(noteButton);
        
        // Get the note MIDI number
        int noteMidi = noteNameToMidiNumber(noteButton->getButtonText());
        
        // Check if this note is already selected
        bool wasSelected = noteButton->getIsSelectedNote();
        
        // If already selected, deselect it
        if (wasSelected)
        {
            noteButton->setIsSelectedNote(false);
            selectedNoteIndex = -1; // Clear selection
            
            // Update hardware through processor
            if (processor.isHardwareConnected()) {
                // Update selection state for hardware
                std::vector<bool> noteSelections(noteButtons.size(), false);
                processor.registerNoteSelectionState(noteSelections);
                
                processor.updateNoteButtonOnHardware(noteButtonIndex, false);
                processor.setSelectedNote(-1); // Clear selection in processor
            }
        }
        else
        {
            // Otherwise select this note and deselect all others
            for (int i = 0; i < noteButtons.size(); i++)
            {
                // Update button state
                bool isSelected = (noteButtons[i] == noteButton);
                noteButtons[i]->setIsSelectedNote(isSelected);
                
                // Update hardware through processor
                if (processor.isHardwareConnected()) {
                    processor.updateNoteButtonOnHardware(i, isSelected);
                }
            }
            
            selectedNoteIndex = noteMidi;
            
            // Update selection state for hardware
            if (processor.isHardwareConnected()) {
                std::vector<bool> noteSelections(noteButtons.size(), false);
                noteSelections[noteButtonIndex] = true;
                processor.registerNoteSelectionState(noteSelections);
            }
            
            // IMPORTANT: Pass the actual MIDI note number (not the button index)
            // The processor needs to know the actual MIDI note to correctly highlight step buttons
            processor.setSelectedNote(noteMidi);
        }
        
        // Update all step buttons to reflect the new note selection
        updateStepButtonsForSelectedNote();
        
        // Update processor state
        updateProcessorHardwareState();
    }
    else if (ModifierButton* modifierButton = dynamic_cast<ModifierButton*>(button))
    {
        selectedVelocity = modifierButton->getVelocity();
        for (int i = 0; i < modifierButtons.size(); i++)
        {
            if (modifierButtons[i] != modifierButton)
            {
                modifierButtons[i]->setIsSelectedVelocity(false);
            }
            else
            {
                modifierButtons[i]->setIsSelectedVelocity(true);
            }
            
            // Update hardware through processor
            if (processor.isHardwareConnected()) {
                processor.updateModifierButtonOnHardware(i, modifierButtons[i]->getIsSelectedVelocity(), modifierButtons[i]->getVelocity());
            }
        }
        
        // Update processor with selected velocity
        processor.setSelectedVelocity(selectedVelocity);
        
        // Update processor state
        updateProcessorHardwareState();
    }
    else if (button == &octaveUpButton)
    {
        // Limit to reasonable range (-4 to +4 octaves)
        if (currentOctaveTransposition < 4)
        {
            currentOctaveTransposition++;
            octaveValueLabel.setText(juce::String(currentOctaveTransposition), juce::dontSendNotification);
            processor.setOctaveTransposition(currentOctaveTransposition);
            updateNoteButtonsForCurrentScale();
        }
    }
    else if (button == &octaveDownButton)
    {
        // Limit to reasonable range (-4 to +4 octaves)
        if (currentOctaveTransposition > -4)
        {
            currentOctaveTransposition--;
            octaveValueLabel.setText(juce::String(currentOctaveTransposition), juce::dontSendNotification);
            processor.setOctaveTransposition(currentOctaveTransposition);
            updateNoteButtonsForCurrentScale();
        }
    }
}

// Handle step change from the processor callback
void APCXAudioProcessorEditor::handleStepChange(int newStep)
{
    // Update our local currentStep
    currentStep = newStep;
    
    // Update button states
    updateButtonStates();
}

void APCXAudioProcessorEditor::updateButtonStates()
{
    for (int i = 0; i < 32; ++i)  // Assuming the first 32 buttons are for the sequencer
    {
        StepButton* stepButton = stepButtons[i];
        if (i == currentStep)
        {
            stepButton->setIsCurrentStep(true);
        }
        else {
            stepButton->setIsCurrentStep(false);
        }
        stepButton->repaint();
        
        // Also update hardware through processor
        if (processor.isHardwareConnected()) {
            processor.updateStepButtonOnHardware(i, i == currentStep);
        }
    }

    // Sequence transitions can be handled here if needed
}

void APCXAudioProcessorEditor::updateProcessorHardwareState()
{
    // Update note button selection state in the processor
    std::vector<bool> noteSelections;
    noteSelections.resize(noteButtons.size());
    for (int i = 0; i < noteButtons.size(); i++) {
        noteSelections[i] = noteButtons[i]->getIsSelectedNote();
    }
    processor.registerNoteSelectionState(noteSelections);
    
    // Update velocity button selection state in the processor
    std::vector<bool> velocitySelections;
    std::vector<int> velocityValues;
    velocitySelections.resize(modifierButtons.size());
    velocityValues.resize(modifierButtons.size());
    for (int i = 0; i < modifierButtons.size(); i++) {
        velocitySelections[i] = modifierButtons[i]->getIsSelectedVelocity();
        velocityValues[i] = modifierButtons[i]->getVelocity();
    }
    processor.registerVelocitySelectionState(velocitySelections, velocityValues);
}

void APCXAudioProcessorEditor::timerCallback()
{
    // Check if hardware connection status has changed
    bool wasConnected = processor.isHardwareConnected();
    bool isNowConnected = processor.checkMidiConnectionStatus();
    
    // If connection status changed, update UI
    if (wasConnected != isNowConnected)
    {
        DBG("Connection status changed: " + juce::String(wasConnected ? "Connected->Disconnected" : "Disconnected->Connected"));
    }
    
    // Handle current connection state
    if (!processor.isHardwareConnected())
    {
        // Create a subtle pulsing effect using sine wave
        float pulseAlpha = static_cast<float>((std::sin(juce::Time::getMillisecondCounterHiRes() * 0.002) + 1.0) * 0.5);
        float brightness = 0.5f + (pulseAlpha * 0.5f); // Range from 0.5 to 1.0
        
        connectionStatusLabel.setColour(juce::Label::textColourId, 
            juce::Colours::white.withAlpha(brightness));
        connectionStatusLabel.setText("Searching for APC Mini MK2...", juce::dontSendNotification);
        
        // Attempt to find and connect to APC
        processor.findAndConnectToAPC();
    }
    else
    {
        // Device is connected, update status
        connectionStatusLabel.setColour(juce::Label::textColourId, juce::Colours::white);
        connectionStatusLabel.setText("Connected to APC Mini MK2", juce::dontSendNotification);
        
        // If we just connected, update hardware state
        if (!wasConnected && isNowConnected)
        {
            DBG("Hardware reconnected - updating state");
            
            // Update hardware with current state
            updateProcessorHardwareState();
            
            // Make sure the processor has the latest step data
            processor.updateStepCache();
            
            // Force a full hardware sync to ensure all buttons are lit properly
            processor.syncHardwareToCurrentState();
        }
    }
}

void APCXAudioProcessorEditor::handleHardwareButtonPress(int noteNumber, int velocity)
{
    // Acknowledge velocity parameter to prevent warning
    juce::ignoreUnused(velocity);
    
    // Get the rows and cols for this note number
    int col = noteNumber % 8;
    int row = 7 - (noteNumber / 8); // Flip the row to match our UI layout
    
    // Find the corresponding button in our UI
    if (juce::Button* button = findButtonAt(row, col))
    {
        // Simulate a click on this button
        buttonClicked(button);
    }
}

juce::Button* APCXAudioProcessorEditor::findButtonAt(int row, int col)
{
    // Map coordinates to button in our UI
    
    // Step buttons (rows 0-3)
    if (row < 4)
    {
        int index = row * 8 + col;
        if (index >= 0 && index < stepButtons.size())
            return stepButtons[index];
    }
    // Note buttons (rows 4-7, cols 0-3)
    else if (col < 4)
    {
        int index = (row - 4) * 4 + col;
        if (index >= 0 && index < noteButtons.size())
            return noteButtons[index];
    }
    // Modifier buttons (rows 4-7, cols 4-7)
    else
    {
        int index = (row - 4) * 4 + (col - 4);
        if (index >= 0 && index < modifierButtons.size())
            return modifierButtons[index];
    }
    
    return nullptr;
}

juce::String APCXAudioProcessorEditor::getNoteName(int buttonIndex)
{
    // Map the button index (0-15) to a MIDI note name (C3 to D#4)
    // Starting at C3 (MIDI note 60) and using a chromatic scale
    // We'll only use white keys (C, D, E, F, G, A, B) and black keys (C#, D#, F#, G#, A#)
    
    // Adjust for starting at C3 (MIDI note 60)
    int midiNote = 60 + buttonIndex;
    
    // Calculate octave and note within octave
    int octave = midiNote / 12 - 1; // MIDI note 60 is in octave 4, but we'll call it 3 for simplicity
    int noteInOctave = midiNote % 12; // 0=C, 1=C#, 2=D, etc.
    
    // Map from note number to name
    static const char* noteNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    
    // Return note name with octave
    return noteNames[noteInOctave] + juce::String(octave);
}

int APCXAudioProcessorEditor::noteNameToMidiNumber(const juce::String& noteName) {
    // Map of note names to their semitone offsets from C
    std::map<char, int> noteOffsets = {
        {'C', 0}, {'D', 2}, {'E', 4}, {'F', 5},
        {'G', 7}, {'A', 9}, {'B', 11}
    };

    // Extract the note and octave from the string
    char note = static_cast<char>(noteName[0]);
    int octave = noteName.getLastCharacter() - '0'; // Convert char to int

    // Calculate the base MIDI number (C0 is MIDI note number 12)
    int midiNumber = 12 + octave * 12 + noteOffsets[note];

    // If there's a sharp note, increment the MIDI number
    if (noteName.length() == 3 && noteName[1] == '#') {
        midiNumber++;
    }

    return midiNumber;
}

void APCXAudioProcessorEditor::updateStepButtonsForSelectedNote()
{
    // Update all step buttons based on whether they have the selected note
    for (int i = 0; i < stepButtons.size(); ++i)
    {
        StepButton* stepButton = stepButtons[i];
        
        // Update UI button highlight state
        if (selectedNoteIndex != -1) {
            // Highlight if it has the selected note
            bool hasNote = stepButton->hasNote(selectedNoteIndex);
            stepButton->setHighlightForSelectedNote(hasNote);
            
            // Update hardware if connected
            if (processor.isHardwareConnected()) {
                processor.updateStepButtonOnHardware(i, i == currentStep);
            }
        } else {
            // No note selected, so no highlight
            stepButton->setHighlightForSelectedNote(false);
            
            // Update hardware if connected
            if (processor.isHardwareConnected()) {
                processor.updateStepButtonOnHardware(i, i == currentStep);
            }
        }
    }
    
    // First update the processor's cache with our current button states
    processor.updateStepCache();
    
    // Make sure the processor knows which note is selected
    // We need to update the processor's selectedNote to match our selectedNoteIndex
    if (selectedNoteIndex != -1) {
        // Update the processor with the selected note
        processor.setSelectedNote(selectedNoteIndex);
    } else {
        processor.setSelectedNote(-1);
    }
    
    // Now update the hardware to reflect our updated state
    // This ensures hardware gets the latest button info
    if (processor.isHardwareConnected()) {
        for (int i = 0; i < stepButtons.size(); ++i) {
            processor.updateStepButtonOnHardware(i, i == currentStep);
        }
    }
}

void APCXAudioProcessorEditor::setupMusicTheoryControls()
{
    // Set up the root note combo box
    rootNoteLabel.setText("Key:", juce::dontSendNotification);
    rootNoteLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(rootNoteLabel);
    
    rootNoteComboBox.addItem("C", 1);
    rootNoteComboBox.addItem("C#", 2);
    rootNoteComboBox.addItem("D", 3);
    rootNoteComboBox.addItem("D#", 4);
    rootNoteComboBox.addItem("E", 5);
    rootNoteComboBox.addItem("F", 6);
    rootNoteComboBox.addItem("F#", 7);
    rootNoteComboBox.addItem("G", 8);
    rootNoteComboBox.addItem("G#", 9);
    rootNoteComboBox.addItem("A", 10);
    rootNoteComboBox.addItem("A#", 11);
    rootNoteComboBox.addItem("B", 12);
    
    rootNoteComboBox.setSelectedId(1, juce::dontSendNotification);
    addAndMakeVisible(rootNoteComboBox);
    
    // Set up the scale type combo box
    scaleTypeLabel.setText("Scale:", juce::dontSendNotification);
    scaleTypeLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(scaleTypeLabel);
    
    scaleTypeComboBox.addItem("Major", 1);
    scaleTypeComboBox.addItem("Minor", 2);
    scaleTypeComboBox.addItem("Pentatonic Major", 3);
    scaleTypeComboBox.addItem("Pentatonic Minor", 4);
    scaleTypeComboBox.addItem("Dorian", 5);
    scaleTypeComboBox.addItem("Phrygian", 6);
    scaleTypeComboBox.addItem("Lydian", 7);
    scaleTypeComboBox.addItem("Mixolydian", 8);
    scaleTypeComboBox.addItem("Locrian", 9);
    scaleTypeComboBox.addItem("Chromatic", 10);
    
    scaleTypeComboBox.setSelectedId(1, juce::dontSendNotification);
    addAndMakeVisible(scaleTypeComboBox);
    
    // Set up the octave control with + and - buttons
    octaveLabel.setText("Octave:", juce::dontSendNotification);
    octaveLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(octaveLabel);
    
    // Set up the octave value display
    currentOctaveTransposition = 0; // Initialize to 0
    octaveValueLabel.setText(juce::String(currentOctaveTransposition), juce::dontSendNotification);
    octaveValueLabel.setJustificationType(juce::Justification::centred);
    octaveValueLabel.setColour(juce::Label::backgroundColourId, juce::Colours::black);
    octaveValueLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(octaveValueLabel);
    
    // Set up octave down button (-)
    octaveDownButton.setButtonText("-");
    octaveDownButton.addListener(this);
    addAndMakeVisible(octaveDownButton);
    
    // Set up octave up button (+)
    octaveUpButton.setButtonText("+");
    octaveUpButton.addListener(this);
    addAndMakeVisible(octaveUpButton);
}

void APCXAudioProcessorEditor::updateNoteButtonsForCurrentScale()
{
    // Get current root note for proper coloring
    int rootNote = processor.getRootNote();
    
    // Find the currently selected button
    int selectedButtonIndex = -1;
    for (int i = 0; i < noteButtons.size(); ++i) {
        if (noteButtons[i]->getIsSelectedNote()) {
            selectedButtonIndex = i;
            break;
        }
    }
    
    // Update all note buttons with their new note names based on the current scale
    for (int i = 0; i < noteButtons.size(); ++i)
    {
        // Set the note name text
        noteButtons[i]->setNote(getMappedNoteName(i));
        
        // Get the actual MIDI note number for this button
        int midiNote = getMappedMidiNote(i);
        
        // Check if this is a root note (only notes that match the root note value)
        bool isRootNote = (midiNote % 12 == rootNote);
        
        // Set appropriate button color - only root notes should be red
        noteButtons[i]->setRootNote(isRootNote);
    }
    
    // If there was a selected button, update the selected note
    if (selectedButtonIndex != -1) {
        int newMidiNote = getMappedMidiNote(selectedButtonIndex);
        selectedNoteIndex = newMidiNote;
        processor.setSelectedNote(newMidiNote);
    }
    
    // Update the step buttons to reflect any changes
    updateStepButtonsForSelectedNote();
    
    // Update all hardware - this will ensure selected notes stay blue
    if (processor.isHardwareConnected()) {
        processor.syncHardwareToCurrentState();
    }
}

juce::String APCXAudioProcessorEditor::getMappedNoteName(int buttonIndex)
{
    // Get the MIDI note number from the music theory mapping
    int midiNote = getMappedMidiNote(buttonIndex);
    
    // Convert to a note name
    return MusicTheory::getMidiNoteName(midiNote);
}

int APCXAudioProcessorEditor::getMappedMidiNote(int buttonIndex)
{
    // We want notes to start from bottom left and go right, then up
    // Our grid layout is:
    // 0  1  2  3
    // 4  5  6  7
    // 8  9  10 11
    // 12 13 14 15
    
    // To get bottom-left to right, then up ordering, we need to remap as:
    // 12 13 14 15
    // 8  9  10 11
    // 4  5  6  7
    // 0  1  2  3
    
    // Flip the row order (invert the row number)
    int row = buttonIndex / 4;         // Get current row
    int col = buttonIndex % 4;         // Get column
    int invertedRow = 3 - row;         // Invert row (0->3, 1->2, 2->1, 3->0)
    int remappedIndex = invertedRow * 4 + col; // Calculate new index
    
    // Use the music theory class to map the remapped button index to a scale-appropriate note
    return processor.getMusicTheory().mapButtonToScaleNote(remappedIndex, baseNoteOctave);
}
