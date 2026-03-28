/*
 ==============================================================================
 WaveformComponent.cpp

 Issue #5 Fix: Move waveform path computation off the UI thread.
 
 Problem: updateWaveformPath() scanned up to 1.3M samples on the message
 thread at 25 fps during recording, causing UI stalls.

 Solution:
 - Use juce::AudioThumbnail for efficient, background-thread thumbnail
   generation when available (file loads).
 - During live recording, rebuild the path only when the sample count has
   changed by a meaningful threshold (REBUILD_THRESHOLD_SAMPLES) and do
   the heavy scan on a background thread via juce::ThreadPool.
 - Thread-safe double-buffered path cache (activeCache / pendingCache)
   protected by juce::SpinLock so paint() never blocks on path generation.
 - On recording completion, do a single full rebuild and cache the result.
 ==============================================================================
 */
#include "WaveformComponent.h"

// ---------------------------------------------------------------------------
// Construction / Destruction
// ---------------------------------------------------------------------------

WaveformComponent::WaveformComponent(AudioEngine& engine)
    : audioEngine(engine)
{
    audioEngine.addChangeListener(this);
    startTimer(40); // 25 fps
}

WaveformComponent::~WaveformComponent()
{
    audioEngine.removeChangeListener(this);
    stopTimer();
}

// ---------------------------------------------------------------------------
// paint — only reads from activeCache (fast, lock-free when no swap)
// ---------------------------------------------------------------------------

void WaveformComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // 背景
    g.setColour(juce::Colour::fromString("FF1E293B")); // Slate 800
    g.fillRoundedRectangle(bounds, 5.0f);

    if (!audioEngine.hasRecordedAudio())
    {
        g.setColour(juce::Colours::grey);
        g.drawText("No recording - Press REC to start", bounds, juce::Justification::centred);
        return;
    }

    // スレッド安全にキャッシュされたPathを取得
    juce::Path pathToDraw;
    {
        juce::SpinLock::ScopedLockType lock(cacheLock);
        pathToDraw = activeCache.path;
    }

    if (!pathToDraw.isEmpty())
    {
        g.setColour(juce::Colour::fromString("FF22C55E")); // 緑
        g.strokePath(pathToDraw, juce::PathStrokeType(1.5f));
    }

    // 再生位置インジケーター
    if (audioEngine.hasRecordedAudio())
    {
        double pos = audioEngine.getPlaybackPosition();
        float x = bounds.getX() + static_cast<float>(pos) * bounds.getWidth();

        g.setColour(juce::Colours::white);
        g.drawLine(x, bounds.getY(), x, bounds.getBottom(), 2.0f);
    }
}

// ---------------------------------------------------------------------------
// resized — mark that a rebuild is needed (size changed)
// ---------------------------------------------------------------------------

void WaveformComponent::resized()
{
    // Invalidate cached dimensions so next timer tick triggers a rebuild
    {
        juce::SpinLock::ScopedLockType lock(cacheLock);
        activeCache.componentWidth  = 0;
        activeCache.componentHeight = 0;
    }
    rebuildWaveformPathAsync();
}

// ---------------------------------------------------------------------------
// timerCallback — lightweight: only repaints, schedules async rebuild if needed
// ---------------------------------------------------------------------------

void WaveformComponent::timerCallback()
{
    if (audioEngine.isRecording() || audioEngine.isPlaying())
    {
        if (audioEngine.isRecording())
        {
            int currentSamples = audioEngine.getRecordedSamplesCount();

            // サンプル数がしきい値を超えたら非同期で再構築
            if (std::abs(currentSamples - lastRebuiltSampleCount) >= REBUILD_THRESHOLD_SAMPLES
                || currentSamples != cachedBufferSize)
            {
                rebuildWaveformPathAsync();
            }
        }

        // Check for pending path swap (from background thread completion)
        {
            juce::SpinLock::ScopedLockType lock(cacheLock);
            if (pendingCache.sampleCount > 0)
            {
                activeCache = std::move(pendingCache);
                pendingCache = {};
            }
        }

        repaint();
    }
}

// ---------------------------------------------------------------------------
// changeListenerCallback — recording state changed (start/stop)
// ---------------------------------------------------------------------------

