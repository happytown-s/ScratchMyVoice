/*
 ==============================================================================
 MainComponent.h
 ==============================================================================
 */
#pragma once
#include <JuceHeader.h>
#include "AudioEngine.h"
#include "TurntableComponent.h"
#include "WaveformComponent.h"
#include "CrossfaderComponent.h"
#include "SampleListComponent.h"

class MainComponent : public juce::AudioAppComponent, public juce::Button::Listener, public juce::Timer
{
	public:
	MainComponent();
	~MainComponent() override;

	void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;
	void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override;
	void releaseResources() override;

	void paint(juce::Graphics& g) override;
	void resized() override;

	void buttonClicked(juce::Button* button) override;
	void timerCallback() override;

	private:
	// Core Engine
	AudioEngine audioEngine;

	// Child Components
	std::unique_ptr<TurntableComponent> turntable;
	std::unique_ptr<WaveformComponent> waveform;
	std::unique_ptr<CrossfaderComponent> crossfader;
	std::unique_ptr<SampleListComponent> sampleList;

	// Layout Controls
	juce::TextButton libraryToggleButton { "Library" };
	juce::TextButton playStopButton { "PLAY" };
	juce::TextButton audioSettingsButton { "Audio" };
	juce::TextButton recordButton { "REC" };

	bool isLibraryOpen = false;

	// Audio Settings
	std::unique_ptr<juce::AudioDeviceSelectorComponent> audioSelector;
	bool isAudioSettingsOpen = false;
	juce::File getAudioSettingsFile() const;

	// Helper
	void updateButtonColors();

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
