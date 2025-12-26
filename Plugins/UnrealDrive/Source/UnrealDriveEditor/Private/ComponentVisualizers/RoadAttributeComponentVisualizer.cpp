/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "ComponentVisualizers/RoadAttributeComponentVisualizer.h"
#include "CoreMinimal.h"
#include "Algo/AnyOf.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Commands/InputChord.h"
#include "Framework/Commands/Commands.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Styling/AppStyle.h"
#include "UnrealWidgetFwd.h"
#include "Editor.h"
#include "EditorViewportClient.h"
#include "EditorViewportCommands.h"
#include "LevelEditorActions.h"
#include "Components/SplineComponent.h"
#include "ScopedTransaction.h"
#include "ActorEditorUtils.h"
#include "WorldCollision.h"
#include "Widgets/Docking/SDockTab.h"
#include "SplineGeneratorPanel.h"
#include "EngineUtils.h"
#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "Math/UnrealMathUtility.h"
#include "UnrealEdGlobals.h"
#include "DefaultRoadLaneAttributes.h"
#include "UnrealDriveEditorModule.h"
#include "UnrealDriveEditorSettings.h"
#include "RoadEditorCommands.h"
#include "RoadLaneAttributeEntries.h"
#include "Utils/DrawUtils.h"
#include "Utils/CompVisUtils.h"
#include "UnrealDrive.h"

#define LOCTEXT_NAMESPACE "FRoadAttributeComponentVisualizer"

#define LOCTEXT_STR(InKey, InTextLiteral) FInternationalization::ForUseOnlyByLocMacroAndGraphNodeTextLiterals_CreateText(InTextLiteral, TEXT(LOCTEXT_NAMESPACE), InKey)

IMPLEMENT_HIT_PROXY(HRoadLaneAttributeVisProxy, HRoadLaneVisProxy);
IMPLEMENT_HIT_PROXY(HRoadLaneAttributeSegmentVisProxy, HRoadLaneAttributeVisProxy);
IMPLEMENT_HIT_PROXY(HRoadLaneAttributeKeyVisProxy, HRoadLaneAttributeSegmentVisProxy);


static double GetAttributeOffset(const FRoadLaneSection& Section, int LaneIndex, int AttributeIndex, FName AttributeName)
{
	const auto& Attributes = LaneIndex == LANE_INDEX_NONE ? Section.Attributes : Section.GetLaneByIndex(LaneIndex).Attributes;
	if (auto* Found = Attributes.Find(AttributeName))
	{
		return Found->Keys[AttributeIndex].SOffset;
	}
	return 0.0;
}




// -------------------------------------------------------------------------------------------------------------------------------------------------
class FRoadAttributeComponentVisualizerCommands : public TCommands<FRoadAttributeComponentVisualizerCommands>
{
public:
	FRoadAttributeComponentVisualizerCommands() : TCommands <FRoadAttributeComponentVisualizerCommands>
	(
		"RoadAttributeComponentVisualizerCommands",	// Context name for fast lookup
		LOCTEXT("RoadAttributeComponentVisualizerCommands", "Road Attribute Component Visualizer Commands"),	// Localized context name for displaying
		NAME_None,	// Parent
		FUnrealDriveEditorStyle::Get().GetStyleSetName()
	)
	{
	}

	virtual void RegisterCommands() override
	{
		UI_COMMAND(CreateAttribute, "Create Attribute", "Create a new attribute for selected lane.", EUserInterfaceActionType::Button, FInputChord());
		UI_COMMAND(DeleteAttribute, "Delete Attribute", "Delete attribute for selected lane.", EUserInterfaceActionType::Button, FInputChord());
		UI_COMMAND(AddAttributeKey, "Add Key", "Add new key for selected lane attribute.", EUserInterfaceActionType::Button, FInputChord());
		UI_COMMAND(DeleteAttributeKey, "Delete Key", "Delete selected attribute key.", EUserInterfaceActionType::Button, FInputChord());
	}

	virtual ~FRoadAttributeComponentVisualizerCommands()
	{
	}

public:
	TSharedPtr<FUICommandInfo> CreateAttribute;
	TSharedPtr<FUICommandInfo> DeleteAttribute;
	TSharedPtr<FUICommandInfo> AddAttributeKey;
	TSharedPtr<FUICommandInfo> DeleteAttributeKey;
};

