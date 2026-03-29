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

	// Mouse interaction (desktop)
	void mouseDown(const juce::MouseEvent& e) override;
	void mouseDrag(const juce::MouseEvent& e) override;
	void mouseUp(const juce::MouseEvent& e) override;

	// Touch interaction (mobile) — Issue #14
	void touchStarted(const juce::TouchEvent& e) override;
	void touchMoved(const juce::TouchEvent& e) override;
	void touchEnded(const juce::TouchEvent& e) override;

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

	// Issue #14: Multi-touch tracking
	struct TouchState
	{
		int touchIndex = -1;
		float lastAngle = 0.0f;
		double lastTime = 0.0;
		float initialAngle = 0.0f;
		float initialRotation = 0.0f;
		juce::Point<float> startPos;
		bool isSwipeGesture = false;
	};
	TouchState primaryTouch;
	TouchState secondaryTouch;
	float multiTouchInitialPinchAngle = 0.0f;
	float multiTouchInitialRotation = 0.0f;
	bool isMultiTouchDragging = false;

	// Issue #14: Swipe detection
	static constexpr float swipeThreshold = 30.0f;   // pixels
	static constexpr double swipeTimeout = 200.0;     // ms

	// Helper
	float getAngleFromPoint(juce::Point<float> p);

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TurntableComponent)
};
