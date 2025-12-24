/*
 ==============================================================================
 AudioEngine.cpp
 ==============================================================================
 */
#include "AudioEngine.h"

AudioEngine::AudioEngine()
: thumbnail(512, formatManager, thumbnailCache)
{
	formatManager.registerBasicFormats();
	
	// 録音バッファを初期化（最大30秒分）
	recordedBuffer.setSize(2, 44100 * 30);
	recordedBuffer.clear();
}

AudioEngine::~AudioEngine()
{
    transportSource.setSource(nullptr);
}

void AudioEngine::prepareToPlay(int samplesPerBlockExpected, double sampleRate)
{
    currentSampleRate = sampleRate;
    transportSource.prepareToPlay(samplesPerBlockExpected, sampleRate);
    
    // 録音バッファのサイズを現在のサンプルレートに合わせる（30秒分）
    int maxSamples = static_cast<int>(sampleRate * 30.0);
    recordedBuffer.setSize(2, maxSamples, true, true, true);
    
    crossfaderGain.reset(sampleRate, 0.01); // 10msスムージング
    
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
    // 録音中は入力をバッファに記録（MainComponentで呼び出される）
    
    // 再生中は録音バッファからスクラッチ再生
    if (playing && recordedBuffer.getNumSamples() > 0 && recordWritePosition > 0)
    {
        auto* outputBuffer = bufferToFill.buffer;
        int numSamplesToFill = bufferToFill.numSamples;
        int numChannels = juce::jmin(outputBuffer->getNumChannels(), recordedBuffer.getNumChannels());
        
        for (int sample = 0; sample < numSamplesToFill; ++sample)
        {
            // 現在の再生位置からサンプルを取得
            int readPos = static_cast<int>(playbackPosition);
            
            // バッファ範囲内に収める
            if (readPos >= 0 && readPos < recordWritePosition)
            {
                for (int ch = 0; ch < numChannels; ++ch)
                {
                    float sampleValue = recordedBuffer.getSample(ch, readPos);
                    outputBuffer->addSample(ch, bufferToFill.startSample + sample, sampleValue * crossfaderGain.getNextValue());
                }
            }
            
            // 再生位置を進める
            playbackPosition += scratchSpeed;
            
            // ループ再生（録音範囲内でループ）
            if (playbackPosition >= recordWritePosition)
                playbackPosition = 0.0;
            else if (playbackPosition < 0.0)
                playbackPosition = recordWritePosition - 1;
        }
    }
    else
    {
        // 再生していない場合、クロスフェーダーのスムーズ値を更新
        for (int i = 0; i < bufferToFill.numSamples; ++i)
            crossfaderGain.getNextValue();
    }
}

void AudioEngine::recordAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
    if (!recordingState) return;
    
    auto* inputBuffer = bufferToFill.buffer;
    int numSamples = bufferToFill.numSamples;
    int numChannels = juce::jmin(inputBuffer->getNumChannels(), recordedBuffer.getNumChannels());
    
    // バッファに余裕があれば書き込み
    if (recordWritePosition + numSamples <= recordedBuffer.getNumSamples())
    {
        for (int ch = 0; ch < numChannels; ++ch)
        {
            recordedBuffer.copyFrom(ch, recordWritePosition,
                                    *inputBuffer, ch, bufferToFill.startSample, numSamples);
        }
        recordWritePosition += numSamples;
    }
    else
    {
        // バッファがいっぱいになったら録音停止
        stopRecording();
    }
}

void AudioEngine::loadFile(const juce::File& file)
{
    auto* reader = formatManager.createReaderFor(file);
    if (reader != nullptr)
    {
        std::unique_ptr<juce::AudioFormatReaderSource> newSource(new juce::AudioFormatReaderSource(reader, true));
        transportSource.setSource(newSource.get(), 0, nullptr, reader->sampleRate);
        readerSource.reset(newSource.release());
        
        resamplerSource.reset(new juce::ResamplingAudioSource(&transportSource, false, 2));
        
        sendChangeMessage(); 
    }
}

void AudioEngine::play()
{
    if (hasRecordedAudio())
    {
        playing = true;
        scratchSpeed = 1.0;
    }
}

void AudioEngine::stop()
{
    playing = false;
}

void AudioEngine::setScratchRate(double rate)
{
    scratchSpeed = rate;
}

void AudioEngine::setCrossfaderGain(float gain)
{
    crossfaderGain.setTargetValue(gain);
}

void AudioEngine::startRecording()
{
    recordWritePosition = 0;
    recordedBuffer.clear();
    recordingState = true;
    playing = false; // 録音中は再生停止
    sendChangeMessage();
}

void AudioEngine::stopRecording()
{
    recordingState = false;
    playbackPosition = 0.0;
    sendChangeMessage();
}

void AudioEngine::setPlaybackPosition(double normalizedPosition)
{
    if (recordWritePosition > 0)
    {
        playbackPosition = normalizedPosition * recordWritePosition;
        playbackPosition = juce::jlimit(0.0, static_cast<double>(recordWritePosition - 1), playbackPosition);
    }
}

double AudioEngine::getPlaybackPosition() const
{
    if (recordWritePosition > 0)
        return playbackPosition / recordWritePosition;
    return 0.0;
}

void AudioEngine::setScratchSpeed(double speed)
{
    scratchSpeed = speed;
}

void AudioEngine::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    // 必要に応じて実装
}
