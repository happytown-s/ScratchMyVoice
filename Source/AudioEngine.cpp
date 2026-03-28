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
    currentSampleRate.store(sampleRate, std::memory_order_relaxed);
    transportSource.prepareToPlay(samplesPerBlockExpected, sampleRate);
    
    // 録音バッファのサイズを現在のサンプルレートに合わせる（30秒分）
    int maxSamples = static_cast<int>(sampleRate * 30.0);
    
    {
        juce::SpinLock::ScopedLockType lock(bufferLock);
        recordedBuffer.setSize(2, maxSamples, true, true, true);
    }
    
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
    if (playing.load(std::memory_order_relaxed) && recordedBuffer.getNumSamples() > 0)
    {
        juce::SpinLock::ScopedLockType lock(bufferLock);
        
        int localRecordWritePosition = recordWritePosition.load(std::memory_order_relaxed);
        if (localRecordWritePosition <= 0)
            return;

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
            pos0 = juce::jlimit(0, localRecordWritePosition - 1, pos0);
            pos1 = juce::jlimit(0, localRecordWritePosition - 1, pos1);
            
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
            if (playbackPosition >= localRecordWritePosition)
                playbackPosition = 0.0;
            else if (playbackPosition < 0.0)
                playbackPosition = localRecordWritePosition - 1;
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
    if (!recordingState.load(std::memory_order_acquire)) return;
    
    auto* inputBuffer = bufferToFill.buffer;
    int numSamples = bufferToFill.numSamples;
    
    {
        juce::SpinLock::ScopedLockType lock(bufferLock);
        
        int numChannels = juce::jmin(inputBuffer->getNumChannels(), recordedBuffer.getNumChannels());
        int localRecordWritePosition = recordWritePosition.load(std::memory_order_relaxed);
        
        // バッファに余裕があれば書き込み
        if (localRecordWritePosition + numSamples <= recordedBuffer.getNumSamples())
        {
            for (int ch = 0; ch < numChannels; ++ch)
            {
                recordedBuffer.copyFrom(ch, localRecordWritePosition,
                                        *inputBuffer, ch, bufferToFill.startSample, numSamples);
            }
            recordWritePosition.store(localRecordWritePosition + numSamples, std::memory_order_release);
        }
        else
        {
            // バッファがいっぱいになったら録音停止
            recordingState.store(false, std::memory_order_release);
            playbackPosition = 0.0;
        }
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
        juce::SpinLock::ScopedLockType lock(bufferLock);
        playing.store(true, std::memory_order_release);
        targetScratchSpeed = 1.0;
        playbackPosition = 0.0;
    }
}

void AudioEngine::stop()
{
    playing.store(false, std::memory_order_release);
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
    {
        juce::SpinLock::ScopedLockType lock(bufferLock);
        recordWritePosition.store(0, std::memory_order_relaxed);
        recordedBuffer.clear();
        playbackPosition = 0.0;
    }
    recordingState.store(true, std::memory_order_release);
    playing.store(false, std::memory_order_release);
    sendChangeMessage();
}

void AudioEngine::stopRecording()
{
    recordingState.store(false, std::memory_order_release);
    {
        juce::SpinLock::ScopedLockType lock(bufferLock);
        playbackPosition = 0.0;
    }
    sendChangeMessage();
}

void AudioEngine::setPlaybackPosition(double normalizedPosition)
{
    int localRecordWritePosition = recordWritePosition.load(std::memory_order_relaxed);
    if (localRecordWritePosition > 0)
    {
        juce::SpinLock::ScopedLockType lock(bufferLock);
        playbackPosition = normalizedPosition * localRecordWritePosition;
        playbackPosition = juce::jlimit(0.0, static_cast<double>(localRecordWritePosition - 1), playbackPosition);
    }
}

double AudioEngine::getPlaybackPosition() const
{
    int localRecordWritePosition = recordWritePosition.load(std::memory_order_relaxed);
    if (localRecordWritePosition > 0)
    {
        juce::SpinLock::ScopedLockType lock(bufferLock);
        return playbackPosition / localRecordWritePosition;
    }
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

juce::AudioBuffer<float> AudioEngine::getRecordedBufferCopy() const
{
    juce::SpinLock::ScopedLockType lock(bufferLock);
    juce::AudioBuffer<float> copy;
    int localRecordWritePosition = recordWritePosition.load(std::memory_order_relaxed);
    if (localRecordWritePosition > 0)
    {
        copy.makeCopyOf(recordedBuffer);
    }
    return copy;
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
    int localRecordWritePosition = recordWritePosition.load(std::memory_order_relaxed);
    if (localRecordWritePosition <= 0)
        return juce::File();
    
    auto libraryFolder = getLibraryFolder();
    
    // タイムスタンプでファイル名を生成
    auto now = juce::Time::getCurrentTime();
    auto fileName = now.formatted("Recording_%Y%m%d_%H%M%S.wav");
    auto outputFile = libraryFolder.getChildFile(fileName);
    
    // WAVファイルとして保存（バッファ内容をロック内でスナップショット）
    double localSampleRate;
    {
        juce::SpinLock::ScopedLockType lock(bufferLock);
        localSampleRate = currentSampleRate.load(std::memory_order_relaxed);
    }
    
    juce::WavAudioFormat wavFormat;
    std::unique_ptr<juce::AudioFormatWriter> writer(
        wavFormat.createWriterFor(
            new juce::FileOutputStream(outputFile),
            localSampleRate,
            static_cast<unsigned int>(recordedBuffer.getNumChannels()),
            16, // bits per sample
            {},
            0
        )
    );
    
    if (writer != nullptr)
    {
        {
            juce::SpinLock::ScopedLockType lock(bufferLock);
            writer->writeFromAudioSampleBuffer(recordedBuffer, 0, localRecordWritePosition);
        }
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
        
        {
            juce::SpinLock::ScopedLockType lock(bufferLock);
            recordedBuffer.setSize(juce::jmax(2, numChannels), numSamples);
            reader->read(&recordedBuffer, 0, numSamples, 0, true, true);
            recordWritePosition.store(numSamples, std::memory_order_release);
            currentSampleRate.store(reader->sampleRate, std::memory_order_relaxed);
            playbackPosition = 0.0;
        }
        
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
        {
            juce::SpinLock::ScopedLockType lock(bufferLock);
            recordedBuffer.makeCopyOf(slot.buffer);
            recordWritePosition.store(slot.numSamples, std::memory_order_release);
            playbackPosition = 0.0;
        }
        
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
