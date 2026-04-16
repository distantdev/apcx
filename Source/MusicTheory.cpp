/*
  ==============================================================================

    MusicTheory.cpp

  ==============================================================================
*/

// SPDX-License-Identifier: AGPL-3.0-or-later

#include "MusicTheory.hpp"
#include <map>

//==============================================================================
MusicTheory::MusicTheory()
{
    updateScaleNotes();
}

juce::String MusicTheory::getScaleTypeName(ScaleType type)
{
    switch (type)
    {
        case Chromatic:       return "Chromatic";
        case Major:           return "Major";
        case Minor:           return "Minor";
        case HarmonicMinor:   return "Harmonic Minor";
        case MelodicMinor:    return "Melodic Minor";
        case Dorian:          return "Dorian";
        case Phrygian:        return "Phrygian";
        case Lydian:          return "Lydian";
        case Mixolydian:      return "Mixolydian";
        case Locrian:         return "Locrian";
        case Blues:           return "Blues";
        case PentatonicMajor: return "Pentatonic Major";
        case PentatonicMinor: return "Pentatonic Minor";
        default:              return "Unknown";
    }
}

std::vector<int> MusicTheory::getScaleIntervals(ScaleType type)
{
    // Define scale interval patterns (semitones between successive notes)
    // Each vector represents the semitone pattern for a complete octave
    switch (type)
    {
        case Chromatic:
            return { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 }; // All 12 semitones
        
        case Major:
            return { 2, 2, 1, 2, 2, 2, 1 }; // W-W-H-W-W-W-H
        
        case Minor:
            return { 2, 1, 2, 2, 1, 2, 2 }; // W-H-W-W-H-W-W (natural minor)
        
        case HarmonicMinor:
            return { 2, 1, 2, 2, 1, 3, 1 }; // W-H-W-W-H-WH-H
        
        case MelodicMinor:
            return { 2, 1, 2, 2, 2, 2, 1 }; // W-H-W-W-W-W-H (ascending melodic minor)
        
        case Dorian:
            return { 2, 1, 2, 2, 2, 1, 2 }; // W-H-W-W-W-H-W
        
        case Phrygian:
            return { 1, 2, 2, 2, 1, 2, 2 }; // H-W-W-W-H-W-W
        
        case Lydian:
            return { 2, 2, 2, 1, 2, 2, 1 }; // W-W-W-H-W-W-H
        
        case Mixolydian:
            return { 2, 2, 1, 2, 2, 1, 2 }; // W-W-H-W-W-H-W
        
        case Locrian:
            return { 1, 2, 2, 1, 2, 2, 2 }; // H-W-W-H-W-W-W
        
        case Blues:
            return { 3, 2, 1, 1, 3, 2 }; // Blues scale
        
        case PentatonicMajor:
            return { 2, 2, 3, 2, 3 }; // Major pentatonic
        
        case PentatonicMinor:
            return { 3, 2, 2, 3, 2 }; // Minor pentatonic
        
        default:
            return { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 }; // Default to chromatic
    }
}

void MusicTheory::setRootNote(int newRootNote)
{
    // Ensure root note is in range 0-11
    rootNote = newRootNote % 12;
    updateScaleNotes();
}

void MusicTheory::setScaleType(ScaleType type)
{
    currentScaleType = type;
    updateScaleNotes();
}

void MusicTheory::setOctaveTransposition(int octaves)
{
    // Limit to reasonable range (-4 to +4 octaves)
    octaveTransposition = juce::jlimit(-4, 4, octaves);
    // No need to update scale notes here as transposition is applied during mapping
}

bool MusicTheory::isNoteInScale(int midiNote) const
{
    // Check if this note % 12 exists in our scale notes
    int noteClass = midiNote % 12; // Note class (0-11)
    
    // If it's chromatic scale, all notes are in the scale
    if (currentScaleType == Chromatic)
        return true;
    
    // Check if this note class is in our scale
    for (int scaleNote : scaleNotes)
    {
        if (scaleNote % 12 == noteClass)
            return true;
    }
    
    return false;
}

std::vector<int> MusicTheory::getNotesInScaleRange(int startNote, int endNote) const
{
    std::vector<int> notesInRange;
    
    // If it's chromatic, just return all notes in range
    if (currentScaleType == Chromatic)
    {
        for (int note = startNote; note <= endNote; ++note)
        {
            notesInRange.push_back(note);
        }
        return notesInRange;
    }
    
    // For other scales, find all notes in the scale within the range
    for (int note = startNote; note <= endNote; ++note)
    {
        if (isNoteInScale(note))
        {
            notesInRange.push_back(note);
        }
    }
    
    return notesInRange;
}

int MusicTheory::mapButtonToScaleNote(int buttonIndex, int baseOctave) const
{
    // Apply octave transposition to the base octave
    baseOctave += octaveTransposition;
    
    // Base MIDI note (C in the specified octave)
    int baseMidiNote = 12 * (baseOctave + 1);
    
    // If chromatic scale, just offset from the base note
    if (currentScaleType == Chromatic)
    {
        return baseMidiNote + rootNote + buttonIndex;
    }
    
    // For other scales, we need to map the button index to scale degrees
    // First, generate a list of scale notes for a few octaves
    std::vector<int> scaleNotesExtended;
    
    // Start with the root note in the base octave
    int currentNote = baseMidiNote + rootNote;
    scaleNotesExtended.push_back(currentNote);
    
    // Use scale intervals to generate notes for at least 2 octaves
    std::vector<int> intervals = getScaleIntervals(currentScaleType);
    for (int i = 0; i < 16; ++i) // Generate more notes than we need
    {
        int intervalIndex = i % intervals.size();
        currentNote += intervals[intervalIndex];
        scaleNotesExtended.push_back(currentNote);
    }
    
    // Now map the button index to a note in our extended scale
    if (buttonIndex < scaleNotesExtended.size())
    {
        return scaleNotesExtended[buttonIndex];
    }
    
    // Fallback in case of error
    return baseMidiNote + rootNote;
}

void MusicTheory::updateScaleNotes()
{
    scaleNotes.clear();
    
    // Start with the root note
    int currentNote = rootNote;
    scaleNotes.push_back(currentNote);
    
    // If it's chromatic, add all 12 semitones
    if (currentScaleType == Chromatic)
    {
        for (int i = 1; i < 12; ++i)
        {
            scaleNotes.push_back((rootNote + i) % 12);
        }
        return;
    }
    
    // For other scales, follow the interval pattern
    std::vector<int> intervals = getScaleIntervals(currentScaleType);
    for (int interval : intervals)
    {
        currentNote = (currentNote + interval) % 12;
        scaleNotes.push_back(currentNote);
    }
}

juce::String MusicTheory::getMidiNoteName(int midiNote)
{
    // Calculate octave and note within octave
    int octave = midiNote / 12 - 1; // MIDI note 60 is in octave 4, but we'll call it 3 for simplicity
    int noteInOctave = midiNote % 12; // 0=C, 1=C#, 2=D, etc.
    
    // Map from note number to name
    static const char* noteNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    
    // Return note name with octave
    return noteNames[noteInOctave] + juce::String(octave);
}

int MusicTheory::noteNameToMidiNumber(const juce::String& noteName)
{
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