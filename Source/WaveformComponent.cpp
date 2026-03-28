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
	
	// 録音完了後はjuce::AudioThumbnailから描画（UIスレッドブロックなし）
	if (!audioEngine.isRecording() && cachedForFinishedRecording)
	{
		drawWaveformFromThumbnail(g);
	}
	else
	{
		// 録音中はキャッシュしたPathから描画
		g.setColour(juce::Colour::fromString("FF22C55E")); // 緑
		g.strokePath(waveformPath, juce::PathStrokeType(1.5f));
	}
	
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
		// 録音中は差分更新のみ
		if (audioEngine.isRecording())
		{
			int currentSamples = audioEngine.getRecordedSamplesCount();
			if (currentSamples != cachedBufferSize)
			{
				updateWaveformPathIncremental();
			}
		}
		repaint();
	}
}

void WaveformComponent::changeListenerCallback(juce::ChangeBroadcaster* source)
{
	if (!audioEngine.isRecording() && audioEngine.hasRecordedAudio())
	{
		// 録音完了時 — AudioThumbnailを使ってPathをキャッシュ（一度だけ）
		cachedForFinishedRecording = true;
		cachedBufferSize = audioEngine.getRecordedSamplesCount();
		// 以降のpaintはdrawWaveformFromThumbnail()を使用
	}
	else
	{
		// その他の状態変化
		cachedForFinishedRecording = false;
		updateWaveformPath();
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
	// フルリビルド：録音開始前やファイルロード時に使用
	waveformPath.clear();
	cachedBufferSize = 0;
	updateWaveformPathIncremental();
}

void WaveformComponent::updateWaveformPathIncremental()
{
	const auto& buffer = audioEngine.getRecordedBuffer();
	int numSamples = audioEngine.getRecordedSamplesCount();
	
	if (numSamples <= 0) return;
	
	auto bounds = getLocalBounds().toFloat().reduced(2);
	float width = bounds.getWidth();
	float height = bounds.getHeight();
	float centerY = bounds.getCentreY();
	
	int samplesPerPixel = juce::jmax(1, numSamples / static_cast<int>(width));
	const float* channelData = buffer.getReadPointer(0);
	
	if (cachedBufferSize == 0)
	{
		// 初回：Pathをゼロから構築
		waveformPath.clear();
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
	}
	else
	{
		// 差分更新：新しく追加されたサンプルのみスキャン
		int newPixelsStart = cachedBufferSize / samplesPerPixel;
		int totalPixels = numSamples / samplesPerPixel;
		
		if (newPixelsStart < totalPixels)
		{
			// 既存の最後のピクセル位置から再開するため、
			// 新規ピクセル範囲だけをスキャン
			for (int x = newPixelsStart; x < totalPixels; ++x)
			{
				int startSample = x * samplesPerPixel;
				int endSample = juce::jmin(startSample + samplesPerPixel, numSamples);
				
				float maxValue = 0.0f;
				for (int i = startSample; i < endSample; ++i)
				{
					maxValue = juce::jmax(maxValue, std::abs(channelData[i]));
				}
				
				float yTop = centerY - maxValue * (height / 2.0f) * 0.9f;
				float yBot = centerY + maxValue * (height / 2.0f) * 0.9f;
				float xPos = bounds.getX() + static_cast<float>(x);
				
				// 上半分：既存の線を引き継ぎ
				waveformPath.lineTo(xPos, yTop);
			}
			
			// 下半分の差分（逆方向）
			for (int x = totalPixels - 1; x >= newPixelsStart; --x)
			{
				int startSample = x * samplesPerPixel;
				int endSample = juce::jmin(startSample + samplesPerPixel, numSamples);
				
				float maxValue = 0.0f;
				for (int i = startSample; i < endSample; ++i)
				{
					maxValue = juce::jmax(maxValue, std::abs(channelData[i]));
				}
				
				float yBot = centerY + maxValue * (height / 2.0f) * 0.9f;
				waveformPath.lineTo(bounds.getX() + static_cast<float>(x), yBot);
			}
			
			waveformPath.closeSubPath();
		}
	}
	
	cachedBufferSize = numSamples;
}

void WaveformComponent::drawWaveformFromThumbnail(juce::Graphics& g)
{
	// juce::AudioThumbnailはバックグラウンドで非同期に波形データを保持するため
	// UIスレッドをブロックしない
	auto& thumbnail = audioEngine.getThumbnail();
	
	if (thumbnail.getNumChannels() == 0)
		return;
	
	auto bounds = getLocalBounds().toFloat().reduced(2);
	float centerY = bounds.getCentreY();
	
	g.setColour(juce::Colour::fromString("FF22C55E")); // 緑
	
	// 上半分
	thumbnail.drawChannel(g, bounds.reduced(0, 0, 0, bounds.getHeight() / 2.0f),
		0.0, thumbnail.getTotalLength(), 0, 0.9f);
	
	// 下半分（ミラー）
	juce::Graphics::ScopedSaveState saveState(g);
	juce::AffineTransform mirror = juce::AffineTransform::verticalFlip(bounds.getHeight())
		.translated(0, bounds.getBottom());
	g.addTransform(mirror);
	thumbnail.drawChannel(g, bounds.reduced(0, 0, 0, bounds.getHeight() / 2.0f),
		0.0, thumbnail.getTotalLength(), 0, 0.9f);
}
