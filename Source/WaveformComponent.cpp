/*
 ==============================================================================
 WaveformComponent.cpp
 ==============================================================================
 Issue #5  — waveform drawing no longer blocks the UI thread.
 Issue #15 — Pinch-to-zoom, double-tap reset, flick-to-scroll.

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
    // Enable multi-touch for pinch-zoom gestures
    setInterceptsMouseClicks (true, true);

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

    // ── Zoom & scroll clip region (Issue #15) ──────────────────────────
    float visibleFraction = 1.0f / juce::jmax (zoomLevel, 1.0f);

    // Apply clip for scrolled view
    g.saveState();
    g.reduceClipRegion (bounds.toNearestInt());

    // Scale waveform path for zoom
    juce::AffineTransform zoomTransform;
    float scaledWidth = bounds.getWidth() * zoomLevel;
    float xOffset = -scrollOffset * (scaledWidth - bounds.getWidth());
    zoomTransform = juce::AffineTransform::scale (zoomLevel).translated (xOffset, 0.0f);

    // 波形の描画 — cached pathをblitするだけ (O(width))
    g.setColour (juce::Colour::fromString ("FF22C55E")); // 緑
    g.strokePath (waveformPath, juce::PathStrokeType (1.5f), zoomTransform);

    // 再生位置インジケーター
    if (audioEngine.hasRecordedAudio())
    {
        double pos = audioEngine.getPlaybackPosition();
        float x = bounds.getX() + static_cast<float> (pos) * bounds.getWidth() * zoomLevel + xOffset;

        // Only draw if visible
        if (x >= bounds.getX() && x <= bounds.getRight())
        {
            g.setColour (juce::Colours::white);
            g.drawLine (x, bounds.getY(), x, bounds.getBottom(), 2.0f);
        }
    }

    g.restoreState();

    // ── Zoom level indicator ────────────────────────────────────────────
    if (zoomLevel > 1.05f)
    {
        g.setColour (juce::Colours::white.withAlpha (0.7f));
        g.setFont (11.0f);
        g.drawText (juce::String::formatted ("%.1fx", (double) zoomLevel),
                    bounds.removeFromBottom (16.0f), juce::Justification::centredRight, false);
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
        rebuildWaveformPath();
        repaint();
        return;
    }

    // ── Flick momentum animation (Issue #15) ────────────────────────────
    if (std::abs (flickVelocity) > 5.0f)
    {
        double now = juce::Time::getMillisecondCounterHiRes();
        double dt = (now - lastFlickTime) * 0.001;
        lastFlickTime = now;

        if (dt > 0.0 && dt < 0.2)
        {
            float visibleFraction = 1.0f / juce::jmax (zoomLevel, 1.0f);
            float maxScroll = 1.0f - visibleFraction;

            scrollOffset += flickVelocity * dt * 0.001f;
            scrollOffset = juce::jlimit (0.0f, juce::jmax (maxScroll, 0.0f), scrollOffset);

            // Apply friction
            flickVelocity *= 0.92f;

            repaint();
        }
    }
    else if (std::abs (flickVelocity) > 0.0f)
    {
        flickVelocity = 0.0f;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
void WaveformComponent::changeListenerCallback (juce::ChangeBroadcaster* /*source*/)
{
    // Recording started/stopped or file loaded → full rebuild needed
    cachedPathWidth = 0;
    cachedThumbHash = 0;
    zoomLevel = 1.0f;
    scrollOffset = 0.0f;
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

// ─── Mouse interaction (desktop) ─────────────────────────────────────────────

void WaveformComponent::mouseDrag (const juce::MouseEvent& e)
{
    float visibleFraction = 1.0f / juce::jmax (zoomLevel, 1.0f);
    float maxScroll = 1.0f - visibleFraction;
    if (maxScroll <= 0.0f) return;

    float dx = e.getDistanceFromDragStartX() - e.mouseDownPosition.getX();
    float width = (float) getWidth();
    if (width <= 0.0f) return;

    scrollOffset -= dx / (width * visibleFraction);
    scrollOffset = juce::jlimit (0.0f, maxScroll, scrollOffset);
    repaint();
}

void WaveformComponent::mouseWheelMove (const juce::MouseEvent& e,
                                         const juce::MouseWheelDetails& wheel)
{
    juce::ignoreUnused (e);

    if (wheel.deltaY != 0.0f)
    {
        // Ctrl+scroll or Shift+scroll → zoom
        if (e.mods.isCtrlDown() || e.mods.isShiftDown())
        {
            float oldZoom = zoomLevel;
            zoomLevel *= (wheel.deltaY > 0.0f) ? 1.15f : (1.0f / 1.15f);
            zoomLevel = juce::jlimit (1.0f, 20.0f, zoomLevel);

            // Zoom towards mouse position
            if (oldZoom > 0.0f && getWidth() > 0)
            {
                float mouseX = (float) e.position.x / (float) getWidth();
                float oldScrollCenter = scrollOffset + mouseX / oldZoom;
                float newScrollCenter = scrollOffset + mouseX / zoomLevel;
                scrollOffset += (oldScrollCenter - newScrollCenter) * 0.5f;
            }

            clampScroll();
            cachedPathWidth = 0; // rebuild for new zoom
            rebuildWaveformPath();
        }
        else
        {
            // Normal scroll
            float visibleFraction = 1.0f / juce::jmax (zoomLevel, 1.0f);
            float maxScroll = 1.0f - visibleFraction;
            scrollOffset += wheel.deltaY * 0.1f;
            scrollOffset = juce::jlimit (0.0f, juce::jmax (maxScroll, 0.0f), scrollOffset);
        }
        repaint();
    }
}

// ─── Touch interaction (Issue #15) ───────────────────────────────────────────

void WaveformComponent::touchStarted (const juce::TouchEvent& e)
{
    flickVelocity = 0.0f;

    if (e.getTouches().size() == 1)
    {
        const auto& touch = e.getTouch (e.stack.size() - 1);
        primaryTouchIndex = touch.getIndex();

        // ── Double-tap detection ─────────────────────────────────────────
        double now = juce::Time::getMillisecondCounterHiRes();
        float dx = touch.position.x - lastTapPos.x;
        float dy = touch.position.y - lastTapPos.y;
        float dist = std::sqrt (dx * dx + dy * dy);

        if (dist < 30.0f && (now - lastTapTime) < doubleTapThreshold)
        {
            // Double-tap → reset zoom to fit
            zoomLevel = 1.0f;
            scrollOffset = 0.0f;
            cachedPathWidth = 0;
            rebuildWaveformPath();
            repaint();
            lastTapTime = 0.0; // prevent triple-tap
            return;
        }

        lastTapTime = now;
        lastTapPos = touch.position;
        lastFlickTime = now;
        lastFlickX = touch.position.x;
    }
    else if (e.getTouches().size() >= 2 && primaryTouchIndex >= 0)
    {
        // ── Pinch-zoom start ────────────────────────────────────────────
        juce::Point<float> p1, p2;
        bool found1 = false, found2 = false;

        for (const auto& t : e.getTouches())
        {
            if (t.getIndex() == primaryTouchIndex) { p1 = t.position; found1 = true; }
            else if (!found2) { p2 = t.position; secondaryTouchIndex = t.getIndex(); found2 = true; }
        }

        if (found1 && found2)
        {
            pinchStartDistance = p1.getDistanceFrom (p2);
            pinchStartZoom = zoomLevel;
            isPinching = true;
            flickVelocity = 0.0f;
        }
    }
}

void WaveformComponent::touchMoved (const juce::TouchEvent& e)
{
    const auto& touches = e.getTouches();

    if (isPinching && touches.size() >= 2)
    {
        // ── Pinch-to-zoom ───────────────────────────────────────────────
        juce::Point<float> p1, p2;
        bool found1 = false, found2 = false;

        for (const auto& t : touches)
        {
            if (t.getIndex() == primaryTouchIndex) { p1 = t.position; found1 = true; }
            if (t.getIndex() == secondaryTouchIndex) { p2 = t.position; found2 = true; }
        }

        if (found1 && found2 && pinchStartDistance > 1.0f)
        {
            float currentDistance = p1.getDistanceFrom (p2);
            float scale = currentDistance / pinchStartDistance;

            zoomLevel = pinchStartZoom * scale;
            zoomLevel = juce::jlimit (1.0f, 20.0f, zoomLevel);

            // Pinch center → adjust scroll to zoom toward center
            juce::Point<float> pinchCenter = (p1 + p2) * 0.5f;
            float centerNorm = pinchCenter.x / (float) getWidth();

            float oldZoom = pinchStartZoom;
            if (oldZoom > 0.0f)
            {
                float visibleFraction = 1.0f / zoomLevel;
                float maxScroll = 1.0f - visibleFraction;
                scrollOffset = centerNorm - centerNorm / zoomLevel;
                scrollOffset = juce::jlimit (0.0f, juce::jmax (maxScroll, 0.0f), scrollOffset);
            }

            cachedPathWidth = 0;
            rebuildWaveformPath();
            repaint();
        }
    }
    else if (!isPinching && primaryTouchIndex >= 0 && touches.size() == 1)
    {
        // ── Single-touch scroll with flick tracking ─────────────────────
        const auto& touch = e.getTouch (primaryTouchIndex);
        if (!touch.isValid()) return;

        float visibleFraction = 1.0f / juce::jmax (zoomLevel, 1.0f);
        if (visibleFraction >= 1.0f) return;

        double now = juce::Time::getMillisecondCounterHiRes();
        double dt = now - lastFlickTime;
        float dx = touch.position.x - lastFlickX;

        if (dt > 0.0 && dt < 200.0)
        {
            flickVelocity = dx / (float) dt * 1000.0f; // px/s
        }

        lastFlickTime = now;
        lastFlickX = touch.position.x;

        float maxScroll = 1.0f - visibleFraction;
        float width = (float) getWidth();
        if (width > 0.0f)
        {
            scrollOffset -= dx / (width * visibleFraction);
            scrollOffset = juce::jlimit (0.0f, juce::jmax (maxScroll, 0.0f), scrollOffset);
            repaint();
        }
    }
}

void WaveformComponent::touchEnded (const juce::TouchEvent& e)
{
    const auto& endedTouch = e.getTouch (e.stack.size() - 1);

    if (endedTouch.getIndex() == secondaryTouchIndex)
    {
        secondaryTouchIndex = -1;
        isPinching = false;
    }
    else if (endedTouch.getIndex() == primaryTouchIndex)
    {
        primaryTouchIndex = -1;
        isPinching = false;
        secondaryTouchIndex = -1;
        // Flick momentum continues via timerCallback
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

// ── Helper ──────────────────────────────────────────────────────────────────
void WaveformComponent::clampScroll()
{
    float visibleFraction = 1.0f / juce::jmax (zoomLevel, 1.0f);
    float maxScroll = 1.0f - visibleFraction;
    scrollOffset = juce::jlimit (0.0f, juce::jmax (maxScroll, 0.0f), scrollOffset);
}
