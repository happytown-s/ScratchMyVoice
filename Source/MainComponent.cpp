/*
 ==============================================================================
 MainComponent.cpp
 ==============================================================================
 Issue #13 — Responsive aspect ratio for stage/turntable components
 Issue #16 — Mobile-first layout redesign:
             • Stage (turntable) → top
             • Waveform → bottom
             • Slot selection → bottom sheet / tab bar
             • Crossfader → fixed at bottom
             • Uses FlexBox / relative proportional layout
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

    // Issue #16: Use full screen by default; layout adapts in resized()
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

// ─── Layout detection ───────────────────────────────────────────────────────

bool MainComponent::detectMobileLayout() const
{
    // Portrait orientation or narrow width → mobile layout
    int w = getWidth();
    int h = getHeight();
    return (h > w) || (w < 600);
}

// ─── Responsive resized() ───────────────────────────────────────────────────

void MainComponent::resized()
{
	auto area = getLocalBounds();
	const int width = getWidth();
	const int height = getHeight();

	// Issue #13/#16: Detect mobile vs desktop and delegate
	isMobileLayout = detectMobileLayout();

	// Safe area insets (iOS notch, Android status bar)
#if JUCE_IOS
	area.removeFromTop(40);
#endif
#if JUCE_ANDROID
	area.removeFromTop(24);
#endif

	if (isMobileLayout)
	{
		layoutMobile(area);
	}
	else
	{
		layoutDesktop(area);
	}
}

// ─── Issue #16: Mobile-first layout ─────────────────────────────────────────
//
// Layout from top to bottom:
//   1. Header bar (PLAY, REC, Library, Audio buttons)
//   2. Sample slots (horizontal pill buttons)
//   3. Stage / Turntable (square, aspect-ratio responsive)
//   4. Waveform
//   5. Crossfader (fixed at bottom)
//
void MainComponent::layoutMobile(juce::Rectangle<int>& area)
{
	const int width = area.getWidth();
	const int height = area.getHeight();

	// ── 1. Header bar ──────────────────────────────────────────────────
	const int headerHeight = juce::jmax(44, height / 16);
	auto header = area.removeFromTop(headerHeight);
	const int pad = 4;

	// FlexBox for header buttons (row, evenly spaced)
	juce::FlexBox headerFlex;
	headerFlex.flexDirection = juce::FlexBox::Direction::row;
	headerFlex.justifyContent = juce::FlexBox::JustifyContent::spaceBetween;
	headerFlex.alignItems = juce::FlexBox::AlignItems::stretch;

	headerFlex.items.add(juce::FlexItem(playStopButton).withMinWidth(60).withMinHeight(headerHeight - pad * 2));
	headerFlex.items.add(juce::FlexItem(recordButton).withMinWidth(60).withMinHeight(headerHeight - pad * 2));
	headerFlex.items.add(juce::FlexItem(libraryToggleButton).withMinWidth(60).withMinHeight(headerHeight - pad * 2));
	headerFlex.items.add(juce::FlexItem(audioSettingsButton).withMinWidth(60).withMinHeight(headerHeight - pad * 2));

	headerFlex.performLayout(header.reduced(pad));

	// ── Library overlay (full width panel, below header) ───────────────
	if (isLibraryOpen)
	{
		sampleList->setBounds(area.removeFromTop(juce::jmin(height / 3, 250)));
	}

	// ── Audio settings overlay ─────────────────────────────────────────
	if (isAudioSettingsOpen && audioSelector != nullptr)
	{
		audioSelector->setBounds(area.removeFromTop(juce::jmin(height / 3, 250)));
	}

	// ── 2. Sample slots (horizontal bottom-sheet style) ────────────────
	const int slotHeight = juce::jmax(40, height / 14);
	auto slotArea = area.removeFromTop(slotHeight);
	sampleSlots->setBounds(slotArea);

	// ── 3. Stage / Turntable (square, aspect-ratio responsive) ─────────
	// Issue #13: Maintain square aspect ratio, fill available width
	int turntableSize = juce::jmin(area.getWidth(), (int)(area.getHeight() * 0.55f));
	int turntableX = (area.getWidth() - turntableSize) / 2;
	auto turntableArea = area.removeFromTop(turntableSize);
	turntable->setBounds(turntableArea.withX(turntableX).withWidth(turntableSize));

	// ── 4. Waveform (remaining space) ───────────────────────────────────
	const int waveformHeight = juce::jmax(60, area.getHeight() / 3);
	waveform->setBounds(area.removeFromTop(waveformHeight));

	// ── 5. Crossfader (fixed at bottom) ────────────────────────────────
	// Issue #16: Crossfader pinned to bottom
	const int crossfaderHeight = juce::jmax(60, height / 10);
	crossfader->setBounds(area.removeFromBottom(crossfaderHeight));

	// Hide any remaining audio selector if not active
	if (!isAudioSettingsOpen && audioSelector != nullptr)
	{
		audioSelector->setBounds(0, 0, 0, 0);
	}
}

// ─── Desktop layout (enhanced responsive, Issue #13) ────────────────────────

void MainComponent::layoutDesktop(juce::Rectangle<int>& area)
{
	const int width = area.getWidth();
	const int height = area.getHeight();

	// 【レスポンシブ設定】画面サイズに応じたパラメータ
	const int headerHeight = juce::jmax(50, height / 15);
	const int buttonWidth = juce::jmax(70, width / 12);
	const int sidebarWidth = juce::jmax(150, width / 6);
	const int audioSettingsWidth = juce::jmax(280, width / 4);
	const int bottomControlHeight = juce::jmax(100, height / 7);
	const int crossfaderHeight = juce::jmax(50, height / 14);

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
	
	// Issue #13: Crossfader responsive sizing
	const int crossfaderActualHeight = static_cast<int>(crossfaderHeight * 1.5f);
	auto crossfaderArea = bottomControl.removeFromBottom(crossfaderActualHeight);
	const int crossfaderWidth = juce::jmin(crossfaderArea.getWidth() / 2, 350);
	crossfader->setBounds(crossfaderArea.withSizeKeepingCentre(crossfaderWidth, crossfaderActualHeight - 6));

	waveform->setBounds(bottomControl);

	// Issue #13: Responsive slot width
	const int slotWidth = juce::jmax(80, width / 12);
	sampleSlots->setBounds(area.removeFromLeft(slotWidth));

	// Issue #13: Turntable fills remaining space (aspect ratio handled inside paint)
	turntable->setBounds(area);
}

// ─── Button/Timer callbacks ─────────────────────────────────────────────────

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
