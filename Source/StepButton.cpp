#include "StepButton.hpp"

StepButton::StepButton()
    : juce::TextButton()
{
}

void StepButton::setIsCurrentStep(bool newState)
{
    isCurrentStep = newState;
    updateBackgroundColour();
}

void StepButton::updateBackgroundColour()
{
    // These colors should match the APC Mini MK2 hardware colors 
    // based on the official documentation color chart
    if (isCurrentStep)
    {
        // Yellow/Amber for the current step
        setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(255, 255, 0)); // Updated to #FFFF00
    }
    else if (highlightForSelectedNote)
    {
        // Blue for steps that have the selected note
        setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(0, 0, 255)); // Blue #0000FF
    }
    else if (notes.size() > 0)
    {
        // Green for steps with notes
        setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(0, 255, 0)); // #00FF00
    }
    else
    {
        // Slightly darker gray that better matches the hardware's dim white
        setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(100, 100, 100)); // Medium gray
    }
    
}

void StepButton::addNote(NoteData note) {
    notes.push_back(note);
    updateBackgroundColour();
}

void StepButton::removeNote(int noteNumber) {
    // Remove note with the matching MIDI note number
    notes.erase(std::remove_if(notes.begin(), notes.end(),
                [noteNumber](const NoteData& n) { return n.note == noteNumber; }),
                notes.end());
    updateBackgroundColour();
}

void StepButton::clearNotes() {
    notes.clear();
    updateBackgroundColour();
}

bool StepButton::hasNote(int noteNumber) const {
    // Check if any note in this step matches the given noteNumber
    for (const auto& note : notes) {
        if (note.note == noteNumber) {
            return true;
        }
    }
    return false;
}

void StepButton::setHighlightForSelectedNote(bool shouldHighlight) {
    highlightForSelectedNote = shouldHighlight;
    updateBackgroundColour();
}
