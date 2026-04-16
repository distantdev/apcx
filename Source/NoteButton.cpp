/*
  ==============================================================================

    NoteButton.cpp
    Created: 10 Dec 2023 9:50:04pm
    Author:  sonic

  ==============================================================================
*/

#include <JuceHeader.h>
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "NoteButton.hpp"

//==============================================================================
NoteButton::NoteButton()
    : isSelectedNote(false), isRoot(false)
{
    // In your constructor, you should add any child components, and
    // initialise any special settings that your component needs.

    setButtonText("C3");
    setColour(juce::TextButton::textColourOffId, juce::Colours::black);
    updateBackgroundColour();
}

NoteButton::~NoteButton()
{
}

void NoteButton::setNote(juce::String noteName)
{
    setButtonText(noteName);
    updateBackgroundColour();
}

void NoteButton::setIsSelectedNote(bool newState)
{
    isSelectedNote = newState;
    updateBackgroundColour();
}

void NoteButton::setRootNote(bool rootState)
{
    this->isRoot = rootState;
    updateBackgroundColour();
}

void NoteButton::updateBackgroundColour()
{
    // When selected, use a specific color regardless of note type
    if (isSelectedNote)
    {
        // Blue for selected notes
        setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(0, 100, 255));
    }
    // If it's a root note, use red
    else if (isRoot)
    {
        // Red for root notes (e.g., C, D, E, F, etc. depending on selected key)
        setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(255, 0, 0));
    }
    // For other notes in the scale, use yellow
    else
    {
        // Yellow/Amber for other notes
        setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(255, 215, 0));
    }
}

bool NoteButton::isRootNote() const
{
    juce::String buttonText = getButtonText();
    
    // Extract just the note name without the octave
    juce::String noteName;
    if (buttonText.length() >= 1)
    {
        noteName = buttonText.substring(0, buttonText.length() - 1);
    }
    
    // If it's a root note (like C, D, E, etc. without octave), then it might be a root
    if (!noteName.isEmpty())
    {
        char lastChar = noteName[noteName.length() - 1];
        
        // If the last character is a digit, then remove it
        if (lastChar >= '0' && lastChar <= '9')
        {
            noteName = noteName.substring(0, noteName.length() - 1);
        }
        
        // Check if it's a root note (no #)
        return !noteName.contains("#");
    }
    
    return false;
}