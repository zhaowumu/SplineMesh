/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "ComponentVisualizers/RoadSectionComponentVisualizer.h"
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

#define LOCTEXT_NAMESPACE "FRoadSectionComponentVisualizer"

#define LOCTEXT_STR(InKey, InTextLiteral) FInternationalization::ForUseOnlyByLocMacroAndGraphNodeTextLiterals_CreateText(InTextLiteral, TEXT(LOCTEXT_NAMESPACE), InKey)

DEFINE_LOG_CATEGORY_STATIC(LogSectionSplineComponentVisualizer, Log, All)


IMPLEMENT_HIT_PROXY(HRoadSectionKeyVisProxy, HRoadLaneVisProxy);


// -------------------------------------------------------------------------------------------------------------------------------------------------

void URoadSectionComponentVisualizerSelectionState::SetCashedData(const FVector& Position, const FQuat& Rotation, float SplineKey)
{
	CahedPosition = Position;
	CachedRotation = Rotation;
	CashedSplineKey = SplineKey;
}

void URoadSectionComponentVisualizerSelectionState::SetCashedDataAtSplineDistance(float SOffset)
{
	const URoadSplineComponent* SplineComp = GetSelectedSpline();
	check(SplineComp);
	float Key = SplineComp->GetInputKeyValueAtDistanceAlongSpline(SOffset);
	auto Pos = SplineComp->GetRoadPosition(SOffset, SplineComp->RoadLayout.ROffset.Eval(SOffset), ESplineCoordinateSpace::World);
	SetCashedData(Pos.Location, Pos.Quat, Key);
}

void URoadSectionComponentVisualizerSelectionState::SetCashedDataAtSplineInputKey(float Key)
{
	const URoadSplineComponent* SplineComp = GetSelectedSpline();
	check(SplineComp);
	float S = SplineComp->GetDistanceAlongSplineAtSplineInputKey(Key);
	auto Pos = SplineComp->GetRoadPosition(S, SplineComp->RoadLayout.ROffset.Eval(S), ESplineCoordinateSpace::World);
	SetCashedData(Pos.Location, Pos.Quat, Key);
}

void URoadSectionComponentVisualizerSelectionState::SetCashedDataAtLane(int SectionIndex, int LaneIndex, double SOffset, double Aplha)
{
	const URoadSplineComponent* SplineComp = GetSelectedSpline();
	check(SplineComp);
	auto Pos = SplineComp->GetRoadPosition(SectionIndex, LaneIndex, Aplha, SOffset, ESplineCoordinateSpace::World);
	float Key = SplineComp->GetInputKeyValueAtDistanceAlongSpline(SOffset);
	SetCashedData(Pos.Location, Pos.Quat, Key);
}

void URoadSectionComponentVisualizerSelectionState::ResetCahedData()
{
	CachedRotation = FQuat();
	CahedPosition = FVector::ZeroVector;
	CashedSplineKey = 0;
}

void URoadSectionComponentVisualizerSelectionState::UpdateSplineSelection() const
{
	if (URoadSplineComponent* Component = GetSelectedSpline())
	{
		Component->SetSelectedLane(SelectedSectionIndex, SelectedLaneIndex);
	}
}

void URoadSectionComponentVisualizerSelectionState::FixState()
{
	ERoadSectionSelectionState StateVerified = GetStateVerified();
	if (StateVerified != State)
	{
		if (StateVerified < ERoadSectionSelectionState::Key)
		{
			SelectedKeyIndex = INDEX_NONE;
			SelectedTangentHandleType = ESelectedTangentHandle::None;
		}
		if (StateVerified < ERoadSectionSelectionState::Lane)
		{
			SelectedLaneIndex = LANE_INDEX_NONE;
		}
		if (StateVerified < ERoadSectionSelectionState::Section)
		{
			SelectedSectionIndex = INDEX_NONE;
		}
		if (StateVerified < ERoadSectionSelectionState::Component)
		{
			SplinePropertyPath = FComponentPropertyPath();
		}
		State = StateVerified;
		//ResetCahedData();
		UpdateSplineSelection();
	}
}

void URoadSectionComponentVisualizerSelectionState::ResetSelection(bool bSaveSplineSelection)
{
	SelectedSectionIndex = INDEX_NONE;
	SelectedLaneIndex = LANE_INDEX_NONE;
	SelectedKeyIndex = INDEX_NONE;
	SelectedTangentHandleType = ESelectedTangentHandle::None;

	UpdateSplineSelection();

	if (bSaveSplineSelection && SplinePropertyPath.IsValid())
	{
		State = ERoadSectionSelectionState::Lane;
	}
	else
	{
		State = ERoadSectionSelectionState::None;
		SplinePropertyPath = FComponentPropertyPath();
	}

	ResetCahedData();
}

