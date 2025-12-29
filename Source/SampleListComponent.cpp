/*
 ==============================================================================
 SampleListComponent.cpp
 ==============================================================================
 */
#include "SampleListComponent.h"
#include "Constants.h"

SampleListComponent::SampleListComponent(AudioEngine& engine)
    : audioEngine(engine)
{
    // ListBoxの設定
    listBox.setModel(this);
    listBox.setColour(juce::ListBox::backgroundColourId, Constants::bgDark);
    listBox.setRowHeight(44);
    addAndMakeVisible(listBox);
    
    updateFileList();
}

SampleListComponent::~SampleListComponent()
{
}

void SampleListComponent::paint(juce::Graphics& g)
{
    g.fillAll(Constants::bgDark);
    
    // ヘッダー
    g.setColour(juce::Colour::fromString("FF1E293B"));
    g.fillRect(0, 0, getWidth(), 30);
    
    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    g.drawText("Library", 10, 0, getWidth() - 20, 30, juce::Justification::centredLeft);
}

void SampleListComponent::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop(30); // ヘッダー分
    listBox.setBounds(area);
}

int SampleListComponent::getNumRows()
{
    return static_cast<int>(sampleFiles.size());
}

void SampleListComponent::paintListBoxItem(int /*rowNumber*/, juce::Graphics& /*g*/, int /*width*/, int /*height*/, bool /*rowIsSelected*/)
{
    // カスタムコンポーネントを使うので空
}

juce::Component* SampleListComponent::refreshComponentForRow(int rowNumber, bool isRowSelected, juce::Component* existingComponentToUpdate)
{
    if (rowNumber < 0 || rowNumber >= static_cast<int>(sampleFiles.size()))
    {
        delete existingComponentToUpdate;
        return nullptr;
    }
    
    SampleRowComponent* rowComponent = dynamic_cast<SampleRowComponent*>(existingComponentToUpdate);
    
    if (rowComponent == nullptr)
    {
        rowComponent = new SampleRowComponent(
            [this, rowNumber] { renameFile(rowNumber); },
            [this, rowNumber] { deleteFile(rowNumber); },
            [this, rowNumber] { 
                listBox.selectRow(rowNumber);
                // selectedRowsChangedが呼ばれるので、そこで処理される
            }
        );
    }
    
    rowComponent->setFileName(sampleFiles[static_cast<size_t>(rowNumber)].getFileNameWithoutExtension());
    rowComponent->setSelected(isRowSelected);
    rowComponent->setRowIndex(rowNumber);
    
    return rowComponent;
}

void SampleListComponent::selectedRowsChanged(int lastRowSelected)
{
    selectedRow = lastRowSelected;
    if (lastRowSelected >= 0 && lastRowSelected < static_cast<int>(sampleFiles.size()))
    {
        // ファイルをバッファにロードしてスクラッチ再生可能に
        audioEngine.loadFileToBuffer(sampleFiles[static_cast<size_t>(lastRowSelected)]);
    }
}

void SampleListComponent::updateFileList()
{
    sampleFiles.clear();
    
    auto libraryFolder = audioEngine.getLibraryFolder();
    
    // WAV, AIFF, MP3ファイルを検索
    for (const auto& entry : juce::RangedDirectoryIterator(libraryFolder, false, "*.wav;*.aiff;*.mp3"))
    {
        sampleFiles.push_back(entry.getFile());
    }
    
    // 日付順にソート（新しいものが上）
    std::sort(sampleFiles.begin(), sampleFiles.end(),
        [](const juce::File& a, const juce::File& b) {
            return a.getLastModificationTime() > b.getLastModificationTime();
        });
    
    listBox.updateContent();
}

void SampleListComponent::refreshLibrary()
{
    updateFileList();
}

juce::File SampleListComponent::getSelectedFile() const
{
    // ListBoxの選択状態を直接取得
    int row = listBox.getSelectedRow();
    DBG("getSelectedFile called - ListBox selectedRow: " << row << ", sampleFiles count: " << sampleFiles.size());
    if (row >= 0 && row < static_cast<int>(sampleFiles.size()))
    {
        DBG("Returning file: " << sampleFiles[static_cast<size_t>(row)].getFullPathName());
        return sampleFiles[static_cast<size_t>(row)];
    }
    return juce::File();
}

void SampleListComponent::clearSelection()
{
    listBox.deselectAllRows();
}

void SampleListComponent::renameFile(int row)
{
    if (row < 0 || row >= static_cast<int>(sampleFiles.size()))
        return;
    
    auto file = sampleFiles[static_cast<size_t>(row)];
    auto currentName = file.getFileNameWithoutExtension();
    
    auto* alertWindow = new juce::AlertWindow("Rename", "Enter new name:", juce::MessageBoxIconType::QuestionIcon);
    alertWindow->addTextEditor("name", currentName, "Name:");
    alertWindow->addButton("OK", 1, juce::KeyPress(juce::KeyPress::returnKey));
    alertWindow->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    
    alertWindow->enterModalState(true, juce::ModalCallbackFunction::create(
        [this, alertWindow, file](int result) {
            if (result == 1)
            {
                auto newName = alertWindow->getTextEditorContents("name");
                if (newName.isNotEmpty())
                {
                    auto newFile = file.getParentDirectory().getChildFile(newName + file.getFileExtension());
                    if (file.moveFileTo(newFile))
                    {
                        updateFileList();
                    }
                }
            }
            delete alertWindow;
        }), true);
}

void SampleListComponent::deleteFile(int row)
{
    if (row < 0 || row >= static_cast<int>(sampleFiles.size()))
        return;
    
    auto file = sampleFiles[static_cast<size_t>(row)];
    
    auto options = juce::MessageBoxOptions()
        .withIconType(juce::MessageBoxIconType::WarningIcon)
        .withTitle("Delete")
        .withMessage("Are you sure you want to delete \"" + file.getFileName() + "\"?")
        .withButton("Delete")
        .withButton("Cancel");
    
    juce::AlertWindow::showAsync(options,
        [this, file](int result) {
            if (result == 1) // Delete button
            {
                file.deleteFile();
                updateFileList();
            }
        });
}