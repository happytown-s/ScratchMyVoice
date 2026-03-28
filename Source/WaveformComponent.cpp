/*
 ==============================================================================
 WaveformComponent.cpp
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
#include "WaveformComponent.h"

WaveformComponent::WaveformComponent (AudioEngine& engine)
    : audioEngine (engine)
{
    // AudioEngineの変更を監視
    audioEngine.addChangeListener (this);
    startTimer (40); // 25fps
}

WaveformComponent::~WaveformComponent()
{
    audioEngine.removeChangeListener (this);
    stopTimer();
}

// ─────────────────────────────────────────────────────────────────────────────
void WaveformComponent::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // 背景
    g.setColour (juce::Colour::fromString ("FF1E293B")); // Slate 800
    g.fillRoundedRectangle (bounds, 5.0f);

    if (! audioEngine.hasRecordedAudio())
    {
        // 録音がない場合のメッセージ
        g.setColour (juce::Colours::grey);
        g.drawText ("No recording - Press REC to start", bounds, juce::Justification::centred);
        return;
    }

    // 波形の描画 — cached pathをblitするだけ (O(width))
    g.setColour (juce::Colour::fromString ("FF22C55E")); // 緑
    g.strokePath (waveformPath, juce::PathStrokeType (1.5f));

    // 再生位置インジケーター
    if (audioEngine.hasRecordedAudio())
    {
        double pos = audioEngine.getPlaybackPosition();
        float x = bounds.getX() + static_cast<float> (pos) * bounds.getWidth();

        g.setColour (juce::Colours::white);
        g.drawLine (x, bounds.getY(), x, bounds.getBottom(), 2.0f);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
void WaveformComponent::resized()
{
    // Width changed → force path rebuild on next timer tick
    cachedPathWidth = 0;
}

// ─────────────────────────────────────────────────────────────────────────────
void WaveformComponent::timerCallback()
{
    // 録音中または再生中は再描画
    if (audioEngine.isRecording() || audioEngine.isPlaying())
    {
        // Check if waveform path needs rebuilding (new samples during recording,
        // or width changed).  rebuildWaveformPath() is O(width) — not O(numSamples).
        rebuildWaveformPath();
        repaint();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
void WaveformComponent::changeListenerCallback (juce::ChangeBroadcaster* /*source*/)
{
    // Recording started/stopped or file loaded → full rebuild needed
    cachedPathWidth = 0;
    cachedThumbHash = 0;
    rebuildWaveformPath();
    repaint();
}

// ─────────────────────────────────────────────────────────────────────────────
void WaveformComponent::setExpanded (bool shouldExpand)
{
    if (isExpanded != shouldExpand)
    {
        isExpanded = shouldExpand;
        cachedPathWidth = 0;
        resized();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// rebuildWaveformPath()
//
// Uses juce::AudioThumbnail::getMinAndMaxChannel() which returns pre-computed
// peak data — each call is O(1), so the whole loop is O(width) regardless of
// how many samples are in the buffer (e.g. 1.3M samples @ 44.1kHz = 30s).
// ─────────────────────────────────────────────────────────────────────────────
void WaveformComponent::rebuildWaveformPath()
{
    auto& thumb = audioEngine.getRecordedThumbnail();
    int64 totalFinished = thumb.getNumSamplesFinished();

    // Skip if nothing to draw
    if (totalFinished <= 0)
    {
        waveformPath.clear();
        return;
    }

    int currentWidth = getWidth();

    // ── Cache check: skip rebuild if nothing changed ───────────────────
    if (currentWidth == cachedPathWidth && totalFinished == cachedThumbHash)
        return;

    auto bounds = getLocalBounds().toFloat().reduced (2);
    float width  = bounds.getWidth();
    float height = bounds.getHeight();
    float centerY = bounds.getCentreY();

    if (width <= 0.0f || height <= 0.0f)
        return;

    waveformPath.clear();

    // Build waveform using thumbnail peaks — O(width) loop
    waveformPath.startNewSubPath (bounds.getX(), centerY);

    for (int x = 0; x < static_cast<int> (width); ++x)
    {
        // Map pixel x to sample range
        double startSample = (static_cast<double> (x) / width) * static_cast<double> (totalFinished);
        double endSample   = (static_cast<double> (x + 1) / width) * static_cast<double> (totalFinished);

        float minValue = 0.0f, maxValue = 0.0f;

        // getMinAndMaxChannel is O(1) — reads pre-computed peaks from
        // the AudioThumbnail's internal cache.  NO raw sample scanning.
        if (thumb.getMinAndMaxChannel (0,
                                       static_cast<int64> (startSample),
                                       static_cast<int64> (endSample),
                                       minValue, maxValue))
        {
            float absMax = juce::jmax (std::abs (minValue), std::abs (maxValue));
            float y = centerY - absMax * (height * 0.45f);
            waveformPath.lineTo (bounds.getX() + static_cast<float> (x), y);
        }
        else
        {
            waveformPath.lineTo (bounds.getX() + static_cast<float> (x), centerY);
        }
    }

    // Mirror (bottom half)
    for (int x = static_cast<int> (width) - 1; x >= 0; --x)
    {
        double startSample = (static_cast<double> (x) / width) * static_cast<double> (totalFinished);
        double endSample   = (static_cast<double> (x + 1) / width) * static_cast<double> (totalFinished);

        float minValue = 0.0f, maxValue = 0.0f;

        if (thumb.getMinAndMaxChannel (0,
                                       static_cast<int64> (startSample),
                                       static_cast<int64> (endSample),
                                       minValue, maxValue))
        {
            float absMax = juce::jmax (std::abs (minValue), std::abs (maxValue));
            float y = centerY + absMax * (height * 0.45f);
            waveformPath.lineTo (bounds.getX() + static_cast<float> (x), y);
        }
        else
        {
            waveformPath.lineTo (bounds.getX() + static_cast<float> (x), centerY);
        }
    }

    waveformPath.closeSubPath();

    // Update cache stamps
    cachedPathWidth = currentWidth;
    cachedThumbHash = totalFinished;
}