void URoadSectionComponentVisualizerSelectionState::SetSelectedSpline(FComponentPropertyPath& InSplinePropertyPath) 
{
	ResetSelection(false);

	check(InSplinePropertyPath.IsValid());

	State = ERoadSectionSelectionState::Component;
	SplinePropertyPath = InSplinePropertyPath; 
	SelectedSectionIndex = INDEX_NONE;
	SelectedLaneIndex = LANE_INDEX_NONE;
	SelectedKeyIndex = INDEX_NONE;
	SelectedTangentHandleType = ESelectedTangentHandle::None;

}

void URoadSectionComponentVisualizerSelectionState::SetSelectedSection(int32 InSelectedSectionIndex) 
{
	check(InSelectedSectionIndex >= 0);
	check(SplinePropertyPath.IsValid());
	check(uint8(State) >= uint8(ERoadSectionSelectionState::Component));

	State = ERoadSectionSelectionState::Section;
	SelectedSectionIndex = InSelectedSectionIndex; 
	SelectedLaneIndex = LANE_INDEX_NONE;
	SelectedKeyIndex = INDEX_NONE;
	SelectedTangentHandleType = ESelectedTangentHandle::None;

	UpdateSplineSelection();
}

void URoadSectionComponentVisualizerSelectionState::SetSelectedLane(int32 InSelectedLaneIndex) 
{
	check(SplinePropertyPath.IsValid());
	check(SelectedSectionIndex != INDEX_NONE);
	check(uint8(State) >= uint8(ERoadSectionSelectionState::Section));

	State = ERoadSectionSelectionState::Lane;
	SelectedLaneIndex = InSelectedLaneIndex;
	SelectedKeyIndex = INDEX_NONE;
	SelectedTangentHandleType = ESelectedTangentHandle::None;

	UpdateSplineSelection();
}

void URoadSectionComponentVisualizerSelectionState::SetSelectedAttributeName(FName AttributeName)
{
	SelectedAttributeName = AttributeName;
	SelectedKeyIndex = INDEX_NONE;
	SelectedTangentHandleType = ESelectedTangentHandle::None;
}

void URoadSectionComponentVisualizerSelectionState::SetSelectedKeyIndex(int32 KeyIndex)
{
	check(SplinePropertyPath.IsValid());
	check(SelectedSectionIndex != INDEX_NONE);
	check(uint8(State) >= uint8(ERoadSectionSelectionState::Section));

	State = ERoadSectionSelectionState::Key;
	SelectedKeyIndex = KeyIndex;
	SelectedTangentHandleType = ESelectedTangentHandle::None;
}

void URoadSectionComponentVisualizerSelectionState::SetSelectedTangent(ESelectedTangentHandle TangentHandle)
{
	check(SplinePropertyPath.IsValid());
	check(SelectedSectionIndex != INDEX_NONE);
	check(SelectedKeyIndex >= 0);
	check(uint8(State) >= uint8(ERoadSectionSelectionState::Key));

	State = ERoadSectionSelectionState::KeyTangent;
	SelectedTangentHandleType = TangentHandle;
}

ERoadSectionSelectionState URoadSectionComponentVisualizerSelectionState::GetStateVerified() const
{
	URoadSplineComponent* Component = GetSelectedSpline();

	if (State > ERoadSectionSelectionState::None)
	{
		if (!IsValid(Component))
		{
			return ERoadSectionSelectionState::None;
		}

		if (State > ERoadSectionSelectionState::Component)
		{
			if (SelectedSectionIndex < 0 || SelectedSectionIndex >= Component->GetLaneSectionsNum())
			{
				return ERoadSectionSelectionState::Component;
			}

			if (State == ERoadSectionSelectionState::Section)
			{
				if (SelectedLaneIndex != LANE_INDEX_NONE)
				{
					return ERoadSectionSelectionState::Component;
				}
			}

			if (State >= ERoadSectionSelectionState::Lane)
			{
				const auto& Section = Component->GetLaneSection(SelectedSectionIndex);
				if (SelectedLaneIndex != LANE_INDEX_NONE && !Section.CheckLaneIndex(SelectedLaneIndex))
				{
					return ERoadSectionSelectionState::Component;
				}

				if (State >= ERoadSectionSelectionState::Key)
				{
					if (!IsKeyValid.IsBound() || !IsKeyValid.Execute())
					{
						return ERoadSectionSelectionState::Lane;
					}
				}
			}
		}
	}

	return State;
}

// -------------------------------------------------------------------------------------------------------------------------------------------------
class FRoadSectionComponentVisualizerCommands : public TCommands<FRoadSectionComponentVisualizerCommands>
{
public:
	FRoadSectionComponentVisualizerCommands() : TCommands <FRoadSectionComponentVisualizerCommands>
	(
		"RoadSectionComponentVisualizer",	// Context name for fast lookup
		LOCTEXT("RoadSectionComponentVisualizer", "Road Spline Section Component Visualizer"),	// Localized context name for displaying
		NAME_None,	// Parent
		FUnrealDriveEditorStyle::Get().GetStyleSetName()
	)
	{
	}