// -------------------------------------------------------------------------------------------------------------------------------------------------
 
FRoadAttributeComponentVisualizer::FRoadAttributeComponentVisualizer()
	: FRoadSectionComponentVisualizer()
{
	FRoadAttributeComponentVisualizerCommands::Register();

	RoadScetionComponentVisualizerActions = MakeShareable(new FUICommandList);
	SelectionState->IsKeyValid.BindLambda([Self=TWeakObjectPtr<URoadSectionComponentVisualizerSelectionState>(SelectionState)]()
	{
		const URoadSplineComponent* Component = Self->GetSelectedSpline();
		const auto& Section = Component->GetLaneSection(Self->GetSelectedSectionIndex());
		const auto& Attributes = Self->GetSelectedLaneIndex() == LANE_INDEX_NONE ? Section.Attributes : Section.GetLaneByIndex(Self->GetSelectedLaneIndex()).Attributes;

		auto* Attribute = Attributes.Find(Self->GetSelectedAttributeName());
		if (!Attribute)
		{
			return false;
		}
		if (!Attribute->Keys.IsValidIndex(Self->GetSelectedKeyIndex()))
		{
			return false;
		}
		return true;
	});
}

void FRoadAttributeComponentVisualizer::OnRegister()
{
	FRoadSectionComponentVisualizer::OnRegister();

	const auto& Commands = FRoadAttributeComponentVisualizerCommands::Get();


	RoadScetionComponentVisualizerActions->MapAction(
		Commands.CreateAttribute,
		FExecuteAction::CreateSP(this, &FRoadAttributeComponentVisualizer::OnCreateAttribute),
		FCanExecuteAction::CreateLambda([this]() { return SelectionState->GetState() >= ERoadSectionSelectionState::Section; }));

	RoadScetionComponentVisualizerActions->MapAction(
		Commands.DeleteAttribute,
		FExecuteAction::CreateSP(this, &FRoadAttributeComponentVisualizer::OnDeleteAttribute),
		FCanExecuteAction::CreateLambda([this]() { return SelectionState->GetState() >= ERoadSectionSelectionState::Section; }));

	RoadScetionComponentVisualizerActions->MapAction(
		Commands.AddAttributeKey,
		FExecuteAction::CreateSP(this, &FRoadAttributeComponentVisualizer::OnAddKey),
		FCanExecuteAction::CreateLambda([this]() { return SelectionState->GetState() >= ERoadSectionSelectionState::Section; }));

	RoadScetionComponentVisualizerActions->MapAction(
		Commands.DeleteAttributeKey,
		FExecuteAction::CreateSP(this, &FRoadAttributeComponentVisualizer::OnDeleteKey),
		FCanExecuteAction::CreateLambda([this]() { return SelectionState->GetState() == ERoadSectionSelectionState::Key; }));

}

FRoadAttributeComponentVisualizer::~FRoadAttributeComponentVisualizer()
{
}


