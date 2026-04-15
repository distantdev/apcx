/*
  ==============================================================================
    ModifierButton.hpp
    Created: 10 Dec 2023 10:24:24pm
    Author:  sonic
  ==============================================================================
*/
#pragma once
#include <JuceHeader.h>
/**
 * @class ModifierButton
 * @brief Represents a button that modifies note properties like velocity.
 * 
 * This class manages buttons that control additional properties of notes,
 * such as velocity levels.
 */
class ModifierButton : public juce::TextButton
{
public:
    /** Constructor */
    ModifierButton();
    
    /** Destructor */
    ~ModifierButton() override;
    /**
     * Set the velocity value for this button
     * @param newVelocity The MIDI velocity value (0-127)
     */
    void setVelocity(int newVelocity);
    
    /**
     * Get the velocity value for this button
     * @return The MIDI velocity value
     */
    int getVelocity();
    /**
     * Set whether this velocity is currently selected
     * @param newState True if selected, false otherwise
     */
    void setIsSelectedVelocity(bool newState);
    
    /**
     * Get whether this velocity is currently selected
     * @return True if selected, false otherwise
     */
    bool getIsSelectedVelocity() const { return isSelectedVelocity; }
private:
    /** The MIDI velocity value for this button */
    int velocity = -1;
    
    /** Whether this velocity is currently selected */
    bool isSelectedVelocity = false;
    
    /**
     * Update the button's appearance based on its state
     */
    void updateBackgroundColour();
    /**
     * Calculate brightness level based on velocity
     * @return A brightness value for UI rendering
     */
    int calculateBrightness();
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ModifierButton)
};
