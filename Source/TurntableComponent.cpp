/*
 ==============================================================================
 TurntableComponent.cpp
 ==============================================================================
 */
#include "TurntableComponent.h"

TurntableComponent::TurntableComponent(AudioEngine& engine)
: audioEngine(engine)
{
	startTimer(16); // ~60fps for smooth animation
}

TurntableComponent::~TurntableComponent()
{
	stopTimer();
}

void TurntableComponent::paint(juce::Graphics& g)
{
	// --- 1. 背景色の変更 ---
	// 画像からスポイトした濃いネイビー色
	const auto bgNavy = juce::Colour::fromString("FF1A1C2B");
	g.fillAll(bgNavy);

	// --- 描画エリアの計算 ---
	auto area = getLocalBounds().toFloat();
	// アーム用のスペースを確保するため、上部に余白を設ける
	auto discBounds = area.reduced(60, 40);

	// レコードのサイズを少し小さくしてアームのスペースを確保
	float diameter = juce::jmin(discBounds.getWidth(), discBounds.getHeight()) * 0.85f;
	float radius = diameter / 2.0f;
	auto center = discBounds.getCentre();

	// レコード盤の正円領域
	juce::Rectangle<float> discArea(center.x - radius, center.y - radius, diameter, diameter);

	// --- 2. レコード盤の描画（リッチバージョン） ---

	// 影（ドロップシャドウ効果）
	g.setColour(juce::Colours::black.withAlpha(0.5f));
	g.fillEllipse(discArea.translated(4.0f, 4.0f));

	// レコード外周のリング（メタリック効果）
	juce::ColourGradient outerRingGradient(
		juce::Colour::fromString("FF444444"), discArea.getCentreX(), discArea.getY(),
		juce::Colour::fromString("FF111111"), discArea.getCentreX(), discArea.getBottom(),
		false);
	g.setGradientFill(outerRingGradient);
	g.fillEllipse(discArea);

	// 本体（ビニールの光沢感）
	auto innerDisc = discArea.reduced(3.0f);
	juce::ColourGradient discGradient(
		juce::Colour::fromString("FF333333"), innerDisc.getCentreX(), innerDisc.getY(),
		juce::Colour::fromString("FF1A1A1A"), innerDisc.getCentreX(), innerDisc.getBottom(),
		false);
	g.setGradientFill(discGradient);
	g.fillEllipse(innerDisc);

	// 溝（Grooves）の表現 - リアルなビニール表現
	// 黒と少し明るい黒の交互で溝を表現
	for (float r = radius * 0.42f; r < radius * 0.97f; r += 2.0f)
	{
		// 暗い線（溝の影）
		g.setColour(juce::Colours::black.withAlpha(0.4f));
		g.drawEllipse(center.x - r, center.y - r, r * 2.0f, r * 2.0f, 1.0f);
		// 少し明るい線（溝の反射）
		g.setColour(juce::Colour::fromString("FF2A2A2A"));
		g.drawEllipse(center.x - (r + 1.0f), center.y - (r + 1.0f), (r + 1.0f) * 2.0f, (r + 1.0f) * 2.0f, 0.5f);
	}

	// --- 3. ラベルの描画 (紫グラデ + 文字) ---
	float labelDiameter = diameter * 0.4f;
	juce::Rectangle<float> labelArea(center.x - labelDiameter/2, center.y - labelDiameter/2, labelDiameter, labelDiameter);

	// 回転開始
	g.saveState();
	g.addTransform(juce::AffineTransform::rotation(rotationAngle, center.x, center.y));

	// ラベルの影
	g.setColour(juce::Colours::black.withAlpha(0.3f));
	g.fillEllipse(labelArea.translated(2.0f, 2.0f));

	// 紫色のグラデーション背景
	const auto purpleLight = juce::Colour::fromString("FF9B7EFF"); // より明るい紫
	const auto purpleDark  = juce::Colour::fromString("FF4A2FB3");

	juce::ColourGradient labelGradient(
		purpleLight, labelArea.getTopLeft(),
		purpleDark, labelArea.getBottomRight(),
		false);
	g.setGradientFill(labelGradient);
	g.fillEllipse(labelArea);

	// 白い枠線
	g.setColour(juce::Colours::white.withAlpha(0.8f));
	g.drawEllipse(labelArea.reduced(3.0f), 2.5f);

	// 文字（ドロップシャドウ付き）
	g.setFont(juce::Font("Helvetica Neue", labelDiameter * 0.18f, juce::Font::bold | juce::Font::italic));
	auto textArea = labelArea.reduced(labelDiameter * 0.1f);
	auto topHalf = textArea.removeFromTop(textArea.getHeight() / 2);

	// 影
	g.setColour(juce::Colours::black.withAlpha(0.3f));
	g.drawText("Scratch", topHalf.translated(1, -labelDiameter * 0.08f + 1), juce::Justification::centredBottom, true);
	g.drawText("MyVoice", textArea.translated(1, labelDiameter * 0.08f + 1), juce::Justification::centredTop, true);
	
	// 本体の文字
	g.setColour(juce::Colours::white);
	topHalf = labelArea.reduced(labelDiameter * 0.1f);
	topHalf = topHalf.removeFromTop(topHalf.getHeight() / 2);
	g.drawText("Scratch", topHalf.translated(0, -labelDiameter * 0.08f), juce::Justification::centredBottom, true);
	g.drawText("MyVoice", labelArea.reduced(labelDiameter * 0.1f).withTrimmedTop(labelArea.getHeight() * 0.4f).translated(0, labelDiameter * 0.08f), juce::Justification::centredTop, true);
	
	g.restoreState();

	// センターホール（メタリック）
	g.setColour(juce::Colour::fromString("FF444444"));
	g.fillEllipse(center.x - 6, center.y - 6, 12, 12);
	g.setColour(juce::Colours::black);
	g.fillEllipse(center.x - 3, center.y - 3, 6, 6);


	// --- 4. トーンアームの追加描画（リッチバージョン） ---

	juce::Point<float> pivotCenter(discArea.getX() - 30.0f, discArea.getY() - 30.0f);
	float pivotRadius = 20.0f;
	float armLength = radius * 1.3f;
	float armWidth = 12.0f;

	float armAngle;
	if (audioEngine.isPlaying() || isDragging) {
		armAngle = juce::MathConstants<float>::pi * 0.12f;
	} else {
		armAngle = 0.0f;
	}

	juce::Point<float> armEnd(
		pivotCenter.x + std::cos(armAngle) * armLength,
		pivotCenter.y + std::sin(armAngle) * armLength
	);

	// アームの影
	g.setColour(juce::Colours::black.withAlpha(0.4f));
	juce::Line<float> shadowLine(pivotCenter.translated(3.0f, 3.0f), armEnd.translated(3.0f, 3.0f));
	g.drawLine(shadowLine, armWidth);

	// アーム本体（メタリックグラデーション）
	juce::ColourGradient armGradient(
		juce::Colour::fromString("FFCCCCCC"), pivotCenter.x, pivotCenter.y - armWidth,
		juce::Colour::fromString("FF666666"), pivotCenter.x, pivotCenter.y + armWidth,
		false);
	g.setGradientFill(armGradient);
	juce::Line<float> armLine(pivotCenter, armEnd);
	g.drawLine(armLine, armWidth);

	// カートリッジ（リッチ版）
	float cartLength = 30.0f;
	float cartWidth = armWidth * 1.2f;
	
	g.saveState();
	g.addTransform(juce::AffineTransform::rotation(armAngle + juce::MathConstants<float>::halfPi, armEnd.x, armEnd.y));
	
	// カートリッジ影
	g.setColour(juce::Colours::black.withAlpha(0.4f));
	g.fillRoundedRectangle(armEnd.x - cartWidth/2 + 2, armEnd.y - cartLength/2 + 2, cartWidth, cartLength, 3.0f);
	
	// カートリッジ本体
	juce::ColourGradient cartGradient(
		juce::Colour::fromString("FF555555"), armEnd.x - cartWidth/2, armEnd.y,
		juce::Colour::fromString("FF222222"), armEnd.x + cartWidth/2, armEnd.y,
		false);
	g.setGradientFill(cartGradient);
	g.fillRoundedRectangle(armEnd.x - cartWidth/2, armEnd.y - cartLength/2, cartWidth, cartLength, 3.0f);
	
	// 針先（ダイヤモンド風）
	g.setColour(juce::Colour::fromString("FFCCCCCC"));
	g.fillEllipse(armEnd.x - 2, armEnd.y + cartLength/2 - 4, 4, 4);
	
	g.restoreState();

	// ピボット（メタリック）
	juce::ColourGradient pivotGradient(
		juce::Colour::fromString("FF666666"), pivotCenter.x, pivotCenter.y - pivotRadius,
		juce::Colour::fromString("FF222222"), pivotCenter.x, pivotCenter.y + pivotRadius,
		false);
	g.setGradientFill(pivotGradient);
	g.fillEllipse(pivotCenter.x - pivotRadius, pivotCenter.y - pivotRadius, pivotRadius*2, pivotRadius*2);
	
	// ピボット中心
	g.setColour(juce::Colour::fromString("FF111111"));
	g.fillEllipse(pivotCenter.x - pivotRadius*0.3f, pivotCenter.y - pivotRadius*0.3f, pivotRadius*0.6f, pivotRadius*0.6f);
}