void FRoadAttributeComponentVisualizer::DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	if (!ShouldDraw(Component))
	{
		return;
	}

	const URoadSplineComponent* SplineComp = CastChecked<const URoadSplineComponent>(Component);
	const bool bIsEditingComponent = GetEditedSplineComponent() == SplineComp;

	FName AttributeName = SelectionState->GetSelectedAttributeName();

	const float GrabHandleSize = 14.0f +GetDefault<ULevelEditorViewportSettings>()->SelectedSplinePointSizeAdjustment;

	for (int32 SectionIndex = 0; SectionIndex < SplineComp->GetLaneSectionsNum(); ++SectionIndex)
	{
		const auto& Section = SplineComp->GetLaneSection(SectionIndex);
		const bool bIsSectionSelected = SelectionState->IsSelected(SplineComp, SectionIndex, LANE_INDEX_NONE);

		auto DrawAttributePoint = [&](double SOffset, int LaneIndex, int AttrInd, const FColor & Color)
		{
			const FVector Pos = SplineComp->EvalLanePoistion(SectionIndex, LaneIndex, Section.SOffset + SOffset, 1.0, ESplineCoordinateSpace::World);
			PDI->SetHitProxy(new HRoadLaneAttributeKeyVisProxy(SplineComp, SectionIndex, LaneIndex, AttributeName, AttrInd));
			PDI->DrawPoint(Pos, Color, GrabHandleSize, SDPG_Foreground);
			PDI->SetHitProxy(NULL);
		};


		if (auto* Attribute = Section.Attributes.Find(AttributeName))
		{
			// Draw found center lane attribute
			for (int AttributeIndex = 0; AttributeIndex < Attribute->Keys.Num(); ++AttributeIndex)
			{
				const auto& AttributeKey = Attribute->Keys[AttributeIndex];
				const auto* Value = AttributeKey.GetValuePtr<FRoadLaneAttributeValue>();

				FColor Color1; 
				FColor Color2;
				if (SelectionState->IsSelected(SplineComp, SectionIndex, LANE_INDEX_NONE, AttributeName, AttributeIndex))
				{
					Color1 = Color2 = FUnrealDriveColors::SelectedColor;
				}
				else if (Value)
				{
					auto & DrawStyle = Value->GetDrawStyle();
					Color1 = DrawStyle.Color1;
					Color2 = DrawStyle.Color2;

					if (!SelectionState->IsSelected(SplineComp, SectionIndex, LANE_INDEX_NONE))
					{
						Color1 = DrawUtils::MakeLowAccent(Color1).ToFColor(true);
						Color2 = DrawUtils::MakeLowAccent(Color2).ToFColor(true);
					}
				}

				PDI->SetHitProxy(new HRoadLaneAttributeSegmentVisProxy(SplineComp, SectionIndex, LANE_INDEX_NONE, AttributeName, AttributeIndex));
				const double S0 = Section.SOffset + AttributeKey.SOffset;
				const double S1 = (AttributeIndex < (Attribute->Keys.Num() - 1)
					? Section.SOffset + Attribute->Keys[AttributeIndex + 1].SOffset 
					: Section.SOffsetEnd_Cashed);
				DrawUtils::DrawLaneBorder(PDI, SplineComp, SectionIndex, LANE_INDEX_NONE, S0, S1, Color1, Color2, SDPG_Foreground, 4.0, 0, true);
				PDI->SetHitProxy(NULL);

				if (bIsSectionSelected)
				{
					DrawAttributePoint(AttributeKey.SOffset, LANE_INDEX_NONE, AttributeIndex, Color1);
				}
			}
		}
		else
		{
			// Draw not found center lane attribute
			PDI->SetHitProxy(new HRoadLaneAttributeVisProxy(SplineComp, SectionIndex, LANE_INDEX_NONE, AttributeName));
			const FColor& Color = SelectionState->IsSelected(SplineComp, SectionIndex, LANE_INDEX_NONE)
				? FUnrealDriveColors::SelectedColor 
				: FUnrealDriveColors::EmptyColor;
			DrawUtils::DrawLaneBorder(PDI, SplineComp, SectionIndex, LANE_INDEX_NONE, Color, Color, SDPG_Foreground, 4.0, 0, true);
			PDI->SetHitProxy(NULL);
		}
			

		for (int LaneIndex = -Section.Left.Num(); LaneIndex <= Section.Right.Num(); ++LaneIndex)
		{
			if (LaneIndex == LANE_INDEX_NONE)
			{
				continue;
			}

			const auto& Lane = Section.GetLaneByIndex(LaneIndex);
			const bool bIsLaneSelected = SelectionState->IsSelected(SplineComp, SectionIndex, LaneIndex);

			if (auto* Attribute = Lane.Attributes.Find(AttributeName))
			{
				// Draw attribute line
				for (int AttributeIndex = 0; AttributeIndex < Attribute->Keys.Num(); ++AttributeIndex)
				{
					const auto& AttributeKey = Attribute->Keys[AttributeIndex];
					const auto* Value = AttributeKey.GetValuePtr<FRoadLaneAttributeValue>();

					FColor Color1;
					FColor Color2;
					if (SelectionState->IsSelected(SplineComp, SectionIndex, LaneIndex, AttributeName, AttributeIndex))
					{
						Color1 = Color2 = FUnrealDriveColors::SelectedColor;
					}
					else if (Value)
					{
						auto& DrawStyle = Value->GetDrawStyle();
						Color1 = DrawStyle.Color1;
						Color2 = DrawStyle.Color2;

						if (!SelectionState->IsSelected(SplineComp, SectionIndex, LaneIndex))
						{
							Color1 = DrawUtils::MakeLowAccent(Color1).ToFColor(true);
							Color2 = DrawUtils::MakeLowAccent(Color2).ToFColor(true);
						}
					}

					PDI->SetHitProxy(new HRoadLaneAttributeSegmentVisProxy(SplineComp, SectionIndex, LaneIndex, AttributeName, AttributeIndex));
					double S0 = Lane.GetStartOffset() + Attribute->Keys[AttributeIndex].SOffset;
					double S1 = (AttributeIndex < (Attribute->Keys.Num() - 1)
						? Section.SOffset + Attribute->Keys[AttributeIndex + 1].SOffset 
						: Lane.GetEndOffset());
					DrawUtils::DrawLaneBorder(PDI, SplineComp, SectionIndex, LaneIndex, S0, S1, Color1, Color2, SDPG_Foreground, 4.0, 0.0, true);
					PDI->SetHitProxy(NULL);

					if (bIsLaneSelected)
					{
						DrawAttributePoint(Attribute->Keys[AttributeIndex].SOffset, LaneIndex, AttributeIndex, Color1);
					}
				}
			}
		}
	}

	if (bIsEditingComponent)
	{
		if (SelectionState->GetState() == ERoadSectionSelectionState::Section || SelectionState->GetState() == ERoadSectionSelectionState::Lane)
		{
			DrawUtils::DrawCrossSpline(PDI, SplineComp, SelectionState->GetCachedSplineKey(), FUnrealDriveColors::CrossSplineColor, SDPG_Foreground);
		}
	}
}