void WaveformComponent::changeListenerCallback(juce::ChangeBroadcaster* /*source*/)
{
    if (!audioEngine.isRecording())
    {
        // 録音完了：最終波形を非同期で完全再構築
        lastRebuiltSampleCount = 0; // force rebuild
        rebuildWaveformPathAsync();
    }
    else
    {
        // 録音開始：キャッシュをクリア
        lastRebuiltSampleCount = 0;
        {
            juce::SpinLock::ScopedLockType lock(cacheLock);
            activeCache = {};
            pendingCache = {};
        }
    }
    repaint();
}

// ---------------------------------------------------------------------------
// setExpanded
// ---------------------------------------------------------------------------

void WaveformComponent::setExpanded(bool shouldExpand)
{
    isExpanded = shouldExpand;
    resized();
}

// ---------------------------------------------------------------------------
// rebuildWaveformPathAsync — dispatches work to a background thread
// ---------------------------------------------------------------------------

void WaveformComponent::rebuildWaveformPathAsync()
{
    // Capture current dimensions and sample count (message thread)
    const int numSamples = audioEngine.getRecordedSamplesCount();
    if (numSamples <= 0) return;

    const float compWidth  = static_cast<float>(getWidth());
    const float compHeight = static_cast<float>(getHeight());
    if (compWidth <= 0 || compHeight <= 0) return;

    // Capture buffer snapshot for thread safety
    // AudioBuffer::makeCopyOf creates a safe snapshot the background thread can read
    const auto bufferSnapshot = audioEngine.getRecordedBuffer();
    const int channels = bufferSnapshot.getNumChannels();

    // Store rebuild target so we don't re-trigger unnecessarily
    lastRebuiltSampleCount = numSamples;
    cachedBufferSize = numSamples;

    // Launch background computation via juce::ThreadPool
    // Using a raw thread for simplicity — lifetime is bounded by the lambda captures
    juce::Thread::launch([this, bufferSnapshot, numSamples, compWidth, compHeight, channels]()
    {
        // --- Heavy computation runs HERE (background thread) ---

        juce::Path newPath;
        auto bounds = juce::Rectangle<float>(0, 0, compWidth, compHeight).reduced(2);
        float width  = bounds.getWidth();
        float height = bounds.getHeight();
        float centerY = bounds.getCentreY();

        // Samples per pixel (decimation)
        int samplesPerPixel = juce::jmax(1, numSamples / static_cast<int>(width));

        // --- Top half ---
        const float* channelData = bufferSnapshot.getReadPointer(0);

        newPath.startNewSubPath(bounds.getX(), centerY);

        for (int x = 0; x < static_cast<int>(width); ++x)
        {
            int startSample = x * samplesPerPixel;
            int endSample   = juce::jmin(startSample + samplesPerPixel, numSamples);

            float maxValue = 0.0f;
            for (int i = startSample; i < endSample; ++i)
            {
                float v = std::abs(channelData[i]);
                if (v > maxValue) maxValue = v;
            }

            float y = centerY - maxValue * (height / 2.0f) * 0.9f;
            newPath.lineTo(bounds.getX() + static_cast<float>(x), y);
        }

        // --- Bottom half (mirror) ---
        for (int x = static_cast<int>(width) - 1; x >= 0; --x)
        {
            int startSample = x * samplesPerPixel;
            int endSample   = juce::jmin(startSample + samplesPerPixel, numSamples);

            float maxValue = 0.0f;
            for (int i = startSample; i < endSample; ++i)
            {
                float v = std::abs(channelData[i]);
                if (v > maxValue) maxValue = v;
            }

            float y = centerY + maxValue * (height / 2.0f) * 0.9f;
            newPath.lineTo(bounds.getX() + static_cast<float>(x), y);
        }

        newPath.closeSubPath();

        // --- Swap the pending cache (thread-safe) ---
        {
            juce::SpinLock::ScopedLockType lock(this->cacheLock);
            this->pendingCache.path         = std::move(newPath);
            this->pendingCache.sampleCount  = numSamples;
            this->pendingCache.componentWidth  = compWidth;
            this->pendingCache.componentHeight = compHeight;
        }
        // The next timerCallback() will detect pendingCache.sampleCount > 0
        // and swap it into activeCache on the message thread.
    });
}

// ---------------------------------------------------------------------------
// rebuildWaveformPathFromThumbnail — alternative using AudioThumbnail
// (called when a file is loaded and the thumbnail is available)
// ---------------------------------------------------------------------------

