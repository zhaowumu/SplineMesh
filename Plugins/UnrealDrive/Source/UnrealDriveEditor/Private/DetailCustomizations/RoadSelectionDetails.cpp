/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "RoadSelectionDetails.h"
#include "RoadEditorCommands.h"
#include "UnrealDriveEditorModule.h"
#include "IDetailGroup.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "RichCurveEditorModel.h"
#include "CurveEditorCommands.h"
#include "Misc/Optional.h"
#include "DefaultRoadLaneAttributes.h"
#include "CurveKeyDetails.h"
#include "Utils/PropertyEditorUtils.h"
#include "Utils/CurveUtils.h"
#include "IStructureDataProvider.h"

#define LOCTEXT_NAMESPACE "FRoadSelectionDetails"

namespace UnrealDrive
{

class FInstancedStructProvider : public IStructureDataProvider
{
public:
	FInstancedStructProvider() = default;
	
	explicit FInstancedStructProvider(const TSharedPtr<IPropertyHandle>& InStructProperty)
		: StructProperty(InStructProperty)
	{
	}
	
	virtual ~FInstancedStructProvider() override {}

	void Reset()
	{
		StructProperty = nullptr;
	}
	
	virtual bool IsValid() const override
	{
		if (!StructProperty.IsValid() || !StructProperty->IsValidHandle())
		{
			return false;
		}

		bool bHasValidData = false;
		EnumerateInstances([&bHasValidData](const UScriptStruct* ScriptStruct, uint8* Memory, UPackage* Package)
		{
			if (ScriptStruct && Memory)
			{
				bHasValidData = true;
				return false; // Stop
			}
			return true; // Continue
		});

		return bHasValidData;
	}
	
	virtual const UStruct* GetBaseStructure() const override
	{
		// Taken from UClass::FindCommonBase
		auto FindCommonBaseStruct = [](const UScriptStruct* StructA, const UScriptStruct* StructB)
		{
			const UScriptStruct* CommonBaseStruct = StructA;
			while (CommonBaseStruct && StructB && !StructB->IsChildOf(CommonBaseStruct))
			{
				CommonBaseStruct = Cast<UScriptStruct>(CommonBaseStruct->GetSuperStruct());
			}
			return CommonBaseStruct;
		};

		const UScriptStruct* CommonStruct = nullptr;
		EnumerateInstances([&CommonStruct, &FindCommonBaseStruct](const UScriptStruct* ScriptStruct, uint8* Memory, UPackage* Package)
		{
			if (ScriptStruct)
			{
				CommonStruct = FindCommonBaseStruct(ScriptStruct, CommonStruct);
			}
			return true; // Continue
		});

		return CommonStruct;
	}

	virtual void GetInstances(TArray<TSharedPtr<FStructOnScope>>& OutInstances, const UStruct* ExpectedBaseStructure) const override
	{
		// The returned instances need to be compatible with base structure.
		// This function returns empty instances in case they are not compatible, with the idea that we have as many instances as we have outer objects.
		EnumerateInstances([&OutInstances, ExpectedBaseStructure](const UScriptStruct* ScriptStruct, uint8* Memory, UPackage* Package)
		{
			TSharedPtr<FStructOnScope> Result;
			
			if (ExpectedBaseStructure && ScriptStruct && ScriptStruct->IsChildOf(ExpectedBaseStructure))
			{
				Result = MakeShared<FStructOnScope>(ScriptStruct, Memory);
				Result->SetPackage(Package);
			}

			OutInstances.Add(Result);

			return true; // Continue
		});
	}
	
protected:

	void EnumerateInstances(TFunctionRef<bool(const UScriptStruct* ScriptStruct, uint8* Memory, UPackage* Package)> InFunc) const
	{
		if (!StructProperty.IsValid() || !StructProperty->IsValidHandle())
		{
			return;
		}
		
		TArray<UPackage*> Packages;
		StructProperty->GetOuterPackages(Packages);

		StructProperty->EnumerateRawData([&InFunc, &Packages](void* RawData, const int32 DataIndex, const int32 /*NumDatas*/)
		{
			const UScriptStruct* ScriptStruct = nullptr;
			uint8* Memory = nullptr;
			UPackage* Package = nullptr;
			if (FInstancedStruct* InstancedStruct = static_cast<FInstancedStruct*>(RawData))
			{
				ScriptStruct = InstancedStruct->GetScriptStruct();
				Memory = InstancedStruct->GetMutableMemory();
				if (ensureMsgf(Packages.IsValidIndex(DataIndex), TEXT("Expecting packges and raw data to match.")))
				{
					Package = Packages[DataIndex];
				}
			}

			return InFunc(ScriptStruct, Memory, Package);
		});
	}
	
