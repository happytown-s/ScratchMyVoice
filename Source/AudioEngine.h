/*
 ==============================================================================
 AudioEngine.h
 ==============================================================================
 */
#pragma once
#include <JuceHeader.h>
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
	bool isRecording() const { return recordingState; }
	bool hasRecordedAudio() const { return recordedBuffer.getNumSamples() > 0; }

	// Scratch playback - 録音したバッファをスクラッチ再生
	void setPlaybackPosition(double normalizedPosition); // 0.0〜1.0
	double getPlaybackPosition() const;
	void setScratchSpeed(double speed); // 負の値で逆再生

	// Getters for Visualizers
	juce::AudioTransportSource& getTransportSource() { return transportSource; }
	juce::AudioThumbnail& getThumbnail() { return thumbnail; }
	double getCurrentPosition() { return transportSource.getCurrentPosition(); }
	double getLengthInSeconds() { return transportSource.getLengthInSeconds(); }
	bool isPlaying() const { return playing; }
	
	// 録音バッファへのアクセス（波形表示用）
	const juce::AudioBuffer<float>& getRecordedBuffer() const { return recordedBuffer; }
	double getRecordedSampleRate() const { return currentSampleRate; }
	int getRecordedSamplesCount() const { return recordWritePosition; } // 実際に録音されたサンプル数
	
	// ライブラリフォルダへの保存
	juce::File getLibraryFolder() const;
	juce::File saveRecordingToFile(); // 録音データをWAVとして保存し、ファイルを返す
	
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
	juce::AudioFormatManager formatManager;
	std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
	juce::AudioTransportSource transportSource;
	std::unique_ptr<juce::ResamplingAudioSource> resamplerSource;

	// Thumbnail Cache
	juce::AudioThumbnailCache thumbnailCache{ 5 };
	juce::AudioThumbnail thumbnail;

	// Crossfader
	juce::LinearSmoothedValue<float> crossfaderGain { 1.0f };

	// Recording buffer
	bool recordingState = false;
	juce::AudioBuffer<float> recordedBuffer;
	int recordWritePosition = 0;
	double currentSampleRate = 44100.0;
	
	// Playback state
	bool playing = false;
	double playbackPosition = 0.0; // サンプル位置
	double targetScratchSpeed = 1.0;     // 目標再生速度
	double currentScratchSpeed = 1.0;    // 現在の再生速度（スムーズ変化用）

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
