/*
 ==============================================================================
 SampleSlotComponent.cpp
 ==============================================================================
 Issue #16 — Responsive layout: horizontal on mobile (narrow height),
             vertical on desktop (narrow width).
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
    
    // タイトル — only show when in vertical (desktop) mode
    if (getHeight() > getWidth())
    {
        g.setColour(juce::Colours::white);
        g.setFont(juce::FontOptions(14.0f).withStyle("Bold"));
        g.drawText("SLOTS", getLocalBounds().removeFromTop(25), juce::Justification::centred);
    }
}

void SampleSlotComponent::resized()
{
    auto area = getLocalBounds().reduced(4);

    // Issue #16: Mobile (horizontal / wide) vs Desktop (vertical / narrow)
    bool isHorizontal = (getWidth() > getHeight() * 1.2f);

    if (isHorizontal)
    {
        // ── Horizontal layout for mobile (pill buttons in a row) ────────
        // Use FlexBox for even distribution
        juce::FlexBox flex;
        flex.flexDirection = juce::FlexBox::Direction::row;
        flex.justifyContent = juce::FlexBox::JustifyContent::spaceEvenly;
        flex.alignItems = juce::FlexBox::AlignItems::stretch;

        for (int i = 0; i < NUM_SLOTS; ++i)
        {
            flex.items.add(juce::FlexItem(slotButtons[static_cast<size_t>(i)])
                              .withMinWidth(50)
                              .withFlex(1.0f));
        }

        flex.performLayout(area);
    }
    else
    {
        // ── Vertical layout for desktop (column) ────────────────────────
        area.removeFromTop(25); // タイトルスペース
        int slotHeight = area.getHeight() / NUM_SLOTS;

        for (int i = 0; i < NUM_SLOTS; ++i)
        {
            slotButtons[static_cast<size_t>(i)].setBounds(area.removeFromTop(slotHeight).reduced(2));
        }
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
    bool isHorizontal = (getWidth() > getHeight() * 1.2f);
    
    for (int i = 0; i < NUM_SLOTS; ++i)
    {
        auto& btn = slotButtons[static_cast<size_t>(i)];
        juce::String label;
        
        if (isHorizontal)
        {
            // Short label for horizontal (mobile) mode
            label = juce::String(labels[i]);
            if (audioEngine.isSlotLoaded(i))
                label += " " + audioEngine.getSlotFileName(i).upToFirstChar();
        }
        else
        {
            // Full label for vertical (desktop) mode
            label = juce::String(labels[i]) + ": ";
            if (audioEngine.isSlotLoaded(i))
                label += audioEngine.getSlotFileName(i);
            else
                label += "---";
        }
        
        if (audioEngine.isSlotLoaded(i))
        {
            if (i == activeSlot)
                btn.setColour(juce::TextButton::buttonColourId, activeColor);
            else
                btn.setColour(juce::TextButton::buttonColourId, loadedColor);
        }
        else
        {
            btn.setColour(juce::TextButton::buttonColourId, emptyColor);
        }
        
        btn.setButtonText(label);
    }
}