	virtual void RegisterCommands() override
	{
		UI_COMMAND(SplitFullSection, "Split Full Section", "Split road section at the cursor location.", EUserInterfaceActionType::Button, FInputChord());
		UI_COMMAND(SplitSideSection, "Split Side Section", "Split road section at the cursor location.", EUserInterfaceActionType::Button, FInputChord());
		UI_COMMAND(DeleteSection, "Delete Section", "Delete current road section.", EUserInterfaceActionType::Button, FInputChord());

		UI_COMMAND(AddLaneToLeft,  "Add Lane to Left", "Add a new road lane to the left sida of the currently selected lane.", EUserInterfaceActionType::Button, FInputChord());
		UI_COMMAND(AddLaneToRight, "Add Lane to Right", "Add a new road lane to the right sida of the currently selected lane.", EUserInterfaceActionType::Button, FInputChord());
		UI_COMMAND(DeleteLane, "Delete Lane", "Delete selected lane.", EUserInterfaceActionType::Button, FInputChord());
	}

	virtual ~FRoadSectionComponentVisualizerCommands()
	{
	}

public:
	TSharedPtr<FUICommandInfo> SplitFullSection;
	TSharedPtr<FUICommandInfo> SplitSideSection;
	TSharedPtr<FUICommandInfo> DeleteSection;

	TSharedPtr<FUICommandInfo> AddLaneToLeft;
	TSharedPtr<FUICommandInfo> AddLaneToRight;
	TSharedPtr<FUICommandInfo> DeleteLane;
};

// -------------------------------------------------------------------------------------------------------------------------------------------------
 
FRoadSectionComponentVisualizer::FRoadSectionComponentVisualizer()
	: FComponentVisualizer()
{
	FRoadSectionComponentVisualizerCommands::Register();

	RoadScetionComponentVisualizerActions = MakeShareable(new FUICommandList);
	SelectionState = NewObject<URoadSectionComponentVisualizerSelectionState>(GetTransientPackage(), TEXT("RoadSectionComponentVisualizerSelectionState"), RF_Transactional);
}

void FRoadSectionComponentVisualizer::OnRegister()
{
	const auto& Commands = FRoadSectionComponentVisualizerCommands::Get();

	RoadScetionComponentVisualizerActions->MapAction(
		Commands.SplitFullSection,
		FExecuteAction::CreateSP(this, &FRoadSectionComponentVisualizer::OnSplitSection, true),
		FCanExecuteAction::CreateLambda([this]() { return SelectionState->GetState() >= ERoadSectionSelectionState::Section; }));

	RoadScetionComponentVisualizerActions->MapAction(
		Commands.SplitSideSection,
		FExecuteAction::CreateSP(this, &FRoadSectionComponentVisualizer::OnSplitSection, false),
		FCanExecuteAction::CreateLambda([this]() { return SelectionState->GetState() >= ERoadSectionSelectionState::Section; }));

	RoadScetionComponentVisualizerActions->MapAction(
		Commands.DeleteSection,
		FExecuteAction::CreateSP(this, &FRoadSectionComponentVisualizer::OnDeleteSection),
		FCanExecuteAction::CreateLambda([this]() { return SelectionState->GetState() >= ERoadSectionSelectionState::Section; }));

	RoadScetionComponentVisualizerActions->MapAction(
		Commands.AddLaneToLeft,
		FExecuteAction::CreateSP(this, &FRoadSectionComponentVisualizer::OnAddLane, true),
		FCanExecuteAction::CreateLambda([this]() { return  SelectionState->GetState() >= ERoadSectionSelectionState::Section; }));

	RoadScetionComponentVisualizerActions->MapAction(
		Commands.AddLaneToRight,
		FExecuteAction::CreateSP(this, &FRoadSectionComponentVisualizer::OnAddLane, false),
		FCanExecuteAction::CreateLambda([this]() { return  SelectionState->GetState() >= ERoadSectionSelectionState::Section; }));

	RoadScetionComponentVisualizerActions->MapAction(
		Commands.DeleteLane,
		FExecuteAction::CreateSP(this, &FRoadSectionComponentVisualizer::OnDeleteLane),
		FCanExecuteAction::CreateLambda([this]() { return SelectionState->GetState() >= ERoadSectionSelectionState::Lane; }));


}

FRoadSectionComponentVisualizer::~FRoadSectionComponentVisualizer()
{
	//FRoadSectionComponentVisualizerCommands::Unregister();
	EndEditing();
	SelectionState->ConditionalBeginDestroy();
}

void FRoadSectionComponentVisualizer::AddReferencedObjects(FReferenceCollector& Collector)
{
	if (SelectionState)
	{
		Collector.AddReferencedObject(SelectionState);
	}
}

