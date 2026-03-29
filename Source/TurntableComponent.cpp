/*
 ==============================================================================
 TurntableComponent.cpp
 ==============================================================================
 Issue #14 — touch/multi-touch support, swipe gestures, Safe Area
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

	// Issue #13: Responsive aspect ratio — maintain square disc within available bounds
	float availableWidth = area.getWidth() - 80.0f;  // margins for arm
	float availableHeight = area.getHeight() - 80.0f;
	float diameter = juce::jmin(availableWidth, availableHeight) * 0.85f;
	diameter = juce::jmax(diameter, 100.0f); // minimum disc size

	float radius = diameter / 2.0f;
	auto center = area.getCentre();

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
	float fontSize = juce::jmax(labelDiameter * 0.18f, 8.0f);
	g.setFont(juce::Font("Helvetica Neue", fontSize, juce::Font::bold | juce::Font::italic));
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
	float holeRadius = juce::jmax(6.0f, diameter * 0.035f);
	g.setColour(juce::Colour::fromString("FF444444"));
	g.fillEllipse(center.x - holeRadius, center.y - holeRadius, holeRadius * 2, holeRadius * 2);
	g.setColour(juce::Colours::black);
	g.fillEllipse(center.x - holeRadius * 0.5f, center.y - holeRadius * 0.5f, holeRadius, holeRadius);


	// --- 4. トーンアームの追加描画（リッチバージョン） ---

	juce::Point<float> pivotCenter(discArea.getX() - 30.0f, discArea.getY() - 30.0f);
	float pivotRadius = juce::jmax(15.0f, radius * 0.08f);
	float armLength = radius * 1.3f;
	float armWidth = juce::jmax(8.0f, radius * 0.08f);

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
	float cartLength = juce::jmax(20.0f, radius * 0.2f);
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
	float needleSize = juce::jmax(3.0f, radius * 0.02f);
	g.fillEllipse(armEnd.x - needleSize, armEnd.y + cartLength/2 - needleSize * 2, needleSize * 2, needleSize * 2);
	
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

// ─── Mouse interaction (desktop fallback) ────────────────────────────────────

void TurntableComponent::mouseDown(const juce::MouseEvent& e)
{
	isDragging = true;
	lastAngle = getAngleFromPoint(e.position);
	lastTime = juce::Time::getMillisecondCounterHiRes();
	audioEngine.setScratchSpeed(0.0);
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
		double scratchSpeed = (diff / juce::MathConstants<float>::pi) * 16.0;
		scratchSpeed = juce::jlimit(-8.0, 8.0, scratchSpeed);
		audioEngine.setScratchSpeed(scratchSpeed);
	}
	
	lastAngle = currentAngle;
	lastTime = currentTime;
	
	repaint();
}

void TurntableComponent::mouseUp(const juce::MouseEvent& e)
{
	isDragging = false;
	if (audioEngine.isPlaying())
		audioEngine.setScratchSpeed(1.0);
	else
		audioEngine.setScratchSpeed(0.0);
}

// ─── Touch interaction (Issue #14) ───────────────────────────────────────────

void TurntableComponent::touchStarted(const juce::TouchEvent& e)
{
	// Track initial touch positions for multi-touch
	const auto& touches = e.getTouches();
	const auto& newTouch = e.getTouch(e.stack.size() - 1);

	if (primaryTouch.touchIndex < 0)
	{
		// First touch — start single-finger scratch
		primaryTouch.touchIndex = newTouch.getIndex();
		primaryTouch.lastAngle = getAngleFromPoint(newTouch.position);
		primaryTouch.lastTime = juce::Time::getMillisecondCounterHiRes();
		primaryTouch.startPos = newTouch.position;
		primaryTouch.initialAngle = primaryTouch.lastAngle;
		primaryTouch.initialRotation = rotationAngle;
		primaryTouch.isSwipeGesture = false;
		isDragging = true;
		audioEngine.setScratchSpeed(0.0);
	}
	else if (secondaryTouch.touchIndex < 0 && touches.size() >= 2)
	{
		// Second touch — switch to 2-finger scratch
		const auto& secondTouch = newTouch;
		secondaryTouch.touchIndex = secondTouch.getIndex();
		secondaryTouch.lastAngle = getAngleFromPoint(secondTouch.position);
		secondaryTouch.lastTime = juce::Time::getMillisecondCounterHiRes();
		secondaryTouch.startPos = secondTouch.position;

		// Record multi-touch initial state: use the angle between the two touches
		multiTouchInitialRotation = rotationAngle;
		float angle1 = getAngleFromPoint(primaryTouch.startPos);
		float angle2 = getAngleFromPoint(secondTouch.position);
		multiTouchInitialPinchAngle = angle2 - angle1;
		isMultiTouchDragging = true;
	}
}

void TurntableComponent::touchMoved(const juce::TouchEvent& e)
{
	const auto& touches = e.getTouches();

	if (isMultiTouchDragging && touches.size() >= 2)
	{
		// 2-finger scratch: use the average angular movement
		juce::Point<float> p1, p2;
		bool found1 = false, found2 = false;

		for (const auto& t : touches)
		{
			if (t.getIndex() == primaryTouch.touchIndex) { p1 = t.position; found1 = true; }
			if (t.getIndex() == secondaryTouch.touchIndex) { p2 = t.position; found2 = true; }
		}

		if (found1 && found2)
		{
			float angle1 = getAngleFromPoint(p1);
			float angle2 = getAngleFromPoint(p2);
			float currentPinchAngle = angle2 - angle1;

			float pinchDiff = currentPinchAngle - multiTouchInitialPinchAngle;

			// Wrap angle difference
			if (pinchDiff < -juce::MathConstants<float>::pi) pinchDiff += juce::MathConstants<float>::twoPi;
			if (pinchDiff > juce::MathConstants<float>::pi) pinchDiff -= juce::MathConstants<float>::twoPi;

			rotationAngle = multiTouchInitialRotation + pinchDiff;

			// Speed based on angular change rate
			double currentTime = juce::Time::getMillisecondCounterHiRes();
			double timeDiff = currentTime - primaryTouch.lastTime;

			if (timeDiff > 0)
			{
				float angleDiff = angle1 - primaryTouch.lastAngle;
				if (angleDiff < -juce::MathConstants<float>::pi) angleDiff += juce::MathConstants<float>::twoPi;
				if (angleDiff > juce::MathConstants<float>::pi) angleDiff -= juce::MathConstants<float>::twoPi;

				double scratchSpeed = (angleDiff / juce::MathConstants<float>::pi) * 12.0;
				scratchSpeed = juce::jlimit(-6.0, 6.0, scratchSpeed);
				audioEngine.setScratchSpeed(scratchSpeed);
			}

			primaryTouch.lastAngle = angle1;
			primaryTouch.lastTime = currentTime;
		}
	}
	else if (isDragging && primaryTouch.touchIndex >= 0)
	{
		// Single-finger scratch — check for swipe gesture
		const auto& currentTouch = e.getTouch(primaryTouch.touchIndex);
		if (!currentTouch.isValid()) return;

		// Detect swipe vs. rotation
		float dx = currentTouch.position.x - primaryTouch.startPos.x;
		float dy = currentTouch.position.y - primaryTouch.startPos.y;
		float totalDist = std::sqrt(dx * dx + dy * dy);

		if (!primaryTouch.isSwipeGesture && totalDist > swipeThreshold)
		{
			// Check if this looks like a swipe (primarily linear) or rotation (circular)
			float directDist = std::abs(dx) + std::abs(dy);
			float currentAngle = getAngleFromPoint(currentTouch.position);
			float angleDiff = std::abs(currentAngle - primaryTouch.initialAngle);
			if (angleDiff > juce::MathConstants<float>::pi) angleDiff = juce::MathConstants<float>::twoPi - angleDiff;

			// If angle change is small relative to distance → swipe
			if (angleDiff < 0.3f && (std::abs(dx) > swipeThreshold || std::abs(dy) > swipeThreshold))
			{
				primaryTouch.isSwipeGesture = true;
			}
		}

		if (primaryTouch.isSwipeGesture)
		{
			// Swipe → fast scratch based on horizontal velocity
			double currentTime = juce::Time::getMillisecondCounterHiRes();
			double timeDiff = currentTime - primaryTouch.lastTime;

			if (timeDiff > 0)
			{
				float horizSpeed = (currentTouch.position.x - primaryTouch.startPos.x) / (float)(currentTime - primaryTouch.lastTime + 1.0) * 100.0f;
				double scratchSpeed = juce::jlimit(-8.0, 8.0, (double)horizSpeed * 0.5);
				audioEngine.setScratchSpeed(scratchSpeed);
				rotationAngle += (float)(scratchSpeed * timeDiff * 0.001f);
			}
			primaryTouch.lastTime = currentTime;
		}
		else
		{
			// Rotational scratch — same logic as mouseDrag
			float currentAngle = getAngleFromPoint(currentTouch.position);
			float diff = currentAngle - primaryTouch.lastAngle;

			if (diff < -juce::MathConstants<float>::pi) diff += juce::MathConstants<float>::twoPi;
			if (diff > juce::MathConstants<float>::pi) diff -= juce::MathConstants<float>::twoPi;

			rotationAngle += diff;

			double currentTime = juce::Time::getMillisecondCounterHiRes();
			double timeDiff = currentTime - primaryTouch.lastTime;

			if (timeDiff > 0)
			{
				double scratchSpeed = (diff / juce::MathConstants<float>::pi) * 16.0;
				scratchSpeed = juce::jlimit(-8.0, 8.0, scratchSpeed);
				audioEngine.setScratchSpeed(scratchSpeed);
			}

			primaryTouch.lastAngle = currentAngle;
			primaryTouch.lastTime = currentTime;
		}
	}

	repaint();
}

void TurntableComponent::touchEnded(const juce::TouchEvent& e)
{
	const auto& endedTouch = e.getTouch(e.stack.size() - 1);
	const auto& touches = e.getTouches();

	if (endedTouch.getIndex() == secondaryTouch.touchIndex)
	{
		// Secondary finger lifted — fall back to single-finger scratch
		secondaryTouch.touchIndex = -1;
		isMultiTouchDragging = false;

		// Re-sync primary touch state
		if (primaryTouch.touchIndex >= 0 && touches.size() > 0)
		{
			bool foundPrimary = false;
			for (const auto& t : touches)
			{
				if (t.getIndex() == primaryTouch.touchIndex)
				{
					primaryTouch.lastAngle = getAngleFromPoint(t.position);
					primaryTouch.lastTime = juce::Time::getMillisecondCounterHiRes();
					primaryTouch.initialRotation = rotationAngle;
					foundPrimary = true;
					break;
				}
			}
			if (!foundPrimary)
			{
				primaryTouch.touchIndex = -1;
				isDragging = false;
			}
		}
	}
	else if (endedTouch.getIndex() == primaryTouch.touchIndex)
	{
		// Primary finger lifted
		primaryTouch.touchIndex = -1;
		isDragging = false;
		isMultiTouchDragging = false;
		secondaryTouch.touchIndex = -1;

		if (audioEngine.isPlaying())
			audioEngine.setScratchSpeed(1.0);
		else
			audioEngine.setScratchSpeed(0.0);
	}
}

// ─── Timer / Animation ──────────────────────────────────────────────────────

void TurntableComponent::timerCallback()
{
	if (!isDragging && audioEngine.isPlaying())
	{
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
	auto center = area.getCentre();
	return std::atan2(p.y - center.y, p.x - center.x);
}
