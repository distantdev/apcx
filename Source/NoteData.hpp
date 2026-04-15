/*
  ==============================================================================
    NoteData.hpp
    Created: 10 Dec 2023 9:27:25pm
    Author:  sonic
  ==============================================================================
*/
#pragma once
/**
 * @struct NoteData
 * @brief Stores data for a musical note in a sequencer step.
 * 
 * This structure encapsulates all the properties of a note that can be
 * triggered by the sequencer, including pitch, velocity, panning, and
 * probability of playing.
 */
struct NoteData {
    int note;          ///< MIDI note number (0-127)
    int velocity;      ///< Note velocity/loudness (0-127)
    float panning;     ///< Stereo panning, from -1.0 (left) to 1.0 (right)
    float probability; ///< Probability of the note playing (0.0-1.0)
    /**
     * Constructor
     * @param note MIDI note number
     * @param velocity Note velocity (loudness)
     * @param panning Stereo position (-1.0 left to 1.0 right)
     * @param probability Chance of the note playing (0.0-1.0)
     */
    NoteData(int note, int velocity, float panning = 0.0f, float probability = 1.0f)
        : note(note), velocity(velocity), panning(panning), probability(probability) {}
};