bool FRoadAttributeComponentVisualizer::VisProxyHandleClick(FEditorViewportClient* InViewportClient, HComponentVisProxy* VisProxy, const FViewportClick& Click)
{
	if (VisProxy && VisProxy->Component.IsValid())
	{
		if (VisProxy->IsA(HRoadLaneAttributeSegmentVisProxy::StaticGetType()))
		{
			HRoadLaneAttributeSegmentVisProxy* Proxy = (HRoadLaneAttributeSegmentVisProxy*)VisProxy;
			const FScopedTransaction Transaction(LOCTEXT("SelectRoadSectionLaneAttributKey", "Select Road Lane Attribut Key"));
			SelectionState->Modify();

			if (const URoadSplineComponent* SplineComp = UpdateSelectedComponentAndSectionAndLane(VisProxy))
			{
				SelectionState->SetSelectedAttributeName(Proxy->AttributeName);
				SelectionState->SetSelectedKeyIndex(Proxy->AttributeIndex);

				const FRoadLaneSection& Section = SplineComp->GetLaneSection(Proxy->SectionIndex);

				if (VisProxy->IsA(HRoadLaneAttributeKeyVisProxy::StaticGetType()))
				{
					SelectionState->SetCashedDataAtLane(Proxy->SectionIndex, Proxy->LaneIndex, Section.SOffset + GetAttributeOffset(Section, Proxy->LaneIndex, Proxy->AttributeIndex, Proxy->AttributeName), 1.0);
				}
				else
				{
					auto Rang = SplineComp->GetLaneRang(Proxy->SectionIndex, Proxy->LaneIndex);
					const float Key = SplineComp->ClosetsKeyToSegmant2(Rang.StartS, Rang.EndS, Click.GetOrigin(), Click.GetOrigin() + Click.GetDirection() * 50000.0f);
					SelectionState->SetCashedDataAtSplineInputKey(Key);
				}
			}
			GEditor->RedrawLevelEditingViewports(true);
			return true;
		}
	}

	return FRoadSectionComponentVisualizer::VisProxyHandleClick(InViewportClient, VisProxy, Click);
}