	TSharedPtr<IPropertyHandle> StructProperty;
};

} // UnrealDrive

//-----------------------------------------------------------------------------------------------------------------------------

class SRoadLaneLanePicker : public SCompoundWidget
{
public:

	struct FItem
	{
		TObjectPtr<const UScriptStruct> Struct;
		FText Caption;
		FText ToolTip;
	};

	typedef typename SComboBox<TSharedPtr<FItem>>::FOnSelectionChanged FOnSelectionChanged;

	SLATE_BEGIN_ARGS(SRoadLaneLanePicker) {}
		SLATE_EVENT(FOnSelectionChanged, OnSelectionChanged)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		for (TObjectIterator<UScriptStruct> StructIt; StructIt; ++StructIt)
		{
			if (StructIt->IsChildOf(FRoadLaneInstance::StaticStruct()))
			{
				FString Caption = StructIt->GetName();
				FText ToolTip = StructIt->GetToolTipText();

				Caption.RemoveFromStart(TEXT("RoadLaneInstance"), ESearchCase::CaseSensitive);
				Caption.RemoveFromStart(TEXT("RoadLane"), ESearchCase::CaseSensitive);

				if (Caption.IsEmpty())
				{
					Caption = TEXT("None");
					ToolTip = FText::FromString(TEXT("None"));
				}

				LaneTypesComboList.Add(MakeShared<FItem>(
					*StructIt,
					FText::FromString(Caption),
					ToolTip
				));
			}
		}

		ChildSlot
		[
			SAssignNew(LaneTypesComboBox, SComboBox<TSharedPtr<FItem>>)
			.OptionsSource(&LaneTypesComboList)
			.ItemStyle(&FAppStyle::Get().GetWidgetStyle<FTableRowStyle>(TEXT("TableView.Row")))
			.OnGenerateWidget_Lambda([](TSharedPtr<SRoadLaneLanePicker::FItem> Item)
			{
				return SNew(STextBlock)
					.Text(Item->Caption)
					.ToolTipText(Item->ToolTip)
					.Font(IDetailLayoutBuilder::GetDetailFont());
			})
			.OnSelectionChanged(InArgs._OnSelectionChanged)
			[
				SNew(STextBlock)
				.Text_Lambda([this](){
					auto SelectedType = LaneTypesComboBox->GetSelectedItem();
					return (SelectedType.IsValid())
						? SelectedType->Caption
						: FText::GetEmpty();
				})
				.ToolTipText_Lambda([this]() {
					auto SelectedType = LaneTypesComboBox->GetSelectedItem();
					return (SelectedType.IsValid())
						? SelectedType->ToolTip
						: FText::GetEmpty();
				})
				.Font(IDetailLayoutBuilder::GetDetailFont())
			]
		];
	}

	void SetSelectedItem(const UScriptStruct* Item)
	{
		if (auto* Found = LaneTypesComboList.FindByPredicate([Item](const TSharedPtr<FItem>& It) { return It->Struct == Item; }))
		{
			LaneTypesComboBox->SetSelectedItem(*Found);
		}
		else
		{
			LaneTypesComboBox->ClearSelection();
		}
	}

private:
	TSharedPtr<SComboBox<TSharedPtr<FItem>>> LaneTypesComboBox;
	TArray<TSharedPtr<FItem>> LaneTypesComboList;

};

//-----------------------------------------------------------------------------------------------------------------------------


FRoadSelectionDetails::FRoadSelectionDetails(URoadSplineComponent* InOwningComponent, URoadSectionComponentVisualizerSelectionState * SelectionState,  IDetailLayoutBuilder& DetailBuilder)
	: RoadSplineComp(nullptr)
	, SelectionState(SelectionState)
{
	check(InOwningComponent);
	check(SelectionState);

	if (InOwningComponent->IsTemplate())
	{
		// For blueprints, SplineComp will be set to the preview actor in UpdateValues().
		RoadSplineComp = nullptr;
		RoadSplineCompArchetype = InOwningComponent;
	}
	else
	{
		RoadSplineComp = InOwningComponent;
		RoadSplineCompArchetype = nullptr;
	}

	SectionsProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(URoadSplineComponent, RoadLayout))->GetChildHandle(GET_MEMBER_NAME_CHECKED(FRoadLayout, Sections));
	check(SectionsProperty);


	
}

