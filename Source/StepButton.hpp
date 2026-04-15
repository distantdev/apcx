#pragma once
#include <JuceHeader.h>
#include "NoteData.hpp"
/**
 * @class StepButton
 * @brief Represents a single step in the sequencer grid.
 * 
 * This class manages a single step in the sequencer, handling notes
 * assigned to this step and visual representation.
 */
class StepButton : public juce::TextButton
{
public:
    /** Constructor */
    StepButton();
    /**
     * Set whether this step is the currently active step in the sequence
     * @param isCurrentStep True if this is the current step, false otherwise
     */
    void setIsCurrentStep(bool isCurrentStep);
    
    /**
     * Add a note to this step
     * @param note The note data to add
     */
    void addNote(NoteData note);
    
    /**
     * Remove a note by MIDI note number
     * @param noteNumber The MIDI note number to remove
     */
    void removeNote(int noteNumber);
    
    /**
     * Clear all notes from this step
     */
    void clearNotes();
    
    /**
     * Get all notes for MIDI output
     * @return A reference to the vector of notes
     */
    const std::vector<NoteData>& getNotes() const { return notes; }
    
    /**
     * Check if this step has any notes
     * @return True if there are notes, false otherwise
     */
    bool hasNotes() const { return !notes.empty(); }
    
    /**
     * Check if this step has a specific note
     * @param noteNumber The MIDI note number to check for
     * @return True if the step has this note, false otherwise
     */
    bool hasNote(int noteNumber) const;
    
    /**
     * Set the highlight color for when a specific note is selected
     * @param shouldHighlight Whether to highlight the button
     */
    void setHighlightForSelectedNote(bool shouldHighlight);
private:
    /** Whether this step is the currently active step */
    bool isCurrentStep = false;
    
    /** Whether this step should be highlighted (for selected note) */
    bool highlightForSelectedNote = false;
    
    /** Collection of notes for this step */
    std::vector<NoteData> notes;
    
    /**
     * Update the button's appearance based on its state
     */
    void updateBackgroundColour();
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StepButton)
};
