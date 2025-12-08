/*
 ==============================================================================
 TurntableComponent.h
 ==============================================================================
 */
#pragma once
#include <JuceHeader.h>
#include "AudioEngine.h"

class TurntableComponent : public juce::Component, public juce::Timer
{
	public:
	TurntableComponent(AudioEngine& engine);
	~TurntableComponent() override;

	void paint(juce::Graphics& g) override;
	void resized() override;

	// Interaction
	void mouseDown(const juce::MouseEvent& e) override;
	void mouseDrag(const juce::MouseEvent& e) override;
	void mouseUp(const juce::MouseEvent& e) override;

	// Timer for Animation Loop (60FPS)
	void timerCallback() override;

	void setExpanded(bool shouldExpand);

	private:
	AudioEngine& audioEngine;

	juce::Image recordImage;

	float rotationAngle = 0.0f;
	float lastAngle = 0.0f;
	double lastTime = 0;
	bool isDragging = false;
	bool isExpanded = false;

	// Helper
	float getAngleFromPoint(juce::Point<float> p);

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TurntableComponent)
};