void FRoadSelectionDetails::SetOnRebuildChildren(FSimpleDelegate InOnRegenerateChildren)
{
	OnRegenerateChildren = InOnRegenerateChildren;
}

void FRoadSelectionDetails::GenerateHeaderRowContent(FDetailWidgetRow& NodeRow)
{
}

void FRoadSelectionDetails::GenerateChildContent(IDetailChildrenBuilder& ChildrenBuilder)
{
	auto& Commands = FRoadEditorCommands::Get();

	const ERoadSectionSelectionState State = IsValid(SelectionState) ? SelectionState->GetStateVerified() : ERoadSectionSelectionState::None;
	ERoadSelectionMode EditorMode = FUnrealDriveEditorModule::Get().GetRoadSelectionMode();

	if (State >= ERoadSectionSelectionState::Section && SelectionState->GetSelectedSpline() == RoadSplineComp)
	{

		const int SectionIndex = SelectionState->GetSelectedSectionIndex();
		const int LaneIndex = SelectionState->GetSelectedLaneIndex();

		TSharedPtr<IPropertyHandle> SectionPropertyHandle = SectionsProperty->AsArray()->GetElement(SectionIndex);
		TSharedPtr<IPropertyHandle> LanePropertyHandle;

		if (State >= ERoadSectionSelectionState::Lane)
		{
			if (LaneIndex > 0)
			{
				LanePropertyHandle = SectionPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FRoadLaneSection, Right))->AsArray()->GetElement(LaneIndex - 1);
			}
			else if (LaneIndex < 0)
			{
				LanePropertyHandle = SectionPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FRoadLaneSection, Left))->AsArray()->GetElement(-LaneIndex - 1);
			}
		}

		if (State == ERoadSectionSelectionState::Section || (State == ERoadSectionSelectionState::Lane && LaneIndex == 0))
		{
			PropertyEditorUtils::AddTextRow(
				ChildrenBuilder,
				LOCTEXT("SelectedSection_Search", "Selected Section"),
				LOCTEXT("SelectedSection_Name", "Selected Section"),
				FText::Format(LOCTEXT("SelectedSection_Value", "< {0} >"), SectionIndex));
			ChildrenBuilder.AddProperty(SectionPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FRoadLaneSection, Side)).ToSharedRef());
			ChildrenBuilder.AddProperty(SectionPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FRoadLaneSection, Attributes)).ToSharedRef());
			ChildrenBuilder.AddProperty(SectionPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FRoadLaneSection, SOffset)).ToSharedRef());
			ChildrenBuilder.AddProperty(SectionPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FRoadLaneSection, SOffsetEnd_Cashed)).ToSharedRef());

		}
		else if (State == ERoadSectionSelectionState::Lane)
		{
			if (LanePropertyHandle)
			{
				PropertyEditorUtils::AddTextRow(
					ChildrenBuilder,
					LOCTEXT("SelectedLane_Search", "Selected Lane"),
					LOCTEXT("SelectedLane_Name", "Selected Lane"),
					FText::Format(LOCTEXT("SelectedLane_Value", "< {0} >"), LaneIndex)
				);
				ChildrenBuilder.AddCustomRow(LOCTEXT("SelectedLane_Instance_Search", "Lane Instance"))
					.NameContent()
					[
						SNew(SBox)
						.Content()
						[
							SNew(STextBlock)
							.Text(LOCTEXT("LaneInstance_Caption", "Lane Instance"))
							.ToolTipText(LOCTEXT("LaneInstance_ToolTip", "Data that fits into the lane"))
							.Font(IDetailLayoutBuilder::GetDetailFont())
						]
					]
					.ValueContent()
					//.MinDesiredWidth(166.0f)
					[
						SAssignNew(RoadLaneLanePicker, SRoadLaneLanePicker)
						.OnSelectionChanged_Lambda([this](TSharedPtr<SRoadLaneLanePicker::FItem> Selection, ESelectInfo::Type SelectionType)
						{
							if (Selection && SelectionState->GetStateVerified() >= ERoadSectionSelectionState::Lane)
							{
								int SectionIndex = SelectionState->GetSelectedSectionIndex();
								int LaneIndex = SelectionState->GetSelectedLaneIndex();
								check(LaneIndex != 0);
								auto& Lane = RoadSplineComp->GetLaneSection(SectionIndex).GetLaneByIndex(LaneIndex);

								if (Selection->Struct != Lane.LaneInstance.GetScriptStruct())
								{
									Lane.LaneInstance.InitializeAsScriptStruct(Selection->Struct);
									RoadSplineComp->UpdateRoadLayout();
									RoadSplineComp->MarkRenderStateDirty();
								}
							}
						})
					];

				ChildrenBuilder.AddProperty(LanePropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FRoadLane, Direction)).ToSharedRef());
				ChildrenBuilder.AddProperty(LanePropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FRoadLane, bSkipProcrdureGeneration)).ToSharedRef());
				ChildrenBuilder.AddProperty(LanePropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FRoadLane, Attributes)).ToSharedRef());
				ChildrenBuilder.AddProperty(LanePropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FRoadLane, SOffsetEnd_Cashed)).ToSharedRef());

				const FRoadLane& SelectedLane = RoadSplineComp->GetLaneSection(SectionIndex).GetLaneByIndex(LaneIndex);
				const UScriptStruct* SelectedRoadLaneClass = SelectedLane.LaneInstance.GetScriptStruct();

				if (SelectedRoadLaneClass)
				{
					RoadLaneLanePicker->SetSelectedItem(SelectedRoadLaneClass);
				}

				
				//ChildrenBuilder.AddProperty(LanePropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FRoadLane, LaneCustomGenerators)).ToSharedRef());

				/*
				if (SelectedLane.Width.Num())
				{
					TSharedPtr<IPropertyHandle> WidthPropertyHandle = LanePropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FRoadLane, Width))
						->AsArray()
						->GetElement(0)
						->GetChildHandle(GET_MEMBER_NAME_CHECKED(FRoadLaneWidth, P0));
					//WidthPropertyHandle->SetPropertyDisplayName(FText::FromString("Start Width"));
					ChildrenBuilder.AddProperty(WidthPropertyHandle.ToSharedRef());
				}
				*/

				if (SelectedRoadLaneClass)
				{
					for (TFieldIterator<FProperty> PropertyIt(SelectedRoadLaneClass); PropertyIt; ++PropertyIt)
					{
						if (PropertyIt->HasAnyPropertyFlags(CPF_Edit) && PropertyIt->GetFName() != GET_MEMBER_NAME_CHECKED(FRoadLane, Width))
						{
							auto Prop = LanePropertyHandle->GetChildHandle(PropertyIt->GetFName());
							if (Prop)
							{
								ChildrenBuilder.AddProperty(Prop.ToSharedRef());
							}
						}
					}
				}
			}
		}
		else if (State == ERoadSectionSelectionState::Key || State == ERoadSectionSelectionState::KeyTangent)
		{
			const int AttributeIndex = SelectionState->GetSelectedKeyIndex();
			if (EditorMode == ERoadSelectionMode::Width)
			{
				auto WidthPropertyHandle = LanePropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FRoadLane, Width));
				check(WidthPropertyHandle);

				TSharedPtr<IPropertyHandle> WidthKeyPropertyHandle = WidthPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FRichCurve, Keys))->AsArray()->GetElement(AttributeIndex);
				if (WidthKeyPropertyHandle)
				{
					PropertyEditorUtils::AddTextRow(
						ChildrenBuilder,
						LOCTEXT("SelectedWidthKey_Search", "Selected Width Key"),
						LOCTEXT("SelectedWidthKey_Name", "Selected Width Key"),
						FText::Format(LOCTEXT("SelectedWidthKey_Value", "< {0} >"), AttributeIndex)
					);
						
					FRichCurve* RichCurve = PropertyEditorUtils::GetFirstData<FRichCurve>(WidthPropertyHandle);
					FRichCurveKey* CurveKey = PropertyEditorUtils::GetFirstData<FRichCurveKey>(WidthKeyPropertyHandle);
					if (ensure(RichCurve && CurveKey))
					{
						FKeyHandle KeyHandle = CurveUtils::GetKeyHandle(*RichCurve, AttributeIndex);
						check(KeyHandle != FKeyHandle::Invalid());
						auto CurveKeyDetails = MakeShared<UnrealDrive::FCurveKeyDetails>(WidthPropertyHandle.ToSharedRef(), KeyHandle, RoadSplineComp);
						CurveKeyDetails->OnTangenModeChanged.BindLambda([this]()
						{
							if(IsValid(RoadSplineComp))
							{
								RoadSplineComp->UpdateMagicTransform();
								RoadSplineComp->MarkRenderStateDirty();
								GEditor->RedrawLevelEditingViewports(true);
							}
						});
						ChildrenBuilder.AddCustomBuilder(CurveKeyDetails);
					}

					auto TimeProperty = WidthKeyPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FRichCurveKey, Time));
					TimeProperty->SetPropertyDisplayName(LOCTEXT("SelectedWidthKey_TimeLabel", "SOffset"));
					TimeProperty->SetToolTipText(LOCTEXT("SelectedWidthKey_TimeTipText", "Spline offset [cm] starts from current road section"));

					ChildrenBuilder.AddProperty(TimeProperty.ToSharedRef());
					ChildrenBuilder.AddProperty(WidthKeyPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FRichCurveKey, Value)).ToSharedRef());
					//ChildrenBuilder.AddProperty(WidthKeyPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FRichCurveKey, InterpMode)).ToSharedRef());
					//ChildrenBuilder.AddProperty(WidthKeyPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FRichCurveKey, TangentMode)).ToSharedRef());
					//ChildrenBuilder.AddProperty(WidthKeyPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FRichCurveKey, TangentWeightMode)).ToSharedRef());

					/*
					if (ensure(CurveKey && CurveKey))
					{
						AddTextRow(ChildrenBuilder,
							LOCTEXT("SelectedWidthKey_ArriveTangent_Search", "Arrive Tangent"),
							LOCTEXT("SelectedWidthKey_ArriveTangent_Name", "Arrive Tangent"),
							[CurveKey]() { return FText::AsNumber(CurveKey->ArriveTangent); }
						);
						AddTextRow(ChildrenBuilder,
							LOCTEXT("SelectedWidthKey_LeaveTangent_Search", "Leave Tangent"),
							LOCTEXT("SelectedWidthKey_LeaveTangent_Name", "Leave Tangent"),
							[CurveKey]() { return FText::AsNumber(CurveKey->LeaveTangent); }
						);
						AddTextRow(ChildrenBuilder,
							LOCTEXT("SelectedWidthKey_ArriveTangentWeight_Search", "Arrive Tangent Weight"),
							LOCTEXT("SelectedWidthKey_ArriveTangentWeight_Name", "Arrive Tangent Weight"),
							[CurveKey]() { return FText::AsNumber(CurveKey->ArriveTangentWeight); }
						);
						AddTextRow(ChildrenBuilder,
							LOCTEXT("SelectedWidthKey_LeaveTangentWeight_Search", "Leave Tangent Weight"),
							LOCTEXT("SelectedWidthKey_LeaveTangentWeight_Name", "Leave Tangent Weight"),
							[CurveKey]() { return FText::AsNumber(CurveKey->LeaveTangentWeight); }
						);
					}
					*/

				}
			}
			else if (EditorMode == ERoadSelectionMode::Attribute)
			{
				PropertyEditorUtils::AddTextRow(
					ChildrenBuilder,
					FText::Format(LOCTEXT("SelectedAttributeKey_Search", "Selected {0} Key"), FText::FromName(SelectedAttributeName)),
					FText::Format(LOCTEXT("SelectedAttributeKey_Name", "Selected {0} Key"), FText::FromName(SelectedAttributeName)),
					FText::Format(LOCTEXT("SelectedLane_Value", "< {0} >"), AttributeIndex));

				auto AttributesPropertyHandle = LaneIndex == LANE_INDEX_NONE
					? SectionPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FRoadLaneSection, Attributes)) 
					: LanePropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FRoadLane, Attributes));
				uint32 NumEl = 0;
				check(AttributesPropertyHandle->AsMap()->GetNumElements(NumEl) == FPropertyAccess::Success);
				for (uint32 ChildIndex = 0; ChildIndex < NumEl; ++ChildIndex)
				{
					TSharedPtr<IPropertyHandle> AttributePropertyHandle = AttributesPropertyHandle->GetChildHandle(ChildIndex);
					if (AttributePropertyHandle.IsValid())
					{
						TSharedPtr<IPropertyHandle> KeyProperty = AttributePropertyHandle->GetKeyHandle();
						if (KeyProperty.IsValid())
						{
							FName KeyName;
							if (KeyProperty->GetValue(KeyName) == FPropertyAccess::Success)
							{
								if (KeyName == SelectionState->GetSelectedAttributeName())
								{
									TSharedPtr<IPropertyHandle> KeyPropertHandle = AttributePropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FRoadLaneAttribute, Keys))->GetChildHandle(AttributeIndex);
									check(KeyPropertHandle);

									ChildrenBuilder.AddProperty(KeyPropertHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FRoadLaneAttributeKey, SOffset)).ToSharedRef());

									TSharedPtr<IPropertyHandle> ValuePropertHandle = KeyPropertHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FRoadLaneAttributeKey, Value));
									check(ValuePropertHandle);

									RoadLaneAttributeStruct = MakeShared<UnrealDrive::FInstancedStructProvider>(ValuePropertHandle); 
									for (TFieldIterator<FProperty> PropertyIt(RoadLaneAttributeStruct->GetBaseStructure()); PropertyIt; ++PropertyIt)
									{
										if (auto* Row = ChildrenBuilder.AddExternalStructureProperty(RoadLaneAttributeStruct.ToSharedRef(), PropertyIt->GetFName()))
										{
											if (auto PropertyHandle = Row->GetPropertyHandle())
											{
												PropertyHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateLambda([ValuePropertHandle]()
												{
													ValuePropertHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
												}));
												PropertyHandle->SetOnChildPropertyValueChanged(FSimpleDelegate::CreateLambda([ValuePropertHandle]()
												{
													ValuePropertHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
												}));
											}
										}
									}
									
									break;
								}
							}
						}
					}
				}
			}
		}
		else
		{
			check(0); // WTF?
		}
	}
	else
	{
		ChildrenBuilder.AddCustomRow(LOCTEXT("NoneSelected", "None selected"))
		.RowTag(TEXT("NoneSelected"))
		[
			SNew(SBox)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("NoneSelected", "No road elements are selected."))
				.Font(IDetailLayoutBuilder::GetDetailFont())
			]
		];
	}
}



