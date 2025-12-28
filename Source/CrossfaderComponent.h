/*
 ==============================================================================
 CrossfaderComponent.h
 ==============================================================================
 */
#pragma once
#include <JuceHeader.h>
#include "AudioEngine.h"

// カスタムフェーダー用LookAndFeel
class CrossfaderLookAndFeel : public juce::LookAndFeel_V4
{
public:
	void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
						  float sliderPos, float minSliderPos, float maxSliderPos,
						  const juce::Slider::SliderStyle style, juce::Slider& slider) override
	{
		juce::ignoreUnused(minSliderPos, maxSliderPos, style, slider);
		
		// フェーダー溝（トラック）
		const float trackHeight = 8.0f;
		const float trackY = y + (height - trackHeight) * 0.5f;
		
		juce::Rectangle<float> trackBounds(x + 10, trackY, width - 20, trackHeight);
		
		// 溝の背景（グラデーション）
		juce::ColourGradient trackGradient(
			juce::Colour::fromString("FF0F172A"), trackBounds.getX(), trackBounds.getY(),
			juce::Colour::fromString("FF1E293B"), trackBounds.getX(), trackBounds.getBottom(), false);
		g.setGradientFill(trackGradient);
		g.fillRoundedRectangle(trackBounds, 4.0f);
		
		// 溝の枠
		g.setColour(juce::Colour::fromString("FF334155"));
		g.drawRoundedRectangle(trackBounds, 4.0f, 1.0f);
		
		// センターマーク
		const float centerX = x + width * 0.5f;
		g.setColour(juce::Colour::fromString("FF64748B"));
		g.fillRect(centerX - 1, trackY - 3, 2.0f, trackHeight + 6);
		
		// フェーダーノブ（つまみ）- 縦長デザイン
		const float knobWidth = 24.0f;
		const float knobHeight = height - 4.0f;
		const float knobX = sliderPos - knobWidth * 0.5f;
		const float knobY = y + 2;
		
		juce::Rectangle<float> knobBounds(knobX, knobY, knobWidth, knobHeight);
		
		// ノブのグラデーション（メタリック風 - 左から右）
		juce::ColourGradient knobGradient(
			juce::Colour::fromString("FF64748B"), knobBounds.getX(), knobBounds.getY(),
			juce::Colour::fromString("FF334155"), knobBounds.getRight(), knobBounds.getY(), false);
		g.setGradientFill(knobGradient);
		g.fillRoundedRectangle(knobBounds, 4.0f);
		
		// ノブの枠
		g.setColour(juce::Colour::fromString("FF94A3B8"));
		g.drawRoundedRectangle(knobBounds, 4.0f, 1.5f);
		
		// ノブの中央ライン（グリップ）- 横向き
		g.setColour(juce::Colour::fromString("FFCBD5E1"));
		const float lineSpacing = 5.0f;
		const float lineMargin = 5.0f;
		for (int i = -2; i <= 2; ++i)
		{
			float lineY = knobBounds.getCentreY() + i * lineSpacing;
			g.drawLine(knobX + lineMargin, lineY, knobX + knobWidth - lineMargin, lineY, 1.0f);
		}
		
		// ノブ左側のハイライト
		g.setColour(juce::Colours::white.withAlpha(0.15f));
		g.fillRoundedRectangle(knobBounds.reduced(2).withWidth(knobWidth * 0.3f), 3.0f);
	}
};

class CrossfaderComponent : public juce::Component
{
	public:
	CrossfaderComponent(AudioEngine& engine);
	~CrossfaderComponent() override;

	void paint(juce::Graphics& g) override;
	void resized() override;

	private:
	AudioEngine& audioEngine;
	
	// カスタムLookAndFeel
	CrossfaderLookAndFeel crossfaderLookAndFeel;
	
	// クロスフェーダースライダー
	juce::Slider crossfaderSlider;
	
	// CUTボタン（ホールドで無音）
	juce::TextButton cutButton { "CUT" };
	
	// THRUボタン（ホールドで音量最大）
	juce::TextButton thruButton { "THRU" };
	double savedFaderValue = 0.0; // ボタン押下前のフェーダー位置を保存

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CrossfaderComponent)
};
