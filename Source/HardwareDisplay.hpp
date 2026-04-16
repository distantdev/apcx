// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once
#include <JuceHeader.h>
#include <map>
namespace HardwareDisplay {
    // 3x5 font (15 bits per character)
    static const std::map<char, uint16_t> font3x5 = {
        {'A', 0b010101111101101},
        {'B', 0b110101110101110},
        {'C', 0b011100100100011},
        {'D', 0b110101101101110},
        {'E', 0b111100110100111},
        {'F', 0b111100110100100},
        {'G', 0b011100101101011},
        {'H', 0b101101111101101},
        {'I', 0b111010010010111},
        {'L', 0b100100100100111},
        {'M', 0b101111101101101},
        {'O', 0b010101101101010},
        {'P', 0b110101110100100},
        {'X', 0b101101010101101},
        {'Y', 0b101101010010100},
        {'m', 0b000000111111111}, // Special small 'm'
    };
    inline bool getPixel3x5(uint16_t glyph, int x, int y) {
        if (x < 0 || x >= 3 || y < 0 || y >= 5) return false;
        int bitIndex = 14 - (y * 3 + x);
        return (glyph & (1 << bitIndex)) != 0;
    }
    // Returns a 2-char ID for each scale
    inline juce::String getScaleID(int scaleIndex) {
        switch (scaleIndex) {
            case 0:  return "CH"; // Chromatic
            case 1:  return "MA"; // Major
            case 2:  return "MI"; // Minor
            case 3:  return "HM"; // Harmonic Minor
            case 4:  return "MM"; // Melodic Minor
            case 5:  return "DO"; // Dorian
            case 6:  return "PH"; // Phrygian
            case 7:  return "LY"; // Lydian
            case 8:  return "MX"; // Mixolydian
            case 9:  return "LO"; // Locrian
            case 10: return "BL"; // Blues
            case 11: return "PM"; // Pentatonic Major
            case 12: return "Pm"; // Pentatonic Minor
            default: return "??";
        }
    }
}
