/*
 ==============================================================================
 AudioEngine.cpp
 ==============================================================================
 Thread safety strategy:
   1. Atomic flags (recordingState, playing, recordWritePosition, etc.)
      for fast lock-free checks in the audio thread.
   2. Buffer swap pattern: main thread modifies a staging buffer under
      CriticalSection, then atomically publishes the pointer. The audio
      thread loads the pointer once per callback and reads from it — zero
      contention, zero blocking.
   3. recordAudioBlock writes directly into activeBuffer (the audio thread
      "owns" the recording write position). startRecording() stops playback
      first, clears the buffer, then sets recordingState.
   4. playbackPosition is std::atomic<double> — written only by the audio
      thread (single-writer) and read by the main thread for UI display.
 */
#include "AudioEngine.h"

AudioEngine::AudioEngine()
: thumbnail(512, formatManager, thumbnailCache)
{
	formatManager.registerBasicFormats();
	allocateBuffers(44100 * 30, 44100.0);
}

AudioEngine::~AudioEngine()
{
    transportSource.setSource(nullptr);
    recordingState.store(false, std::memory_order_release);
    playing.store(false, std::memory_order_release);
}

// ---------------------------------------------------------------------------
// Buffer management
// ---------------------------------------------------------------------------

void AudioEngine::allocateBuffers(int maxSamples, double sampleRate)
{
    // Called from main thread before audio starts — no lock needed
    auto makeBuf = [maxSamples]() {
        auto sb = std::make_unique<SharedBuffer>();
        sb->buffer.setSize(2, maxSamples, true, true, true);
        sb->buffer.clear();
        sb->writePosition = 0;
        return sb;
    };
    activeBuffer = makeBuf();
    stagingBuffer = makeBuf();
    audioBufferPtr.store(activeBuffer.get(), std::memory_order_release);
}

void AudioEngine::commitStagingBuffer()
{
    // Main thread only. Swaps staging <-> active under CS.
    juce::ScopedLock lock(bufferSwapLock);
    std::swap(activeBuffer, stagingBuffer);
    audioBufferPtr.store(activeBuffer.get(), std::memory_order_release);
}

// ---------------------------------------------------------------------------
// AudioSource overrides
// ---------------------------------------------------------------------------

void AudioEngine::prepareToPlay(int samplesPerBlockExpected, double sampleRate)
{
    currentSampleRate.store(sampleRate, std::memory_order_relaxed);
    transportSource.prepareToPlay(samplesPerBlockExpected, sampleRate);

    // Reallocate buffers for the actual sample rate (30 seconds)
    int maxSamples = static_cast<int>(sampleRate * 30.0);
    allocateBuffers(maxSamples, sampleRate);

    crossfaderGain.reset(sampleRate, 0.01); // 10ms smoothing

    if (resamplerSource)
        resamplerSource->prepareToPlay(samplesPerBlockExpected, sampleRate);
}

void AudioEngine::releaseResources()
{
    transportSource.releaseResources();
    if (resamplerSource)
        resamplerSource->releaseResources();
}

void AudioEngine::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
    // Fast atomic check — no lock, no blocking
    if (playing.load(std::memory_order_acquire))
    {
        // Snapshot the shared pointer once — this is the only atomic read
        SharedBuffer* buf = audioBufferPtr.load(std::memory_order_acquire);
        int wp = buf ? buf->writePosition : 0;

        if (buf && wp > 0 && buf->buffer.getNumSamples() > 0)
        {
            auto* outputBuffer = bufferToFill.buffer;
            int numSamplesToFill = bufferToFill.numSamples;
            int numChannels = juce::jmin(outputBuffer->getNumChannels(),
                                        buf->buffer.getNumChannels());

            double localPlaybackPos = playbackPosition.load(std::memory_order_relaxed);
            double localSpeed = targetScratchSpeed.load(std::memory_order_relaxed);

            for (int sample = 0; sample < numSamplesToFill; ++sample)
            {
                int pos0 = static_cast<int>(localPlaybackPos);
                int pos1 = pos0 + 1;
                float frac = static_cast<float>(localPlaybackPos - pos0);

                pos0 = juce::jlimit(0, wp - 1, pos0);
                pos1 = juce::jlimit(0, wp - 1, pos1);

                float gain = crossfaderGain.getNextValue();

                for (int ch = 0; ch < numChannels; ++ch)
                {
                    float s0 = buf->buffer.getSample(ch, pos0);
                    float s1 = buf->buffer.getSample(ch, pos1);
                    float interpolated = s0 + frac * (s1 - s0);
                    outputBuffer->addSample(ch, bufferToFill.startSample + sample,
                                            interpolated * gain);
                }

                localPlaybackPos += localSpeed;

                if (localPlaybackPos >= wp)
                    localPlaybackPos = 0.0;
                else if (localPlaybackPos < 0.0)
                    localPlaybackPos = static_cast<double>(wp - 1);
            }

            // Write back (single-writer atomic)
            playbackPosition.store(localPlaybackPos, std::memory_order_relaxed);
            return;
        }
    }

    // Not playing or no data — advance the crossfader smoother
    for (int i = 0; i < bufferToFill.numSamples; ++i)
        crossfaderGain.getNextValue();
}

