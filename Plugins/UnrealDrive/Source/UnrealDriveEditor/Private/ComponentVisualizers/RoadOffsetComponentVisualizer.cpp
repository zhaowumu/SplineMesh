/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "ComponentVisualizers/RoadOffsetComponentVisualizer.h"
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
#include "RoadEditorCommands.h"
#include "RoadLaneAttributeEntries.h"
#include "Utils/DrawUtils.h"
#include "Utils/CurveUtils.h"
#include "Utils/CompVisUtils.h"
#include "UnrealDriveEditorSettings.h"

#define LOCTEXT_NAMESPACE "FRoadOffsetComponentVisualizer"

IMPLEMENT_HIT_PROXY(HRoadOffsetLineVisProxy, HRoadSplineVisProxy);
IMPLEMENT_HIT_PROXY(HRoadOffsetKeyVisProxy, HRoadSplineVisProxy);
IMPLEMENT_HIT_PROXY(HRoadOffsetTangentVisProxy, HRoadOffsetKeyVisProxy);


template <typename T, typename COMPARATOR_CLASS>
int FindBestFit(const TArray<T>& Array, const COMPARATOR_CLASS& Comparator)
{
	int Ind = 0;
	double Diff = Comparator(Array[0]);
	for (int i = 1; i < Array.Num(); ++i)
	{
		double CheckDiff = Comparator(Array[i]);
		if (CheckDiff < Diff)
		{
			Ind = i;
			Diff = CheckDiff;
		}
	}
	return Ind;
}

void URoadOffsetComponentVisualizerSelectionState::SetCashedData(const FVector& Position, const FQuat& Rotation, float SplineKey)
{
	CahedPosition = Position;
	CachedRotation = Rotation;
	CashedSplineKey = SplineKey;
}

void URoadOffsetComponentVisualizerSelectionState::SetCashedDataAtSplineDistance(float S)
{
	const URoadSplineComponent* SplineComp = GetSelectedSpline();
	check(SplineComp);
	float Key = SplineComp->GetInputKeyValueAtDistanceAlongSpline(S);
	auto Pos = SplineComp->GetRoadPosition(S, SplineComp->RoadLayout.ROffset.Eval(S), ESplineCoordinateSpace::World);
	SetCashedData(Pos.Location, Pos.Quat, Key);
}

void URoadOffsetComponentVisualizerSelectionState::SetCashedDataAtSplineInputKey(float Key)
{
	const URoadSplineComponent* SplineComp = GetSelectedSpline();
	check(SplineComp);
	float S = SplineComp->GetDistanceAlongSplineAtSplineInputKey(Key);
	auto Pos = SplineComp->GetRoadPosition(S, SplineComp->RoadLayout.ROffset.Eval(S), ESplineCoordinateSpace::World);
	SetCashedData(Pos.Location, Pos.Quat, Key);
}

void URoadOffsetComponentVisualizerSelectionState::ResetCahedData()
{
	CachedRotation = FQuat();
	CahedPosition = FVector::ZeroVector;
	CashedSplineKey = 0;
}

void URoadOffsetComponentVisualizerSelectionState::ResetSelection(bool bSaveSplineSelection)
{
	SelectedKey = INDEX_NONE;
	SelectedTangentType = ESelectedTangentHandle::None;

	if (!bSaveSplineSelection || !SplinePropertyPath.IsValid())
	{
		SplinePropertyPath = FComponentPropertyPath();
	}

	ResetCahedData();
}

void URoadOffsetComponentVisualizerSelectionState::SetSelectedSpline(FComponentPropertyPath& InSplinePropertyPath) 
{
	ResetSelection(false);

	check(InSplinePropertyPath.IsValid());

	SplinePropertyPath = InSplinePropertyPath; 
	SelectedKey = INDEX_NONE;
	SelectedTangentType = ESelectedTangentHandle::None;

}

void URoadOffsetComponentVisualizerSelectionState::SetSelectedKey(int32 InSelectedKey)
{
	check(InSelectedKey >= 0);
	auto* SplineComp = GetSelectedSpline();
	check(SplineComp);
	check(InSelectedKey < SplineComp->RoadLayout.ROffset.GetNumKeys());

	SelectedKey = InSelectedKey;
	SelectedTangentType = ESelectedTangentHandle::None;
}

