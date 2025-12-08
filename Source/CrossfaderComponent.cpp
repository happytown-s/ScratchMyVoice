/*
 ==============================================================================
 CrossfaderComponent.cpp
 ==============================================================================
 */
#include "CrossfaderComponent.h" // ヘッダーのファイル名と合わせる

CrossfaderComponent::CrossfaderComponent(AudioEngine& engine)
: audioEngine(engine)
{
}

CrossfaderComponent::~CrossfaderComponent()
{
}

void CrossfaderComponent::paint(juce::Graphics& g)
{
	g.fillAll(juce::Colours::grey);
	g.setColour(juce::Colours::white);
	g.drawText("Crossfader", getLocalBounds(), juce::Justification::centred, true);
}

void CrossfaderComponent::resized()
{
}

void CrossfaderComponent::mouseDown(const juce::MouseEvent& e)
{
	isPressed = true;
}

void CrossfaderComponent::mouseUp(const juce::MouseEvent& e)
{
	isPressed = false;
}