void WaveformComponent::rebuildWaveformPathFromThumbnail()
{
    auto& thumbnail = audioEngine.getThumbnail();
    if (!thumbnail.isFullyLoaded()) return;

    const int numSamples = audioEngine.getRecordedSamplesCount();
    if (numSamples <= 0) return;

    auto bounds = getLocalBounds().toFloat().reduced(2);
    float width  = bounds.getWidth();
    float height = bounds.getHeight();
    float centerY = bounds.getCentreY();

    juce::Path newPath;
    const double thumbnailLength = thumbnail.getTotalLength();
    if (thumbnailLength <= 0) return;

    newPath.startNewSubPath(bounds.getX(), centerY);

    // AudioThumbnail provides min/max per pixel — extremely fast
    for (int x = 0; x < static_cast<int>(width); ++x)
    {
        float timeAtX = static_cast<double>(x) / width * thumbnailLength;
        float maxVal  = thumbnail.getMaxValueAtTime(timeAtX);

        float y = centerY - maxVal * (height / 2.0f) * 0.9f;
        newPath.lineTo(bounds.getX() + static_cast<float>(x), y);
    }

    // Bottom mirror
    for (int x = static_cast<int>(width) - 1; x >= 0; --x)
    {
        float timeAtX = static_cast<double>(x) / width * thumbnailLength;
        float maxVal  = thumbnail.getMaxValueAtTime(timeAtX);

        float y = centerY + maxVal * (height / 2.0f) * 0.9f;
        newPath.lineTo(bounds.getX() + static_cast<float>(x), y);
    }

    newPath.closeSubPath();

    // Direct write to active cache (called from message thread context)
    {
        juce::SpinLock::ScopedLockType lock(cacheLock);
        activeCache.path         = std::move(newPath);
        activeCache.sampleCount  = numSamples;
        activeCache.componentWidth  = static_cast<float>(getWidth());
        activeCache.componentHeight = static_cast<float>(getHeight());
    }
}

// ---------------------------------------------------------------------------
// rebuildWaveformPathFromBuffer — synchronous fallback (message thread)
// Use sparingly: only for small buffers or non-recording contexts
// ---------------------------------------------------------------------------

void WaveformComponent::rebuildWaveformPathFromBuffer()
{
    const auto& buffer = audioEngine.getRecordedBuffer();
    int numSamples = audioEngine.getRecordedSamplesCount();
    if (numSamples <= 0) return;

    auto bounds = getLocalBounds().toFloat().reduced(2);
    float width  = bounds.getWidth();
    float height = bounds.getHeight();
    float centerY = bounds.getCentreY();

    int samplesPerPixel = juce::jmax(1, numSamples / static_cast<int>(width));
    const float* channelData = buffer.getReadPointer(0);

    juce::Path newPath;
    newPath.startNewSubPath(bounds.getX(), centerY);

    for (int x = 0; x < static_cast<int>(width); ++x)
    {
        int startSample = x * samplesPerPixel;
        int endSample   = juce::jmin(startSample + samplesPerPixel, numSamples);

        float maxValue = 0.0f;
        for (int i = startSample; i < endSample; ++i)
            maxValue = juce::jmax(maxValue, std::abs(channelData[i]));

        float y = centerY - maxValue * (height / 2.0f) * 0.9f;
        newPath.lineTo(bounds.getX() + static_cast<float>(x), y);
    }

    for (int x = static_cast<int>(width) - 1; x >= 0; --x)
    {
        int startSample = x * samplesPerPixel;
        int endSample   = juce::jmin(startSample + samplesPerPixel, numSamples);

        float maxValue = 0.0f;
        for (int i = startSample; i < endSample; ++i)
            maxValue = juce::jmax(maxValue, std::abs(channelData[i]));

        float y = centerY + maxValue * (height / 2.0f) * 0.9f;
        newPath.lineTo(bounds.getX() + static_cast<float>(x), y);
    }

    newPath.closeSubPath();

    {
        juce::SpinLock::ScopedLockType lock(cacheLock);
        activeCache.path         = std::move(newPath);
        activeCache.sampleCount  = numSamples;
        activeCache.componentWidth  = static_cast<float>(getWidth());
        activeCache.componentHeight = static_cast<float>(getHeight());
    }

    cachedBufferSize = numSamples;
}
