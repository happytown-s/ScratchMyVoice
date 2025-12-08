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
            updateFileList(); // 録音ファイルをリストに追加（要実装）
        }
        else
        {
            // 仮のファイル名で録音開始
            juce::File file = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                              .getChildFile("recording.wav");
            audioEngine.startRecording(file);
            recordButton.setButtonText("Stop");
        }
    }
}

void SampleListComponent::updateFileList()
{
    sampleFiles.clear();

    // ドキュメントフォルダをスキャン
    auto dir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);

    // wavファイルを探す
    if (dir.isDirectory())
    {
        auto files = dir.findChildFiles(juce::File::findFiles, false, "*.wav");
        for (auto& f : files)
        {
            // "ScratchAI_" で始まるファイルだけを対象にするなど、フィルタリングも可能
            if (f.getFileName().startsWith("ScratchAI_"))
                sampleFiles.push_back(f);
        }
    }

    // 並び替え（新しい順）
    std::sort(sampleFiles.begin(), sampleFiles.end(), [](const juce::File& a, const juce::File& b) {
        return a.getLastModificationTime() > b.getLastModificationTime();
    });

    listBox.updateContent();
}