void URoadOffsetComponentVisualizerSelectionState::SetSelectedTangent(ESelectedTangentHandle Tangent)
{
	check(GetSelectedKeyVerified() != INDEX_NONE);
	SelectedTangentType = Tangent;
}

int32 URoadOffsetComponentVisualizerSelectionState::GetSelectedKeyVerified() const
{
	if (auto* SplineComp = GetSelectedSpline())
	{
		if (SelectedKey < SplineComp->RoadLayout.ROffset.GetNumKeys())
		{
			return SelectedKey;
		}
	}
	return INDEX_NONE;
}

// -------------------------------------------------------------------------------------------------------------------------------------------------
class FRoadOffsetComponentVisualizerCommands : public TCommands<FRoadOffsetComponentVisualizerCommands>
{
public:
	FRoadOffsetComponentVisualizerCommands() : TCommands <FRoadOffsetComponentVisualizerCommands>
	(
		"RoadOffsetComponentVisualize",	// Context name for fast lookup
		LOCTEXT("RoadOffsetComponentVisualize", "Road Offset Component Visualize"),	// Localized context name for displaying
		NAME_None,	// Parent
		FUnrealDriveEditorStyle::Get().GetStyleSetName()
	)
	{
	}

	virtual void RegisterCommands() override
	{
		UI_COMMAND(AddKey, "Add Key", "Add the offset key.", EUserInterfaceActionType::Button, FInputChord());
		UI_COMMAND(DeleteKey, "Delete Key", "Delete the offset key.", EUserInterfaceActionType::Button, FInputChord());
	}

	virtual ~FRoadOffsetComponentVisualizerCommands()
	{
	}
	

public:
	TSharedPtr<FUICommandInfo> AddKey;
	TSharedPtr<FUICommandInfo> DeleteKey;
};

// -------------------------------------------------------------------------------------------------------------------------------------------------

FRoadOffsetComponentVisualizer::FRoadOffsetComponentVisualizer()
	: FComponentVisualizer()
{
	FRoadOffsetComponentVisualizerCommands::Register();

	RoadOffsetComponentVisualizerActions = MakeShareable(new FUICommandList);
	SelectionState = NewObject<URoadOffsetComponentVisualizerSelectionState>(GetTransientPackage(), TEXT("RoadOffsetComponentVisualizerSelectionState"), RF_Transactional);
}

void FRoadOffsetComponentVisualizer::OnRegister()
{
	const auto& Commands = FRoadOffsetComponentVisualizerCommands::Get();

	RoadOffsetComponentVisualizerActions->MapAction(
		Commands.AddKey,
		FExecuteAction::CreateSP(this, &FRoadOffsetComponentVisualizer::OnAddKey),
		FCanExecuteAction::CreateLambda([this]() { return !!SelectionState->GetSelectedSpline(); }));

	RoadOffsetComponentVisualizerActions->MapAction(
		Commands.DeleteKey,
		FExecuteAction::CreateSP(this, &FRoadOffsetComponentVisualizer::OnDeleteKey),
		FCanExecuteAction::CreateLambda([this]() { return SelectionState->GetSelectedKey() != INDEX_NONE;  }));

}

FRoadOffsetComponentVisualizer::~FRoadOffsetComponentVisualizer()
{
	//FRoadOffsetComponentVisualizerCommands::Unregister();
	EndEditing();
}

void FRoadOffsetComponentVisualizer::AddReferencedObjects(FReferenceCollector& Collector)
{
	if (SelectionState)
	{
		Collector.AddReferencedObject(SelectionState);
	}
}

