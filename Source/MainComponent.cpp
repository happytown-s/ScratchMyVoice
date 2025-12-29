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

    sampleSlots = std::make_unique<SampleSlotComponent>(audioEngine);
    sampleSlots->setSlotAssignCallback([this](int slotIndex) {
        // ライブラリで選択中のファイルをスロットにロード
        auto selectedFile = sampleList->getSelectedFile();
        DBG("Slot " << slotIndex << " clicked, selected file: " << selectedFile.getFullPathName());
        if (selectedFile.existsAsFile())
        {
            DBG("Loading file to slot " << slotIndex);
            audioEngine.loadFileToSlot(slotIndex, selectedFile);
            sampleList->clearSelection(); // スロットへロード後、選択を解除
        }
        else
        {
            DBG("No file selected or file doesn't exist");
        }
    });
    addAndMakeVisible(sampleSlots.get());

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
            // 録音データをファイルに保存してライブラリを更新
            auto savedFile = audioEngine.saveRecordingToFile();
            if (savedFile.existsAsFile()) {
                sampleList->refreshLibrary();
            }
        } else {
            audioEngine.startRecording();
        }
        updateButtonColors();
    };

    // ボタン色の初期設定
    updateButtonColors();

    // 状態更新用タイマー (100ms間隔)
    startTimer(100);

    // タブレット横画面に最適化したサイズ
    setSize(824, 768);

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
	const int width = getWidth();
	const int height = getHeight();

	// 【レスポンシブ設定】画面サイズに応じたパラメータ
	const int headerHeight = juce::jmax(50, height / 15);  // 最低50px、高さの1/15
	const int buttonWidth = juce::jmax(70, width / 12);    // 最低70px、幅の1/12
	const int sidebarWidth = juce::jmax(150, width / 6);   // 最低150px、幅の1/6
	const int audioSettingsWidth = juce::jmax(280, width / 4); // 最低280px、幅の1/4
	const int bottomControlHeight = juce::jmax(100, height / 7); // 最低100px、高さの1/7
	const int crossfaderHeight = juce::jmax(50, height / 14); // 最低50px、高さの1/14

	// 【重要】スマホの上部ノッチ（ステータスバー）を避けるための余白
#if JUCE_IOS
	area.removeFromTop(40); // 40px下げる
#endif

	// 上部ヘッダー（ボタンなど）
	auto header = area.removeFromTop(headerHeight);
	const int buttonPadding = 5;
	playStopButton.setBounds(header.removeFromLeft(buttonWidth).reduced(buttonPadding));
	recordButton.setBounds(header.removeFromLeft(buttonWidth).reduced(buttonPadding));
	libraryToggleButton.setBounds(header.removeFromRight(buttonWidth + 20).reduced(buttonPadding));
	audioSettingsButton.setBounds(header.removeFromRight(buttonWidth + 20).reduced(buttonPadding));

	// ライブラリ表示/非表示
	if (isLibraryOpen)
	{
		sampleList->setBounds(area.removeFromRight(sidebarWidth));
	}
	else
	{
		sampleList->setBounds(0, 0, 0, 0);
	}

	// オーディオ設定パネル表示/非表示
	if (isAudioSettingsOpen && audioSelector != nullptr)
	{
		audioSelector->setBounds(area.removeFromRight(audioSettingsWidth));
	}
	else if (audioSelector != nullptr)
	{
		audioSelector->setBounds(0, 0, 0, 0);
	}

	// 残りのエリアを分割
	auto bottomControl = area.removeFromBottom(bottomControlHeight);
	
	// クロスフェーダーを中央に配置（幅は1/3、高さは1.5倍）
	const int crossfaderActualHeight = static_cast<int>(crossfaderHeight * 1.5f);
	auto crossfaderArea = bottomControl.removeFromBottom(crossfaderActualHeight);
	const int crossfaderWidth = juce::jmin(crossfaderArea.getWidth() / 2, 350); // 幅1/2、最大350px
	crossfader->setBounds(crossfaderArea.withSizeKeepingCentre(crossfaderWidth, crossfaderActualHeight - 6));

	waveform->setBounds(bottomControl); // 波形表示
	
	// サンプルスロットを左側に配置
	const int slotWidth = juce::jmax(80, width / 12);
	sampleSlots->setBounds(area.removeFromLeft(slotWidth));
	
	turntable->setBounds(area); // 残りをターンテーブルに
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
