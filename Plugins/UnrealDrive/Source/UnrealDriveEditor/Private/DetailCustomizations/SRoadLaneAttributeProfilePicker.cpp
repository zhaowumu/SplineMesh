/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "SRoadLaneAttributeProfilePicker.h"
#include "DetailLayoutBuilder.h"
#include "Editor.h"
#include "IPropertyUtilities.h"
#include "PropertyCustomizationHelpers.h"
#include "Modules/ModuleManager.h"
#include "PropertyHandle.h"
#include "ScopedTransaction.h"
#include "StructUtils/InstancedStruct.h"
#include "StructUtils/UserDefinedStruct.h"
#include "Styling/SlateIconFinder.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Layout/SBox.h"
#include "UnrealDriveEditorModule.h"
#include "UnrealDrivePreset.h"
#include "RoadLaneAttributeEntries.h"

#define LOCTEXT_NAMESPACE "SRoadLaneAttributeProfilePicker"


void SRoadLaneAttributeProfilePicker::Construct(const FArguments& InArgs, TSharedPtr<IPropertyHandle> RoadLaneAttributeProfileProperty, TSharedPtr<IPropertyUtilities> InPropertyUtils)
{
	//OnStructPicked = InArgs._OnStructPicked;
	PropUtils = MoveTemp(InPropertyUtils);
	if (!RoadLaneAttributeProfileProperty.IsValid() || !PropUtils.IsValid())
	{
		return;
	}

	AttributeValueProperty = RoadLaneAttributeProfileProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FRoadLaneAttributeProfile, AttributeValueTemplate));
	AttributeNameProperty = RoadLaneAttributeProfileProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FRoadLaneAttributeProfile, AttributeName));
	if (!AttributeValueProperty.IsValid() || !AttributeNameProperty.IsValid())
	{
		return;
	}


	FName AttributeName;
	AttributeNameProperty->GetValue(AttributeName);

	if (AttributeName == NAME_None)
	{
		ComboBoxContent.Lable = LOCTEXT("AttributeEmpty_Lable", "Empty");
		ComboBoxContent.Tooltip = LOCTEXT("AttributeEmpty_ToolTip", "Attribute isn't set");
		ComboBoxContent.Icon = FSlateIcon();
	}
	else
	{
		auto* EntryStruct = FUnrealDriveEditorModule::Get().ForEachRoadLaneAttributEntries([AttributeNameA = AttributeName](FName AttributeNameB, const TInstancedStruct<FRoadLaneAttributeEntry>* Value)
		{
			return AttributeNameA == AttributeNameB;
		});
		if (EntryStruct)
		{
			SetComboBoxContent(AttributeName, EntryStruct->Get<FRoadLaneAttributeEntry>());
		}
		else
		{
			ComboBoxContent.Lable = FText::Format(LOCTEXT("AttributeNotFound_Lable", "({0})"), FText::FromName(AttributeName));
			ComboBoxContent.Tooltip = FText::Format(LOCTEXT("AttributeNotFound_ToolTip", "Attribute \"{0}\" not found"), FText::FromName(AttributeName));
			ComboBoxContent.Icon = FSlateIcon();
		}
	}

	ChildSlot
	[
		SAssignNew(ComboButton, SComboButton)
		.OnGetMenuContent(this, &SRoadLaneAttributeProfilePicker::GenerateStructPicker)
		.ContentPadding(0)
		.IsEnabled(AttributeValueProperty->IsEditable())
		.ButtonContent()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.0f, 0.0f, 4.0f, 0.0f)
			[
				SNew(SImage)
				.Image_Lambda([this]()
				{ 
					return ComboBoxContent.Icon.GetIcon();
				})
			]
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text_Lambda([this]() 
				{ 
					return ComboBoxContent.Lable; 
				})
				.ToolTipText_Lambda([this]() { return ComboBoxContent.Tooltip; })
				.Font(IDetailLayoutBuilder::GetDetailFont())
			]
		]
	];
}


