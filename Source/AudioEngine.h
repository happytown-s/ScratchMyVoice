/*
 ==============================================================================
 AudioEngine.h
 ==============================================================================
 Thread safety design:
   - Audio thread (getNextAudioBlock, recordAudioBlock): NEVER blocks.
     Uses std::atomic flags and reads from a stable buffer pointer.
   - Main thread (startRecording, stopRecording, loadFileToBuffer, etc.):
     Writes to a staging buffer under a juce::CriticalSection, then
     atomically swaps the pointer (std::atomic_exchange) for the audio thread.
   - Playback state (playbackPosition) is only written by the audio thread
     and read by the main thread via getPlaybackPosition() — safe by
     single-writer discipline with relaxed atomics.
 */
#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <memory>
#include "Constants.h"

class AudioEngine : public juce::AudioSource,
public juce::ChangeListener,
public juce::ChangeBroadcaster
{
	public:
	AudioEngine();
	~AudioEngine() override;

	// AudioSource overrides
	void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;
	void releaseResources() override;
	void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override;

	// Control Methods
	void loadFile(const juce::File& file);
	void play();
	void stop();
	void setScratchRate(double rate); // Controls playback speed/pitch
	void setCrossfaderGain(float gain); // 0.0 (Silent) to 1.0 (Thru)

	// Recording - マイクから録音してバッファに保存
	void startRecording();
	void stopRecording();
	void recordAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill);
	bool isRecording() const { return recordingState.load(std::memory_order_acquire); }
	bool hasRecordedAudio() const { return recordWritePosition.load(std::memory_order_acquire) > 0; }

	// Scratch playback - 録音したバッファをスクラッチ再生
	void setPlaybackPosition(double normalizedPosition); // 0.0〜1.0
	double getPlaybackPosition() const;
	void setScratchSpeed(double speed); // 負の値で逆再生

	// Getters for Visualizers
	juce::AudioTransportSource& getTransportSource() { return transportSource; }
	juce::AudioThumbnail& getThumbnail() { return thumbnail; }
	double getCurrentPosition() { return transportSource.getCurrentPosition(); }
	double getLengthInSeconds() { return transportSource.getLengthInSeconds(); }
	bool isPlaying() const { return playing.load(std::memory_order_acquire); }
	
	// 録音バッファへのアクセス（波形表示用）— スナップショットコピーを返す
	juce::AudioBuffer<float> getRecordedBufferCopy() const;
	double getRecordedSampleRate() const { return currentSampleRate.load(std::memory_order_relaxed); }
	int getRecordedSamplesCount() const { return recordWritePosition.load(std::memory_order_acquire); }

	// ライブラリフォルダへの保存
	juce::File getLibraryFolder() const;
	juce::File saveRecordingToFile();

	// ファイルから録音バッファにロード（スクラッチ再生用）
	void loadFileToBuffer(const juce::File& file);

	// --- Sample Slots (A/B/C/D) ---
	static constexpr int NUM_SLOTS = 4;
	void loadFileToSlot(int slotIndex, const juce::File& file);
	void setActiveSlot(int slotIndex);
	int getActiveSlot() const { return activeSlotIndex; }
	juce::String getSlotFileName(int slotIndex) const;
	bool isSlotLoaded(int slotIndex) const;

	// ChangeListener
	void changeListenerCallback(juce::ChangeBroadcaster* source) override;

	private:
	// --- Buffer management (lock-free for audio thread) ---

	// Shared buffer — audio thread reads, main thread swaps atomically.
	struct SharedBuffer {
		juce::AudioBuffer<float> buffer;
		int writePosition = 0;  // valid recorded samples count
	};
	std::unique_ptr<SharedBuffer> activeBuffer;  // current buffer (audio reads)
	std::unique_ptr<SharedBuffer> stagingBuffer;  // next buffer (main writes)
	std::atomic<SharedBuffer*> audioBufferPtr{ nullptr };

	// Swap staging -> active under CS; audio thread picks up via atomic load.
	void commitStagingBuffer();

	// Allocate both buffers to hold maxSamples (2-channel).
	void allocateBuffers(int maxSamples, double sampleRate);

	juce::CriticalSection bufferSwapLock;  // protects staging writes + pointer swap

	// --- Format / transport ---
	juce::AudioFormatManager formatManager;
	std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
	juce::AudioTransportSource transportSource;
	std::unique_ptr<juce::ResamplingAudioSource> resamplerSource;

	// Thumbnail Cache
	juce::AudioThumbnailCache thumbnailCache{ 5 };
	juce::AudioThumbnail thumbnail;

	// Crossfader
	juce::LinearSmoothedValue<float> crossfaderGain { 1.0f };

	// --- Atomic state flags (lock-free) ---
	std::atomic<bool> recordingState{ false };
	std::atomic<bool> playing{ false };
	std::atomic<int>  recordWritePosition{ 0 };
	std::atomic<double> currentSampleRate{ 44100.0 };
	
	// Playback position — single-writer (audio thread), multi-reader (main/UI).
	std::atomic<double> playbackPosition{ 0.0 };

	// Scratch speed (written by main, read by audio — single value, atomic)
	std::atomic<double> targetScratchSpeed{ 1.0 };

	// Sample Slots
	struct SampleSlot {
		juce::AudioBuffer<float> buffer;
		juce::String fileName;
		int numSamples = 0;
	};
	std::array<SampleSlot, NUM_SLOTS> sampleSlots;
	int activeSlotIndex = 0;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioEngine)
};
