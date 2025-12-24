/*
 ==============================================================================
 MainComponent.cpp
 ==============================================================================
 */
#include "MainComponent.h"

MainComponent::MainComponent()
    : audioEngine() // メンバー初期化
{
    // UIコンポーネントの初期化と追加
    turntable = std::make_unique<TurntableComponent>(audioEngine);
    addAndMakeVisible(turntable.get());

    waveform = std::make_unique<WaveformComponent>(audioEngine);
    addAndMakeVisible(waveform.get());

    crossfader = std::make_unique<CrossfaderComponent>(audioEngine);
    addAndMakeVisible(crossfader.get());

    sampleList = std::make_unique<SampleListComponent>(audioEngine);
    addAndMakeVisible(sampleList.get());

    // ボタン設定
    addAndMakeVisible(playStopButton);
    playStopButton.onClick = [this] {
        if (audioEngine.isPlaying()) audioEngine.stop();
        else audioEngine.play();
        updateButtonColors();
    };

    addAndMakeVisible(libraryToggleButton);
    libraryToggleButton.onClick = [this] {
        isLibraryOpen = !isLibraryOpen;
        isAudioSettingsOpen = false;
        updateButtonColors();
        resized();
    };

    addAndMakeVisible(audioSettingsButton);
    audioSettingsButton.onClick = [this] {
        isAudioSettingsOpen = !isAudioSettingsOpen;
        if (isAudioSettingsOpen) {
            isLibraryOpen = false;
            audioSelector = std::make_unique<juce::AudioDeviceSelectorComponent>(
                deviceManager,
                0, 2, 0, 2, true, true, true, false);
            addAndMakeVisible(audioSelector.get());
        } else {
            audioSelector = nullptr;
        }
        updateButtonColors();
        resized();
    };

    // RECボタン
    addAndMakeVisible(recordButton);
    recordButton.onClick = [this] {
        if (audioEngine.isRecording()) {
            audioEngine.stopRecording();
        } else {
            audioEngine.startRecording();
        }
        updateButtonColors();
    };

    // ボタン色の初期設定
    updateButtonColors();

    // 状態更新用タイマー (100ms間隔)
    startTimer(100);

    setSize(800, 600);

    // 録音権限の要求 (モバイルや最近のmacOSで必須)
    if (juce::RuntimePermissions::isRequired (juce::RuntimePermissions::recordAudio)
        && ! juce::RuntimePermissions::isGranted (juce::RuntimePermissions::recordAudio))
    {
        juce::RuntimePermissions::request (juce::RuntimePermissions::recordAudio,
                                           [&] (bool granted) { setAudioChannels (granted ? 2 : 0, 2); });
    }
    else
    {
        setAudioChannels(2, 2); // 入力2ch (マイク), 出力2ch
    }
    // オーディオデバイス設定の読み込み
    auto settingsFile = getAudioSettingsFile();
    if (settingsFile.existsAsFile())
    {
        auto xml = juce::XmlDocument::parse(settingsFile);
        if (xml != nullptr)
            deviceManager.initialise(2, 2, xml.get(), true);
    }
}

juce::File MainComponent::getAudioSettingsFile() const
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("ScratchMyVoice")
        .getChildFile("audioSettings.xml");
}

MainComponent::~MainComponent()
{
    stopTimer();
    
    // オーディオデバイス設定の保存
    auto settingsFile = getAudioSettingsFile();
    settingsFile.getParentDirectory().createDirectory();
    
    auto state = deviceManager.createStateXml();
    if (state != nullptr)
        state->writeTo(settingsFile, {});

    audioSelector = nullptr;
    shutdownAudio();
}

void MainComponent::prepareToPlay(int samplesPerBlockExpected, double sampleRate)
{
    audioEngine.prepareToPlay(samplesPerBlockExpected, sampleRate);
}

void MainComponent::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
    // 1. 録音中ならマイク入力をAudioEngineのバッファに保存
    if (audioEngine.isRecording())
    {
        audioEngine.recordAudioBlock(bufferToFill);
    }

    // 2. 出力をクリア
    bufferToFill.clearActiveBufferRegion();

    // 3. AudioEngineから再生音を取得
    audioEngine.getNextAudioBlock(bufferToFill);
}

