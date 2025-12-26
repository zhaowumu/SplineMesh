/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "ComponentVisualizers/RoadWidthComponentVisualizer.h"
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
#include "UnrealDriveEditorModule.h"
#include "RoadEditorCommands.h"
#include "Utils/DrawUtils.h"
#include "UnrealDriveEditorSettings.h"
#include "RichCurveEditorModel.h"
#include "Utils/CurveUtils.h"
#include "Utils/CompVisUtils.h"
#include "UnrealDrive.h"

#define LOCTEXT_NAMESPACE "FRoadWidthComponentVisualizer"

#define LOCTEXT_STR(InKey, InTextLiteral) FInternationalization::ForUseOnlyByLocMacroAndGraphNodeTextLiterals_CreateText(InTextLiteral, TEXT(LOCTEXT_NAMESPACE), InKey)

IMPLEMENT_HIT_PROXY(HRoadLaneWidthSegmentVisProxy, HRoadLaneVisProxy);
IMPLEMENT_HIT_PROXY(HRoadLaneWidthKeyVisProxy, HRoadLaneWidthSegmentVisProxy);
IMPLEMENT_HIT_PROXY(HRoadLaneWidthTangentVisProxy, HRoadLaneWidthKeyVisProxy);


// -------------------------------------------------------------------------------------------------------------------------------------------------
class FRoadWidthComponentVisualizerCommands : public TCommands<FRoadWidthComponentVisualizerCommands>
{
public:
	FRoadWidthComponentVisualizerCommands() : TCommands <FRoadWidthComponentVisualizerCommands>
	(
		"RoadWidthComponentVisualizerCommands",	// Context name for fast lookup
		LOCTEXT("RoadWidthComponentVisualizerCommands", "Road Width Component Visualizer Commands"),	// Localized context name for displaying
		NAME_None,	// Parent
		FUnrealDriveEditorStyle::Get().GetStyleSetName()
	)
	{
	}

	virtual void RegisterCommands() override
	{
		UI_COMMAND(AddWidthKey, "Add Width Key", "Add new width key.", EUserInterfaceActionType::Button, FInputChord());
		UI_COMMAND(DeleteWidthKey, "Delete Width Key", "Delete selected width key.", EUserInterfaceActionType::Button, FInputChord());

		//UI_COMMAND(SetLaneWidthTypeToFixed, "Fixed", "Lane width is constant", EUserInterfaceActionType::RadioButton, FInputChord());
		//UI_COMMAND(SetLaneWidthTypeToLinear, "Linear", "The lane width can be different at the beginning and end, approximated by linear function", EUserInterfaceActionType::RadioButton, FInputChord());
		//UI_COMMAND(SetLaneWidthTypeToCubic, "Cubic", "The lane width can be different at the beginning and end, approximated by cubic (smooth) function", EUserInterfaceActionType::RadioButton, FInputChord());
	}

	virtual ~FRoadWidthComponentVisualizerCommands()
	{
	}

public:
	TSharedPtr<FUICommandInfo> AddWidthKey;
	TSharedPtr<FUICommandInfo> DeleteWidthKey;

	//TSharedPtr<FUICommandInfo> SetLaneWidthTypeToFixed;
	//TSharedPtr<FUICommandInfo> SetLaneWidthTypeToLinear;
	//TSharedPtr<FUICommandInfo> SetLaneWidthTypeToCubic;
};

// -------------------------------------------------------------------------------------------------------------------------------------------------
 
FRoadWidthComponentVisualizer::FRoadWidthComponentVisualizer()
	: FRoadSectionComponentVisualizer()
{
	FRoadWidthComponentVisualizerCommands::Register();

	RoadScetionComponentVisualizerActions = MakeShareable(new FUICommandList);

	RoadScetionComponentVisualizerActions = MakeShareable(new FUICommandList);
	SelectionState->IsKeyValid.BindLambda([Self=TWeakObjectPtr<URoadSectionComponentVisualizerSelectionState>(SelectionState)]()
	{
		const URoadSplineComponent* Component = Self->GetSelectedSpline();
		const auto& Section = Component->GetLaneSection(Self->GetSelectedSectionIndex());
		const auto& Lane = Section.GetLaneByIndex(Self->GetSelectedLaneIndex());
		if (!Lane.Width.Keys.IsValidIndex(Self->GetSelectedKeyIndex()))
		{
			return false;
		}
		return true;
	});
}

