/*
  ==============================================================================

    ModifierButton.cpp
    Created: 10 Dec 2023 10:24:24pm
    Author:  sonic

  ==============================================================================
*/

#include <JuceHeader.h>
#include "ModifierButton.hpp"

//==============================================================================
ModifierButton::ModifierButton()
{
    // Initialize the button with default settings
    setButtonText(""); // No text needed for these buttons
}

ModifierButton::~ModifierButton()
{
    // Destructor - nothing specific to clean up
}

void ModifierButton::setVelocity(int newVelocity)
{
    velocity = newVelocity;
    setButtonText(juce::String(velocity)); // Display velocity value on the button
    updateBackgroundColour();
}

int ModifierButton::getVelocity()
{
    return velocity;
}

void ModifierButton::setIsSelectedVelocity(bool newState)
{
    isSelectedVelocity = newState;
    updateBackgroundColour();
}

void ModifierButton::updateBackgroundColour()
{
    if (isSelectedVelocity)
    {
        // Use magenta/purple for the selected velocity
        setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(255, 0, 255));
    }
    else 
    {
        // Calculate brightness based on velocity
        int brightnessLevel = calculateBrightness();
        
        // Create a color between dim and bright cyan based on velocity
        float brightnessFactor = brightnessLevel / 10.0f; // Scale from 0-1
        
        // Create the adjusted cyan color - ranges from dark to bright cyan
        // Use a non-linear brightness curve for better visual differentiation
        int brightness = juce::jlimit(40, 255, static_cast<int>(40 + 215 * std::sqrt(brightnessFactor)));
        
        juce::Colour cyanColor = juce::Colour::fromRGB(0, static_cast<juce::uint8>(brightness), static_cast<juce::uint8>(brightness));
        setColour(juce::TextButton::buttonColourId, cyanColor);
    }
}

int ModifierButton::calculateBrightness() 
{
    // Map velocity (0-127) to brightness levels (0-10)
    if (velocity <= 0)
        return 0;
    
    // Scale and limit brightness level
    return juce::jlimit(0, 10, velocity / 13);
}