bool FRoadSectionComponentVisualizer::ShouldDraw(const UActorComponent* Component) const
{
	const URoadSplineComponent* SplineComp = Cast<const URoadSplineComponent>(Component);
	if (!SplineComp)
	{
		return false;
	}

	if (!SplineComp->IsVisibleInEditor())
	{
		return false;
	}

	// Allow draw only one manuale selected components
	/*
	TArray<TObjectPtr<URoadSplineComponent>> OwnerComponents;
	SplineComp->GetOwner()->GetComponents(OwnerComponents);
	if (OwnerComponents.Num() > 1 && SplineComp->SceneProxy && !SplineComp->SceneProxy->IsIndividuallySelected())
	{
		return false;
	}
	*/

	return true;
}

void FRoadSectionComponentVisualizer::DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	if (!ShouldDraw(Component))
	{
		return;
	}

	const URoadSplineComponent* SplineComp = CastChecked<const URoadSplineComponent>(Component);

	const bool bIsEditingComponent = GetEditedSplineComponent() == SplineComp;
	const float GrabHandleSize = 14.0f +GetDefault<ULevelEditorViewportSettings>()->SelectedSplinePointSizeAdjustment;

	// Draw sections line
	for (int32 SectionIndex = 0; SectionIndex < SplineComp->GetLaneSectionsNum(); ++SectionIndex)
	{
		PDI->SetHitProxy(new HRoadLaneVisProxy(SplineComp, SectionIndex, LANE_INDEX_NONE));

		const FColor& Color = SelectionState->IsSelected(SplineComp, SectionIndex, LANE_INDEX_NONE)
			? FUnrealDriveColors::SelectedColor : FUnrealDriveColors::AccentColorHi;
		DrawUtils::DrawLaneBorder(PDI, SplineComp, SectionIndex, 0, Color, Color, SDPG_Foreground, 4.0, 0.0, true);
			
		if (bIsEditingComponent)
		{
			PDI->SetHitProxy(new HRoadSectionKeyVisProxy(SplineComp, SectionIndex));
			FVector Location = SplineComp->EvalLanePoistion(SectionIndex, LANE_INDEX_NONE, SplineComp->GetLaneSection(SectionIndex).SOffset, 0.0, ESplineCoordinateSpace::World);
			PDI->DrawPoint(Location, Color, GrabHandleSize, SDPG_Foreground);
			PDI->SetHitProxy(NULL);
		}

		PDI->SetHitProxy(NULL);
	}

	if (bIsEditingComponent)
	{
		if (SelectionState->GetState() == ERoadSectionSelectionState::Section || SelectionState->GetState() == ERoadSectionSelectionState::Lane)
		{
			DrawUtils::DrawCrossSpline(PDI, SplineComp, SelectionState->GetCachedSplineKey(), FUnrealDriveColors::CrossSplineColor, SDPG_Foreground);
		}
	}
}


void FRoadSectionComponentVisualizer::DrawVisualizationHUD(const UActorComponent* Component, const FViewport* Viewport, const FSceneView* View, FCanvas* Canvas) 
{
}

const URoadSplineComponent* FRoadSectionComponentVisualizer::UpdateSelectedComponentAndSectionAndLane(HComponentVisProxy* VisProxy)
{
	check(SelectionState);

	const URoadSplineComponent* NewSplineComp = CastChecked<const URoadSplineComponent>(VisProxy->Component.Get());

	AActor* OldSplineOwningActor = SelectionState->GetSplinePropertyPath().GetParentOwningActor();
	FComponentPropertyPath NewSplinePropertyPath(NewSplineComp);
	SelectionState->SetSelectedSpline(NewSplinePropertyPath);
	SelectionState->SetSelectedAttributeName(FUnrealDriveEditorModule::Get().GetSelectionRoadLaneAttribute());
	AActor* NewSplineOwningActor = NewSplinePropertyPath.GetParentOwningActor();

	if (NewSplinePropertyPath.IsValid())
	{
		if (OldSplineOwningActor != NewSplineOwningActor)
		{
			// Reset selection state if we are selecting a different actor to the one previously selected
			SelectionState->ResetSelection(true);
		}

		URoadSplineComponent* Spline = GetEditedSplineComponent();
		//DeselectedInEditorDelegateHandle = Spline->OnDeselectedInEditor.AddRaw(this, &FRoadSectionComponentVisualizer::OnDeselectedInEditor);

		SelectionState->SetCashedDataAtSplineInputKey(0.0);
	}
	else
	{
		SelectionState->ResetSelection(false);
		return nullptr;
	}

	CompVisUtils::DeselectAllExcept(NewSplineComp);

	if (VisProxy->IsA(HRoadSectionVisProxy::StaticGetType()))
	{
		HRoadSectionVisProxy* SectionProxy = (HRoadSectionVisProxy*)VisProxy;
		check(SectionProxy->SectionIndex >= 0);
		check(SectionProxy->SectionIndex < NewSplineComp->GetLaneSectionsNum());
		SelectionState->SetSelectedSection(SectionProxy->SectionIndex);
		const FRoadLaneSection& Section = NewSplineComp->GetLaneSection(SectionProxy->SectionIndex);

		if (VisProxy->IsA(HRoadLaneVisProxy::StaticGetType()))
		{
			HRoadLaneVisProxy* LaneProxy = (HRoadLaneVisProxy*)VisProxy;
			check(LaneProxy->LaneIndex == LANE_INDEX_NONE || Section.CheckLaneIndex(LaneProxy->LaneIndex));
			SelectionState->SetSelectedLane(LaneProxy->LaneIndex);
		}
	}

	return NewSplineComp;
}