bool FRoadAttributeComponentVisualizer::HandleInputDelta(FEditorViewportClient* ViewportClient, FViewport* Viewport, FVector& DeltaTranslate, FRotator& DeltaRotate, FVector& DeltaScale)
{
	if (FRoadSectionComponentVisualizer::HandleInputDelta(ViewportClient, Viewport, DeltaTranslate, DeltaRotate, DeltaScale))
	{
		return true;
	}

	check(SelectionState);
	auto State = SelectionState->GetStateVerified();

	if (State != ERoadSectionSelectionState::Key)
	{
		return false;
	}

	URoadSplineComponent* SplineComp = GetEditedSplineComponent();
	FRoadLaneSection& Section = SplineComp->GetLaneSection(SelectionState->GetSelectedSectionIndex());
	
	const FVector WidgetLocationWorld = SelectionState->GetCashedPosition() + DeltaTranslate;
	const float ClosestKey = SplineComp->FindInputKeyClosestToWorldLocation(WidgetLocationWorld);
	const float ClosestS = SplineComp->GetDistanceAlongSplineAtSplineInputKey(ClosestKey);

	FName AttributeName = SelectionState->GetSelectedAttributeName();
	int SectionIndex = SelectionState->GetSelectedSectionIndex();
	int LaneIndex = SelectionState->GetSelectedLaneIndex();
	int AttributeIndex = SelectionState->GetSelectedKeyIndex();

	auto& Attributes = LaneIndex == LANE_INDEX_NONE ? Section.Attributes : Section.GetLaneByIndex(LaneIndex).Attributes;
	if (auto* Attribute = Attributes.Find(AttributeName))
	{
		auto& Key = Attribute->Keys[AttributeIndex];
		double SOffset = ClosestS - Section.SOffset;
		Key.SOffset = SOffset;

		Attribute->Keys.Sort([](auto& A, auto& B) { return A.SOffset < B.SOffset; });

		SelectionState->SetSelectedKeyIndex(CompVisUtils::FindBestFit(Attribute->Keys, [SOffset](auto& It) { return FMath::Abs(SOffset - It.SOffset); }));
		SelectionState->SetCashedDataAtLane(SectionIndex, LaneIndex, Section.SOffset + Key.SOffset, 1.0);
	}
		
	SplineComp->GetRoadLayout().UpdateAttributesVersion();
	SplineComp->UpdateMagicTransform();
	SplineComp->MarkRenderStateDirty();
	GEditor->RedrawLevelEditingViewports(true);
	return true;
}


void FRoadAttributeComponentVisualizer::OnCreateAttribute()
{
	FName AttributeName = SelectionState->GetSelectedAttributeName();
	auto* Entry = FUnrealDriveEditorModule::Get().ForEachRoadLaneAttributEntries([AttributeName](FName Key, const TInstancedStruct<FRoadLaneAttributeEntry>*) { return AttributeName == Key; });
	check(Entry);
	const auto& AttributeValueTemplate = Entry->Get<FRoadLaneAttributeEntry>().AttributeValueTemplate;
	check(AttributeValueTemplate.IsValid());

	if (SelectionState->GetStateVerified() >= ERoadSectionSelectionState::Section && !AttributeName.IsNone())
	{
		const FScopedTransaction Transaction(LOCTEXT("CreateAttribute", "Create Attribute"));

		URoadSplineComponent* SplineComp = GetEditedSplineComponent();
		SplineComp->Modify();

		int SectionIndex = SelectionState->GetSelectedSectionIndex();
		int LaneIndex = SelectionState->GetSelectedLaneIndex();
		auto& SelectedSection = GetEditedSplineComponent()->GetLaneSection(SectionIndex);
		auto& Attributes = LaneIndex == LANE_INDEX_NONE ? SelectedSection.Attributes : SelectedSection.GetLaneByIndex(LaneIndex).Attributes;
		auto& Attribute = Attributes.FindOrAdd(AttributeName);
		Attribute.Reset();
		Attribute.SetScriptStruct(AttributeValueTemplate.GetScriptStruct());
		Attribute.UpdateOrAddTypedKey(0.0, AttributeValueTemplate.GetMemory(), AttributeValueTemplate.GetScriptStruct());

		SplineComp->GetRoadLayout().UpdateAttributesVersion();
		GEditor->RedrawLevelEditingViewports(true);
	}
}

