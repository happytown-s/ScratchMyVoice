/*
 ==============================================================================
 WaveformComponent.h
 ==============================================================================
 */
#pragma once
#include <JuceHeader.h>
#include "AudioEngine.h"

class WaveformComponent : public juce::Component, 
                          public juce::Timer,
                          public juce::ChangeListener
{
	public:
	WaveformComponent(AudioEngine& engine);
	~WaveformComponent() override;

	void paint(juce::Graphics& g) override;
	void resized() override;
	void timerCallback() override;
	void changeListenerCallback(juce::ChangeBroadcaster* source) override;

	void setExpanded(bool shouldExpand);
	void updateWaveformPath();

	private:
	void rebuildFullWaveformPath();
	void appendWaveformPath(int fromSample, int toSample);

	AudioEngine& audioEngine;
	bool isExpanded = false;
	juce::Path waveformPath;
	int cachedBufferSize = 0;      // last total sample count when full path was built
	int cachedRecordedSamples = 0; // samples already rendered into waveformPath

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WaveformComponent)
};