bool FRoadSectionComponentVisualizer::VisProxyHandleClick(FEditorViewportClient* InViewportClient, HComponentVisProxy* VisProxy, const FViewportClick& Click)
{

	bool bVisProxyClickHandled = false;

	if(VisProxy && VisProxy->Component.IsValid())
	{
		if (VisProxy->IsA(HRoadLaneVisProxy::StaticGetType()))
		{
			const FScopedTransaction Transaction(LOCTEXT("SelectRoadSectionLane", "Select Road Lane"));
			SelectionState->Modify();
			HRoadLaneVisProxy* Proxy = (HRoadLaneVisProxy*)VisProxy;
			if (const URoadSplineComponent* SplineComp = UpdateSelectedComponentAndSectionAndLane(VisProxy))
			{
				if (Proxy->LaneIndex == LANE_INDEX_NONE)
				{
					SelectionState->SetSelectedSection(Proxy->SectionIndex);
				}
				else
				{
					SelectionState->SetSelectedLane(Proxy->LaneIndex);
				}

				bVisProxyClickHandled = true;

				const auto& Section = SplineComp->GetLaneSection(Proxy->SectionIndex);
				auto Rang = SplineComp->GetLaneRang(Proxy->SectionIndex, Proxy->LaneIndex);
				const float Key = SplineComp->ClosetsKeyToSegmant2(Rang.StartS, Rang.EndS, Click.GetOrigin(), Click.GetOrigin() + Click.GetDirection() * 50000.0f);
				SelectionState->SetCashedDataAtSplineInputKey(Key);
			}
		}
		else
		{
			const FScopedTransaction Transaction(LOCTEXT("UnselectRoad", "Unselect Road"));
			SelectionState->Modify();
			SelectionState->ResetSelection(false);
		}
	}

	if (bVisProxyClickHandled)
	{
		GEditor->RedrawLevelEditingViewports(true);
	}

	return bVisProxyClickHandled;
}

URoadSplineComponent* FRoadSectionComponentVisualizer::GetEditedSplineComponent() const
{
	check(SelectionState);

	URoadSplineComponent* SplineComp = SelectionState->GetSelectedSpline();

	if (SplineComp && CompVisUtils::IsSelectedInViewport(SplineComp))
	{
		return SplineComp;
	}
	
	return nullptr;
}

UActorComponent* FRoadSectionComponentVisualizer::GetEditedComponent() const
{
	return Cast<UActorComponent>(GetEditedSplineComponent());
}

bool FRoadSectionComponentVisualizer::GetWidgetLocation(const FEditorViewportClient* ViewportClient, FVector& OutLocation) const
{
	if (URoadSplineComponent* SplineComp = GetEditedSplineComponent())
	{
		if (SelectionState->GetState() == ERoadSectionSelectionState::Section || SelectionState->GetState() > ERoadSectionSelectionState::Lane)
		{
			//const float S = SplineComp->Sections[SelectionState->GetSelectedSectionIndex()].SOffset;
			OutLocation = SelectionState->GetCashedPosition();// SplineComp->GetWorldLocationAtDistanceAlongSpline(S);
			return true;
		}
	}
	return false;
}

bool FRoadSectionComponentVisualizer::GetCustomInputCoordinateSystem(const FEditorViewportClient* ViewportClient, FMatrix& OutMatrix) const
{
	if (ViewportClient->GetWidgetCoordSystemSpace() == COORD_Local || ViewportClient->GetWidgetMode() == UE::Widget::WM_Rotate)
	{
		if (URoadSplineComponent* SplineComp = GetEditedSplineComponent())
		{
			if (SelectionState->GetState() == ERoadSectionSelectionState::Section || SelectionState->GetState() > ERoadSectionSelectionState::Lane)
			{
				OutMatrix = FRotationMatrix::Make(SelectionState->GetCachedRotation());
				return true;
			}
		}
	}

	return false;
}

bool FRoadSectionComponentVisualizer::IsVisualizingArchetype() const
{
	URoadSplineComponent* SplineComp = GetEditedSplineComponent();
	return (SplineComp && SplineComp->GetOwner() && FActorEditorUtils::IsAPreviewOrInactiveActor(SplineComp->GetOwner()));
}