void FRoadOffsetComponentVisualizer::DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	const URoadSplineComponent* SplineComp = Cast<const URoadSplineComponent>(Component);
	if (!SplineComp)
	{
		return;
	}

	if (!SplineComp->IsVisibleInEditor())
	{
		return;
	}

	
	TArray<TObjectPtr<URoadSplineComponent>> OwnerComponents;
	SplineComp->GetOwner()->GetComponents(OwnerComponents);
	if (OwnerComponents.Num() > 1 && SplineComp->SceneProxy && !SplineComp->SceneProxy->IsIndividuallySelected())
	{
		return;
	}

	const bool bIsEditingComponent = GetEditedSplineComponent() == SplineComp;

	const float GrabHandleSize = 14.0f + GetDefault<ULevelEditorViewportSettings>()->SelectedSplinePointSizeAdjustment;

	DrawUtils::DrawSpline(PDI, SplineComp, 0.0, SplineComp->GetSplineLength(), FUnrealDriveColors::AccentColorLow, SDPG_Foreground, 4.0, 0.0, true);

	for (int32 SectionIndex = 0; SectionIndex < SplineComp->GetLaneSectionsNum(); ++SectionIndex)
	{
		PDI->SetHitProxy(new HRoadOffsetLineVisProxy(SplineComp));
		DrawUtils::DrawLaneBorder(PDI, SplineComp, SectionIndex, 0, FUnrealDriveColors::AccentColorHi, FUnrealDriveColors::AccentColorHi, SDPG_Foreground, 4.0, 10.0, true);
		PDI->SetHitProxy(NULL);
	}

	if (bIsEditingComponent)
	{
		for (int32 KeyIndex = 0; KeyIndex < SplineComp->RoadLayout.ROffset.GetNumKeys(); ++KeyIndex)
		{
			const auto& Key = SplineComp->RoadLayout.ROffset.Keys[KeyIndex];

			const FColor& Color = SelectionState->GetSelectedKey() == KeyIndex
				? FUnrealDriveColors::SelectedColor : FUnrealDriveColors::AccentColorHi;

			PDI->SetHitProxy(new HRoadOffsetKeyVisProxy(SplineComp, KeyIndex));
			FVector Pos = SplineComp->GetRoadPosition(Key.Time, SplineComp->RoadLayout.ROffset.Eval(Key.Time), ESplineCoordinateSpace::World).Location;
			PDI->DrawPoint(Pos, Color, GrabHandleSize, SDPG_Foreground);
			PDI->SetHitProxy(NULL);
		}
	}

	if (bIsEditingComponent && SelectionState->GetSelectedKey() != INDEX_NONE)
	{
		int KeyIndex = SelectionState->GetSelectedKeyVerified();
		if (KeyIndex != INDEX_NONE)
		{
			const auto& Curve = SplineComp->RoadLayout.ROffset;
			const auto& Key = Curve.Keys[KeyIndex];
			const ESelectedTangentHandle SelectedTangentHandleType = SelectionState->GetSelectedTangent();

			const float TangentHandleSize = 8.0f + GetDefault<UUnrealDriveEditorSettings>()->SplineTangentHandleSizeAdjustment;
			//const float TangentScale = GetDefault<UUnrealDriveEditorSettings>()->SplineTangentScale;

			auto RoadPos = SplineComp->GetRoadPosition(Key.Time, Curve.Eval(Key.Time), ESplineCoordinateSpace::World);

			FVector2D ArriveTangentOffset;
			if (CurveUtils::GetArriveTangentOffset(Curve, SplineComp, KeyIndex, false, ArriveTangentOffset))
			{
				FVector ArriveTangent = RoadPos.Quat.RotateVector(FVector{ ArriveTangentOffset.X, ArriveTangentOffset.Y, 0.0 });
				const bool bArriveSelected = SelectedTangentHandleType == ESelectedTangentHandle::Arrive;
				FColor ArriveColor = bArriveSelected ? FUnrealDriveColors::SelectedColor : FUnrealDriveColors::TangentColor;

				PDI->SetHitProxy(new HRoadOffsetTangentVisProxy(SplineComp, KeyIndex, true));
				PDI->DrawLine(RoadPos.Location, RoadPos.Location + ArriveTangent, ArriveColor, SDPG_Foreground);
				PDI->DrawPoint(RoadPos.Location + ArriveTangent, ArriveColor, TangentHandleSize, SDPG_Foreground);
				PDI->SetHitProxy(NULL);
			}

			FVector2D LeaveTangentOffset;
			if (CurveUtils::GetLeaveTangentOffset(Curve, SplineComp, KeyIndex, false, LeaveTangentOffset))
			{
				FVector LeaveTangent = RoadPos.Quat.RotateVector(FVector{ LeaveTangentOffset.X, LeaveTangentOffset.Y, 0.0 });
				const bool bLeaveSelected = SelectedTangentHandleType == ESelectedTangentHandle::Leave;
				FColor LeaveColor = bLeaveSelected ? FUnrealDriveColors::SelectedColor : FUnrealDriveColors::TangentColor;

				PDI->SetHitProxy(new HRoadOffsetTangentVisProxy(SplineComp, KeyIndex, false));
				PDI->DrawLine(RoadPos.Location, RoadPos.Location + LeaveTangent, LeaveColor, SDPG_Foreground);
				PDI->DrawPoint(RoadPos.Location + LeaveTangent, LeaveColor, TangentHandleSize, SDPG_Foreground);
				PDI->SetHitProxy(NULL);
			}
		}
	}
}


