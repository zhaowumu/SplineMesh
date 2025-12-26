/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "DrawRoadToolPropertiesDetails.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "IDetailChildrenBuilder.h"
#include "DetailWidgetRow.h"
#include "ModelingTools/DrawRoadTool.h"

#define LOCTEXT_NAMESPACE "DrawRoadToolPropertiesDetails"

DECLARE_DELEGATE_OneParam(FOnRoadProfilePickedDelegate, const FRoadLaneSectionProfile&);

class SDrawProfilePicker : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SDrawProfilePicker) {}
		SLATE_EVENT(FOnRoadProfilePickedDelegate, OnRoadProfilePicked)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const FString& InitProfileName)
	{
		OnPickedDelegate = InArgs._OnRoadProfilePicked;

		RoadProfiles.Add(TEXT(""), FRoadLaneSectionProfile::EmptyProfile);
		SetComboBoxContent(&FRoadLaneSectionProfile::EmptyProfile);

		UUnrealDrivePresetBase::ForEachPreset<UUnrealDrivePreset>([this, &InitProfileName](const UUnrealDrivePreset* Preset)
		{
			for (auto& It : Preset->RoadLanesProfiles)
			{
				auto ProfileName = It.GetFullName();
				RoadProfiles.Add({ ProfileName, It });
				if (ProfileName == InitProfileName)
				{
					SetComboBoxContent(&It);
				}
			}
		});
		
		ChildSlot
		[
			SAssignNew(ComboButton, SComboButton)
			.OnGetMenuContent(this, &SDrawProfilePicker::GenerateProfilePicker)
			.ContentPadding(0)
			.ButtonContent()
			[
				SNew(SHorizontalBox)
				//+ SHorizontalBox::Slot()
				//.AutoWidth()
				//.VAlign(VAlign_Center)
				//.Padding(0.0f, 0.0f, 4.0f, 0.0f)
				//[
				//	SNew(SImage)
				//	.Image_Lambda([this]()
				//	{ 
				//		return ComboBoxContent.Icon.GetIcon();
				//	})
				//]
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

	void SetProfile(const FString& ProfileName)
	{
		if (ProfileName.IsEmpty() || ProfileName == "None" || ProfileName == "Empty")
		{
			SetComboBoxContent(&FRoadLaneSectionProfile::EmptyProfile);
		}
		else
		{
			UUnrealDrivePresetBase::ForEachPreset<UUnrealDrivePreset>([this, &ProfileName](const UUnrealDrivePreset* Preset)
			{
				for (auto& It : Preset->RoadLanesProfiles)
				{
					if (It.GetFullName() == ProfileName)
					{
						SetComboBoxContent(&It);
					}
				}
			});
		}
	}

private:
	TMap<FString, FRoadLaneSectionProfile> RoadProfiles;

	FOnRoadProfilePickedDelegate OnPickedDelegate;
	TSharedPtr<SComboButton> ComboButton;

	struct FComboBoxContent
	{
		FText Lable;
		FText Tooltip;
		//FSlateIcon Icon;
	};
	FComboBoxContent ComboBoxContent{};

	TSharedRef<SWidget> GenerateProfilePicker()
	{
		TMap<FString, TArray<const FRoadLaneSectionProfile*>> ProfilePerCategories;
		for (auto& Profile : RoadProfiles)
		{
			ProfilePerCategories.FindOrAdd(Profile.Value.Category).Add(&Profile.Value);
		}

		TArray<FString> CategoriesSorted;
		ProfilePerCategories.GetKeys(CategoriesSorted);
		Algo::Sort(CategoriesSorted);

		FMenuBuilder MenuBuilder(true, nullptr, TSharedPtr<FExtender>(), false, &FAppStyle::Get());
		for (auto& Category : CategoriesSorted)
		{
			MenuBuilder.BeginSection(NAME_None, FText::FromString(Category));
			for (auto& Profile : ProfilePerCategories[Category])
			{
				static const FSlateIcon DummyIcon(NAME_None, NAME_None);
				MenuBuilder.AddMenuEntry(
					FText::FromString(Profile->ProfileName),
					FText::FromString(Profile->Tooltip),
					DummyIcon,
					FUIAction(FExecuteAction::CreateLambda([this, Profile]() { OnPicked(Profile); })),
					NAME_None,
					EUserInterfaceActionType::Button
				);
			}
			MenuBuilder.EndSection();
		}

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

	void OnPicked(const FRoadLaneSectionProfile* Profile)
	{
		SetComboBoxContent(Profile);
		OnPickedDelegate.ExecuteIfBound(*Profile);

	}

	void SetComboBoxContent(const FRoadLaneSectionProfile* Profile)
	{
		check(Profile);
		ComboBoxContent.Lable = FText::FromString(Profile->ProfileName);
		ComboBoxContent.Tooltip = FText::FromString(Profile->Tooltip);
	}
};

// -------------------------------------------------------------------------------------------------------------

TSharedRef<IPropertyTypeCustomization> FDrawRoadToolPropertiesDetails::MakeInstance()
{
	return MakeShareable(new FDrawRoadToolPropertiesDetails);
}

void FDrawRoadToolPropertiesDetails::CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	auto ProfileNamePropertyHandler = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FRoadDrawProfilePicker, ProfileName));

	FString SelectedProfileName;
	ProfileNamePropertyHandler->GetValue(SelectedProfileName);

	HeaderRow
		.NameContent()
		[
			//SNew(STextBlock)
			//.Font(IDetailLayoutBuilder::GetDetailFontBold())
			//.Text(LOCTEXT("DrawProfile_Name", "Draw Profile"))
			//.ToolTipText(LOCTEXT("DrawProfile_TipText", "Draw profile name"))
			StructPropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		[
			SAssignNew(ProfilePicker, SDrawProfilePicker, SelectedProfileName)
			.OnRoadProfilePicked_Lambda([ProfileNamePropertyHandler](const FRoadLaneSectionProfile& Profile)
			{
				ProfileNamePropertyHandler->SetValue(Profile.GetFullName());
			})
		];

	StructPropertyHandle->SetOnPropertyResetToDefault(FSimpleDelegate::CreateLambda([this, ProfileNamePropertyHandler = ProfileNamePropertyHandler]()
	{
		if(ProfilePicker)
		{
			FName Value;
			if (ProfileNamePropertyHandler->GetValue(Value) == FPropertyAccess::Success)
			{
				ProfilePicker->SetProfile(Value.ToString());
			}
		}
	}));
}

void FDrawRoadToolPropertiesDetails::CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
}

#undef LOCTEXT_NAMESPACE