bool FRoadSectionComponentVisualizer::HandleInputDelta(FEditorViewportClient* ViewportClient, FViewport* Viewport, FVector& DeltaTranslate, FRotator& DeltaRotate, FVector& DeltaScale)
{
	URoadSplineComponent* SplineComp = GetEditedSplineComponent();
	if (!SplineComp)
	{
		return false;
	}

	check(SelectionState);
	auto State = SelectionState->GetStateVerified();

	if (State == ERoadSectionSelectionState::Section)
	{
		const FVector WidgetLocationWorld = SelectionState->GetCashedPosition() + DeltaTranslate;
		const float ClosestKey = SplineComp->FindInputKeyClosestToWorldLocation(WidgetLocationWorld);
		const float ClosestS = SplineComp->GetDistanceAlongSplineAtSplineInputKey(ClosestKey);

		FRoadLaneSection& Section = SplineComp->GetLaneSection(SelectionState->GetSelectedSectionIndex());
		Section.SOffset = ClosestS;
		SelectionState->SetCashedDataAtSplineInputKey(ClosestKey);

		SplineComp->UpdateLaneSectionBounds();
		SplineComp->UpdateMagicTransform();
		SplineComp->MarkRenderStateDirty();
		GEditor->RedrawLevelEditingViewports(true);
		return true;
	}
	else if (State == ERoadSectionSelectionState::Lane)
	{
		//return true;
	}


	return false;
}

bool FRoadSectionComponentVisualizer::HandleInputKey(FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event)
{
	bool bHandled = false;

	URoadSplineComponent* SplineComp = GetEditedSplineComponent();
	if (SplineComp != nullptr)
	{
		// Something external has changed the number of lane sactions, meaning that the cached selected are no longer valid
		if (SelectionState->GetSelectedSectionIndex() != INDEX_NONE && SelectionState->GetSelectedSectionIndex() >= SplineComp->GetLaneSectionsNum())
		{
			EndEditing();
			return false;
		}
	}

	if (Event == IE_Pressed)
	{
		bHandled = RoadScetionComponentVisualizerActions->ProcessCommandBindings(Key, FSlateApplication::Get().GetModifierKeys(), false);
	}

	return bHandled;
}

bool FRoadSectionComponentVisualizer::HandleModifiedClick(FEditorViewportClient* InViewportClient, HHitProxy* HitProxy, const FViewportClick& Click)
{
	return false;
}

bool FRoadSectionComponentVisualizer::HasFocusOnSelectionBoundingBox(FBox& OutBoundingBox)
{
	return false;
}

bool FRoadSectionComponentVisualizer::HandleSnapTo(const bool bInAlign, const bool bInUseLineTrace, const bool bInUseBounds, const bool bInUsePivot, AActor* InDestination)
{
	return false;
}

void FRoadSectionComponentVisualizer::TrackingStopped(FEditorViewportClient* InViewportClient, bool bInDidMove)
{
	if (bInDidMove)
	{
		// After dragging, notify that the spline curves property has changed one last time, this time as a EPropertyChangeType::ValueSet :
		URoadSplineComponent* SplineComp = GetEditedSplineComponent();
		check(SplineComp != nullptr);
		//NotifyPropertiesModified(SplineComp, { SplineCurvesProperty, SplineSegmentsProperty }, EPropertyChangeType::ValueSet);

		// 
		if (GetSelectionState()->GetState() == ERoadSectionSelectionState::Section)
		{
			// The section(s) may be deleted, so this situation needs to be handled here.

			ERoadSectionSelectionState StateVerified = SelectionState->GetStateVerified();
			double SOffset = 0;
			bool bValid = false;
			if (StateVerified >= ERoadSectionSelectionState::Section)
			{
				SOffset = SplineComp->GetLaneSection(SelectionState->GetSelectedSectionIndex()).SOffset ;
				bValid = true;
			}

			SplineComp->TrimLaneSections();

			SelectionState->FixState();

			if (bValid)
			{
				SelectionState->SetSelectedSection(CompVisUtils::FindBestFit(SplineComp->GetLaneSections(), [SOffset](auto& It) { return FMath::Abs(SOffset - It.SOffset); }));
			}
		}
		else
		{
			SplineComp->TrimLaneSections();
		}
		
		SplineComp->GetRoadLayout().UpdateLayoutVersion();
		SplineComp->MarkRenderStateDirty();
		GEditor->RedrawLevelEditingViewports(true);
	}
}

void FRoadSectionComponentVisualizer::EndEditing()
{
	// Ignore if there is an undo/redo operation in progress
	if (!GIsTransacting)
	{
		if (IsValid(SelectionState))
		{
			SelectionState->ResetSelection(false);
		}
	}
}

