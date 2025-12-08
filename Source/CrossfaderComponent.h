/*
 ==============================================================================
 CrossfaderComponent.h
 ==============================================================================
 */
#pragma once
#include <JuceHeader.h>
#include "AudioEngine.h"

class CrossfaderComponent : public juce::Component
{
	public:
	CrossfaderComponent(AudioEngine& engine);
	~CrossfaderComponent() override;

	void paint(juce::Graphics& g) override;
	void resized() override;

	void mouseDown(const juce::MouseEvent& e) override;
	void mouseUp(const juce::MouseEvent& e) override;

	private:
	AudioEngine& audioEngine;
	bool isPressed = false;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CrossfaderComponent)
};