void FRoadWidthComponentVisualizer::OnRegister()
{
	FRoadSectionComponentVisualizer::OnRegister();

	const auto& Commands = FRoadWidthComponentVisualizerCommands::Get();

	RoadScetionComponentVisualizerActions->MapAction(
		Commands.AddWidthKey,
		FExecuteAction::CreateSP(this, &FRoadWidthComponentVisualizer::OnAddWidthKey),
		FCanExecuteAction::CreateLambda([this]() { return SelectionState->GetState() >= ERoadSectionSelectionState::Section; }));

	RoadScetionComponentVisualizerActions->MapAction(
		Commands.DeleteWidthKey,
		FExecuteAction::CreateSP(this, &FRoadWidthComponentVisualizer::OnDeleteWidthKey),
		FCanExecuteAction::CreateLambda([this]() { return SelectionState->GetState() == ERoadSectionSelectionState::Key; }));

}

FRoadWidthComponentVisualizer::~FRoadWidthComponentVisualizer()
{
}


void FRoadWidthComponentVisualizer::DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	if (!ShouldDraw(Component))
	{
		return;
	}

	const URoadSplineComponent* SplineComp = CastChecked<const URoadSplineComponent>(Component);
	const bool bIsEditingComponent = GetEditedSplineComponent() == SplineComp;

	const float GrabHandleSize = 14.0f +GetDefault<ULevelEditorViewportSettings>()->SelectedSplinePointSizeAdjustment;

	for (int32 SectionIndex = 0; SectionIndex < SplineComp->GetLaneSectionsNum(); ++SectionIndex)
	{
		const auto& Section = SplineComp->GetLaneSection(SectionIndex);
		for (int LaneIndex = -Section.Left.Num(); LaneIndex <= Section.Right.Num(); ++LaneIndex)
		{
			if (LaneIndex == 0)
			{
				continue;
			}

			const auto& Lane = Section.GetLaneByIndex(LaneIndex);
			const bool bIsLaneSelected = SelectionState->IsSelected(SplineComp, SectionIndex, LaneIndex);

			for (int KeyIndex = 0; KeyIndex < Lane.Width.Keys.Num(); ++KeyIndex)
			{
				PDI->SetHitProxy(new HRoadLaneWidthSegmentVisProxy(SplineComp, SectionIndex, LaneIndex, KeyIndex));
				double S0 = Lane.GetStartOffset() + (KeyIndex == 0 ? 0.0 : Lane.Width.Keys[KeyIndex].Time);
				double S1 = (KeyIndex < (Lane.Width.Keys.Num() - 1)
					? Section.SOffset + Lane.Width.Keys[KeyIndex + 1].Time
					: Lane.GetEndOffset());
				const FColor& Color = SelectionState->IsSelected(SplineComp, SectionIndex, LaneIndex, KeyIndex)
					? FUnrealDriveColors::SelectedColor
					: (bIsLaneSelected ? FUnrealDriveColors::AccentColorHi : FUnrealDriveColors::AccentColorLow);
				DrawUtils::DrawLaneBorder(PDI, SplineComp, SectionIndex, LaneIndex, S0, S1, Color, Color, SDPG_Foreground, 4.0, 0.0, true);
				PDI->SetHitProxy(NULL);

				if (bIsLaneSelected)
				{
					const FVector Location = SplineComp->EvalLanePoistion(SectionIndex, LaneIndex, Section.SOffset + Lane.Width.Keys[KeyIndex].Time, 1.0, ESplineCoordinateSpace::World);
					PDI->SetHitProxy(new HRoadLaneWidthKeyVisProxy(SplineComp, SectionIndex, LaneIndex, KeyIndex));
					PDI->DrawPoint(Location, Color, GrabHandleSize, SDPG_Foreground);
					PDI->SetHitProxy(NULL);
				}
			}
		}
	}

	if(bIsEditingComponent && SelectionState->GetState() >= ERoadSectionSelectionState::Key)
	{
		int SectionIndex = SelectionState->GetSelectedSectionIndex();
		int LaneIndex = SelectionState->GetSelectedLaneIndex();
		int KeyIndex = SelectionState->GetSelectedKeyIndex();

		const auto& Section = SplineComp->GetLaneSection(SectionIndex);
		const auto& Lane = Section.GetLaneByIndex(LaneIndex);
		const auto& WidthKey = Lane.Width.Keys[KeyIndex];
		const ESelectedTangentHandle SelectedTangentHandleType = SelectionState->GetSelectedTangent();

		const float TangentHandleSize = 8.0f + GetDefault<UUnrealDriveEditorSettings>()->SplineTangentHandleSizeAdjustment;
		//const float TangentScale = GetDefault<UUnrealDriveEditorSettings>()->SplineTangentScale;

		auto RoadPos = SplineComp->GetRoadPosition(SectionIndex, LaneIndex, 1.0, Section.SOffset + WidthKey.Time, ESplineCoordinateSpace::World);

		FVector2D ArriveTangentOffset;
		if (CurveUtils::GetArriveTangentOffset(Lane.Width, SplineComp, KeyIndex, LaneIndex < 0, ArriveTangentOffset))
		{
			FVector ArriveTangent = RoadPos.Quat.RotateVector(FVector{ ArriveTangentOffset.X, ArriveTangentOffset.Y, 0.0 });
			const bool bArriveSelected = SelectedTangentHandleType == ESelectedTangentHandle::Arrive;
			FColor ArriveColor = bArriveSelected ? FUnrealDriveColors::SelectedColor : FUnrealDriveColors::TangentColor;

			PDI->SetHitProxy(new HRoadLaneWidthTangentVisProxy(SplineComp, SectionIndex, LaneIndex, KeyIndex, true));
			PDI->DrawLine(RoadPos.Location, RoadPos.Location + ArriveTangent, ArriveColor, SDPG_Foreground);
			PDI->DrawPoint(RoadPos.Location + ArriveTangent, ArriveColor, TangentHandleSize, SDPG_Foreground);
			PDI->SetHitProxy(NULL);
		}

		FVector2D LeaveTangentOffset;
		if (CurveUtils::GetLeaveTangentOffset(Lane.Width, SplineComp, KeyIndex, LaneIndex < 0, LeaveTangentOffset))
		{
			FVector LeaveTangent = RoadPos.Quat.RotateVector(FVector{ LeaveTangentOffset.X, LeaveTangentOffset.Y, 0.0 });
			const bool bLeaveSelected = SelectedTangentHandleType == ESelectedTangentHandle::Leave;
			FColor LeaveColor = bLeaveSelected ? FUnrealDriveColors::SelectedColor : FUnrealDriveColors::TangentColor;

			PDI->SetHitProxy(new HRoadLaneWidthTangentVisProxy(SplineComp, SectionIndex, LaneIndex, KeyIndex, false));
			PDI->DrawLine(RoadPos.Location, RoadPos.Location + LeaveTangent, LeaveColor, SDPG_Foreground);
			PDI->DrawPoint(RoadPos.Location + LeaveTangent, LeaveColor, TangentHandleSize, SDPG_Foreground);
			PDI->SetHitProxy(NULL);
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

bool FRoadWidthComponentVisualizer::VisProxyHandleClick(FEditorViewportClient* InViewportClient, HComponentVisProxy* VisProxy, const FViewportClick& Click)
{
	if (VisProxy && VisProxy->Component.IsValid())
	{
		if (VisProxy->IsA(HRoadLaneWidthTangentVisProxy::StaticGetType()))
		{
			HRoadLaneWidthTangentVisProxy* Proxy = (HRoadLaneWidthTangentVisProxy*)VisProxy;
			const FScopedTransaction Transaction(LOCTEXT("SelectRoadSectionLaneWidthTangent", "Select Road Lane Width Tangent"));
			SelectionState->Modify();

			if (const URoadSplineComponent* SplineComp = UpdateSelectedComponentAndSectionAndLane(VisProxy))
			{
				const auto& Section = SplineComp->GetLaneSection(Proxy->SectionIndex);
				const auto& Lane = Section.GetLaneByIndex(Proxy->LaneIndex);
				const auto& WidthKey = Lane.Width.Keys[Proxy->WidthIndex];

				auto RoadPos = SplineComp->GetRoadPosition(Proxy->SectionIndex, Proxy->LaneIndex, 1.0, Section.SOffset + WidthKey.Time, ESplineCoordinateSpace::World);
				FVector2D TangentOffset;
				bool bOk = false;
				if (Proxy->bArriveTangent)
				{
					bOk = CurveUtils::GetArriveTangentOffset(Lane.Width, SplineComp, Proxy->WidthIndex, Proxy->LaneIndex < 0, TangentOffset);
				}
				else
				{
					bOk = CurveUtils::GetLeaveTangentOffset(Lane.Width, SplineComp, Proxy->WidthIndex, Proxy->LaneIndex < 0, TangentOffset);
				}

				if (bOk)
				{
					SelectionState->SetSelectedKeyIndex(Proxy->WidthIndex);
					SelectionState->SetSelectedTangent(Proxy->bArriveTangent ? ESelectedTangentHandle::Arrive : ESelectedTangentHandle::Leave);

					RoadPos.Location += RoadPos.Quat.RotateVector(FVector{ TangentOffset.X, TangentOffset.Y, 0.0 });
					SelectionState->SetCashedData(RoadPos.Location, RoadPos.Quat, SplineComp->GetInputKeyValueAtDistanceAlongSpline(RoadPos.SOffset));
				}
			}

			GEditor->RedrawLevelEditingViewports(true);
			return true;
		}
		else if (VisProxy->IsA(HRoadLaneWidthSegmentVisProxy::StaticGetType()))
		{
			HRoadLaneWidthSegmentVisProxy* Proxy = (HRoadLaneWidthSegmentVisProxy*)VisProxy;
			const FScopedTransaction Transaction(LOCTEXT("SelectRoadSectionLaneWidthKey", "Select Road Lane Width Key"));
			SelectionState->Modify();

			if (const URoadSplineComponent* SplineComp = UpdateSelectedComponentAndSectionAndLane(VisProxy))
			{
				SelectionState->SetSelectedKeyIndex(Proxy->WidthIndex);

				const auto& Section = SplineComp->GetLaneSection(Proxy->SectionIndex);
				const auto Lane = Section.GetLaneByIndex(Proxy->LaneIndex);

				if (VisProxy->IsA(HRoadLaneWidthKeyVisProxy::StaticGetType()))
				{
					double S = Lane.Width.Keys[Proxy->WidthIndex].Time;
					SelectionState->SetCashedDataAtLane(Proxy->SectionIndex, Proxy->LaneIndex, Section.SOffset + S, 1.0);
				}
				else
				{
					const float Key = SplineComp->ClosetsKeyToSegmant2(Lane.GetStartOffset(), Lane.GetEndOffset(), Click.GetOrigin(), Click.GetOrigin() + Click.GetDirection() * 50000.0f);
					SelectionState->SetCashedDataAtSplineInputKey(Key);
				}
			}
			GEditor->RedrawLevelEditingViewports(true);
			return true;
		}
	}

	return FRoadSectionComponentVisualizer::VisProxyHandleClick(InViewportClient, VisProxy, Click);
}

bool FRoadWidthComponentVisualizer::HandleInputDelta(FEditorViewportClient* ViewportClient, FViewport* Viewport, FVector& DeltaTranslate, FRotator& DeltaRotate, FVector& DeltaScale)
{
	if (FRoadSectionComponentVisualizer::HandleInputDelta(ViewportClient, Viewport, DeltaTranslate, DeltaRotate, DeltaScale))
	{
		return true;
	}

	check(SelectionState);
	auto State = SelectionState->GetStateVerified();

	if (State == ERoadSectionSelectionState::Key)
	{
		URoadSplineComponent* SplineComp = GetEditedSplineComponent();
		FRoadLaneSection& Section = SplineComp->GetLaneSection(SelectionState->GetSelectedSectionIndex());

		const FVector WidgetLocationWorld = SelectionState->GetCashedPosition() + DeltaTranslate;
		const float ClosestKey = SplineComp->FindInputKeyClosestToWorldLocation(WidgetLocationWorld);
		const float ClosestS = SplineComp->GetDistanceAlongSplineAtSplineInputKey(ClosestKey);

		int SectionIndex = SelectionState->GetSelectedSectionIndex();
		int LaneIndex = SelectionState->GetSelectedLaneIndex();
		int KeyIndex = SelectionState->GetSelectedKeyIndex();

		FRoadLane& Lane = Section.GetLaneByIndex(LaneIndex);
		auto& Key = Lane.Width.Keys[KeyIndex];
		const double FillSOffset = Section.SOffset + Key.Time;

		const FTransform KeyTransform = SplineComp->GetTransformAtSplineInputKey(SplineComp->GetInputKeyValueAtDistanceAlongSpline(FillSOffset), ESplineCoordinateSpace::World);
		const FVector WidgetLocationLocal = KeyTransform.InverseTransformPositionNoScale(WidgetLocationWorld);

		const double TargetROffset = WidgetLocationLocal.Y;
		const double CurrentROffset = Section.EvalLaneROffset(LaneIndex, FillSOffset, 1.0) + SplineComp->EvalROffset(FillSOffset);

		double SOffset = ClosestS - Section.SOffset;
		Key.Value += (TargetROffset - CurrentROffset) * (LaneIndex >= LANE_INDEX_NONE ? 1 : -1);
		Key.Time = SOffset;

		Lane.Width.Keys.Sort([](auto& A, auto& B) { return A.Time < B.Time; });

		SelectionState->SetSelectedKeyIndex(CompVisUtils::FindBestFit(Lane.Width.Keys, [SOffset](auto& It) { return FMath::Abs(SOffset - It.Time); }));
		SelectionState->SetCashedDataAtLane(SectionIndex, LaneIndex, Section.SOffset + Key.Time, 1.0);

		SplineComp->GetRoadLayout().UpdateLayoutVersion();
		SplineComp->UpdateMagicTransform();
		SplineComp->MarkRenderStateDirty();
		GEditor->RedrawLevelEditingViewports(true);

		return true;
	}
	else if (State == ERoadSectionSelectionState::KeyTangent)
	{
		URoadSplineComponent* SplineComp = GetEditedSplineComponent();
		int SectionIndex = SelectionState->GetSelectedSectionIndex();
		int LaneIndex = SelectionState->GetSelectedLaneIndex();
		int KeyIndex = SelectionState->GetSelectedKeyIndex();

		FRoadLaneSection& Section = SplineComp->GetLaneSection(SectionIndex);
		FRoadLane& Lane = Section.GetLaneByIndex(LaneIndex);
		const auto& WidthKey = Lane.Width.Keys[KeyIndex];

		if (SelectionState->GetSelectedTangent() != ESelectedTangentHandle::None)
		{
			CurveUtils::DragTangent(Lane.Width, SplineComp, KeyIndex, 
				FVector2D{ SelectionState->GetCachedRotation().UnrotateVector(DeltaTranslate) }, 
				LaneIndex < 0,
				SelectionState->GetSelectedTangent() == ESelectedTangentHandle::Arrive);
		}

		// Update Cashed position
		auto RoadPos = SplineComp->GetRoadPosition(SectionIndex, LaneIndex, 1.0, Section.SOffset + WidthKey.Time, ESplineCoordinateSpace::World);

		FVector2D TangentOffset{};
		if (SelectionState->GetSelectedTangent() == ESelectedTangentHandle::Arrive)
		{
			CurveUtils::GetArriveTangentOffset(Lane.Width, SplineComp, KeyIndex, LaneIndex < 0, TangentOffset);
		}
		else if (SelectionState->GetSelectedTangent() == ESelectedTangentHandle::Leave)
		{
			CurveUtils::GetLeaveTangentOffset(Lane.Width, SplineComp, KeyIndex, LaneIndex < 0, TangentOffset);
		}
		FVector NewCachedPos = RoadPos.Location + RoadPos.Quat.RotateVector(FVector{ TangentOffset.X, TangentOffset.Y, 0.0 });
		SelectionState->SetCashedData(NewCachedPos, RoadPos.Quat, SelectionState->GetCachedSplineKey());

		SplineComp->GetRoadLayout().UpdateLayoutVersion();
		SplineComp->UpdateMagicTransform();
		SplineComp->MarkRenderStateDirty();
		GEditor->RedrawLevelEditingViewports(true);

		return true;
	}
	else
	{
		return false;
	}
}

void FRoadWidthComponentVisualizer::TrackingStarted(FEditorViewportClient* InViewportClient)
{
	FRoadSectionComponentVisualizer::TrackingStarted(InViewportClient);
}

void FRoadWidthComponentVisualizer::OnAddWidthKey()
{
	if (SelectionState->GetStateVerified() >= ERoadSectionSelectionState::Section)
	{
		const FScopedTransaction Transaction(LOCTEXT("AddWidthKey", "Add Width Key"));

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
			UE_LOG(LogUnrealDrive, Error, TEXT("FRoadWidthComponentVisualizer::OnAddWidthKey() S %f not in [%f %f]"), S, Rang.StartS, Rang.EndS);
			return;
		}

		int KeyIndex = 0;
		auto& SelectedLane = SelectedSection.GetLaneByIndex(LaneIndex);
		check(SelectedLane.Width.GetNumKeys());

		const float WidthKeyTolerance = 30.0;
		if (CurveUtils::DoesContaintKey(SelectedLane.Width, S, WidthKeyTolerance))
		{
			UE_LOG(LogUnrealDrive, Error, TEXT("FRoadWidthComponentVisualizer::OnAddWidthKey() key with S=%f already found"), S);
			return;
		}
			
		for (int i = 1; i < SelectedLane.Width.GetNumKeys(); ++i)
		{
			if (S > SelectedLane.Width.Keys[i].Time)
			{
				KeyIndex = i;
			}
			else
			{
				break;
			}
		}

		auto& Key = SelectedLane.Width.Keys[KeyIndex];
		FRichCurveKey NewKay = Key;
		NewKay.Time = S;
		NewKay.InterpMode = ERichCurveInterpMode::RCIM_Cubic;
		NewKay.TangentMode = ERichCurveTangentMode::RCTM_Auto;
		SelectedLane.Width.Keys.Insert(MoveTemp(NewKay), KeyIndex + 1);
		SelectedLane.Width.AutoSetTangents();
		

		SelectionState->Modify();
		SelectionState->SetCashedDataAtLane(SectionIndex, LaneIndex, SelectedSection.SOffset + SelectedSection.GetLaneByIndex(LaneIndex).Width.Keys[KeyIndex + 1].Time, 1.0);
		SelectionState->SetSelectedKeyIndex(KeyIndex + 1);

		SplineComp->GetRoadLayout().UpdateLayoutVersion();
		SplineComp->UpdateMagicTransform();
		SplineComp->MarkRenderStateDirty();
		GEditor->RedrawLevelEditingViewports(true);
	}
}

void FRoadWidthComponentVisualizer::OnDeleteWidthKey()
{
	if (SelectionState->GetStateVerified() >= ERoadSectionSelectionState::Key)
	{
		const FScopedTransaction Transaction(LOCTEXT("DeleteWidthKey", "Delete Width Key"));

		URoadSplineComponent* SplineComp = GetEditedSplineComponent();
		const int SectionIndex = SelectionState->GetSelectedSectionIndex();
		const int LaneIndex = SelectionState->GetSelectedLaneIndex();
		const int KeyIndex = SelectionState->GetSelectedKeyIndex();
		auto& SelectedSection = GetEditedSplineComponent()->GetLaneSection(SectionIndex);
		
		SplineComp->Modify();


		auto& SelectedLane = SelectedSection.GetLaneByIndex(LaneIndex);
		if (SelectedLane.Width.Keys.Num() > 1)
		{
			SelectedLane.Width.Keys.RemoveAt(KeyIndex);
			SelectedLane.Width.AutoSetTangents();
		}


		SelectionState->Modify();
		SelectionState->SetSelectedLane(LaneIndex);

		SplineComp->GetRoadLayout().UpdateLayoutVersion();
		SplineComp->UpdateMagicTransform();
		SplineComp->MarkRenderStateDirty();
		GEditor->RedrawLevelEditingViewports(true);
	}
}


void FRoadWidthComponentVisualizer::GenerateChildContextMenuSections(FMenuBuilder& InMenuBuilder) const
{
	check(SelectionState);
	auto State = SelectionState->GetStateVerified();

	if (State >= ERoadSectionSelectionState::Section )
	{
		InMenuBuilder.BeginSection("RoadLaneWidth", LOCTEXT("ContextMenuRoadWidth", "Width"));
		InMenuBuilder.AddMenuEntry(FRoadWidthComponentVisualizerCommands::Get().AddWidthKey);
		InMenuBuilder.AddMenuEntry(FRoadWidthComponentVisualizerCommands::Get().DeleteWidthKey);
		InMenuBuilder.EndSection();
	}
}

#undef LOCTEXT_NAMESPACE