void FRoadSectionComponentVisualizer::OnSplitSection(bool bFull)
{
	auto State = SelectionState->GetStateVerified();
	if (State < ERoadSectionSelectionState::Section)
	{
		return;

	}
	
	ERoadLaneSectionSide Side;
	if (bFull)
	{
		Side = ERoadLaneSectionSide::Both;
	}
	else
	{
		if (State >= ERoadSectionSelectionState::Lane)
		{
			int LaneIndex = SelectionState->GetSelectedLaneIndex();
			if (LaneIndex != LANE_INDEX_NONE)
			{
				Side = LaneIndex > 0 ? ERoadLaneSectionSide::Right : ERoadLaneSectionSide::Left;
			}
			else
			{
				return;
			}
		}
		else
		{
			return;
		}
	}

	const FScopedTransaction Transaction(LOCTEXT("SpliteSection", "Split Section"));

	URoadSplineComponent* SplineComp = GetEditedSplineComponent();
	SplineComp->Modify();

	
	int NewSectionIndex =  SplineComp->SplitSection( SelectionState->GetCachedSplineKey(), Side);
	if (NewSectionIndex != INDEX_NONE)
	{

		SelectionState->Modify();
		SelectionState->SetSelectedSection(NewSectionIndex);
		//SelectionState->SetSelectedSegmentIndex(INDEX_NONE);
		//SelectionState->SetCashedPosition(FVector::ZeroVector);
		//SelectionState->SetCachedRotation(SplineComp->GetQuaternionAtSplinePoint(SelectionState->GetLastKeyIndexSelected(), ESplineCoordinateSpace::World));
		GEditor->RedrawLevelEditingViewports(true);
	}
	
}

void FRoadSectionComponentVisualizer::OnDeleteSection()
{
	if (SelectionState->GetStateVerified() >= ERoadSectionSelectionState::Section)
	{
		const FScopedTransaction Transaction(LOCTEXT("DeleteSection", "Delete Scection"));

		URoadSplineComponent* SplineComp = GetEditedSplineComponent();
		SplineComp->Modify();
		SplineComp->GetLaneSections().RemoveAt(SelectionState->GetSelectedSectionIndex());
		SplineComp->UpdateRoadLayout();
		SplineComp->UpdateLaneSectionBounds();
		SplineComp->TrimLaneSections();
		SplineComp->UpdateMagicTransform();
		SplineComp->MarkRenderStateDirty();

		SelectionState->Modify();
		SelectionState->SetSelectedLane(0);

		GEditor->RedrawLevelEditingViewports(true);
	}
}


void FRoadSectionComponentVisualizer::OnAddLane(bool bOnLeft)
{
	if (SelectionState->GetStateVerified() >= ERoadSectionSelectionState::Section)
	{
		const FScopedTransaction Transaction(LOCTEXT("AddLane", "Add Lane"));

		URoadSplineComponent* SplineComp = GetEditedSplineComponent();
		SplineComp->Modify();

		int SectionIndex = SelectionState->GetSelectedSectionIndex();
		int LaneIndex = SelectionState->GetSelectedLaneIndex();
		auto& SelectedSection = GetEditedSplineComponent()->GetLaneSection(SectionIndex);
		
		FRoadLane NewLane{};

		if (LaneIndex == LANE_INDEX_NONE)
		{
			NewLane.LaneInstance.InitializeAs<FRoadLaneDriving>();
			NewLane.Width.AddKey(0, UnrealDrive::DefaultRoadLaneWidth);
		}
		else // Copy lane profile
		{
			auto& SelectedLane = SelectedSection.GetLaneByIndex(LaneIndex);
			NewLane.LaneInstance = SelectedLane.LaneInstance;
			NewLane.Width.AddKey(0, UnrealDrive::DefaultRoadLaneWidth);
			NewLane.Direction = SelectedLane.Direction;
			if (SelectedLane.Width.GetNumKeys() && SelectedLane.Width.Keys[0].Value > UE_KINDA_SMALL_NUMBER)
			{
				NewLane.Width.Keys[0].Value = SelectedLane.Width.Keys[0].Value;
			}
		}

		NewLane.Width.Keys[0].InterpMode = ERichCurveInterpMode::RCIM_Cubic;
		NewLane.Width.Keys[0].TangentMode = ERichCurveTangentMode::RCTM_Auto;

		int NewSalactedLaneIndex = LANE_INDEX_NONE;

		if (LaneIndex == LANE_INDEX_NONE)
		{
			if (bOnLeft)
			{
				SelectedSection.Left.Insert(NewLane, 0);
				NewSalactedLaneIndex = -1;
			}
			else
			{
				SelectedSection.Right.Insert(NewLane, 0);
				NewSalactedLaneIndex = 1;
			}
		}
		else if (LaneIndex > 0)
		{
			SelectedSection.Right.Insert(MoveTemp(NewLane), LaneIndex - 1 + (bOnLeft ? 0 : 1));
			NewSalactedLaneIndex = LaneIndex + (bOnLeft ? 0 : 1);
		}
		else // LaneIndex <0
		{
			SelectedSection.Left.Insert(MoveTemp(NewLane), -LaneIndex - 1 + (bOnLeft ? 1 : 0));
			NewSalactedLaneIndex = LaneIndex - (bOnLeft ? 1 : 0);
		}

		SplineComp->UpdateRoadLayout();
		SplineComp->MarkRenderStateDirty();
		SplineComp->UpdateMagicTransform();

		SelectionState->Modify();
		SelectionState->SetSelectedLane(NewSalactedLaneIndex);
		GEditor->RedrawLevelEditingViewports(true);
	}
}

