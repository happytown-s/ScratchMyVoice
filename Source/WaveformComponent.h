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
	void invalidateCache();
	void rebuildPathFromPeaks();
	void updatePeakCacheIncremental();

	AudioEngine& audioEngine;
	bool isExpanded = false;
	juce::Path waveformPath;
	int cachedBufferSize = 0;

	// Peak cache: one max-value per display pixel column (upper half only; mirrored in rebuildPathFromPeaks)
	std::vector<float> peakCache;
	int peakCacheNumSamples = 0;   // How many recorded samples the peakCache currently covers
	bool pathIsFinal = false;      // true after recording stops — no further rebuilds unless resized/invalidated

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WaveformComponent)
};