void MainComponent::releaseResources()
{
    audioEngine.releaseResources();
}

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(Constants::bgDark); // Constants.hの色を使用
}

void MainComponent::resized()
{
	auto area = getLocalBounds();

	// 【重要】スマホの上部ノッチ（ステータスバー）を避けるための余白
	// 本当は getSafeInsets() を使いますが、手っ取り早く上を空けます
#if JUCE_IOS
	area.removeFromTop(40); // 40px下げる
#endif

	// 上部ヘッダー（ボタンなど）
	auto header = area.removeFromTop(50);
	playStopButton.setBounds(header.removeFromLeft(80).reduced(5));
	recordButton.setBounds(header.removeFromLeft(80).reduced(5));
	libraryToggleButton.setBounds(header.removeFromRight(100).reduced(5));
	audioSettingsButton.setBounds(header.removeFromRight(100).reduced(5));

	// ライブラリ表示/非表示
	if (isLibraryOpen)
	{
		sampleList->setBounds(area.removeFromRight(150));
	}
	else
	{
		sampleList->setBounds(0, 0, 0, 0);
	}

	// オーディオ設定パネル表示/非表示
	if (isAudioSettingsOpen && audioSelector != nullptr)
	{
		audioSelector->setBounds(area.removeFromRight(300));
	}
	else if (audioSelector != nullptr)
	{
		audioSelector->setBounds(0, 0, 0, 0);
	}

	// 残りのエリアを分割
	auto bottomControl = area.removeFromBottom(120); // 下部を少し広めに
	crossfader->setBounds(bottomControl.removeFromBottom(60).reduced(10, 5));

	waveform->setBounds(bottomControl); // 波形表示
	turntable->setBounds(area); // 残りすべてをターンテーブルに（これで広くなる！）
}

void MainComponent::buttonClicked(juce::Button* button)
{
    // リスナー実装 (Lambdaを使ったのでここは空でもOKですが、SampleListComponent用などに残しています)
}

void MainComponent::timerCallback()
{
    updateButtonColors();
}

void MainComponent::updateButtonColors()
{
    // 色の定義
    const auto greenActive = juce::Colour::fromString("FF22C55E");   // 再生中の緑
    const auto blueActive = juce::Colour::fromString("FF3B82F6");    // パネル開のブルー
    const auto redActive = juce::Colour::fromString("FFEF4444");     // 録音中の赤
    const auto defaultBg = juce::Colour::fromString("FF334155");     // デフォルト背景
    const auto defaultText = juce::Colours::white;
    
    // PLAY/STOP ボタン
    if (audioEngine.isPlaying())
    {
        playStopButton.setButtonText("STOP");
        playStopButton.setColour(juce::TextButton::buttonColourId, greenActive);
        playStopButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    }
    else
    {
        playStopButton.setButtonText("PLAY");
        playStopButton.setColour(juce::TextButton::buttonColourId, defaultBg);
        playStopButton.setColour(juce::TextButton::textColourOffId, defaultText);
    }
    
    // REC ボタン
    if (audioEngine.isRecording())
    {
        recordButton.setButtonText("STOP");
        recordButton.setColour(juce::TextButton::buttonColourId, redActive);
        recordButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    }
    else
    {
        recordButton.setButtonText("REC");
        recordButton.setColour(juce::TextButton::buttonColourId, defaultBg);
        recordButton.setColour(juce::TextButton::textColourOffId, defaultText);
    }
    
    // Library ボタン
    if (isLibraryOpen)
    {
        libraryToggleButton.setColour(juce::TextButton::buttonColourId, blueActive);
        libraryToggleButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    }
    else
    {
        libraryToggleButton.setColour(juce::TextButton::buttonColourId, defaultBg);
        libraryToggleButton.setColour(juce::TextButton::textColourOffId, defaultText);
    }
    
    // Audio Settings ボタン
    if (isAudioSettingsOpen)
    {
        audioSettingsButton.setColour(juce::TextButton::buttonColourId, blueActive);
        audioSettingsButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    }
    else
    {
        audioSettingsButton.setColour(juce::TextButton::buttonColourId, defaultBg);
        audioSettingsButton.setColour(juce::TextButton::textColourOffId, defaultText);
    }
}