void FRoadSelectionDetails::Tick(float DeltaTime)
{
	// If this is a blueprint spline, always update the spline component based on 
	// the spline component visualizer's currently edited spline component.
	if (RoadSplineCompArchetype)
	{
		URoadSplineComponent* EditedSplineComp = IsValid(SelectionState) ? SelectionState->GetSelectedSpline() : nullptr;

		if (!EditedSplineComp || (EditedSplineComp->GetArchetype() != RoadSplineCompArchetype))
		{
			return;
		}

		RoadSplineComp = EditedSplineComp;
	}

	if (!RoadSplineComp)
	{
		return;
	}

	bool bNeedsRebuild = false;

	int NewSectionIndex = INDEX_NONE;
	int NewLaneIndex = LANE_INDEX_NONE;
	FName NewAttributeName = NAME_None;
	int NewAttributeIndex = INDEX_NONE;

	ERoadSectionSelectionState State = SelectionState->GetStateVerified();
	if (State > ERoadSectionSelectionState::Component)
	{
		NewSectionIndex = SelectionState->GetSelectedSectionIndex();
		NewLaneIndex = SelectionState->GetSelectedLaneIndex();
	}
	if (State == ERoadSectionSelectionState::Key)
	{
		NewAttributeName = SelectionState->GetSelectedAttributeName();
		NewAttributeIndex = SelectionState->GetSelectedKeyIndex();
	}

	bNeedsRebuild = NewSectionIndex != SelectedSectiontIndex || NewLaneIndex != SelectedLaneIndex || NewAttributeName != SelectedAttributeName || NewAttributeIndex != SelectedKeyIndex;

	SelectedSectiontIndex = NewSectionIndex;
	SelectedLaneIndex = NewLaneIndex;
	SelectedAttributeName = NewAttributeName;
	SelectedKeyIndex = NewAttributeIndex;

	if (bNeedsRebuild)
	{
		RoadLaneAttributeStruct.Reset();
		OnRegenerateChildren.ExecuteIfBound();
	}
}

FName FRoadSelectionDetails::GetName() const
{
	static const FName Name("RoadSelectionDetails");
	return Name;
}


#undef LOCTEXT_NAMESPACE
