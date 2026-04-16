// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once
#include "PluginProcessor.hpp"
#include "StepButton.hpp"
#include "NoteButton.hpp"
#include "ModifierButton.hpp"

class APCXAudioProcessorEditor : public juce::AudioProcessorEditor, 
                                    public juce::Button::Listener,
                                    public juce::Timer
{
public:
    APCXAudioProcessorEditor(APCXAudioProcessor&);
    ~APCXAudioProcessorEditor() override;
    void setButtonColourAndState(juce::TextButton* button, const juce::String& noteName, int row, int col);
    // Component overrides
    void paint(juce::Graphics&) override;
    void resized() override;
    void buttonClicked(juce::Button* button) override;
    void timerCallback() override;
    
    // Handle hardware button press from processor
    void handleHardwareButtonPress(int noteNumber, int velocity);
    void updateButtonStates();
    
    // Callback from processor to update UI when step changes
    void handleStepChange(int newStep);
    juce::String getNoteName(int buttonIndex);
    
    // Updates all step buttons to show which ones contain the selected note
    void updateStepButtonsForSelectedNote();
    
    // Update processor hardware state
    void updateProcessorHardwareState();
    // Updates note buttons based on the current scale/key settings
    void updateNoteButtonsForCurrentScale();
private:
    APCXAudioProcessor& processor;
    juce::OwnedArray<StepButton> stepButtons;
    juce::OwnedArray<NoteButton> noteButtons;
    juce::OwnedArray<ModifierButton> modifierButtons;
    // UI configuration
    int buttonSize = 60;
    int padding = 5;
    
    // Status message for APC connection
    juce::Label connectionStatusLabel;
    // Corresponding UI colors to match hardware appearance
    const juce::Colour uiColorOff = juce::Colour::fromRGB(0, 0, 0);         // #000000 - Off
    const juce::Colour uiColorGreen = juce::Colour::fromRGB(0, 255, 0);     // #00FF00 - Green
    const juce::Colour uiColorRed = juce::Colour::fromRGB(255, 0, 0);       // #FF0000 - Red
    const juce::Colour uiColorYellow = juce::Colour::fromRGB(255, 255, 0);  // #FFFF00 - Yellow
    const juce::Colour uiColorBlue = juce::Colour::fromRGB(0, 0, 255);      // #0000FF - Blue
    const juce::Colour uiColorMagenta = juce::Colour::fromRGB(255, 0, 255); // #FF00FF - Magenta/Purple
    const juce::Colour uiColorCyan = juce::Colour::fromRGB(0, 169, 255);    // #00A9FF - Cyan
    const juce::Colour uiColorWhite = juce::Colour::fromRGB(255, 255, 255); // #FFFFFF - White
    // Utility methods
    int noteNameToMidiNumber(const juce::String& noteName);
    
    // Methods for hardware APC Mini MK2 integration
    // Utility method to find a button at a specific grid position
    juce::Button* findButtonAt(int row, int col);
    void createButtons();
    void createStepButton(int index);
    void createNoteButton(int index);
    void createModifierButton(int index);
    // Sequencer state
    int currentStep = 0;
    int selectedNoteIndex = -1;
    int selectedVelocity = 127;
    int midiChannel = 7; // Channel 7 (index 6) for 100% brightness
    // Musical scale controls
    juce::ComboBox rootNoteComboBox;
    juce::ComboBox scaleTypeComboBox;
    juce::TextButton octaveUpButton;
    juce::TextButton octaveDownButton;
    juce::Label octaveValueLabel;
    
    juce::Label rootNoteLabel;
    juce::Label scaleTypeLabel;
    juce::Label octaveLabel;
    
    // Helper methods for setting up UI components
    void setupMusicTheoryControls();
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> rootNoteAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> scaleTypeAttachment;
    
    // Keep track of the musical range for note buttons
    int baseNoteOctave = 3; // Default base octave (C3)
    int currentOctaveTransposition = 0; // Track octave transposition value
    // Use the MusicTheory utility for note name calculations
    juce::String getMappedNoteName(int buttonIndex);
    int getMappedMidiNote(int buttonIndex);
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(APCXAudioProcessorEditor)
};
