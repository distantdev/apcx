/*
  ==============================================================================
    MusicTheory.hpp
  ==============================================================================
*/
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once
#include <JuceHeader.h>
#include <vector>
/**
 * @class MusicTheory
 * @brief Handles music theory concepts like scales, keys, and modes.
 * 
 * This class provides functionality for working with musical scales,
 * keys, modes, and transposition.
 */
class MusicTheory
{
public:
    /** Enumerates common musical scale types */
    enum ScaleType
    {
        Chromatic = 0,
        Major,
        Minor,
        HarmonicMinor,
        MelodicMinor,
        Dorian,
        Phrygian,
        Lydian,
        Mixolydian,
        Locrian,
        Blues,
        PentatonicMajor,
        PentatonicMinor,
        NumScaleTypes
    };
    /** Constructor */
    MusicTheory();
    /**
     * Get the name of a scale type
     * @param type The scale type
     * @return The name of the scale
     */
    static juce::String getScaleTypeName(ScaleType type);
    /**
     * Check if a note is in the current scale
     * @param midiNote The MIDI note number to check
     * @return True if the note is in the scale, false otherwise
     */
    bool isNoteInScale(int midiNote) const;
    /**
     * Get the interval pattern for a scale type
     * @param type The scale type
     * @return Vector of semitone intervals that define the scale
     */
    static std::vector<int> getScaleIntervals(ScaleType type);
    /**
     * Set the current key (root note)
     * @param rootNote The MIDI note number of the root (0-11, where 0=C, 1=C#, etc.)
     */
    void setRootNote(int rootNote);
    /**
     * Get the current root note
     * @return The current root note (0-11)
     */
    int getRootNote() const { return rootNote; }
    /**
     * Set the current scale type
     * @param type The scale type to use
     */
    void setScaleType(ScaleType type);
    /**
     * Get the current scale type
     * @return The current scale type
     */
    ScaleType getScaleType() const { return currentScaleType; }
    /**
     * Set the octave transposition
     * @param octaves Number of octaves to transpose (-4 to +4)
     */
    void setOctaveTransposition(int octaves);
    /**
     * Get the current octave transposition
     * @return The current octave transposition
     */
    int getOctaveTransposition() const { return octaveTransposition; }
    /**
     * Get all notes in the current scale within a MIDI range
     * @param startNote The starting MIDI note number
     * @param endNote The ending MIDI note number
     * @return Vector of MIDI note numbers in the scale
     */
    std::vector<int> getNotesInScaleRange(int startNote, int endNote) const;
    /**
     * Map a button index to a note in the current scale
     * @param buttonIndex The button index (0-15 typically)
     * @param baseOctave The base octave to start from
     * @return The MIDI note number
     */
    int mapButtonToScaleNote(int buttonIndex, int baseOctave = 3) const;
    /**
     * Get a note name from a MIDI note number
     * @param midiNote The MIDI note number
     * @return The note name (e.g., "C3", "F#4")
     */
    static juce::String getMidiNoteName(int midiNote);
    /**
     * Convert a note name to a MIDI note number
     * @param noteName The note name (e.g., "C3", "F#4")
     * @return The MIDI note number
     */
    static int noteNameToMidiNumber(const juce::String& noteName);
private:
    /** The current root note (0-11, where 0=C, 1=C#, etc.) */
    int rootNote = 0; // C
    
    /** The current scale type */
    ScaleType currentScaleType = Chromatic;
    
    /** The current octave transposition */
    int octaveTransposition = 0;
    
    /** Cache of notes in the current scale */
    std::vector<int> scaleNotes;
    
    /** Update the scale notes cache */
    void updateScaleNotes();
};
