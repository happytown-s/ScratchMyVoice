/*
 ==============================================================================
 WaveformComponent.h
 ==============================================================================
 Issue #15 — Pinch-to-zoom, double-tap reset, flick-to-scroll
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

    // Mouse interaction (desktop)
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

    // Touch interaction (Issue #15)
    void touchStarted (const juce::TouchEvent& e) override;
    void touchMoved (const juce::TouchEvent& e) override;
    void touchEnded (const juce::TouchEvent& e) override;

    void setExpanded (bool shouldExpand);

private:
    // Rebuild waveformPath from AudioThumbnail.  O(width), NOT O(numSamples).
    void rebuildWaveformPath();
    void clampScroll();

    AudioEngine& audioEngine;
    bool isExpanded = false;

    // ── Waveform zoom & scroll (Issue #15) ─────────────────────────────────
    float zoomLevel = 1.0f;          // 1.0 = fit all, 2.0 = 2x zoom, etc.
    float scrollOffset = 0.0f;       // 0..1 normalized scroll position

    // ── Pinch-zoom state ───────────────────────────────────────────────────
    int  primaryTouchIndex = -1;
    int  secondaryTouchIndex = -1;
    float pinchStartDistance = 0.0f;
    float pinchStartZoom = 1.0f;
    bool isPinching = false;

    // ── Flick (momentum) state ─────────────────────────────────────────────
    float flickVelocity = 0.0f;      // pixels per second
    double lastFlickTime = 0.0;
    float lastFlickX = 0.0f;

    // ── Double-tap state ───────────────────────────────────────────────────
    double lastTapTime = 0.0;
    juce::Point<float> lastTapPos;
    static constexpr double doubleTapThreshold = 300.0; // ms

    // ── Cached waveform path ──────────────────────────────────────────────
    juce::Path waveformPath;

    // Cache stamps — skip rebuild when nothing changed
    int   cachedPathWidth   = 0;
    int64 cachedThumbHash   = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveformComponent)
};
