/*
 ==============================================================================
 SampleSlotComponent.h
 ==============================================================================
 */
#pragma once
#include <JuceHeader.h>
#include "AudioEngine.h"

class SampleSlotComponent : public juce::Component,
                            public juce::ChangeListener
{
public:
    SampleSlotComponent(AudioEngine& engine);
    ~SampleSlotComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;
    
    // スロット割り当てコールバックを設定
    void setSlotAssignCallback(std::function<void(int)> callback) { onSlotAssign = callback; }

private:
    AudioEngine& audioEngine;
    
    static constexpr int NUM_SLOTS = 4;
    std::array<juce::TextButton, NUM_SLOTS> slotButtons;
    std::function<void(int)> onSlotAssign;
    
    void updateSlotLabels();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SampleSlotComponent)
};

