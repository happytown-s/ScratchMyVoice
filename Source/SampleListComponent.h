/*
 ==============================================================================
 SampleListComponent.h
 ==============================================================================
 */
#pragma once
#include <JuceHeader.h>
#include "AudioEngine.h"

// カスタム行コンポーネント
class SampleRowComponent : public juce::Component
{
public:
	SampleRowComponent(std::function<void()> onRename, std::function<void()> onDelete,
	                   std::function<void()> onSelect)
		: renameCallback(onRename), deleteCallback(onDelete), selectCallback(onSelect)
	{
		renameButton.setButtonText(juce::String::fromUTF8("✏️"));
		renameButton.onClick = [this] { if (renameCallback) renameCallback(); };
		renameButton.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
		addAndMakeVisible(renameButton);
		
		deleteButton.setButtonText(juce::String::fromUTF8("🗑️"));
		deleteButton.onClick = [this] { if (deleteCallback) deleteCallback(); };
		deleteButton.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
		addAndMakeVisible(deleteButton);
	}
	
	void setFileName(const juce::String& name) { fileName = name; repaint(); }
	void setSelected(bool selected) { isSelected = selected; repaint(); }
	void setRowIndex(int index) { rowIndex = index; repaint(); }
	
	void paint(juce::Graphics& g) override
	{
		if (isSelected)
			g.fillAll(juce::Colour::fromString("FF3B82F6"));
		else if (rowIndex % 2 == 0)
			g.fillAll(juce::Colour::fromString("FF1E293B"));
		else
			g.fillAll(juce::Colour::fromString("FF0F172A"));
		
		g.setColour(juce::Colours::white);
		g.setFont(juce::FontOptions(13.0f));
		g.drawText(fileName, 8, 0, getWidth() - 90, getHeight(), juce::Justification::centredLeft, true);
	}
	
	void resized() override
	{
		auto area = getLocalBounds();
		deleteButton.setBounds(area.removeFromRight(40).reduced(2));
		renameButton.setBounds(area.removeFromRight(40).reduced(2));
	}
	
	void mouseDown(const juce::MouseEvent&) override
	{
		if (selectCallback) selectCallback();
	}
	
private:
	juce::TextButton renameButton, deleteButton;
	juce::String fileName;
	bool isSelected = false;
	int rowIndex = 0;
	std::function<void()> renameCallback;
	std::function<void()> deleteCallback;
	std::function<void()> selectCallback;
};

// Simple List Model
class SampleListComponent : public juce::Component,
public juce::ListBoxModel
{
	public:
	SampleListComponent(AudioEngine& engine);
	~SampleListComponent() override;

	void paint(juce::Graphics& g) override;
	void resized() override;

	// ListBoxModel
	int getNumRows() override;
	void paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override;
	void selectedRowsChanged(int lastRowSelected) override;
	juce::Component* refreshComponentForRow(int rowNumber, bool isRowSelected, juce::Component* existingComponentToUpdate) override;

	// File Management
	void updateFileList();
	void refreshLibrary(); // 外部から呼び出し用
	juce::File getSelectedFile() const; // 選択中のファイルを取得
	void clearSelection(); // 選択を解除

	private:
	AudioEngine& audioEngine;
	juce::ListBox listBox;

	std::vector<juce::File> sampleFiles;
	int selectedRow = -1;

	// ファイル操作
	void renameFile(int row);
	void deleteFile(int row);

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SampleListComponent)
};
