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

	private:
	// --- Waveform path generation (runs off UI thread) ---
	void rebuildWaveformPathAsync();
	void rebuildWaveformPathFromThumbnail();
	void rebuildWaveformPathFromBuffer();

	// --- Thread-safe cached path swap ---
	struct WaveformCache
	{
		juce::Path path;
		int sampleCount = 0;     // sample count this path was built for
		float componentWidth = 0;
		float componentHeight = 0;
	};
	juce::SpinLock cacheLock;
	WaveformCache activeCache;
	WaveformCache pendingCache;

	AudioEngine& audioEngine;
	bool isExpanded = false;
	int cachedBufferSize = 0;

	// Incremental recording tracking: only rebuild when samples changed significantly
	int lastRebuiltSampleCount = 0;
	static constexpr int REBUILD_THRESHOLD_SAMPLES = 4096; // rebuild every ~4K new samples

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WaveformComponent)
};
