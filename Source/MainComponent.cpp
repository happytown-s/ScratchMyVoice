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
    };

    addAndMakeVisible(libraryToggleButton);
    libraryToggleButton.onClick = [this] {
        isLibraryOpen = !isLibraryOpen;
        resized(); // レイアウト更新
    };

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
}

MainComponent::~MainComponent()
{
    shutdownAudio();
}

void MainComponent::prepareToPlay(int samplesPerBlockExpected, double sampleRate)
{
    audioEngine.prepareToPlay(samplesPerBlockExpected, sampleRate);
}

void MainComponent::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
    // 1. まず出力をクリア
    bufferToFill.clearActiveBufferRegion();

    // 2. マイク入力がある場合、ここにデータが入っています
    // 録音中なら、この bufferToFill の内容をファイルに書き込む処理が必要です
    if (audioEngine.isRecording())
    {
        // ここでWriterに書き込む処理を追加します
        // (簡易実装のため省略しますが、入力バッファはここにあります)
    }

    // 3. AudioEngine（スクラッチ音）の音声をバッファに加算または上書き
    // これによりスクラッチ音がスピーカーから出ます
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
	playStopButton.setBounds(header.removeFromLeft(100).reduced(5));
	libraryToggleButton.setBounds(header.removeFromRight(100).reduced(5));

	// ライブラリ表示/非表示（右側からスライドインする形に）
	if (isLibraryOpen)
	{
		// 画面全体を使わず、右側200pxだけにするのではなく、オーバーレイ的に配置
		// ここでは簡単に、下半分をライブラリにする例
		// sampleList->setBounds(area.removeFromBottom(200));

		// 元のロジックだと右側を切り取っていたので、ターンテーブルが狭くなっていました。
		// ここでは「ライブラリが開いている時はリストを前面に出す」ようなロジックに変えるのが一般的ですが、
		// いったん「右端」を少し狭くします。
		sampleList->setBounds(area.removeFromRight(150));
	}
	else
	{
		// ライブラリが閉じているなら、sampleListは見えなくていい（またはサイズ0）
		sampleList->setBounds(0, 0, 0, 0);
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