TSharedRef<SWidget> SRoadLaneAttributeProfilePicker::GenerateStructPicker()
{
	FMenuBuilder MenuBuilder(true, nullptr, TSharedPtr<FExtender>(), false, &FAppStyle::Get());
	MenuBuilder.BeginSection(NAME_None);
	FUnrealDriveEditorModule::Get().ForEachRoadLaneAttributEntries([&MenuBuilder, this](FName AttributeName, const TInstancedStruct<FRoadLaneAttributeEntry>* Value)
	{
		auto& Entry = Value->Get<FRoadLaneAttributeEntry>();
		MenuBuilder.AddMenuEntry(
			!Entry.LabelOverride.IsEmpty() ? Entry.LabelOverride : FText::FromName(AttributeName),
			Entry.ToolTip.IsEmpty() && Entry.AttributeValueTemplate.GetScriptStruct() ? Entry.AttributeValueTemplate.GetScriptStruct()->GetToolTipText() : Entry.ToolTip,
			Entry.GetIcon(),
			FUIAction( FExecuteAction::CreateLambda([this, AttributeName, &Entry]() { StructPicked(AttributeName, Entry); })),
			NAME_None,
			EUserInterfaceActionType::Button
		);
		return false;
	});
	MenuBuilder.EndSection();

	return SNew(SBox)
		.WidthOverride(280)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.MaxHeight(500)
			[
				MenuBuilder.MakeWidget()
			]
		];
}

void SRoadLaneAttributeProfilePicker::StructPicked(FName AttributeName, const FRoadLaneAttributeEntry& Entry)
{
	if (AttributeValueProperty && AttributeValueProperty->IsValidHandle() && AttributeNameProperty && AttributeNameProperty->IsValidHandle())
	{
		FScopedTransaction Transaction(LOCTEXT("OnStructPicked", "Set Struct"));

		AttributeValueProperty->NotifyPreChange();
		AttributeValueProperty->EnumerateRawData([&Entry](void* RawData, const int32 /*DataIndex*/, const int32 /*NumDatas*/)
		{
			if (auto* InstancedStruct = static_cast<TInstancedStruct<FRoadLaneAttributeValue>*>(RawData))
			{
				*InstancedStruct = Entry.AttributeValueTemplate;
			}
			return true;
		});

		AttributeNameProperty->SetValue(AttributeName);

		SetComboBoxContent(AttributeName, Entry);

		ComboBoxContent.Lable = !Entry.LabelOverride.IsEmpty() ? Entry.LabelOverride : FText::FromName(AttributeName);
		ComboBoxContent.Tooltip = Entry.ToolTip.IsEmpty() && Entry.AttributeValueTemplate.GetScriptStruct() ? Entry.AttributeValueTemplate.GetScriptStruct()->GetToolTipText() : Entry.ToolTip;
		ComboBoxContent.Icon = Entry.GetIcon();

		// Property tree will be invalid after changing the struct type, force update.
		if (PropUtils.IsValid())
		{
			PropUtils->ForceRefresh();
		}
	}

	ComboButton->SetIsOpen(false);
	//OnStructPicked.ExecuteIfBound(InStruct);
}

void SRoadLaneAttributeProfilePicker::SetComboBoxContent(FName AttributeName, const FRoadLaneAttributeEntry& Entry)
{
	ComboBoxContent.Lable = !Entry.LabelOverride.IsEmpty() ? Entry.LabelOverride : FText::FromName(AttributeName);
	ComboBoxContent.Tooltip = Entry.ToolTip.IsEmpty() && Entry.AttributeValueTemplate.GetScriptStruct() ? Entry.AttributeValueTemplate.GetScriptStruct()->GetToolTipText() : Entry.ToolTip;
	ComboBoxContent.Icon = Entry.GetIcon();
}

#undef LOCTEXT_NAMESPACE
