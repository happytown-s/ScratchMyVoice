/*
 ==============================================================================
 Constants.h
 ==============================================================================
 */
#pragma once

namespace Constants
{
// UI Colors (Tailwind Slate-900 palette approximation)
const juce::Colour bgDark      = juce::Colour::fromString("FF0F172A"); // Slate 900
const juce::Colour bgPanel     = juce::Colour::fromString("FF1E293B"); // Slate 800
const juce::Colour accentColor = juce::Colour::fromString("FF6366F1"); // Indigo 500
const juce::Colour textColor   = juce::Colour::fromString("FFF8FAFC"); // Slate 50

// Physics
const double scratchSensitivity = 5.0;
const double idleRotationSpeed = 3.5;
}

enum class PlaybackState
{
	STOPPED,
	PLAYING,
	SCRATCHING
};