void AudioEngine::recordAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
    // Fast atomic check — no lock
    if (!recordingState.load(std::memory_order_acquire))
        return;

    SharedBuffer* buf = audioBufferPtr.load(std::memory_order_acquire);
    if (!buf) return;

    auto* inputBuffer = bufferToFill.buffer;
    int numSamples = bufferToFill.numSamples;
    int numChannels = juce::jmin(inputBuffer->getNumChannels(),
                                buf->buffer.getNumChannels());

    int wp = buf->writePosition;

    if (wp + numSamples <= buf->buffer.getNumSamples())
    {
        for (int ch = 0; ch < numChannels; ++ch)
        {
            buf->buffer.copyFrom(ch, wp,
                                 *inputBuffer, ch, bufferToFill.startSample, numSamples);
        }
        buf->writePosition = wp + numSamples;
        recordWritePosition.store(wp + numSamples, std::memory_order_release);
    }
    else
    {
        // Buffer full — stop recording atomically
        recordingState.store(false, std::memory_order_release);
        playbackPosition.store(0.0, std::memory_order_release);
    }
}

// ---------------------------------------------------------------------------
// Control methods
// ---------------------------------------------------------------------------

void AudioEngine::loadFile(const juce::File& file)
{
    auto* reader = formatManager.createReaderFor(file);
    if (reader != nullptr)
    {
        std::unique_ptr<juce::AudioFormatReaderSource> newSource(
            new juce::AudioFormatReaderSource(reader, true));
        transportSource.setSource(newSource.get(), 0, nullptr, reader->sampleRate);
        readerSource.reset(newSource.release());

        resamplerSource.reset(
            new juce::ResamplingAudioSource(&transportSource, false, 2));

        sendChangeMessage();
    }
}

void AudioEngine::play()
{
    if (hasRecordedAudio())
    {
        playbackPosition.store(0.0, std::memory_order_release);
        targetScratchSpeed.store(1.0, std::memory_order_release);
        playing.store(true, std::memory_order_release);
    }
}

void AudioEngine::stop()
{
    playing.store(false, std::memory_order_release);
}

void AudioEngine::setScratchRate(double rate)
{
    targetScratchSpeed.store(rate, std::memory_order_release);
}

void AudioEngine::setCrossfaderGain(float gain)
{
    crossfaderGain.setTargetValue(gain);
}

// ---------------------------------------------------------------------------
// Recording
// ---------------------------------------------------------------------------

void AudioEngine::startRecording()
{
    // Stop playback first (release ordering ensures audio thread sees it)
    playing.store(false, std::memory_order_release);

    // Clear the recording buffer
    {
        juce::ScopedLock lock(bufferSwapLock);
        // Safe because playback is stopped and recordingState is still false
        activeBuffer->buffer.clear();
        activeBuffer->writePosition = 0;
        recordWritePosition.store(0, std::memory_order_release);
        playbackPosition.store(0.0, std::memory_order_release);
    }

    // Now enable recording — audio thread picks it up on next callback
    recordingState.store(true, std::memory_order_release);
    sendChangeMessage();
}

void AudioEngine::stopRecording()
{
    recordingState.store(false, std::memory_order_release);
    playbackPosition.store(0.0, std::memory_order_release);
    sendChangeMessage();
}

// ---------------------------------------------------------------------------
// Playback position
// ---------------------------------------------------------------------------

void AudioEngine::setPlaybackPosition(double normalizedPosition)
{
    int wp = recordWritePosition.load(std::memory_order_acquire);
    if (wp > 0)
    {
        double pos = normalizedPosition * static_cast<double>(wp);
        pos = juce::jlimit(0.0, static_cast<double>(wp - 1), pos);
        playbackPosition.store(pos, std::memory_order_release);
    }
}

double AudioEngine::getPlaybackPosition() const
{
    int wp = recordWritePosition.load(std::memory_order_acquire);
    if (wp > 0)
        return playbackPosition.load(std::memory_order_relaxed) / static_cast<double>(wp);
    return 0.0;
}

void AudioEngine::setScratchSpeed(double speed)
{
    targetScratchSpeed.store(speed, std::memory_order_release);
}

// ---------------------------------------------------------------------------
// Change listener
// ---------------------------------------------------------------------------

void AudioEngine::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    juce::ignoreUnused(source);
}

// ---------------------------------------------------------------------------
// Buffer access (UI thread)
// ---------------------------------------------------------------------------

juce::AudioBuffer<float> AudioEngine::getRecordedBufferCopy() const
{
    // Snapshot current buffer pointer without blocking audio thread
    SharedBuffer* buf = audioBufferPtr.load(std::memory_order_acquire);
    juce::AudioBuffer<float> copy;

    if (buf)
    {
        int wp = buf->writePosition;
        if (wp > 0)
        {
            copy = juce::AudioBuffer<float>(buf->buffer.getNumChannels(), wp);
            for (int ch = 0; ch < copy.getNumChannels(); ++ch)
                copy.copyFrom(ch, 0, buf->buffer, ch, 0, wp);
        }
    }
    return copy;
}

