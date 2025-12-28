/*
 ==============================================================================
 CrossfaderComponent.cpp
 ==============================================================================
 */
#include "CrossfaderComponent.h"
#include "Constants.h"

CrossfaderComponent::CrossfaderComponent(AudioEngine& engine)
: audioEngine(engine)
{
	// カスタムLookAndFeelを適用
	crossfaderSlider.setLookAndFeel(&crossfaderLookAndFeel);
	
	// スライダー設定
	crossfaderSlider.setSliderStyle(juce::Slider::LinearHorizontal);
	crossfaderSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
	crossfaderSlider.setRange(0.0, 1.0, 0.01);
	crossfaderSlider.setValue(0.5); // デフォルトは中央
	
	crossfaderSlider.onValueChange = [this] {
		// どちらのボタンも押されていない時のみフェーダー値を反映＆保存
		if (!thruButton.isDown() && !cutButton.isDown())
		{
			savedFaderValue = crossfaderSlider.getValue();
			audioEngine.setCrossfaderGain(static_cast<float>(crossfaderSlider.getValue()));
		}
	};
	addAndMakeVisible(crossfaderSlider);
	
	// CUTボタン設定（ホールドで無音）
	cutButton.setColour(juce::TextButton::buttonColourId, juce::Colour::fromString("FF334155"));
	cutButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour::fromString("FFEF4444"));
	cutButton.setColour(juce::TextButton::textColourOffId, juce::Colour::fromString("FF94A3B8"));
	cutButton.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
	
	// ホールドでCUT、離すと元のフェーダー値に戻る
	cutButton.onStateChange = [this] {
		if (cutButton.isDown())
		{
			// ホールド中 = 現在のフェーダー位置を保存してから無音に
			savedFaderValue = crossfaderSlider.getValue();
			crossfaderSlider.setValue(0.0, juce::dontSendNotification);
			audioEngine.setCrossfaderGain(0.0f);
			cutButton.setToggleState(true, juce::dontSendNotification);
		}
		else
		{
			// 離した = 保存した位置に戻す
			crossfaderSlider.setValue(savedFaderValue, juce::dontSendNotification);
			audioEngine.setCrossfaderGain(static_cast<float>(savedFaderValue));
			cutButton.setToggleState(false, juce::dontSendNotification);
		}
	};
	addAndMakeVisible(cutButton);
	
	// THRUボタン設定（ホールドで音量最大）
	thruButton.setColour(juce::TextButton::buttonColourId, juce::Colour::fromString("FF334155"));
	thruButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour::fromString("FF22C55E"));
	thruButton.setColour(juce::TextButton::textColourOffId, juce::Colour::fromString("FF94A3B8"));
	thruButton.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
	
	// ホールドでTHRU、離すと元のフェーダー値に戻る
	thruButton.onStateChange = [this] {
		if (thruButton.isDown())
		{
			// ホールド中 = 現在のフェーダー位置を保存してから最大に
			savedFaderValue = crossfaderSlider.getValue();
			crossfaderSlider.setValue(1.0, juce::dontSendNotification);
			audioEngine.setCrossfaderGain(1.0f);
			thruButton.setToggleState(true, juce::dontSendNotification);
		}
		else
		{
			// 離した = 保存した位置に戻す
			crossfaderSlider.setValue(savedFaderValue, juce::dontSendNotification);
			audioEngine.setCrossfaderGain(static_cast<float>(savedFaderValue));
			thruButton.setToggleState(false, juce::dontSendNotification);
		}
	};
	addAndMakeVisible(thruButton);
	
	// 初期値をAudioEngineに反映（中央）
	savedFaderValue = 0.5;
	audioEngine.setCrossfaderGain(0.5f);
}

CrossfaderComponent::~CrossfaderComponent()
{
	crossfaderSlider.setLookAndFeel(nullptr);
}

void CrossfaderComponent::paint(juce::Graphics& g)
{
	// 背景（少しグラデーション）
	juce::ColourGradient bgGradient(
		juce::Colour::fromString("FF0F172A"), 0, 0,
		Constants::bgDark, 0, (float)getHeight(), false);
	g.setGradientFill(bgGradient);
	g.fillRoundedRectangle(getLocalBounds().toFloat(), 8.0f);
	
	// 枠線
	g.setColour(juce::Colour::fromString("FF334155"));
	g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 8.0f, 1.0f);
}

void CrossfaderComponent::resized()
{
	auto area = getLocalBounds().reduced(8);
	
	// 両側にボタンを配置
	const int buttonWidth = 60;
	cutButton.setBounds(area.removeFromLeft(buttonWidth).reduced(2));
	thruButton.setBounds(area.removeFromRight(buttonWidth).reduced(2));
	
	// 残りをフェーダーに
	crossfaderSlider.setBounds(area);
}
