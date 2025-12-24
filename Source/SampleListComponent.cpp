/*
 ==============================================================================
 SampleListComponent.cpp
 ==============================================================================
 */
#include "SampleListComponent.h"

SampleListComponent::SampleListComponent(AudioEngine& engine)
    : audioEngine(engine)
{
    // ListBoxの設定
    listBox.setModel(this);
    addAndMakeVisible(listBox);
    
    addAndMakeVisible(recordButton);
    recordButton.addListener(this);
    
    updateFileList();
}

SampleListComponent::~SampleListComponent()
{
    recordButton.removeListener(this);
}

void SampleListComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::darkgrey);
}

void SampleListComponent::resized()
{
    auto area = getLocalBounds();
    recordButton.setBounds(area.removeFromBottom(40).reduced(5));
    listBox.setBounds(area);
}

int SampleListComponent::getNumRows()
{
    return static_cast<int>(sampleFiles.size());
}

void SampleListComponent::paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected)
{
    if (rowIsSelected)
        g.fillAll(juce::Colours::lightblue);
        
    g.setColour(juce::Colours::white);
    if (rowNumber < sampleFiles.size())
        g.drawText(sampleFiles[rowNumber].getFileName(), 5, 0, width, height, juce::Justification::centredLeft, true);
}

void SampleListComponent::selectedRowsChanged(int lastRowSelected)
{
    if (lastRowSelected >= 0 && lastRowSelected < sampleFiles.size())
    {
        audioEngine.loadFile(sampleFiles[lastRowSelected]);
    }
}

void SampleListComponent::buttonClicked(juce::Button* button)
{
    if (button == &recordButton)
    {
        if (audioEngine.isRecording())
        {
            audioEngine.stopRecording();
            recordButton.setButtonText("Record");
            updateFileList();
        }
        else
        {
            audioEngine.startRecording();
            recordButton.setButtonText("Stop");
        }
    }
}

void SampleListComponent::updateFileList()
{
    // ここでフォルダをスキャンして sampleFiles に追加する処理が必要
    // 今回は空にしておくか、ダミーを入れる
    sampleFiles.clear();
    listBox.updateContent();
}