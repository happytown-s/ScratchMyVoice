/*
 ==============================================================================
 WaveformComponent.h
 ==============================================================================
 Fixes #5 — waveform drawing no longer blocks the UI thread.

 Performance strategy
 --------------------
 1. juce::AudioThumbnail (inside AudioEngine) computes min/max peaks
    asynchronously in its own thread.  We NEVER touch the raw AudioBuffer
    from the UI thread — only call thumbnail.getMinAndMaxChannel(), which
    is O(1) per call (pre-computed peaks).

 2. A juce::Path is cached and only rebuilt when the component's width or
    the underlying audio content actually changes (detected via thumbnail
    sample count).  Repaint only blits the cached path — O(width).

 3. During recording the thumbnail grows in a background thread; each
    timer tick checks if more samples are available and only then rebuilds
    the path — still O(width), never O(numSamples).

 4. The path rebuild is decoupled from paint() — it happens in the timer
    callback, so paint() only ever calls strokePath() on a ready-made path.
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
    WaveformComponent (AudioEngine& engine);
    ~WaveformComponent() override;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster* source) override;

    void setExpanded (bool shouldExpand);

private:
    // Rebuild waveformPath from AudioThumbnail.  O(width), NOT O(numSamples).
    // Called only from timerCallback / changeListenerCallback, never from paint().
    void rebuildWaveformPath();

    AudioEngine& audioEngine;
    bool isExpanded = false;

    // ── Cached waveform path ──────────────────────────────────────────────
    juce::Path waveformPath;

    // Cache stamps — skip rebuild when nothing changed
    int   cachedPathWidth   = 0;   // getWidth() when path was last built
    int64 cachedThumbHash   = 0;   // thumb.getNumSamplesFinished() at last build

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveformComponent)
};
