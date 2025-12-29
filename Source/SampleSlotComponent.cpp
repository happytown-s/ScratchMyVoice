/*
 ==============================================================================
 SampleSlotComponent.cpp
 ==============================================================================
 */
#include "SampleSlotComponent.h"

SampleSlotComponent::SampleSlotComponent(AudioEngine& engine)
    : audioEngine(engine)
{
    audioEngine.addChangeListener(this);
    
    const char* labels[] = { "A", "B", "C", "D" };
    
    for (int i = 0; i < NUM_SLOTS; ++i)
    {
        slotButtons[static_cast<size_t>(i)].setButtonText(labels[i]);
        slotButtons[static_cast<size_t>(i)].onClick = [this, i] {
            // コールバックがあれば選択ファイルをロード（上書き可）
            // コールバックで何もロードされなかった場合、既存スロットをアクティブ化
            if (onSlotAssign)
            {
                onSlotAssign(i);
            }
            
            // スロットがロード済みならアクティブ化
            if (audioEngine.isSlotLoaded(i))
            {
                audioEngine.setActiveSlot(i);
            }
            updateSlotLabels();
        };
        addAndMakeVisible(slotButtons[static_cast<size_t>(i)]);
    }
    
    updateSlotLabels();
}

SampleSlotComponent::~SampleSlotComponent()
{
    audioEngine.removeChangeListener(this);
}

void SampleSlotComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour::fromString("FF1E293B"));
    
    // タイトル
    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(14.0f).withStyle("Bold"));
    g.drawText("SLOTS", getLocalBounds().removeFromTop(25), juce::Justification::centred);
}

void SampleSlotComponent::resized()
{
    auto area = getLocalBounds().reduced(5);
    area.removeFromTop(25); // タイトルスペース
    
    int slotHeight = area.getHeight() / NUM_SLOTS;
    
    for (int i = 0; i < NUM_SLOTS; ++i)
    {
        slotButtons[static_cast<size_t>(i)].setBounds(area.removeFromTop(slotHeight).reduced(2));
    }
}

void SampleSlotComponent::changeListenerCallback(juce::ChangeBroadcaster*)
{
    updateSlotLabels();
    repaint();
}

void SampleSlotComponent::updateSlotLabels()
{
    const char* labels[] = { "A", "B", "C", "D" };
    const auto activeColor = juce::Colour::fromString("FF22C55E"); // 緑
    const auto loadedColor = juce::Colour::fromString("FF3B82F6"); // 青
    const auto emptyColor = juce::Colour::fromString("FF475569");  // グレー
    
    int activeSlot = audioEngine.getActiveSlot();
    
    for (int i = 0; i < NUM_SLOTS; ++i)
    {
        auto& btn = slotButtons[static_cast<size_t>(i)];
        juce::String label = juce::String(labels[i]) + ": ";
        
        if (audioEngine.isSlotLoaded(i))
        {
            label += audioEngine.getSlotFileName(i);
            
            if (i == activeSlot)
                btn.setColour(juce::TextButton::buttonColourId, activeColor);
            else
                btn.setColour(juce::TextButton::buttonColourId, loadedColor);
        }
        else
        {
            label += "---";
            btn.setColour(juce::TextButton::buttonColourId, emptyColor);
        }
        
        btn.setButtonText(label);
    }
}
