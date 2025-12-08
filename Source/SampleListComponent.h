/*
 ==============================================================================
 SampleListComponent.h
 ==============================================================================
 */
#pragma once
#include <JuceHeader.h>
#include "AudioEngine.h"

// Simple List Model
class SampleListComponent : public juce::Component,
public juce::ListBoxModel,
public juce::Button::Listener
{
	public:
	SampleListComponent(AudioEngine& engine);
	~SampleListComponent() override;

	void paint(juce::Graphics& g) override;
	void resized() override;

	// ListBoxModel
	int getNumRows() override;
	void paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override;
	void selectedRowsChanged(int lastRowSelected) override;

	// Button Listener
	void buttonClicked(juce::Button* button) override;

	// File Management
	void updateFileList();

	private:
	AudioEngine& audioEngine;
	juce::ListBox listBox;
	juce::TextButton recordButton { "Record" };

	std::vector<juce::File> sampleFiles;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SampleListComponent)
};
