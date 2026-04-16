/*
  ==============================================================================
    NoteButton.hpp
    Created: 10 Dec 2023 9:50:04pm
    Author:  sonic
  ==============================================================================
*/
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once
#include <JuceHeader.h>
/**
 * @class NoteButton
 * @brief Represents a selectable note button in the sequencer UI.
 * 
 * This class manages a note button that can be selected to determine
 * which note will be added to a step when it's clicked.
 */
class NoteButton : public juce::TextButton
{
public:
    /** Constructor */
    NoteButton();
    
    /** Destructor */
    ~NoteButton() override;
    /**
     * Set the note name for this button
     * @param noteName The name of the note (e.g., "C3", "F#4")
     */
    void setNote(juce::String noteName);
    
    /**
     * Set whether this note is currently selected
     * @param newState True if selected, false otherwise
     */
    void setIsSelectedNote(bool newState);
    
    /**
     * Get whether this note is currently selected
     * @return True if selected, false otherwise
     */
    bool getIsSelectedNote() const { return isSelectedNote; }
    
    /**
     * Set whether this note is a root note (for coloring purposes)
     * @param isRoot True if this is a root note, false otherwise
     */
    void setRootNote(bool isRoot);
    
    /**
     * Checks if this button represents a root note (natural note, not a sharp/flat)
     * @return True if this is a root note, false otherwise
     */
    bool isRootNote() const;
    /**
     * Update the button's appearance based on its state
     */
    void updateBackgroundColour();
private:
    /** Whether this note is currently selected */
    bool isSelectedNote = false;
    
    /** Whether this is a root note */
    bool isRoot = false;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NoteButton)
};
