/*
 ==============================================================================
 WaveformComponent.cpp
 ==============================================================================
 */
#include "WaveformComponent.h"

WaveformComponent::WaveformComponent(AudioEngine& engine)
: audioEngine(engine)
{
	// AudioEngineの変更を監視
	audioEngine.addChangeListener(this);
	startTimer(40); // 25fps
}

WaveformComponent::~WaveformComponent()
{
	audioEngine.removeChangeListener(this);
	stopTimer();
}

void WaveformComponent::paint(juce::Graphics& g)
{
	auto bounds = getLocalBounds().toFloat();
	
	// 背景
	g.setColour(juce::Colour::fromString("FF1E293B")); // Slate 800
	g.fillRoundedRectangle(bounds, 5.0f);
	
	if (!audioEngine.hasRecordedAudio())
	{
		// 録音がない場合のメッセージ
		g.setColour(juce::Colours::grey);
		g.drawText("No recording - Press REC to start", bounds, juce::Justification::centred);
		return;
	}
	
	// 波形の描画
	g.setColour(juce::Colour::fromString("FF22C55E")); // 緑
	g.strokePath(waveformPath, juce::PathStrokeType(1.5f));
	
	// 再生位置インジケーター
	if (audioEngine.hasRecordedAudio())
	{
		double pos = audioEngine.getPlaybackPosition();
		float x = bounds.getX() + static_cast<float>(pos) * bounds.getWidth();
		
		g.setColour(juce::Colours::white);
		g.drawLine(x, bounds.getY(), x, bounds.getBottom(), 2.0f);
	}
}

void WaveformComponent::resized()
{
	updateWaveformPath();
}

void WaveformComponent::timerCallback()
{
	// 録音中または再生中は再描画
	if (audioEngine.isRecording() || audioEngine.isPlaying())
	{
		// 録音中は波形を更新
		if (audioEngine.isRecording())
		{
			int currentSamples = audioEngine.getRecordedSamplesCount();
			if (currentSamples != cachedBufferSize)
			{
				updateWaveformPath();
			}
		}
		repaint();
	}
}

void WaveformComponent::changeListenerCallback(juce::ChangeBroadcaster* source)
{
	// 録音状態が変わったら波形を更新
	updateWaveformPath();
	repaint();
}

void WaveformComponent::setExpanded(bool shouldExpand)
{
	isExpanded = shouldExpand;
	resized();
}

void WaveformComponent::updateWaveformPath()
{
	waveformPath.clear();
	
	const auto& buffer = audioEngine.getRecordedBuffer();
	int numSamples = audioEngine.getRecordedSamplesCount(); // 実際に録音されたサンプル数を使用
	
	if (numSamples <= 0) return;
	
	auto bounds = getLocalBounds().toFloat().reduced(2);
	float width = bounds.getWidth();
	float height = bounds.getHeight();
	float centerY = bounds.getCentreY();
	
	// サンプリングレートを調整（効率のため）
	int samplesPerPixel = juce::jmax(1, numSamples / static_cast<int>(width));
	
	const float* channelData = buffer.getReadPointer(0);
	
	waveformPath.startNewSubPath(bounds.getX(), centerY);
	
	for (int x = 0; x < static_cast<int>(width); ++x)
	{
		int startSample = x * samplesPerPixel;
		int endSample = juce::jmin(startSample + samplesPerPixel, numSamples);
		
		float maxValue = 0.0f;
		for (int i = startSample; i < endSample; ++i)
		{
			maxValue = juce::jmax(maxValue, std::abs(channelData[i]));
		}
		
		float y = centerY - maxValue * (height / 2.0f) * 0.9f;
		waveformPath.lineTo(bounds.getX() + static_cast<float>(x), y);
	}
	
	// 下半分（ミラー）
	for (int x = static_cast<int>(width) - 1; x >= 0; --x)
	{
		int startSample = x * samplesPerPixel;
		int endSample = juce::jmin(startSample + samplesPerPixel, numSamples);
		
		float maxValue = 0.0f;
		for (int i = startSample; i < endSample; ++i)
		{
			maxValue = juce::jmax(maxValue, std::abs(channelData[i]));
		}
		
		float y = centerY + maxValue * (height / 2.0f) * 0.9f;
		waveformPath.lineTo(bounds.getX() + static_cast<float>(x), y);
	}
	
	waveformPath.closeSubPath();
	
	cachedBufferSize = numSamples;
}