void FRoadSectionComponentVisualizer::OnDeleteLane()
{
	if (SelectionState->GetStateVerified() >= ERoadSectionSelectionState::Lane)
	{
		const FScopedTransaction Transaction(LOCTEXT("DeleteLane", "Delete Lane"));

		URoadSplineComponent* SplineComp = GetEditedSplineComponent();
		SplineComp->Modify();

		int SectionIndex = SelectionState->GetSelectedSectionIndex();
		int LaneIndex = SelectionState->GetSelectedLaneIndex();
		auto& SelectedSection = GetEditedSplineComponent()->GetLaneSection(SectionIndex);

		if (LaneIndex > 0)
		{
			SelectedSection.Right.RemoveAt(LaneIndex - 1);
		}
		else // LaneIndex < 0
		{
			SelectedSection.Left.RemoveAt(-LaneIndex - 1);
		}

		SplineComp->MarkRenderStateDirty();
		SplineComp->UpdateRoadLayout();
		SplineComp->UpdateMagicTransform();

		SelectionState->Modify();
		SelectionState->SetSelectedLane(LANE_INDEX_NONE);

		GEditor->RedrawLevelEditingViewports(true);
	}
}


TSharedPtr<SWidget> FRoadSectionComponentVisualizer::GenerateContextMenu() const
{
	FMenuBuilder MenuBuilder(true, RoadScetionComponentVisualizerActions);

	GenerateContextMenuSections(MenuBuilder);

	TSharedPtr<SWidget> MenuWidget = MenuBuilder.MakeWidget();
	return MenuWidget;
}


void FRoadSectionComponentVisualizer::GenerateContextMenuSections(FMenuBuilder& InMenuBuilder) const
{
	URoadSplineComponent* SplineComp = GetEditedSplineComponent();
	if (SplineComp != nullptr)
	{
		check(SelectionState);

		auto State = SelectionState->GetStateVerified();

		if (State == ERoadSectionSelectionState::Section || State >= ERoadSectionSelectionState::Lane)
		{
			InMenuBuilder.BeginSection("RoadScection", LOCTEXT("ContextMenuRoadScection", "Road Scection"));
			InMenuBuilder.AddMenuEntry(FRoadSectionComponentVisualizerCommands::Get().SplitFullSection);
			InMenuBuilder.AddMenuEntry(FRoadSectionComponentVisualizerCommands::Get().SplitSideSection, 
				NAME_None, 
				FText::Format( LOCTEXT("ContextMenuRoadScection_SideSplit", "Split {0} section"), 
					SelectionState->GetSelectedLaneIndex() < 0 
					? LOCTEXT("ContextMenuRoadScection_SplitLeftSide", "Left")
					: LOCTEXT("ContextMenuRoadScection_SideRightSide", "Right")
				),
				TAttribute<FText>(),
				SelectionState->GetSelectedLaneIndex() < 0 ? FSlateIcon("UnrealDriveEditor", "RoadSectionComponentVisualizer.SplitLeftSection") : FSlateIcon("UnrealDriveEditor", "RoadSectionComponentVisualizer.SplitRightSection")
				
			);
			InMenuBuilder.AddMenuEntry(FRoadSectionComponentVisualizerCommands::Get().DeleteSection);
			InMenuBuilder.EndSection();

			InMenuBuilder.BeginSection("RoadLane", LOCTEXT("ContextMenuRoadLane", "Road Lane"));
			InMenuBuilder.AddMenuEntry(FRoadSectionComponentVisualizerCommands::Get().AddLaneToLeft);
			InMenuBuilder.AddMenuEntry(FRoadSectionComponentVisualizerCommands::Get().AddLaneToRight);
			InMenuBuilder.AddMenuEntry(FRoadSectionComponentVisualizerCommands::Get().DeleteLane);
			InMenuBuilder.EndSection();
		}

		GenerateChildContextMenuSections(InMenuBuilder);

		InMenuBuilder.PushCommandList(FUnrealDriveEditorModule::Get().GetCommandList().ToSharedRef());
		InMenuBuilder.BeginSection("Visualization", LOCTEXT("Visualization", "Visualization"));
		InMenuBuilder.AddMenuEntry(FRoadEditorCommands::Get().HideSelectedSpline);
		InMenuBuilder.AddMenuEntry(FRoadEditorCommands::Get().UnhideAllSpline);

		InMenuBuilder.EndSection();
		
	}
}

#undef LOCTEXT_NAMESPACE