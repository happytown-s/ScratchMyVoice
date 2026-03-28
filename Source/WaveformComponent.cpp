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
	// Window resize requires full path rebuild
	cachedBufferSize = 0;
	updateWaveformPath();
}

void WaveformComponent::timerCallback()
{
	// 録音中または再生中は再描画
	if (audioEngine.isRecording() || audioEngine.isPlaying())
	{
		// 録音中は差分更新のみ（UIスレッドブロック回避）
		if (audioEngine.isRecording())
		{
			updateWaveformPath();
		}
		repaint();
	}
}

void WaveformComponent::changeListenerCallback(juce::ChangeBroadcaster* source)
{
	juce::ignoreUnused(source);
	// 録音状態が変わったらフル再構築（バッファ内容がリセットされる可能性がある）
	cachedBufferSize = 0;
	updateWaveformPath();
	repaint();
}

void WaveformComponent::setExpanded(bool shouldExpand)
{
	isExpanded = shouldExpand;
	cachedBufferSize = 0;
	resized();
}

void WaveformComponent::updateWaveformPath()
{
	const auto& buffer = audioEngine.getRecordedBuffer();
	int numSamples = audioEngine.getRecordedSamplesCount();
	
	if (numSamples <= 0)
	{
		waveformPath.clear();
		cachedBufferSize = 0;
		cachedRecordedSamples = 0;
		return;
	}
	
	auto bounds = getLocalBounds().toFloat().reduced(2);
	if (bounds.getWidth() <= 0 || bounds.getHeight() <= 0) return;
	
	int samplesPerPixel = juce::jmax(1, numSamples / static_cast<int>(bounds.getWidth()));
	
	// バッファサイズが変わった（setSize等）または初回はフル再構築
	if (buffer.getNumSamples() != cachedBufferSize || cachedRecordedSamples == 0)
	{
		rebuildFullWaveformPath();
		return;
	}
	
	// 差分更新：新しく録音されたサンプルのみをスキャン
	int newSamples = numSamples;
	if (newSamples > cachedRecordedSamples)
	{
		appendWaveformPath(cachedRecordedSamples, newSamples);
	}
}

void WaveformComponent::rebuildFullWaveformPath()
{
	const auto& buffer = audioEngine.getRecordedBuffer();
	int numSamples = audioEngine.getRecordedSamplesCount();
	
	waveformPath.clear();
	
	if (numSamples <= 0)
	{
		cachedBufferSize = buffer.getNumSamples();
		cachedRecordedSamples = 0;
		return;
	}
	
	auto bounds = getLocalBounds().toFloat().reduced(2);
	float width = bounds.getWidth();
	float height = bounds.getHeight();
	float centerY = bounds.getCentreY();
	
	// サンプリングレートを調整（効率のため）
	int samplesPerPixel = juce::jmax(1, numSamples / static_cast<int>(width));
	
	const float* channelData = buffer.getReadPointer(0);
	
	// 上半分
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
	
	cachedBufferSize = buffer.getNumSamples();
	cachedRecordedSamples = numSamples;
}

void WaveformComponent::appendWaveformPath(int fromSample, int toSample)
{
	const auto& buffer = audioEngine.getRecordedBuffer();
	int numSamples = audioEngine.getRecordedSamplesCount();
	
	auto bounds = getLocalBounds().toFloat().reduced(2);
	float width = bounds.getWidth();
	float height = bounds.getHeight();
	float centerY = bounds.getCentreY();
	
	int samplesPerPixel = juce::jmax(1, numSamples / static_cast<int>(width));
	const float* channelData = buffer.getReadPointer(0);
	
	// 差分のみスキャン — 新しいピクセル範囲を計算
	int fromPixel = fromSample / samplesPerPixel;
	int toPixel = juce::jmin(static_cast<int>(width), (toSample + samplesPerPixel - 1) / samplesPerPixel);
	
	// 既存パスの最後のポイントを取得し、そこから続きを描画
	// 上半分の新しいピクセルのみ更新
	for (int x = fromPixel; x < toPixel; ++x)
	{
		int startSample = x * samplesPerPixel;
		int endSample = juce::jmin(startSample + samplesPerPixel, toSample);
		
		float maxValue = 0.0f;
		for (int i = startSample; i < endSample; ++i)
		{
			maxValue = juce::jmax(maxValue, std::abs(channelData[i]));
		}
		
		float yTop = centerY - maxValue * (height / 2.0f) * 0.9f;
		float yBot = centerY + maxValue * (height / 2.0f) * 0.9f;
		
		waveformPath.lineTo(bounds.getX() + static_cast<float>(x), yTop);
	}
	
	// 下半分（ミラー）の差分 — reverse order
	for (int x = toPixel - 1; x >= fromPixel; --x)
	{
		int startSample = x * samplesPerPixel;
		int endSample = juce::jmin(startSample + samplesPerPixel, toSample);
		
		float maxValue = 0.0f;
		for (int i = startSample; i < endSample; ++i)
		{
			maxValue = juce::jmax(maxValue, std::abs(channelData[i]));
		}
		
		float yBot = centerY + maxValue * (height / 2.0f) * 0.9f;
		waveformPath.lineTo(bounds.getX() + static_cast<float>(x), yBot);
	}
	
	cachedRecordedSamples = toSample;
}
