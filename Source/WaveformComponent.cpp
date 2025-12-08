/*
 ==============================================================================
 WaveformComponent.cpp
 ==============================================================================
 */
#include "WaveformComponent.h"

WaveformComponent::WaveformComponent(AudioEngine& engine)
: audioEngine(engine)
{
	// タイマーを開始（再生位置の更新用）
	startTimer(40); // 25fpsくらい
}

WaveformComponent::~WaveformComponent()
{
	stopTimer();
}

void WaveformComponent::paint(juce::Graphics& g)
{
	g.fillAll(juce::Colours::black); // 背景
	g.setColour(juce::Colours::green);
	g.drawText("Waveform Display", getLocalBounds(), juce::Justification::centred, true);

	// ここに将来波形描画処理を書く
}

void WaveformComponent::resized()
{
}

void WaveformComponent::timerCallback()
{
	repaint(); // 定期的に再描画
}

void WaveformComponent::setExpanded(bool shouldExpand)
{
	isExpanded = shouldExpand;
	resized();
}
