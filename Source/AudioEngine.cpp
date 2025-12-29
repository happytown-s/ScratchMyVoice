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
    // 再生中は録音バッファからスクラッチ再生
    if (playing && recordedBuffer.getNumSamples() > 0 && recordWritePosition > 0)
    {
        auto* outputBuffer = bufferToFill.buffer;
        int numSamplesToFill = bufferToFill.numSamples;
        int numChannels = juce::jmin(outputBuffer->getNumChannels(), recordedBuffer.getNumChannels());
        
        for (int sample = 0; sample < numSamplesToFill; ++sample)
        {
            // 線形補間のためのインデックスと係数を計算
            int pos0 = static_cast<int>(playbackPosition);
            int pos1 = pos0 + 1;
            float frac = static_cast<float>(playbackPosition - pos0);
            
            // バッファ範囲内にクランプ
            pos0 = juce::jlimit(0, recordWritePosition - 1, pos0);
            pos1 = juce::jlimit(0, recordWritePosition - 1, pos1);
            
            float gain = crossfaderGain.getNextValue();
            
            for (int ch = 0; ch < numChannels; ++ch)
            {
                // 線形補間でサンプル値を計算（ノイズ軽減）
                float sample0 = recordedBuffer.getSample(ch, pos0);
                float sample1 = recordedBuffer.getSample(ch, pos1);
                float interpolatedSample = sample0 + frac * (sample1 - sample0);
                
                outputBuffer->addSample(ch, bufferToFill.startSample + sample, interpolatedSample * gain);
            }
            
            // 再生位置を進める（スムーズなスピード変化）
            playbackPosition += targetScratchSpeed;
            
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
        targetScratchSpeed = 1.0;
    }
}

void AudioEngine::stop()
{
    playing = false;
}

void AudioEngine::setScratchRate(double rate)
{
    targetScratchSpeed = rate;
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
    targetScratchSpeed = speed;
}

void AudioEngine::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    // 必要に応じて実装
    juce::ignoreUnused(source);
}

juce::File AudioEngine::getLibraryFolder() const
{
    // アプリケーションデータフォルダ内にライブラリフォルダを作成
    auto folder = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("ScratchMyVoice")
        .getChildFile("Library");
    
    if (!folder.exists())
        folder.createDirectory();
    
    return folder;
}

juce::File AudioEngine::saveRecordingToFile()
{
    if (recordWritePosition <= 0)
        return juce::File();
    
    auto libraryFolder = getLibraryFolder();
    
    // タイムスタンプでファイル名を生成
    auto now = juce::Time::getCurrentTime();
    auto fileName = now.formatted("Recording_%Y%m%d_%H%M%S.wav");
    auto outputFile = libraryFolder.getChildFile(fileName);
    
    // WAVファイルとして保存
    juce::WavAudioFormat wavFormat;
    std::unique_ptr<juce::AudioFormatWriter> writer(
        wavFormat.createWriterFor(
            new juce::FileOutputStream(outputFile),
            currentSampleRate,
            static_cast<unsigned int>(recordedBuffer.getNumChannels()),
            16, // bits per sample
            {},
            0
        )
    );
    
    if (writer != nullptr)
    {
        writer->writeFromAudioSampleBuffer(recordedBuffer, 0, recordWritePosition);
        return outputFile;
    }
    
    return juce::File();
}

void AudioEngine::loadFileToBuffer(const juce::File& file)
{
    auto* reader = formatManager.createReaderFor(file);
    if (reader != nullptr)
    {
        // 録音バッファにファイルの内容をロード
        int numSamples = static_cast<int>(reader->lengthInSamples);
        int numChannels = static_cast<int>(reader->numChannels);
        
        recordedBuffer.setSize(juce::jmax(2, numChannels), numSamples);
        reader->read(&recordedBuffer, 0, numSamples, 0, true, true);
        
        recordWritePosition = numSamples;
        currentSampleRate = reader->sampleRate;
        playbackPosition = 0.0;
        
        delete reader;
        
        sendChangeMessage();
    }
}

// --- Sample Slots Implementation ---

void AudioEngine::loadFileToSlot(int slotIndex, const juce::File& file)
{
    if (slotIndex < 0 || slotIndex >= NUM_SLOTS) return;
    
    auto* reader = formatManager.createReaderFor(file);
    if (reader != nullptr)
    {
        int numSamples = static_cast<int>(reader->lengthInSamples);
        int numChannels = static_cast<int>(reader->numChannels);
        
        auto& slot = sampleSlots[static_cast<size_t>(slotIndex)];
        slot.buffer.setSize(juce::jmax(2, numChannels), numSamples);
        reader->read(&slot.buffer, 0, numSamples, 0, true, true);
        slot.numSamples = numSamples;
        slot.fileName = file.getFileNameWithoutExtension();
        
        delete reader;
        
        // アクティブスロットならメインバッファにもコピー
        if (slotIndex == activeSlotIndex)
        {
            setActiveSlot(slotIndex);
        }
        
        sendChangeMessage();
    }
}

void AudioEngine::setActiveSlot(int slotIndex)
{
    if (slotIndex < 0 || slotIndex >= NUM_SLOTS) return;
    
    activeSlotIndex = slotIndex;
    
    const auto& slot = sampleSlots[static_cast<size_t>(slotIndex)];
    if (slot.numSamples > 0)
    {
        // スロットのバッファをメイン再生バッファにコピー
        recordedBuffer.makeCopyOf(slot.buffer);
        recordWritePosition = slot.numSamples;
        playbackPosition = 0.0;
        
        sendChangeMessage();
    }
}

juce::String AudioEngine::getSlotFileName(int slotIndex) const
{
    if (slotIndex < 0 || slotIndex >= NUM_SLOTS) return "";
    return sampleSlots[static_cast<size_t>(slotIndex)].fileName;
}

bool AudioEngine::isSlotLoaded(int slotIndex) const
{
    if (slotIndex < 0 || slotIndex >= NUM_SLOTS) return false;
    return sampleSlots[static_cast<size_t>(slotIndex)].numSamples > 0;
}