void FRoadAttributeComponentVisualizer::OnDeleteAttribute()
{
	FName AttributeName = SelectionState->GetSelectedAttributeName();

	if (SelectionState->GetStateVerified() >= ERoadSectionSelectionState::Section && !AttributeName.IsNone())
	{
		const FScopedTransaction Transaction(LOCTEXT("DeleteAttribute", "Delete Attribute"));

		URoadSplineComponent* SplineComp = GetEditedSplineComponent();
		SplineComp->Modify();

		int SectionIndex = SelectionState->GetSelectedSectionIndex();
		int LaneIndex = SelectionState->GetSelectedLaneIndex();
		auto& SelectedSection = GetEditedSplineComponent()->GetLaneSection(SectionIndex);
		auto& Attributes = LaneIndex == LANE_INDEX_NONE ? SelectedSection.Attributes : SelectedSection.GetLaneByIndex(LaneIndex).Attributes;
		Attributes.Remove(AttributeName);

		SplineComp->GetRoadLayout().UpdateAttributesVersion();
		GEditor->RedrawLevelEditingViewports(true);
	}
}

void FRoadAttributeComponentVisualizer::OnAddKey()
{
	FName AttributeName = SelectionState->GetSelectedAttributeName();
	if (SelectionState->GetStateVerified() >= ERoadSectionSelectionState::Section && !AttributeName.IsNone())
	{
		const FScopedTransaction Transaction(LOCTEXT("AddAttributeKey", "Add Attribute Value"));

		URoadSplineComponent* SplineComp = GetEditedSplineComponent();
		SplineComp->Modify();

		const int SectionIndex = SelectionState->GetSelectedSectionIndex();
		const int LaneIndex = SelectionState->GetSelectedLaneIndex();
		auto& SelectedSection = GetEditedSplineComponent()->GetLaneSection(SectionIndex);
		
		const double S0 = SplineComp->GetDistanceAlongSplineAtSplineInputKey(SelectionState->GetCachedSplineKey());
		auto Rang = SplineComp->GetLaneRang(SectionIndex, LaneIndex);
		const double S = S0 - Rang.StartS;

		if (S0 < Rang.StartS || S0 > Rang.EndS)
		{
			UE_LOG(LogUnrealDrive, Error, TEXT("FRoadAttributeComponentVisualizer::OnAddKey() S %f not in [%f %f]"), S, Rang.StartS, Rang.EndS);
			return;
		}
		
		auto& Attributes = LaneIndex == LANE_INDEX_NONE ? SelectedSection.Attributes : SelectedSection.GetLaneByIndex(LaneIndex).Attributes;
		auto* Attribute = Attributes.Find(AttributeName);
		check(Attribute);
		check(Attribute->GetScriptStruct());

		const int AttributeIndex = Attribute->FindKeyBeforeOrAt(S);
		int NewAttributeIndex = INDEX_NONE;

		if (AttributeIndex == INDEX_NONE)
		{
			TArray<uint8> Memory;
			Memory.SetNum(Attribute->GetScriptStruct()->GetStructureSize());
			Attribute->GetScriptStruct()->InitializeStruct(Memory.GetData());
			NewAttributeIndex = Attribute->UpdateOrAddTypedKey(S, Memory.GetData(), Attribute->GetScriptStruct());
		}
		else
		{
			const FRoadLaneAttributeValue& TemplayeValue = Attribute->Keys[AttributeIndex].GetValue<FRoadLaneAttributeValue>();
			NewAttributeIndex = Attribute->UpdateOrAddTypedKey(S, &TemplayeValue, Attribute->GetScriptStruct());
		}
		
		if (NewAttributeIndex != INDEX_NONE)
		{
			SelectionState->Modify();
			SelectionState->SetCashedDataAtLane(SectionIndex, LaneIndex, SelectedSection.SOffset + GetAttributeOffset(SelectedSection, LaneIndex, NewAttributeIndex, AttributeName), 1.0);
			SelectionState->SetSelectedKeyIndex(NewAttributeIndex);
		}

		SplineComp->GetRoadLayout().UpdateAttributesVersion();
		SplineComp->UpdateMagicTransform();
		SplineComp->MarkRenderStateDirty();
		GEditor->RedrawLevelEditingViewports(true);
	}
}