void TurntableComponent::resized()
{
}

void TurntableComponent::mouseDown(const juce::MouseEvent& e)
{
	isDragging = true;
	lastAngle = getAngleFromPoint(e.position);
	lastTime = juce::Time::getMillisecondCounterHiRes();
	audioEngine.setScratchSpeed(0.0); // ドラッグ開始時は停止
}

void TurntableComponent::mouseDrag(const juce::MouseEvent& e)
{
	if (!isDragging) return;

	float currentAngle = getAngleFromPoint(e.position);
	float diff = currentAngle - lastAngle;

	// 角度の不連続性を補正（-PI to PI）
	if (diff < -juce::MathConstants<float>::pi) diff += juce::MathConstants<float>::twoPi;
	if (diff > juce::MathConstants<float>::pi) diff -= juce::MathConstants<float>::twoPi;

	rotationAngle += diff;
	
	// 時間差から速度を計算
	double currentTime = juce::Time::getMillisecondCounterHiRes();
	double timeDiff = currentTime - lastTime;
	
	if (timeDiff > 0)
	{
		// 角度変化を速度に変換（正の値で順再生、負で逆再生）
		// 感度調整: diff * 定数
		double scratchSpeed = (diff / juce::MathConstants<float>::pi) * 8.0;
		
		// 速度の範囲を制限
		scratchSpeed = juce::jlimit(-4.0, 4.0, scratchSpeed);
		
		audioEngine.setScratchSpeed(scratchSpeed);
	}
	
	lastAngle = currentAngle;
	lastTime = currentTime;
	
	repaint();
}

void TurntableComponent::mouseUp(const juce::MouseEvent& e)
{
	isDragging = false;
	// マウスを離したら通常再生速度に戻す
	if (audioEngine.isPlaying())
		audioEngine.setScratchSpeed(1.0);
	else
		audioEngine.setScratchSpeed(0.0);
}

void TurntableComponent::timerCallback()
{
	if (!isDragging && audioEngine.isPlaying())
	{
		// 再生中は自動回転
		rotationAngle += 0.05f;
		repaint();
	}
	else if (isDragging)
	{
		repaint();
	}
}

void TurntableComponent::setExpanded(bool shouldExpand)
{
	isExpanded = shouldExpand;
}

float TurntableComponent::getAngleFromPoint(juce::Point<float> p)
{
	auto area = getLocalBounds().toFloat();
	auto discBounds = area.removeFromLeft(area.getWidth() * 0.85f).reduced(10);
	auto center = discBounds.getCentre();
	return std::atan2(p.y - center.y, p.x - center.x);
}