void FRoadOffsetComponentVisualizer::DrawVisualizationHUD(const UActorComponent* Component, const FViewport* Viewport, const FSceneView* View, FCanvas* Canvas) 
{
}

const URoadSplineComponent* FRoadOffsetComponentVisualizer::UpdateSelectedSpline(HComponentVisProxy* VisProxy)
{
	check(SelectionState);

	const URoadSplineComponent* NewSplineComp = CastChecked<const URoadSplineComponent>(VisProxy->Component.Get());

	AActor* OldSplineOwningActor = SelectionState->GetSplinePropertyPath().GetParentOwningActor();
	FComponentPropertyPath NewSplinePropertyPath(NewSplineComp);
	SelectionState->SetSelectedSpline(NewSplinePropertyPath);

	AActor* NewSplineOwningActor = NewSplinePropertyPath.GetParentOwningActor();

	if (NewSplinePropertyPath.IsValid())
	{
		if (OldSplineOwningActor != NewSplineOwningActor)
		{
			// Reset selection state if we are selecting a different actor to the one previously selected
			SelectionState->ResetSelection(true);
		}

		SelectionState->SetCashedDataAtSplineInputKey(0.0);
	}
	else
	{
		SelectionState->ResetSelection(false);
		return nullptr;
	}

	CompVisUtils::DeselectAllExcept(NewSplineComp);

	return NewSplineComp;
}

