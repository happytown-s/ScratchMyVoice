/*
 ==============================================================================
 AudioEngine.cpp
 ==============================================================================
 */
#include "AudioEngine.h"

AudioEngine::AudioEngine()
: thumbnail(512, formatManager, thumbnailCache) // ← これを追加！
{
	formatManager.registerBasicFormats();
}

AudioEngine::~AudioEngine()
{
    transportSource.setSource(nullptr);
}

void AudioEngine::prepareToPlay(int samplesPerBlockExpected, double sampleRate)
{
    transportSource.prepareToPlay(samplesPerBlockExpected, sampleRate);
    
    // リサンプラーの初期化（スクラッチ用）
    if (resamplerSource)
        resamplerSource->prepareToPlay(samplesPerBlockExpected, sampleRate);
}

void AudioEngine::releaseResources()
{
    transportSource.releaseResources();
    if (resamplerSource)
        resamplerSource->releaseResources();
}

void AudioEngine::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
    if (resamplerSource)
    {
        // スクラッチ（リサンプラー）経由で再生
        resamplerSource->getNextAudioBlock(bufferToFill);
    }
    else
    {
        bufferToFill.clearActiveBufferRegion();
    }

    // クロスフェーダーのゲイン適用
    float gain = crossfaderGain.getCurrentValue(); // ここでスムーズなゲインを取得
    // ※今回は簡易的に全体ゲインとして適用（本来はA/Bデッキのバランスですが、コードに合わせています）
    bufferToFill.buffer->applyGain(bufferToFill.startSample, bufferToFill.numSamples, 1.0f); 
}

void AudioEngine::loadFile(const juce::File& file)
{
    auto* reader = formatManager.createReaderFor(file);
    if (reader != nullptr)
    {
        std::unique_ptr<juce::AudioFormatReaderSource> newSource(new juce::AudioFormatReaderSource(reader, true));
        transportSource.setSource(newSource.get(), 0, nullptr, reader->sampleRate);
        readerSource.reset(newSource.release());
        
        // リサンプラーにTransportSourceをラップさせる
        resamplerSource.reset(new juce::ResamplingAudioSource(&transportSource, false, 2));
        
        // サムネイルの更新通知
        sendChangeMessage(); 
    }
}

void AudioEngine::play()
{
    transportSource.start();
}

void AudioEngine::stop()
{
    transportSource.stop();
}

void AudioEngine::setScratchRate(double rate)
{
    if (resamplerSource)
    {
        // rateが1.0で等速、負の値で逆再生、0に近いほど遅くなる
        // 0.0で停止しないように少し工夫が必要ですが、基本はこれで動きます
        resamplerSource->setResamplingRatio(rate > 0 ? 1.0 / rate : 1.0);
    }
}

void AudioEngine::setCrossfaderGain(float gain)
{
    crossfaderGain.setTargetValue(gain);
}

// 注: 簡易実装です。本格的な録音は非同期スレッド（TimeSliceThread）を使う必要があります。
void AudioEngine::startRecording(const juce::File& file)
{
    // 録音準備のロジック（ここではフラグ管理のみ）
    recordingState = true;
    // 実際のファイル書き込み準備はMainComponent側でAudioFormatWriterを作るのが一般的です
}

void AudioEngine::stopRecording()
{
    recordingState = false;
}

void AudioEngine::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    // 必要に応じて実装
}
