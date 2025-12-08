/*
 ==============================================================================
 WaveformComponent.h
 ==============================================================================
 */
#pragma once
#include <JuceHeader.h>
#include "AudioEngine.h"

class WaveformComponent : public juce::Component, public juce::Timer
{
	public:
	WaveformComponent(AudioEngine& engine);
	~WaveformComponent() override;

	void paint(juce::Graphics& g) override;
	void resized() override;
	void timerCallback() override; // Update playhead position

	void setExpanded(bool shouldExpand);

	private:
	AudioEngine& audioEngine;
	bool isExpanded = false;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WaveformComponent)
};
