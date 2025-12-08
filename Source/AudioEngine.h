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

	// Recording
	void startRecording(const juce::File& file);
	void stopRecording();
	bool isRecording() const { return recordingState; }

	// Getters for Visualizers
	juce::AudioTransportSource& getTransportSource() { return transportSource; }
	juce::AudioThumbnail& getThumbnail() { return thumbnail; }
	double getCurrentPosition() { return transportSource.getCurrentPosition(); }
	double getLengthInSeconds() { return transportSource.getLengthInSeconds(); }
	bool isPlaying() const { return transportSource.isPlaying(); }

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
	juce::LinearSmoothedValue<float> crossfaderGain { 0.0f };

	// Recording
	bool recordingState = false;
	// Note: Actual recording logic in JUCE requires an AudioIODeviceCallback or AudioRecorder class.
	// For brevity, we assume AudioEngine manages the AudioDeviceManager instance internally.

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioEngine)
};