bool FRoadOffsetComponentVisualizer::VisProxyHandleClick(FEditorViewportClient* InViewportClient, HComponentVisProxy* VisProxy, const FViewportClick& Click)
{
	bool bVisProxyClickHandled = false;

	if (VisProxy && VisProxy->Component.IsValid())
	{
		if (VisProxy->IsA(HRoadOffsetTangentVisProxy::StaticGetType()))
		{
			HRoadOffsetTangentVisProxy* Proxy = (HRoadOffsetTangentVisProxy*)VisProxy;
			const FScopedTransaction Transaction(LOCTEXT("SelectOffsetTangent", "Select Road Lane Width Tangent"));
			SelectionState->Modify();

			if (const URoadSplineComponent* SplineComp = UpdateSelectedSpline(VisProxy))
			{
				const auto& Curve = SplineComp->RoadLayout.ROffset;
				const auto& Key = Curve.Keys[Proxy->OffsetKey];
				auto RoadPos = SplineComp->GetRoadPosition(Key.Time, SplineComp->RoadLayout.ROffset.Eval(Key.Time), ESplineCoordinateSpace::World);

				FVector2D TangentOffset;
				bool bOk = false;
				if (Proxy->bArriveTangent)
				{
					bOk = CurveUtils::GetArriveTangentOffset(Curve, SplineComp, Proxy->OffsetKey, false, TangentOffset);
				}
				else
				{
					bOk = CurveUtils::GetLeaveTangentOffset(Curve, SplineComp, Proxy->OffsetKey, false, TangentOffset);
				}

				if (bOk)
				{
					SelectionState->SetSelectedKey(Proxy->OffsetKey);
					SelectionState->SetSelectedTangent(Proxy->bArriveTangent ? ESelectedTangentHandle::Arrive : ESelectedTangentHandle::Leave);

					RoadPos.Location += RoadPos.Quat.RotateVector(FVector{ TangentOffset.X, TangentOffset.Y, 0.0 });
					SelectionState->SetCashedData(RoadPos.Location, RoadPos.Quat, SplineComp->GetInputKeyValueAtDistanceAlongSpline(RoadPos.SOffset));
				}
			}

			GEditor->RedrawLevelEditingViewports(true);
			return true;
		}
		else if (VisProxy->IsA(HRoadOffsetKeyVisProxy::StaticGetType()))
		{
			const FScopedTransaction Transaction(LOCTEXT("SelectRoadOffsetKey", "Select Road Offset Key"));
			SelectionState->Modify();
			if (auto* SplineComp = UpdateSelectedSpline(VisProxy))
			{
				HRoadOffsetKeyVisProxy* KeyProxy = (HRoadOffsetKeyVisProxy*)VisProxy;
				check(KeyProxy->OffsetKey >= 0);
				check(KeyProxy->OffsetKey < SplineComp->RoadLayout.ROffset.GetNumKeys());
				SelectionState->SetSelectedKey(KeyProxy->OffsetKey);
				double S = SplineComp->RoadLayout.ROffset.Keys[KeyProxy->OffsetKey].Time;
				SelectionState->SetCashedDataAtSplineDistance(S);
				bVisProxyClickHandled = true;
			}
		}
		else if (VisProxy->IsA(HRoadSplineVisProxy::StaticGetType()))
		{
			const FScopedTransaction Transaction(LOCTEXT("SelectRoad", "Select Road"));
			SelectionState->Modify();

			if (auto* SplineComp = UpdateSelectedSpline(VisProxy))
			{
				const float Key = SplineComp->ClosetsKeyToSegmant(0, SplineComp->GetNumberOfSplinePoints(), Click.GetOrigin(), Click.GetOrigin() + Click.GetDirection() * 50000.0f);
				SelectionState->SetCashedDataAtSplineInputKey(Key);
			}

			bVisProxyClickHandled = true;
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

URoadSplineComponent* FRoadOffsetComponentVisualizer::GetEditedSplineComponent() const
{
	check(SelectionState);
	return SelectionState->GetSelectedSpline();
}

UActorComponent* FRoadOffsetComponentVisualizer::GetEditedComponent() const
{
	return Cast<UActorComponent>(GetEditedSplineComponent());
}

bool FRoadOffsetComponentVisualizer::GetWidgetLocation(const FEditorViewportClient* ViewportClient, FVector& OutLocation) const
{
	if (URoadSplineComponent* SplineComp = GetEditedSplineComponent())
	{
		if (SelectionState->GetSelectedKey() != INDEX_NONE)
		{
			OutLocation = SelectionState->GetCashedPosition();// SplineComp->GetWorldLocationAtDistanceAlongSpline(S);
			return true;
		}
	}
	return false;
}

bool FRoadOffsetComponentVisualizer::GetCustomInputCoordinateSystem(const FEditorViewportClient* ViewportClient, FMatrix& OutMatrix) const
{
	if (ViewportClient->GetWidgetCoordSystemSpace() == COORD_Local || ViewportClient->GetWidgetMode() == UE::Widget::WM_Rotate)
	{
		if (URoadSplineComponent* SplineComp = GetEditedSplineComponent())
		{
			if (SelectionState->GetSelectedKey() != INDEX_NONE)
			{
				OutMatrix = FRotationMatrix::Make(SelectionState->GetCachedRotation());
				return true;
			}
		}
	}

	return false;
}

bool FRoadOffsetComponentVisualizer::IsVisualizingArchetype() const
{
	URoadSplineComponent* SplineComp = GetEditedSplineComponent();
	return (SplineComp && SplineComp->GetOwner() && FActorEditorUtils::IsAPreviewOrInactiveActor(SplineComp->GetOwner()));
}

bool FRoadOffsetComponentVisualizer::HandleInputDelta(FEditorViewportClient* ViewportClient, FViewport* Viewport, FVector& DeltaTranslate, FRotator& DeltaRotate, FVector& DeltaScale)
{
	URoadSplineComponent* SplineComp = GetEditedSplineComponent();
	if (!SplineComp)
	{
		return false;
	}

	check(SelectionState);
	int KeyIndex = SelectionState->GetSelectedKeyVerified();
	if (KeyIndex == INDEX_NONE)
	{
		return false;
	}

	auto TangentType = SelectionState->GetSelectedTangent();

	if (TangentType == ESelectedTangentHandle::None)
	{
		const FVector WidgetLocationWorld = SelectionState->GetCashedPosition() + DeltaTranslate;
		const float ClosestKey = SplineComp->FindInputKeyClosestToWorldLocation(WidgetLocationWorld);
		const float ClosestS = SplineComp->GetDistanceAlongSplineAtSplineInputKey(ClosestKey);

		auto& Curv = SplineComp->RoadLayout.ROffset;
		auto& Key = Curv.Keys[KeyIndex];

		const FTransform KeyTransform = SplineComp->GetTransformAtSplineInputKey(SplineComp->GetInputKeyValueAtDistanceAlongSpline(Key.Time), ESplineCoordinateSpace::World);
		const FVector WidgetLocationLocal = KeyTransform.InverseTransformPositionNoScale(WidgetLocationWorld);

		const double TargetROffset = WidgetLocationLocal.Y;
		const double CurrentROffset = Curv.Eval(Key.Time);

		Key.Value += (TargetROffset - CurrentROffset);
		Key.Time = ClosestS;

		Curv.Keys.Sort([](auto& A, auto& B) { return A.Time < B.Time; });

		SelectionState->SetCashedDataAtSplineInputKey(ClosestKey);
	}
	else
	{
		const auto& Curve = SplineComp->RoadLayout.ROffset;
		const auto& Key = Curve.Keys[KeyIndex];
		
		CurveUtils::DragTangent(Curve, SplineComp, KeyIndex, 
			FVector2D{ SelectionState->GetCachedRotation().UnrotateVector(DeltaTranslate) }, 
			false,
			SelectionState->GetSelectedTangent() == ESelectedTangentHandle::Arrive);

		// Update Cashed position
		auto RoadPos = SplineComp->GetRoadPosition(Key.Time, SplineComp->RoadLayout.ROffset.Eval(Key.Time), ESplineCoordinateSpace::World);

		FVector2D TangentOffset{};
		if (SelectionState->GetSelectedTangent() == ESelectedTangentHandle::Arrive)
		{
			CurveUtils::GetArriveTangentOffset(Curve, SplineComp, KeyIndex, false, TangentOffset);
		}
		else if (SelectionState->GetSelectedTangent() == ESelectedTangentHandle::Leave)
		{
			CurveUtils::GetLeaveTangentOffset(Curve, SplineComp, KeyIndex, false, TangentOffset);
		}
		FVector NewCachedPos = RoadPos.Location + RoadPos.Quat.RotateVector(FVector{ TangentOffset.X, TangentOffset.Y, 0.0 });
		SelectionState->SetCashedData(NewCachedPos, RoadPos.Quat, SelectionState->GetCachedSplineKey());
	}

	SplineComp->GetRoadLayout().UpdateLayoutVersion();
	SplineComp->UpdateMagicTransform();
	SplineComp->MarkRenderStateDirty();
	GEditor->RedrawLevelEditingViewports(true);

	return true;
}

bool FRoadOffsetComponentVisualizer::HandleInputKey(FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event)
{
	bool bHandled = false;

	URoadSplineComponent* SplineComp = GetEditedSplineComponent();
	if (SplineComp != nullptr)
	{
		if (SelectionState->GetSelectedKeyVerified() == INDEX_NONE)
		{
			EndEditing();
			return false;
		}
	}

	if (Event == IE_Pressed)
	{
		bHandled = RoadOffsetComponentVisualizerActions->ProcessCommandBindings(Key, FSlateApplication::Get().GetModifierKeys(), false);
	}

	return bHandled;
}

bool FRoadOffsetComponentVisualizer::HandleModifiedClick(FEditorViewportClient* InViewportClient, HHitProxy* HitProxy, const FViewportClick& Click)
{
	return false;
}

bool FRoadOffsetComponentVisualizer::HasFocusOnSelectionBoundingBox(FBox& OutBoundingBox)
{
	return false;
}

bool FRoadOffsetComponentVisualizer::HandleSnapTo(const bool bInAlign, const bool bInUseLineTrace, const bool bInUseBounds, const bool bInUsePivot, AActor* InDestination)
{
	return false;
}

void FRoadOffsetComponentVisualizer::TrackingStopped(FEditorViewportClient* InViewportClient, bool bInDidMove)
{
	if (bInDidMove)
	{
		// After dragging, notify that the spline curves property has changed one last time, this time as a EPropertyChangeType::ValueSet :
		URoadSplineComponent* SplineComp = GetEditedSplineComponent();
		check(SplineComp != nullptr);
		//NotifyPropertiesModified(SplineComp, { SplineCurvesProperty, SplineSegmentsProperty }, EPropertyChangeType::ValueSet);

		// 
		if (GetSelectionState()->GetSelectedKeyVerified() != INDEX_NONE)
		{
			SplineComp->TrimLaneSections();

			//SelectionState->SetSelectedKey(FindBestFit(SplineComp->GetLaneOffsets().Keys, [SOffset](auto& It) { return FMath::Abs(SOffset - It.SOffset); }));
			
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

void FRoadOffsetComponentVisualizer::EndEditing()
{
	// Ignore if there is an undo/redo operation in progress
	if (!GIsTransacting)
	{
		URoadSplineComponent* SplineComponent = GetEditedSplineComponent();
		SelectionState->ResetSelection(false);
	}
}

void FRoadOffsetComponentVisualizer::OnAddKey()
{
	URoadSplineComponent* SplineComp = GetEditedSplineComponent();
	if (!SplineComp)
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("AddOffsetKey", "Add Offset Key"));
	SplineComp->Modify();

	const double S = SplineComp->GetDistanceAlongSplineAtSplineInputKey(SelectionState->GetCachedSplineKey());
	auto& OffsetCurve = SplineComp->RoadLayout.ROffset;

	int KeyIndex = INDEX_NONE;
	for (int i = 0; i < OffsetCurve.GetNumKeys(); ++i)
	{
		if (S > OffsetCurve.Keys[i].Time)
		{
			KeyIndex = i;
		}
		else
		{
			break;
		}
	}

	FRichCurveKey NewKay{};
	NewKay.Value = OffsetCurve.Eval(S);
	NewKay.Time = S;
	NewKay.InterpMode = ERichCurveInterpMode::RCIM_Cubic;
	NewKay.TangentMode = ERichCurveTangentMode::RCTM_Auto;
	OffsetCurve.Keys.Insert(MoveTemp(NewKay), KeyIndex + 1);
	OffsetCurve.AutoSetTangents();

	float SplineKey = SplineComp->GetInputKeyValueAtDistanceAlongSpline(S);

	SelectionState->Modify();
	SelectionState->SetCashedDataAtSplineInputKey(SplineKey);
	SelectionState->SetSelectedKey(KeyIndex + 1);

	SplineComp->GetRoadLayout().UpdateLayoutVersion();
	SplineComp->UpdateMagicTransform();
	SplineComp->MarkRenderStateDirty();
	GEditor->RedrawLevelEditingViewports(true);
}

void FRoadOffsetComponentVisualizer::OnDeleteKey()
{
	URoadSplineComponent* SplineComp = GetEditedSplineComponent();
	if (!SplineComp)
	{
		return;
	}

	int KeyIndex = SelectionState->GetSelectedKeyVerified();
	if (KeyIndex == INDEX_NONE)
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("DeleteOffsetKey", "Delete Offset Key"));
	SplineComp->Modify();

	auto& OffsetCurve = SplineComp->RoadLayout.ROffset;
	OffsetCurve.Keys.RemoveAt(KeyIndex);
	OffsetCurve.AutoSetTangents();

	SelectionState->Modify();
	SelectionState->ResetSelection(true);

	SplineComp->GetRoadLayout().UpdateLayoutVersion();
	SplineComp->UpdateMagicTransform();
	SplineComp->MarkRenderStateDirty();
	GEditor->RedrawLevelEditingViewports(true);
}


TSharedPtr<SWidget> FRoadOffsetComponentVisualizer::GenerateContextMenu() const
{
	FMenuBuilder MenuBuilder(true, RoadOffsetComponentVisualizerActions);
	GenerateContextMenuSections(MenuBuilder);
	TSharedPtr<SWidget> MenuWidget = MenuBuilder.MakeWidget();
	return MenuWidget;
}

void FRoadOffsetComponentVisualizer::GenerateContextMenuSections(FMenuBuilder& InMenuBuilder) const
{
	URoadSplineComponent* SplineComp = GetEditedSplineComponent();
	if (SplineComp != nullptr)
	{
		InMenuBuilder.BeginSection("Offset", LOCTEXT("Offset", "Offset"));
		InMenuBuilder.AddMenuEntry(FRoadOffsetComponentVisualizerCommands::Get().AddKey);
		InMenuBuilder.AddMenuEntry(FRoadOffsetComponentVisualizerCommands::Get().DeleteKey);
		InMenuBuilder.EndSection();

		InMenuBuilder.PushCommandList(FUnrealDriveEditorModule::Get().GetCommandList().ToSharedRef());
		InMenuBuilder.BeginSection("Visualization", LOCTEXT("Visualization", "Visualization"));
		InMenuBuilder.AddMenuEntry(FRoadEditorCommands::Get().HideSelectedSpline);
		InMenuBuilder.AddMenuEntry(FRoadEditorCommands::Get().UnhideAllSpline);
		InMenuBuilder.EndSection();
	}
}


#undef LOCTEXT_NAMESPACE