// ---------------------------------------------------------------------------
// File I/O
// ---------------------------------------------------------------------------

juce::File AudioEngine::getLibraryFolder() const
{
    auto folder = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("ScratchMyVoice")
        .getChildFile("Library");

    if (!folder.exists())
        folder.createDirectory();

    return folder;
}

juce::File AudioEngine::saveRecordingToFile()
{
    int wp = recordWritePosition.load(std::memory_order_acquire);
    if (wp <= 0)
        return juce::File();

    // Take a snapshot — no lock held during I/O
    juce::AudioBuffer<float> snapshot = getRecordedBufferCopy();
    double sampleRate = currentSampleRate.load(std::memory_order_relaxed);

    if (snapshot.getNumSamples() <= 0)
        return juce::File();

    auto libraryFolder = getLibraryFolder();
    auto now = juce::Time::getCurrentTime();
    auto fileName = now.formatted("Recording_%Y%m%d_%H%M%S.wav");
    auto outputFile = libraryFolder.getChildFile(fileName);

    juce::WavAudioFormat wavFormat;
    std::unique_ptr<juce::AudioFormatWriter> writer(
        wavFormat.createWriterFor(
            new juce::FileOutputStream(outputFile),
            sampleRate,
            static_cast<unsigned int>(snapshot.getNumChannels()),
            16,
            {},
            0
        )
    );

    if (writer != nullptr)
    {
        writer->writeFromAudioSampleBuffer(snapshot, 0, snapshot.getNumSamples());
        return outputFile;
    }

    return juce::File();
}

void AudioEngine::loadFileToBuffer(const juce::File& file)
{
    auto* reader = formatManager.createReaderFor(file);
    if (reader != nullptr)
    {
        int numSamples = static_cast<int>(reader->lengthInSamples);
        int numChannels = static_cast<int>(reader->numChannels);

        // Stop audio operations while loading
        playing.store(false, std::memory_order_release);
        recordingState.store(false, std::memory_order_release);

        {
            juce::ScopedLock lock(bufferSwapLock);
            stagingBuffer->buffer.setSize(juce::jmax(2, numChannels), numSamples,
                                          true, true, true);
            reader->read(&stagingBuffer->buffer, 0, numSamples, 0, true, true);
            stagingBuffer->writePosition = numSamples;
        }

        currentSampleRate.store(reader->sampleRate, std::memory_order_release);
        delete reader;

        // Atomically publish the new buffer
        commitStagingBuffer();
        recordWritePosition.store(numSamples, std::memory_order_release);
        playbackPosition.store(0.0, std::memory_order_release);

        sendChangeMessage();
    }
}

// ---------------------------------------------------------------------------
// Sample Slots
// ---------------------------------------------------------------------------

void AudioEngine::loadFileToSlot(int slotIndex, const juce::File& file)
{
    if (slotIndex < 0 || slotIndex >= NUM_SLOTS) return;

    auto* reader = formatManager.createReaderFor(file);
    if (reader != nullptr)
    {
        int numSamples = static_cast<int>(reader->lengthInSamples);
        int numChannels = static_cast<int>(reader->numChannels);

        auto& slot = sampleSlots[static_cast<size_t>(slotIndex)];
        slot.buffer.setSize(juce::jmax(2, numChannels), numSamples,
                            true, true, true);
        reader->read(&slot.buffer, 0, numSamples, 0, true, true);
        slot.numSamples = numSamples;
        slot.fileName = file.getFileNameWithoutExtension();

        delete reader;

        if (slotIndex == activeSlotIndex)
            setActiveSlot(slotIndex);

        sendChangeMessage();
    }
}

void AudioEngine::setActiveSlot(int slotIndex)
{
    if (slotIndex < 0 || slotIndex >= NUM_SLOTS) return;

    activeSlotIndex = slotIndex;

    const auto& slot = sampleSlots[static_cast<size_t>(slotIndex)];
    if (slot.numSamples > 0)
    {
        playing.store(false, std::memory_order_release);
        recordingState.store(false, std::memory_order_release);

        {
            juce::ScopedLock lock(bufferSwapLock);
            stagingBuffer->buffer.makeCopyOf(slot.buffer);
            stagingBuffer->writePosition = slot.numSamples;
        }

        commitStagingBuffer();
        recordWritePosition.store(slot.numSamples, std::memory_order_release);
        playbackPosition.store(0.0, std::memory_order_release);

        sendChangeMessage();
    }
}

juce::String AudioEngine::getSlotFileName(int slotIndex) const
{
    if (slotIndex < 0 || slotIndex >= NUM_SLOTS) return "";
    return sampleSlots[static_cast<size_t>(slotIndex)].fileName;
}

bool AudioEngine::isSlotLoaded(int slotIndex) const
{
    if (slotIndex < 0 || slotIndex >= NUM_SLOTS) return false;
    return sampleSlots[static_cast<size_t>(slotIndex)].numSamples > 0;
}