void FRoadAttributeComponentVisualizer::OnDeleteKey()
{
	if (SelectionState->GetStateVerified() >= ERoadSectionSelectionState::Key)
	{
		const FScopedTransaction Transaction(LOCTEXT("DeleteAttributeKey", "Delete Attribute Key"));

		URoadSplineComponent* SplineComp = GetEditedSplineComponent();
		const FName AttributeName = SelectionState->GetSelectedAttributeName();
		const int SectionIndex = SelectionState->GetSelectedSectionIndex();
		const int LaneIndex = SelectionState->GetSelectedLaneIndex();
		const int AttributeIndex = SelectionState->GetSelectedKeyIndex();
		auto& SelectedSection = GetEditedSplineComponent()->GetLaneSection(SectionIndex);
		
		SplineComp->Modify();

		auto& Attributes = (LaneIndex == LANE_INDEX_NONE) ? SelectedSection.Attributes :  SelectedSection.GetLaneByIndex(LaneIndex).Attributes;
		Attributes.Find(AttributeName)->Keys.RemoveAt(AttributeIndex);
		

		SelectionState->Modify();
		SelectionState->SetSelectedLane(LaneIndex);

		SplineComp->GetRoadLayout().UpdateAttributesVersion();
		SplineComp->UpdateMagicTransform();
		SplineComp->MarkRenderStateDirty();
		GEditor->RedrawLevelEditingViewports(true);
	}
}


void FRoadAttributeComponentVisualizer::GenerateChildContextMenuSections(FMenuBuilder& InMenuBuilder) const
{
	check(SelectionState);
	auto State = SelectionState->GetStateVerified();

	FName AttributeName = SelectionState->GetSelectedAttributeName();

	if (State >= ERoadSectionSelectionState::Section && !AttributeName.IsNone())
	{
		URoadSplineComponent* SplineComp = GetEditedSplineComponent();
		const int SelectionIndex = SelectionState->GetSelectedSectionIndex();
		const int LaneIndex = SelectionState->GetSelectedLaneIndex();
		const FRoadLaneSection& Section = SplineComp->GetLaneSection(SelectionIndex);
		const auto& Attributes = (LaneIndex == LANE_INDEX_NONE) ? Section.Attributes : Section.GetLaneByIndex(LaneIndex).Attributes;
		const auto * Attribute = Attributes.Find(AttributeName);

		InMenuBuilder.BeginSection("RoadLaneAttribute", FText::Format(LOCTEXT("ContextMenuRoadAttribute_Section", "Attribute - {0}"), FText::FromString(AttributeName.ToString())));
	
		if (!Attribute)
		{
			InMenuBuilder.AddMenuEntry(
				FRoadAttributeComponentVisualizerCommands::Get().CreateAttribute, 
				NAME_None, 
				FText::Format(LOCTEXT("ContextMenuRoadAttribute_CreateAttribute", "Create {0}"), FText::FromName(AttributeName)),
				FText::Format(LOCTEXT("ContextMenuRoadAttribute_CreateAttribute_ToolTip", "Create '{0}' attribute for selected lane"), FText::FromName(AttributeName))
			);
		}
		else
		{
			InMenuBuilder.AddMenuEntry(
				FRoadAttributeComponentVisualizerCommands::Get().DeleteAttribute,
				NAME_None,
				FText::Format(LOCTEXT("ContextMenuRoadAttribute_DeleteAttribute", "Delete {0}"), FText::FromName(AttributeName)),
				FText::Format(LOCTEXT("ContextMenuRoadAttribute_DeleteAttribute_ToolTip", "Delete '{0}' attribute for selected lane"), FText::FromName(AttributeName))
			);
			InMenuBuilder.AddMenuEntry(
				FRoadAttributeComponentVisualizerCommands::Get().AddAttributeKey,
				NAME_None,
				FText::Format(LOCTEXT("ContextMenuRoadAttribute_AddAttributeKey", "Add {0} key"), FText::FromName(AttributeName)),
				FText::Format(LOCTEXT("ContextMenuRoadAttribute_AddAttributeKey_ToolTip", "Add key for '{0}' attribute"), FText::FromName(AttributeName))
			);
			InMenuBuilder.AddMenuEntry(
				FRoadAttributeComponentVisualizerCommands::Get().DeleteAttributeKey,
				NAME_None,
				FText::Format(LOCTEXT("ContextMenuRoadAttribute_DeleteAttributeKey", "Delete {0} key"), FText::FromName(AttributeName)),
				FText::Format(LOCTEXT("ContextMenuRoadAttribute_DeleteAttributeKey_ToolTip", "Delete key for '{0}' attribute"), FText::FromName(AttributeName))
			);
		}
	
		InMenuBuilder.EndSection();
	}	
	
}

#undef LOCTEXT_NAMESPACE