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
	// アームを描くスペースを空けるため、少し左に寄せる
	auto discBounds = area.removeFromLeft(area.getWidth() * 0.85f).reduced(10);

	float diameter = juce::jmin(discBounds.getWidth(), discBounds.getHeight());
	float radius = diameter / 2.0f;
	auto center = discBounds.getCentre();

	// レコード盤の正円領域
	juce::Rectangle<float> discArea(center.x - radius, center.y - radius, diameter, diameter);

	// --- 2. レコード盤の描画 ---

	// 本体（少し明るめの黒で、溝が見えるように）
	g.setColour(juce::Colour::fromString("FF222222"));
	g.fillEllipse(discArea);

	// 溝（Grooves）の表現
	g.setColour(juce::Colours::white.withAlpha(0.03f));
	for (float r = radius * 0.45f; r < radius * 0.98f; r += 2.5f)
	{
		g.drawEllipse(center.x - r, center.y - r, r * 2.0f, r * 2.0f, 1.2f);
	}

	// --- 3. ラベルの描画 (紫グラデ + 文字) ---
	float labelDiameter = diameter * 0.4f; // 少し大きめに
	juce::Rectangle<float> labelArea(center.x - labelDiameter/2, center.y - labelDiameter/2, labelDiameter, labelDiameter);

	// 回転開始
	g.saveState();
	g.addTransform(juce::AffineTransform::rotation(rotationAngle, center.x, center.y));

	// A. 紫色のグラデーション背景
	// 画像を参考に、左上が明るい紫、右下が暗い紫のグラデーションを作成
	const auto purpleLight = juce::Colour::fromString("FF7B5EFF"); // 明るい紫
	const auto purpleDark  = juce::Colour::fromString("FF4A2FB3"); // 暗い紫

	juce::ColourGradient labelGradient(
									   purpleLight, labelArea.getTopLeft(),
									   purpleDark, labelArea.getBottomRight(),
									   false); // 線形グラデーション

	g.setGradientFill(labelGradient);
	g.fillEllipse(labelArea);

	// B. 白い枠線
	g.setColour(juce::Colours::white);
	g.drawEllipse(labelArea.reduced(3.0f), 3.0f);

	// C. 「SCRATCH.AI」の文字
	g.setColour(juce::Colours::white);
	g.setFont(juce::Font("Helvetica Neue", labelDiameter * 0.18f, juce::Font::bold | juce::Font::italic));

	// 文字の描画エリア
	auto textArea = labelArea.reduced(labelDiameter * 0.1f);

	// 上半分のエリアを取り出す
	auto topHalf = textArea.removeFromTop(textArea.getHeight() / 2);

	// 「SCRATCH」： 下揃え(centredBottom)にして、Y座標をマイナス(上)にずらす
	// -labelDiameter * 0.1f くらいが丁度いい距離感です
	g.drawText("Scratch", topHalf.translated(0, -labelDiameter * 0.08f), juce::Justification::centredBottom, true);

	// 「.AI」： 残った下半分のエリア。上揃え(centredTop)にして、Y座標をプラス(下)にずらす
	g.drawText("MyVoice", textArea.translated(0, labelDiameter * 0.08f), juce::Justification::centredTop, true);
	// 回転終了
	g.restoreState();

	// センターホール
	g.setColour(juce::Colours::black);
	g.fillEllipse(center.x - 4, center.y - 4, 8, 8);


	// --- 4. トーンアームの追加描画（左上基点バージョン） ---

	// ピボット（支点）の位置：画面の左上（少し余白を開ける）
	// 座標(40, 40)あたりに固定します
	juce::Point<float> pivotCenter(40.0f, 40.0f);
	float pivotRadius = 25.0f; // 支点の大きさ

	// アームの角度計算
	// 「支点」から「レコードの中心」に向かう角度を計算し、そこから少しずらして針を落とす
	// atan2を使うと、動的に「レコードのある方向」を向いてくれます
	float angleToRecord = std::atan2(center.y - pivotCenter.y, center.x - pivotCenter.x);
	float armAngle = angleToRecord - 0.15f; // 中心より少し手前（溝）を狙う微調整

	// アームの長さ
	// 支点からレコード中心までの距離から計算し、針先が溝（radius * 0.7あたり）に来るようにする
	float distToCenter = pivotCenter.getDistanceFrom(center);
	float armLength = distToCenter - (radius * 0.7f);
	if (armLength < 50.0f) armLength = 50.0f; // 安全策

	float armWidth = 15.0f;

	// アーム全体の回転を開始
	g.saveState();
	g.addTransform(juce::AffineTransform::rotation(armAngle, pivotCenter.x, pivotCenter.y));

	// A. アーム本体（長い棒） - 右方向（角度0）を基準に描画
	g.setColour(juce::Colour::fromString("FFAAAAAA")); // グレー
	g.fillRoundedRectangle(pivotCenter.x, pivotCenter.y - armWidth/2, armLength, armWidth, armWidth/2);

	// B. カートリッジ（先端の黒い部分）
	float cartWidth = armWidth * 1.4f;  // アームに対して垂直方向の太さ
	float cartLength = 45.0f;           // アームに沿った長さ
	g.setColour(juce::Colour::fromString("FF333333")); // 黒

	// 右端（先端）に配置
	g.fillRect(pivotCenter.x + armLength - cartLength/2, pivotCenter.y - cartWidth/2, cartLength, cartWidth);

	// 回転終了
	g.restoreState();

	// C. ピボット（支点の丸）- 最後に上から描画
	g.setColour(juce::Colour::fromString("FF333333"));
	g.fillEllipse(pivotCenter.x - pivotRadius, pivotCenter.y - pivotRadius, pivotRadius*2, pivotRadius*2);
	// 内側の装飾
	g.setColour(juce::Colour::fromString("FFAAAAAA"));
	g.drawEllipse(pivotCenter.x - pivotRadius, pivotCenter.y - pivotRadius, pivotRadius*2, pivotRadius*2, 2.0f);
	g.fillEllipse(pivotCenter.x - pivotRadius*0.4f, pivotCenter.y - pivotRadius*0.4f, pivotRadius*0.8f, pivotRadius*0.8f);
}

void TurntableComponent::resized()
{
}

void TurntableComponent::mouseDown(const juce::MouseEvent& e)
{
	isDragging = true;
	lastAngle = getAngleFromPoint(e.position);
	audioEngine.setScratchRate(0.0); // ドラッグ中は一時的に止める等の処理
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
	lastAngle = currentAngle;

	// ここでaudioEngineにスクラッチ速度を送る処理が入ります
	repaint();
}

void TurntableComponent::mouseUp(const juce::MouseEvent& e)
{
	isDragging = false;
	audioEngine.setScratchRate(1.0); // 再生再開（仮）
}

void TurntableComponent::timerCallback()
{
	if (!isDragging && audioEngine.isPlaying())
	{
		rotationAngle += 0.05f; // 自動回転
		repaint();
	}
}

void TurntableComponent::setExpanded(bool shouldExpand)
{
	isExpanded = shouldExpand;
}

float TurntableComponent::getAngleFromPoint(juce::Point<float> p)
{
	auto center = getLocalBounds().toFloat().getCentre();
	return std::atan2(p.y - center.y, p.x - center.x);
}
