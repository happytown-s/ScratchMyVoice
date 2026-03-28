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
	
	// 波形の描画（peakCacheから再構築されたキャッシュ済みPathを使用）
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
	invalidateCache();
}

void WaveformComponent::invalidateCache()
{
	peakCache.clear();
	peakCacheNumSamples = 0;
	pathIsFinal = false;
	cachedBufferSize = 0;
	waveformPath.clear();
	updateWaveformPath();
}

void WaveformComponent::timerCallback()
{
	// 録音中または再生中は再描画
	if (audioEngine.isRecording() || audioEngine.isPlaying())
	{
		// 録音中は波形を差分更新
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
	if (audioEngine.isRecording())
	{
		// 録音開始 — キャッシュをリセット
		peakCache.clear();
		peakCacheNumSamples = 0;
		pathIsFinal = false;
		cachedBufferSize = 0;
	}
	else if (audioEngine.hasRecordedAudio())
	{
		// 録音完了またはファイルロード — 最終波形をキャッシュ
		pathIsFinal = false;
		updateWaveformPath();
		pathIsFinal = true;
	}
	repaint();
}

void WaveformComponent::setExpanded(bool shouldExpand)
{
	isExpanded = shouldExpand;
	resized();
}

void WaveformComponent::updateWaveformPath()
{
	int numSamples = audioEngine.getRecordedSamplesCount();
	if (numSamples <= 0) return;

	auto bounds = getLocalBounds().toFloat().reduced(2);
	float width = bounds.getWidth();
	if (width <= 0) return;

	// 録音停止後でサイズ・バッファが変わっていなければskip
	if (pathIsFinal && cachedBufferSize == numSamples
		&& static_cast<int>(width) == static_cast<int>(peakCache.size()))
	{
		return;
	}

	// 差分更新: 新規サンプルのみ処理
	if (!peakCache.empty() && peakCacheNumSamples > 0 && numSamples > peakCacheNumSamples)
	{
		updatePeakCacheIncremental();
		rebuildPathFromPeaks();
		cachedBufferSize = numSamples;
		return;
	}

	// フルリビルド（初回 / resized / ファイルロード / samplesPerPixel変化時）
	const auto& buffer = audioEngine.getRecordedBuffer();
	const float* channelData = buffer.getReadPointer(0);

	int numPixels = static_cast<int>(width);
	int samplesPerPixel = juce::jmax(1, numSamples / numPixels);

	peakCache.resize(static_cast<size_t>(numPixels));
	for (int x = 0; x < numPixels; ++x)
	{
		int startSample = x * samplesPerPixel;
		int endSample = juce::jmin(startSample + samplesPerPixel, numSamples);

		float maxValue = 0.0f;
		for (int i = startSample; i < endSample; ++i)
		{
			maxValue = juce::jmax(maxValue, std::abs(channelData[i]));
		}
		peakCache[static_cast<size_t>(x)] = maxValue;
	}
	peakCacheNumSamples = numSamples;

	rebuildPathFromPeaks();
	cachedBufferSize = numSamples;
}

void WaveformComponent::updatePeakCacheIncremental()
{
	// peakCacheNumSamples以前のサンプルは既にキャッシュ済み。
	// 新規サンプル [peakCacheNumSamples, numSamples) のみ処理する。
	int numSamples = audioEngine.getRecordedSamplesCount();
	auto bounds = getLocalBounds().toFloat().reduced(2);
	float width = bounds.getWidth();
	if (width <= 0) return;

	int numPixels = static_cast<int>(width);
	int newSamplesPerPixel = juce::jmax(1, numSamples / numPixels);

	// samplesPerPixelが変わった場合（録音長が大幅に増加した等）はフルリビルドにフォールバック
	int oldSamplesPerPixel = juce::jmax(1, peakCacheNumSamples / numPixels);
	if (newSamplesPerPixel != oldSamplesPerPixel)
	{
		peakCache.clear();
		peakCacheNumSamples = 0;
		cachedBufferSize = 0;
		updateWaveformPath();
		return;
	}

	const auto& buffer = audioEngine.getRecordedBuffer();
	const float* channelData = buffer.getReadPointer(0);

	// peakCacheのサイズが足りなければ拡張
	if (static_cast<int>(peakCache.size()) < numPixels)
	{
		peakCache.resize(static_cast<size_t>(numPixels), 0.0f);
	}

	// 影響を受けるピクセル範囲を特定
	int startPixel = peakCacheNumSamples / newSamplesPerPixel;
	int endPixel = juce::jmin(numPixels, (numSamples / newSamplesPerPixel) + 1);

	for (int x = startPixel; x < endPixel && x < numPixels; ++x)
	{
		int pixStart = x * newSamplesPerPixel;
		int pixEnd = juce::jmin(pixStart + newSamplesPerPixel, numSamples);

		float maxValue;
		if (pixStart < peakCacheNumSamples)
		{
			// 旧データと新データが混在するピクセル — 既存キャッシュ値を起点に新規部分のみスキャン
			maxValue = peakCache[static_cast<size_t>(x)];
			for (int i = peakCacheNumSamples; i < pixEnd; ++i)
			{
				maxValue = juce::jmax(maxValue, std::abs(channelData[i]));
			}
		}
		else
		{
			// 完全に新規のピクセル
			maxValue = 0.0f;
			for (int i = pixStart; i < pixEnd; ++i)
			{
				maxValue = juce::jmax(maxValue, std::abs(channelData[i]));
			}
		}
		peakCache[static_cast<size_t>(x)] = maxValue;
	}

	peakCacheNumSamples = numSamples;
}

void WaveformComponent::rebuildPathFromPeaks()
{
	waveformPath.clear();

	if (peakCache.empty()) return;

	auto bounds = getLocalBounds().toFloat().reduced(2);
	float height = bounds.getHeight();
	float centerY = bounds.getCentreY();
	int numPixels = static_cast<int>(peakCache.size());

	waveformPath.startNewSubPath(bounds.getX(), centerY);

	// 上半分
	for (int x = 0; x < numPixels; ++x)
	{
		float maxVal = peakCache[static_cast<size_t>(x)];
		float y = centerY - maxVal * (height / 2.0f) * 0.9f;
		waveformPath.lineTo(bounds.getX() + static_cast<float>(x), y);
	}

	// 下半分（ミラー）
	for (int x = numPixels - 1; x >= 0; --x)
	{
		float maxVal = peakCache[static_cast<size_t>(x)];
		float y = centerY + maxVal * (height / 2.0f) * 0.9f;
		waveformPath.lineTo(bounds.getX() + static_cast<float>(x), y);
	}

	waveformPath.closeSubPath();
}
