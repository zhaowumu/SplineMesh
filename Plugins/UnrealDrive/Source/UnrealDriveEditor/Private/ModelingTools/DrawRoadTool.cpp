/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "ModelingTools/DrawRoadTool.h"
#include "Utils/DrawUtils.h"
#include "Utils/CompVisUtils.h"
#include "LevelEditorViewport.h"
#include "ActorFactories/ActorFactoryEmptyActor.h"
#include "AssetSelection.h" // FActorFactoryAssetProxy
#include "BaseBehaviors/SingleClickOrDragBehavior.h"
#include "BaseGizmos/GizmoMath.h"
#include "RoadSplineComponent.h"
#include "CoreGlobals.h" // GUndo
#include "Drawing/PreviewGeometryActor.h"
#include "Editor/UnrealEdEngine.h" // DuplicateActors
#include "Engine/Blueprint.h"
#include "Engine/World.h" 
#include "GameFramework/Actor.h"
#include "ILevelEditor.h"
#include "InputState.h" // FInputDeviceRay
#include "InteractiveToolManager.h"
#include "ISceneOutliner.h"
#include "LevelEditor.h"
#include "Modules/ModuleManager.h"
#include "Kismet2/ComponentEditorUtils.h" // GenerateValidVariableName
#include "Mechanics/ConstructionPlaneMechanic.h"
#include "SceneManagement.h" // FPrimitiveDrawInterface
#include "Selection/ToolSelectionUtil.h"
#include "Settings/LevelEditorViewportSettings.h"
#include "SplineUtil.h"
#include "ToolBuilderUtil.h"
#include "UnrealEdGlobals.h" // GUnrealEd
#include "DefaultRoadLaneAttributes.h"
#include "EditorModeManager.h"
#include "EdModeInteractiveToolsContext.h"
#include "ModelingToolsEditorMode.h"
#include "InteractiveToolsContext.h"
#include "EngineUtils.h"
#include "UnrealDriveEditorModule.h"
#include "UnrealDrivePreset.h"
#include "UnrealDriveSubsystem.h"
#include "UnrealDrive.h"

#define LOCTEXT_NAMESPACE "UDrawRoadTool"

using namespace UE::Geometry;


/**
 * Helper to hide actors from the outliner if it's not an actor that
 *  we defined to be automatically hidden (e.g. APreviewGeometryActor),
 *  that works by being friended to FSetActorHiddenInSceneOutliner.
 * This is a temporary measure until we have a cleaner way to hide
 *  ourselves from the outliner through TEDS.
 * Note that just creating this class doesn't actually refresh the outliner
 *  unless you happen to take an action that does (such as reparenting things),
 *  so you would need to call RefreshOutliner().
 */
class FModelingToolsSetActorHiddenInSceneOutliner
{
public:
	FModelingToolsSetActorHiddenInSceneOutliner(AActor* InActor, bool bHidden)
	{
		FSetActorHiddenInSceneOutliner Setter(InActor, bHidden);
	}

	/**
	 * Does a full refresh of the outliner. Note that this can be comparatively
	 *  slow, so it should happen rarely.
	 */
	void RefreshOutliner()
	{
		FLevelEditorModule* LevelEditorModule = FModuleManager::GetModulePtr<FLevelEditorModule>(TEXT("LevelEditor"));
		if (!LevelEditorModule)
		{
			return;
		}

		TSharedPtr<ILevelEditor> LevelEditor = LevelEditorModule->GetLevelEditorInstance().Pin();
		if (!LevelEditor.IsValid())
		{
			return;
		}

		for (TWeakPtr<ISceneOutliner> OutlinerWeak : LevelEditor->GetAllSceneOutliners())
		{
			if (TSharedPtr<ISceneOutliner> Outliner = OutlinerWeak.Pin())
			{
				Outliner->FullRefresh();
			}
		}
	}
};

namespace DrawSplineToolLocals
{
	FText AddPointTransactionName = LOCTEXT("AddPointTransactionName", "Add Point");

	URoadSplineComponent* CreateNewSplineInActor(AActor* Actor,  bool bTransact = false, bool bSetAsRoot = false)
	{
		if (!ensure(Actor))
		{
			return nullptr;
		}

		if (bTransact)
		{
			Actor->Modify();
		}

		FName NewComponentName = *FComponentEditorUtils::GenerateValidVariableName(URoadSplineComponent::StaticClass(), Actor);
		// Note that the RF_Transactional is important here for the spline to undo/redo properly in the future
		URoadSplineComponent* Spline = NewObject<URoadSplineComponent>(Actor, URoadSplineComponent::StaticClass(), NewComponentName, bTransact ? RF_Transactional : RF_NoFlags);

		if (bSetAsRoot)
		{
			Actor->SetRootComponent(Spline);
		}
		else
		{
			Spline->SetupAttachment(Actor->GetRootComponent());
		}

		Spline->OnComponentCreated();
		Actor->AddInstanceComponent(Spline);
		Spline->RegisterComponent();
		Spline->ResetRelativeTransform();
		Actor->PostEditChange();

		return Spline;
	};

	void CopySplineToSpline(const URoadSplineComponent& Source, URoadSplineComponent& Destination, bool bTransact = false)
	{
		if (bTransact)
		{
			Destination.Modify();
		}

		Destination.ClearSplinePoints();
		Destination.bSplineHasBeenEdited = true;

		// We iterate here (rather than just copying over the SplineCurves data) so that we can transform
		// the data properly into the coordinate space of the target component.
		int32 NumSplinePoints = Source.GetNumberOfSplinePoints();
		for (int32 i = 0; i < NumSplinePoints; ++i)
		{
			Destination.AddSplinePoint(Source.GetLocationAtSplinePoint(i, ESplineCoordinateSpace::World), ESplineCoordinateSpace::World, false);
			Destination.SetUpVectorAtSplinePoint(i, Source.GetUpVectorAtSplinePoint(i, ESplineCoordinateSpace::World), ESplineCoordinateSpace::World, false);
			Destination.SetTangentsAtSplinePoint(i, Source.GetArriveTangentAtSplinePoint(i, ESplineCoordinateSpace::World), Source.GetLeaveTangentAtSplinePoint(i, ESplineCoordinateSpace::World), ESplineCoordinateSpace::World, false);
			Destination.SetSplinePointType(i, Source.GetSplinePointType(i), false);
		}

		Destination.SetClosedLoop(Source.IsClosedLoop());
		Destination.UpdateSpline();
	};

	// Gives the scale used for tangent visualization (and which therefore needs to be used in raycasting the handles)
	float GetTangentScale()
	{
		return GetDefault<ULevelEditorViewportSettings>()->SplineTangentScale;
	}

	// Might be useful to have in SplineUtil, but uncertain what the API should be (should it be part of
	// DrawSpline? Should there be options for selection color?). Also potentially messier to match the tangent
	// scale with the UI interaction..
	void DrawTangent(const URoadSplineComponent& SplineComp, int32 PointIndex, IToolsContextRenderAPI& RenderAPI)
	{
		if (!ensure(PointIndex >= 0 && PointIndex < SplineComp.GetNumberOfSplinePoints()))
		{
			return;
		}

		FPrimitiveDrawInterface* PDI = RenderAPI.GetPrimitiveDrawInterface();

		const float TangentScale = GetDefault<ULevelEditorViewportSettings>()->SplineTangentScale;
		const float TangentHandleSize = 8.0f + GetDefault<ULevelEditorViewportSettings>()->SplineTangentHandleSizeAdjustment;

		const FVector Location = SplineComp.GetLocationAtSplinePoint(PointIndex, ESplineCoordinateSpace::World);
		const FVector LeaveTangent = SplineComp.GetLeaveTangentAtSplinePoint(PointIndex, ESplineCoordinateSpace::World) * TangentScale;
		const FVector ArriveTangent = SplineComp.bAllowDiscontinuousSpline ? SplineComp.GetArriveTangentAtSplinePoint(PointIndex, ESplineCoordinateSpace::World) * TangentScale : LeaveTangent;

		FColor Color = FColor::White;

		PDI->DrawLine(Location, Location - ArriveTangent, Color, SDPG_Foreground);
		PDI->DrawLine(Location, Location + LeaveTangent, Color, SDPG_Foreground);

		PDI->DrawPoint(Location + LeaveTangent, Color, TangentHandleSize, SDPG_Foreground);
		PDI->DrawPoint(Location - ArriveTangent, Color, TangentHandleSize, SDPG_Foreground);
	}

	// Undoes a point addition with an auto tangent
	class FSimplePointInsertionChange : public UDrawRoadTool::FSplineChange
	{
	public:
		FSimplePointInsertionChange(const FVector3d& HitLocationIn, const FVector3d& UpVectorIn)
			: HitLocation(HitLocationIn)
			, UpVector(UpVectorIn)
		{
		}

		virtual void Apply(URoadSplineComponent& Spline) override
		{
			Spline.AddSplinePoint(HitLocation, ESplineCoordinateSpace::World, false);
			int32 PointIndex = Spline.GetNumberOfSplinePoints() - 1;
			Spline.SetUpVectorAtSplinePoint(PointIndex, UpVector, ESplineCoordinateSpace::World, true);
		}
		virtual void Revert(URoadSplineComponent& Spline) override
		{
			if (ensure(Spline.GetNumberOfSplinePoints() > 0))
			{
				Spline.RemoveSplinePoint(Spline.GetNumberOfSplinePoints() - 1, true);
			}
		}
		virtual FString ToString() const override
		{
			return TEXT("FSimplePointInsertionChange");
		}

	protected:
		FVector3d HitLocation;
		FVector3d UpVector;
	};

	// Undoes a point addition with an explicit tangent
	class FTangentPointInsertionChange : public UDrawRoadTool::FSplineChange
	{
	public:
		FTangentPointInsertionChange(const FVector3d& HitLocationIn, const FVector3d& UpVectorIn, const FVector3d& TangentIn)
			: HitLocation(HitLocationIn)
			, UpVector(UpVectorIn)
			, Tangent(TangentIn)
		{
		}

		virtual void Apply(URoadSplineComponent& Spline) override
		{
			Spline.AddSplinePoint(HitLocation, ESplineCoordinateSpace::World, false);
			int32 PointIndex = Spline.GetNumberOfSplinePoints() - 1;
			Spline.SetUpVectorAtSplinePoint(PointIndex, UpVector, ESplineCoordinateSpace::World, false);
			Spline.SetTangentAtSplinePoint(PointIndex, Tangent, ESplineCoordinateSpace::World, true);
		}
		virtual void Revert(URoadSplineComponent& Spline) override
		{
			if (ensure(Spline.GetNumberOfSplinePoints() > 0))
			{
				Spline.RemoveSplinePoint(Spline.GetNumberOfSplinePoints() - 1, true);
			}
		}
		virtual FString ToString() const override
		{
			return TEXT("FTangentPointInsertionChange");
		}

	protected:
		FVector3d HitLocation;
		FVector3d UpVector;
		FVector3d Tangent;
	};

	
	FRoadLane MakeLaneFromTemplate(const FRoadLane& TemplateLane)
	{
		FRoadLane NewLane;

		NewLane.LaneInstance = TemplateLane.LaneInstance;

		double SEnd = TemplateLane.GetEndOffset() - TemplateLane.GetStartOffset();

		if (TemplateLane.Width.GetNumKeys())
		{
			NewLane.Width.AddKey(0, TemplateLane.Width.Eval(SEnd));
		}
		else
		{
			NewLane.Width.AddKey(0, UnrealDrive::DefaultRoadLaneWidth);
		}

		NewLane.Width.Keys[0].InterpMode = ERichCurveInterpMode::RCIM_Cubic;
		NewLane.Width.Keys[0].TangentMode = ERichCurveTangentMode::RCTM_Auto;
		NewLane.Direction = TemplateLane.Direction;

		// Copy omly first item of each attribute
		for (auto& [Name, Attribute] : TemplateLane.Attributes)
		{
			if (Attribute.Keys.Num())
			{
				FRoadLaneAttribute NewAttribute;
				NewAttribute.SetScriptStruct(const_cast<UScriptStruct*>(Attribute.GetScriptStruct()));
				const int Index = Attribute.FindKeyBeforeOrAt(SEnd);
				check(Index >= 0);
				NewAttribute.Keys.Add(Attribute.Keys[Index]);
				NewLane.Attributes.Add(Name, NewAttribute);
			}
		}

		return NewLane;
	}

	void CopyRoadProfileFromConnection_OneSide(const ULaneConnection* Connection, URoadSplineComponent* TargetSpline, bool bCopyFullRoadSide)
	{
		check(Connection);

		const auto& SrcLane = Connection->GetOwnedRoadLane();
		const auto& SrcSection = SrcLane.GetStartSection();
		const int SrcLaneIndex = SrcLane.GetLaneIndex();

		TargetSpline->GetLaneSections().Empty();
		auto& DstSection = TargetSpline->GetLaneSections().Add_GetRef({});
		auto& DstLanes = (SrcLane.Direction == ERoadLaneDirection::Invert) ^ (Connection->GetOwnedRoadSplineChecked()->RoadLayout.Direction == ERoadDirection::RightHand)
			? DstSection.Left 
			: DstSection.Right;


		if (bCopyFullRoadSide)
		{
			if (SrcLaneIndex > 0)
			{
				for (int i = SrcLaneIndex - 1; i < SrcSection.Right.Num(); ++i)
				{
					DstLanes.Add(MakeLaneFromTemplate(SrcSection.Right[i]));
				}
			}
			else // SrcLaneIndex < 0
			{
				for (int i = -SrcLaneIndex - 1; i < SrcSection.Left.Num(); ++i)
				{
					DstLanes.Add(MakeLaneFromTemplate(SrcSection.Left[i]));
				}
			}
		}
		else
		{
			DstLanes.Add(MakeLaneFromTemplate(SrcLane));
		}

		TargetSpline->UpdateRoadLayout();
	}

	void CopyRoadProfileFromConnection_BothSides(const ULaneConnection* Connection, URoadSplineComponent* TargetSpline)
	{
		check(Connection);

		const auto& SrcLane = Connection->GetOwnedRoadLane();
		const auto& SrcSection = SrcLane.GetStartSection();
		const int SrcSectionIndex = SrcLane.GetStartSectionIndex();
		const auto& SrcLayout = Connection->GetOwnedRoadSplineChecked()->GetRoadLayout();

		int LeftSectionIndex = SrcLayout.FindSideSection(SrcSectionIndex, ERoadLaneSectionSide::Left);
		int RightSectionIndex = SrcLayout.FindSideSection(SrcSectionIndex, ERoadLaneSectionSide::Right);

		TargetSpline->GetLaneSections().Empty();
		auto& DstSection = TargetSpline->GetLaneSections().Add_GetRef({});

		if (SrcLane.GetStartSectionIndex() != SrcLayout.Sections.Num() - 1)
		{
			LeftSectionIndex = RightSectionIndex = SrcSectionIndex;
		}

		if (LeftSectionIndex != INDEX_NONE)
		{
			for (auto& Lane : SrcLayout.Sections[LeftSectionIndex].Left)
			{
				DstSection.Left.Add(MakeLaneFromTemplate(Lane));
			}
		}

		if (RightSectionIndex != INDEX_NONE)
		{
			for (auto& Lane : SrcLayout.Sections[RightSectionIndex].Right)
			{
				DstSection.Right.Add(MakeLaneFromTemplate(Lane));
			}
		}


		TargetSpline->UpdateRoadLayout();
	}

	void SetRoadProfile(URoadSplineComponent* TargetSpline, const FRoadLaneSectionProfile& RoadProfile)
	{
		static auto CreateAttributes = [](const TSet<FRoadLaneAttributeProfile>& Src)
		{
			TMap<FName, FRoadLaneAttribute> NewAttributes;
			for (auto& Profile : Src)
			{
				if (Profile.AttributeValueTemplate.GetScriptStruct()->IsChildOf<FRoadLaneAttributeValue>())
				{
					FRoadLaneAttribute NewAttribute;
					NewAttribute.SetScriptStruct(Profile.AttributeValueTemplate.GetScriptStruct());
					NewAttribute.UpdateOrAddTypedKey(0.0, Profile.AttributeValueTemplate.GetMemory(), Profile.AttributeValueTemplate.GetScriptStruct());
					NewAttributes.Add(Profile.AttributeName, MoveTemp(NewAttribute));
				}
			}
			return NewAttributes;
		};

		static auto CreateLane = [](const FRoadLaneProfile& Src)
		{
			FRoadLane NewLane{};
			NewLane.Attributes = CreateAttributes(Src.Attributes);
			NewLane.Width.AddKey(0, Src.Width);
			NewLane.Width.Keys[0].InterpMode = ERichCurveInterpMode::RCIM_Cubic;
			NewLane.Width.Keys[0].TangentMode = ERichCurveTangentMode::RCTM_Auto;
			NewLane.bSkipProcrdureGeneration = Src.bSkipProcrdureGeneration;
			NewLane.Direction = Src.Direction;
			NewLane.LaneInstance = Src.LaneInstance;
			return NewLane;
		};

		TargetSpline->GetLaneSections().Empty();
		auto& NewSection = TargetSpline->GetLaneSections().Add_GetRef({});
		for (auto& LaneProfile : RoadProfile.Left)
		{
			NewSection.Left.Add(CreateLane(LaneProfile));
		}
		for (auto& LaneProfile : RoadProfile.Right)
		{
			NewSection.Right.Add(CreateLane(LaneProfile));
		}
		NewSection.Attributes = CreateAttributes(RoadProfile.CenterAttributes);
		TargetSpline->UpdateRoadLayout();
	}

	void FitFirstSplinePoint(URoadSplineComponent* TargetSpline, const ULaneConnection* LaneConnection, bool bUpdateSpline)
	{
		if (TargetSpline->GetNumberOfSplinePoints() >= 2 && LaneConnection)
		{
			FTransform Transform = LaneConnection->EvalTransform(0.0, ESplineCoordinateSpace::World);
			double TangentSize = (TargetSpline->GetLocationAtSplinePoint(1, ESplineCoordinateSpace::World) - Transform.GetLocation()).Size2D();
			FVector ForwardVector = Transform.GetRotation().GetForwardVector();
			TargetSpline->SetTangentAtSplinePoint(0, ForwardVector * TangentSize, ESplineCoordinateSpace::World, bUpdateSpline);
		}
	}

	void FitLastSplinePoint(URoadSplineComponent* TargetSpline, const ULaneConnection* LaneConnection, bool bUpdateSpline)
	{
		if (TargetSpline->GetNumberOfSplinePoints() >= 2 && LaneConnection)
		{
			int LastPointIndex = TargetSpline->GetNumberOfSplinePoints() - 1;
			const FTransform Transform = LaneConnection->EvalTransform(0.0, ESplineCoordinateSpace::World);
			const double TangentSize = (TargetSpline->GetLocationAtSplinePoint(LastPointIndex - 1, ESplineCoordinateSpace::World) - Transform.GetLocation()).Size2D();
			TargetSpline->SetRotationAtSplinePoint_Fixed(LastPointIndex, Transform.Rotator(), ESplineCoordinateSpace::World, false);
			auto& Point = TargetSpline->SplineCurves.Position.Points[LastPointIndex];
			Point.LeaveTangent *= TangentSize;
			Point.ArriveTangent *= TangentSize;
			Point.InterpMode = EInterpCurveMode::CIM_CurveUser;
			if (bUpdateSpline)
			{
				TargetSpline->UpdateSpline();
			}
		}
	}

	template <typename Predicate>
	FName CreateUniqueName(const FName& InBaseName, Predicate IsUnique)
	{
		FName CurrentName = InBaseName;
		int32 CurrentIndex = 0;

		while (!IsUnique(CurrentName))
		{
			FString PossibleName = InBaseName.ToString() + TEXT("_") + FString::FromInt(CurrentIndex++);
			CurrentName = FName(*PossibleName);
		}

		return CurrentName;
	}

	UEditorInteractiveToolsContext* GetInteractiveToolsContext()
	{
		if (FLevelEditorModule* LevelEditorModule = FModuleManager::GetModulePtr<FLevelEditorModule>(TEXT("LevelEditor")))
		{
			if (TSharedPtr<ILevelEditor> LevelEditorPtr = LevelEditorModule->GetLevelEditorInstance().Pin())
			{
				FEditorModeTools& EditorModeTools = LevelEditorPtr->GetEditorModeManager();
				if (UEdMode* EdMode = EditorModeTools.GetActiveScriptableMode(UModelingToolsEditorMode::EM_ModelingToolsEditorModeId))
				{
					return EdMode->GetInteractiveToolsContext(EToolsContextScope::EdMode);
				}
			}
		}
		return nullptr;
	}
}

// -------------------------------------------------------------------------------------------------------------------
FRoadLaneSectionProfile* FRoadDrawProfilePicker::GetProfile() const
{
	FRoadLaneSectionProfile* Ret = nullptr;
	UUnrealDrivePresetBase::ForEachPreset<UUnrealDrivePreset>([this, &Ret](const UUnrealDrivePreset* Preset)
	{
		for (auto& It : Preset->RoadLanesProfiles)
		{
			if (It.GetFullName() == ProfileName)
			{
				Ret = const_cast<FRoadLaneSectionProfile*>(&It);
			}
		}
	});
	return Ret;
}

UDrawRoadToolProperties::UDrawRoadToolProperties()
{
}

// -------------------------------------------------------------------------------------------------------------------
void UDrawRoadTool::Setup()
{
	UInteractiveTool::Setup();

	Settings = NewObject<UDrawRoadToolProperties>(this);
	Settings->RestoreProperties(this);
	AddToolPropertySource(Settings);

	Settings->ToolMode = ToolMode;

	if (ToolMode == EDrawRoadToolMode::ExistingActor)
	{
		check(TargetActor.IsValid());
	}

	SetToolDisplayName(LOCTEXT("DrawSplineToolName", "Draw Spline"));
	GetToolManager()->DisplayMessage(
		LOCTEXT("DrawSplineToolDescription", "Draw a spline to replace an existing one or add it to an actor."),
		EToolMessageLevel::UserNotification);

	PlaneMechanic = NewObject<UConstructionPlaneMechanic>(this);
	PlaneMechanic->Setup(this);
	PlaneMechanic->Initialize(GetTargetWorld(), FFrame3d(FVector3d::Zero(), FVector3d::UnitX()));
	PlaneMechanic->bShowGrid = Settings->bHitCustomPlane;
	PlaneMechanic->CanUpdatePlaneFunc = [this] { return Settings->bHitCustomPlane; };
	Settings->WatchProperty(Settings->bHitCustomPlane, [this](bool) 
	{
		PlaneMechanic->bShowGrid = Settings->bHitCustomPlane;
	});

	ClickOrDragBehavior = NewObject<USingleClickOrDragInputBehavior>();
	ClickOrDragBehavior->Initialize(this, this);
	AddInputBehavior(ClickOrDragBehavior);

	// Make sure the plane mechanic captures clicks first, to ensure it sees ctrl+clicks to reposition the plane
	PlaneMechanic->UpdateClickPriority(ClickOrDragBehavior->GetPriority().MakeHigher());

	Settings->WatchProperty(Settings->bLoop, [this](bool)
	{
		if (ensure(WorkingSpline.IsValid()))
		{
			WorkingSpline->SetClosedLoop(Settings->bLoop);
			bNeedToRerunConstructionScript = true;
		}
	});

	ReCreatePreview();

	Settings->WatchProperty(Settings->FilledInstance, [this](const TInstancedStruct<FRoadLaneInstance>& FilledInstance)
	{
		ReCreatePreview();
	});

	Settings->WatchProperty(Settings->OutputMode, [this](ENewRoadActorType)
	{
		ReCreatePreview();
	});

	Settings->WatchProperty(Settings->DrawMode, [this](EDrawRoadDrawMode)
	{
		ReCreatePreview();
	});

	Settings->WatchProperty(Settings->LanesSource, [this](ERoadLanesProfileSource)
	{
		ReCreatePreview();
	});

	Settings->WatchProperty(Settings->BlueprintToCreate, [this](TWeakObjectPtr<UBlueprint>) 
	{
		ReCreatePreview();
	});

	Settings->SilentUpdateWatched();


	if (UEditorInteractiveToolsContext* InteractiveToolsContext = DrawSplineToolLocals::GetInteractiveToolsContext())
	{
		bCahedEnableRenderingDuringHitProxyPass = InteractiveToolsContext->GetEnableRenderingDuringHitProxyPass();
		InteractiveToolsContext->SetEnableRenderingDuringHitProxyPass(true);
	}

	
	GetTargetWorld()->GetSubsystem<UUnrealDriveSubsystem>()->CleanObservedConnections();
}

// Set things up for a new output mode or destination
void UDrawRoadTool::ReCreatePreview()
{
	using namespace DrawSplineToolLocals;

	// Setting up the previews seems to be the most error prone part of the tool because editor duplicating, hiding
	// from outliner, and avoiding emitting undo/redo transactions seems to be quite finnicky...

	// This function is sometimes called from inside transactions (such as tool start, or dragging the "component to replace"
	// slider). Several calls here would transact in that case (for instance, the Destroy() calls on the previews seem
	// to do it), which we generally don't want to do. So we disable transacting in this function with the hack below.
	// Note that we still have to take care that any editor functions we call don't open their own transactions...
	ITransaction* UndoState = GUndo;
	GUndo = nullptr; // Pretend we're not in a transaction
	ON_SCOPE_EXIT{ GUndo = UndoState; }; // Revert later


	// Keep the previous spline/preview temporarily so we can transfer over spline data
	// when we make new previews
	URoadSplineComponent* PreviousSpline = WorkingSpline.Get();

	if (WorkingSpline.IsValid())
	{
		WorkingSpline->DestroyComponent();
	}

	if (PreviewActor)
	{
		PreviewActor->Destroy();
	}

	PreviewActor = nullptr;
	WorkingSpline = nullptr;
	
	auto FallbackSplinePlacement = [this]()
	{
		FRotator Rotation(0.0f, 0.0f, 0.0f);
		FActorSpawnParameters SpawnInfo;
		SpawnInfo.ObjectFlags = RF_Transient;
		PreviewActor = GetTargetWorld()->SpawnActor<APreviewGeometryActor>(FVector::ZeroVector, Rotation, SpawnInfo);
		USceneComponent* RootComponent = NewObject<USceneComponent>(PreviewActor);
		PreviewActor->AddOwnedComponent(RootComponent);
		PreviewActor->SetRootComponent(RootComponent);
		RootComponent->RegisterComponent();
		WorkingSpline = CreateNewSplineInActor(PreviewActor);
		WorkingSpline->GetRoadLayout().FilledInstance = Settings->FilledInstance;
	};

	// Set up the new preview
	if (ToolMode == EDrawRoadToolMode::ExistingActor || Settings->OutputMode == ENewRoadActorType::CreateEmptyActor)
	{
		FallbackSplinePlacement();
	}
	else if( Settings->OutputMode == ENewRoadActorType::CreateBlueprint)
	{
		if (Settings->BlueprintToCreate.IsValid()
			&& Settings->BlueprintToCreate->GeneratedClass != nullptr
			&& !Settings->BlueprintToCreate->GeneratedClass->HasAnyClassFlags(CLASS_NotPlaceable | CLASS_Abstract))
		{
			// Instantiate the blueprint
			PreviewActor = FActorFactoryAssetProxy::AddActorForAsset(
				Settings->BlueprintToCreate.Get(),
				/*bSelectActors =*/ false,
				// Important that we don't use the default (RF_Transactional) here, or else we'll end up
				// issuing an undo transaction in this call.
				EObjectFlags::RF_Transient);

			if (PreviewActor)
			{
				// Hide this preview from outliner
				FModelingToolsSetActorHiddenInSceneOutliner Hider(PreviewActor, true);
				Hider.RefreshOutliner();

				WorkingSpline = CreateNewSplineInActor(PreviewActor);
				bNeedToRerunConstructionScript = true;
			}
			else
			{
				FallbackSplinePlacement();
			}
		}
		else
		{
			FallbackSplinePlacement();
		}
	}

	if (ensure(WorkingSpline.IsValid()))
	{
		if (PreviousSpline)
		{
			CopySplineToSpline(*PreviousSpline, *WorkingSpline);
		}
		else
		{
			WorkingSpline->ClearSplinePoints();
		}

		InitRoadProfile(WorkingSpline.Get());

		WorkingSpline->SetClosedLoop(Settings->bLoop);

		// This has to be set so that construction script reruns transfer over current spline state.
		WorkingSpline->bSplineHasBeenEdited = true;

		// Get the index of the spline in the components array for recapturing on construction script reruns.
		if (PreviewActor)
		{
			TInlineComponentArray<URoadSplineComponent*> SplineComponents;
			PreviewActor->GetComponents<URoadSplineComponent>(SplineComponents);
			SplineRecaptureIndex = SplineComponents.IndexOfByKey(WorkingSpline.Get());
			ensure(SplineRecaptureIndex >= 0);
		}
	}
}

void UDrawRoadTool::Shutdown(EToolShutdownType ShutdownType)
{
	using namespace DrawSplineToolLocals;

	LongTransactions.CloseAll(GetToolManager());

	Settings->SaveProperties(this);

	if (ShutdownType == EToolShutdownType::Accept && WorkingSpline.IsValid() && WorkingSpline->GetNumberOfSplinePoints() > 0)
	{
		GenerateAsset();
	}

	PlaneMechanic->Shutdown();
	
	
	if (WorkingSpline.IsValid())
	{
		WorkingSpline->DestroyComponent();
	}

	if (PreviewActor)
	{
		PreviewActor->Destroy();
	}


	if (UEditorInteractiveToolsContext* InteractiveToolsContext = DrawSplineToolLocals::GetInteractiveToolsContext())
	{
		InteractiveToolsContext->SetEnableRenderingDuringHitProxyPass(bCahedEnableRenderingDuringHitProxyPass);
	}


	GetTargetWorld()->GetSubsystem<UUnrealDriveSubsystem>()->CleanObservedConnections();

	Super::Shutdown(ShutdownType);
}

void UDrawRoadTool::GenerateAsset()
{
	using namespace DrawSplineToolLocals;

	URoadSplineComponent* OutputSpline = nullptr;

	auto CreateSpline = [this](AActor* TargetActor, bool bRerunConstructionScripts)
	{
		TargetActor->Modify();

		FName NewComponentName = *FComponentEditorUtils::GenerateValidVariableName(URoadSplineComponent::StaticClass(), TargetActor);
		URoadSplineComponent* OutputSpline = NewObject<URoadSplineComponent>(TargetActor, URoadSplineComponent::StaticClass(), NewComponentName, RF_Transactional);
		OutputSpline->SetupAttachment(TargetActor->GetRootComponent());
		OutputSpline->OnComponentCreated();
		TargetActor->AddInstanceComponent(OutputSpline);
		OutputSpline->ResetRelativeTransform();
		OutputSpline->RegisterComponent();

		OutputSpline->GetRoadLayout() = WorkingSpline->GetRoadLayout();
		OutputSpline->GetRoadLayout().UpdateLayout(OutputSpline);
		CopySplineToSpline(*WorkingSpline, *OutputSpline, true);

		if (StartLaneConnection.IsValid())
		{
			OutputSpline->GetPredecessorConnection()->ConnectTo(StartLaneConnection.Get());
			OutputSpline->GetPredecessorConnection()->SetTransformFormOuter();
		}

		if (EndLaneConnection.IsValid())
		{
			OutputSpline->GetSuccessorConnection()->ConnectTo(EndLaneConnection.Get());
			FitLastSplinePoint(OutputSpline, EndLaneConnection.Get(), true);
			OutputSpline->GetSuccessorConnection()->SetTransformFormOuter();
		}

		OutputSpline->PostEditChange();

		if (bRerunConstructionScripts)
		{
			TargetActor->RerunConstructionScripts();
		}
		return OutputSpline;
	};

	auto CreateSplineAndActor = [this, &CreateSpline]()
	{
		// Get centroid of spline
		int32 NumSplinePoints = WorkingSpline->GetNumberOfSplinePoints();
		FVector3d Center = FVector3d::Zero();
		for (int32 i = 0; i < NumSplinePoints; ++i)
		{
			Center += WorkingSpline->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::World);
		}
		Center /= NumSplinePoints;

		// Spawning via a factory is editor-only
		UActorFactoryEmptyActor* EmptyActorFactory = NewObject<UActorFactoryEmptyActor>();
		FAssetData AssetData(EmptyActorFactory->GetDefaultActorClass(FAssetData()));
		FActorSpawnParameters SpawnParams;
		SpawnParams.Name = TEXT("Spline");
		SpawnParams.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;
		AActor* NewActor = EmptyActorFactory->CreateActor(AssetData.GetAsset(),
			TargetWorld->GetCurrentLevel(),
			FTransform(Center),
			SpawnParams);

		// This is also editor-only: it's the label that shows up in the hierarchy
		FActorLabelUtilities::SetActorLabelUnique(NewActor, TEXT("RoadActor"));

		return CreateSpline(NewActor, false);
	};

	GetToolManager()->BeginUndoTransaction(LOCTEXT("DrawSplineTransactionName", "Draw Spline"));


	if (ToolMode == EDrawRoadToolMode::NewActor)
	{
		if (Settings->OutputMode == ENewRoadActorType::CreateEmptyActor)
		{
			OutputSpline = CreateSplineAndActor();
		}
		else if (Settings->OutputMode == ENewRoadActorType::CreateBlueprint)
		{
			bool bCanCreateActor = Settings->BlueprintToCreate.IsValid()// != nullptr
				&& Settings->BlueprintToCreate->GeneratedClass != nullptr
				&& !Settings->BlueprintToCreate->GeneratedClass->HasAnyClassFlags(CLASS_NotPlaceable | CLASS_Abstract);

			if (bCanCreateActor)
			{
				// Instantiate the blueprint
				AActor* NewActor = FActorFactoryAssetProxy::AddActorForAsset(Settings->BlueprintToCreate.Get(), /*bSelectActors =*/ false);
				if (NewActor)
				{
					OutputSpline = CreateSpline(NewActor, true);
				}
				else
				{
					OutputSpline = CreateSplineAndActor();
				}
			}
			else
			{
				OutputSpline = CreateSplineAndActor();
			}
		}
	}
	else if (ToolMode == EDrawRoadToolMode::ExistingActor)
	{
		if (TargetActor.IsValid())
		{
			OutputSpline = CreateSpline(TargetActor.Get(), true);
		}
		else
		{
			OutputSpline = CreateSplineAndActor();
		}
	}

	// TODO: Someday when we support component selection, we should select OutputSpline directly.
	if (OutputSpline)
	{
		ToolSelectionUtil::SetNewActorSelection(GetToolManager(), OutputSpline->GetAttachmentRootActor());
	}

	GetToolManager()->EndUndoTransaction();
}

// Helper to add a point given a hit location and hit normal
void UDrawRoadTool::AddSplinePoint(const FVector3d& HitLocation, const FVector3d& HitNormal)
{
	using namespace DrawSplineToolLocals;
	if (!WorkingSpline.IsValid())
	{
		return;
	}

	int32 NumSplinePoints = WorkingSpline->GetNumberOfSplinePoints();
	FVector3d UpVectorToUse = GetUpVectorToUse(HitLocation, HitNormal, NumSplinePoints);

	WorkingSpline->AddSplinePoint(HitLocation, ESplineCoordinateSpace::World, /*bUpdate =*/ false);
	WorkingSpline->SetUpVectorAtSplinePoint(NumSplinePoints, UpVectorToUse, ESplineCoordinateSpace::World, /*bUpdate =*/ true);
}

FVector3d UDrawRoadTool::GetUpVectorToUse(const FVector3d& HitLocation, const FVector3d& HitNormal, int32 NumSplinePointsBeforehand)
{
	FVector3d UpVectorToUse = HitNormal;
	switch (Settings->UpVectorMode)
	{
	case EDrawRoadUpVectorMode::AlignToPrevious:
	{
		if (NumSplinePointsBeforehand == 0)
		{
			// TODO: Maybe add some different options of what normal to start with
		}
		else if (NumSplinePointsBeforehand > 1)
		{
			UpVectorToUse = WorkingSpline->GetUpVectorAtSplinePoint(NumSplinePointsBeforehand - 1, ESplineCoordinateSpace::World);
		}
		else // if NumSplinePointsBeforehand == 1
		{
			// If there's only one point, GetUpVectorAtSplinePoint is unreliable because it seeks to build a
			// quaternion from the tangent and the set up vector, and the tangent is zero. We want to use
			// the "stored" up vector directly.
			FVector3d LocalUpVector = WorkingSpline->SplineCurves.Rotation.Points[0].OutVal.RotateVector(WorkingSpline->DefaultUpVector);
			UpVectorToUse = WorkingSpline->GetComponentTransform().TransformVectorNoScale(LocalUpVector);
		}
		break;
	}
	case EDrawRoadUpVectorMode::UseHitNormal:
		break;
	}

	return UpVectorToUse;
}

bool UDrawRoadTool::Raycast(const FRay& InWorldRay, FVector3d& HitLocationOut, FVector3d& HitNormalOut, double& HitTOut) const
{
	double BestHitT = TNumericLimits<double>::Max();

	FRay WorldRay = InWorldRay;
	if (CameraState.bIsOrthographic)
	{
		// Fix buf bug of Orthographic projection in UE 5.5. 
		// Deatils: for Orthographic projection WorldRay.Origin.Z > max float32 precision, but type of FHitResult.Distance is float32.
		// Therefore the distance to the nearest object is not determined correctly
		WorldRay.Origin.Z = 4'194'304;
	}
	
	if (Settings->bHitCustomPlane)
	{
		FVector IntersectionPoint;
		bool bHitPlane = false;
		GizmoMath::RayPlaneIntersectionPoint(PlaneMechanic->Plane.Origin, PlaneMechanic->Plane.Z(),
			WorldRay.Origin, WorldRay.Direction, bHitPlane, IntersectionPoint);

		if (bHitPlane)
		{
			HitLocationOut = IntersectionPoint;
			HitNormalOut = PlaneMechanic->Plane.Z();
			HitTOut = WorldRay.GetParameter(IntersectionPoint);
			BestHitT = HitTOut;
		}
	}

	if (Settings->bHitWorld)
	{
		FHitResult GeometryHit;
		TArray<const UPrimitiveComponent*> ComponentsToIgnore;
		if (PreviewActor)
		{
			PreviewActor->GetComponents<const UPrimitiveComponent>(ComponentsToIgnore);
		}
		if (ToolSceneQueriesUtil::FindNearestVisibleObjectHit(this, GeometryHit, WorldRay, &ComponentsToIgnore)
			&& GeometryHit.Distance < BestHitT)
		{
			HitLocationOut = GeometryHit.ImpactPoint;
			HitNormalOut = GeometryHit.ImpactNormal;
			HitTOut = GeometryHit.Distance;
			BestHitT = HitTOut;
		}
	}

	// Only raycast the ground plane / ortho background if we didn't hit anything else
	if (Settings->bHitGroundPlanes && BestHitT == TNumericLimits<double>::Max())
	{
		FVector3d PlaneNormal = CameraState.bIsOrthographic ? -WorldRay.Direction : FVector3d::UnitZ();
		FVector IntersectionPoint;
		bool bHitPlane = false;
		GizmoMath::RayPlaneIntersectionPoint(FVector3d::Zero(), PlaneNormal,
			WorldRay.Origin, WorldRay.Direction, bHitPlane, IntersectionPoint);

		if (bHitPlane)
		{
			HitLocationOut = IntersectionPoint;
			HitNormalOut = PlaneNormal;
			HitTOut = WorldRay.GetParameter(IntersectionPoint);
			BestHitT = HitTOut;
		}
	}

	if (Settings->ClickOffset != 0.0)
	{
		FVector3d OffsetDirection = HitNormalOut;
		if (Settings->OffsetMethod == ERoadOffsetMethod::Custom)
		{
			OffsetDirection = Settings->OffsetDirection.GetSafeNormal(UE_SMALL_NUMBER, FVector3d::UnitZ());
		}

		HitLocationOut += OffsetDirection * Settings->ClickOffset;
	}
	
	return BestHitT < TNumericLimits<double>::Max();
}

bool UDrawRoadTool::MouseTrace(const FRay& WorldRay, FMouseTraceResult& Result) const
{
	if (ConnectionUnderCursor.IsValid())
	{
		FTransform Transform = ConnectionUnderCursor->EvalTransform(0.0, ESplineCoordinateSpace::World);
		Result.Location = Transform.GetLocation();
		Result.UpVector = Transform.GetRotation().GetUpVector();
		Result.ForwardVector = Transform.GetRotation().GetForwardVector();
		Result.Connection = ConnectionUnderCursor;
		Result.HitT = (Transform.GetLocation() - WorldRay.Origin).Size();
		return true;
	}

	if (Raycast(WorldRay, Result.Location, Result.UpVector, Result.HitT))
	{
		Result.ForwardVector = FVector3d::Zero();
		return true;
	}

	return false;
}

bool UDrawRoadTool::FinishDraw()
{
	if (FLevelEditorModule* LevelEditorModule = FModuleManager::GetModulePtr<FLevelEditorModule>(TEXT("LevelEditor")))
	{
		if (TSharedPtr<ILevelEditor> LevelEditorPtr = LevelEditorModule->GetLevelEditorInstance().Pin())
		{
			FEditorModeTools& EditorModeTools = LevelEditorPtr->GetEditorModeManager();
			if (UEdMode* EdMode = EditorModeTools.GetActiveScriptableMode(UModelingToolsEditorMode::EM_ModelingToolsEditorModeId))
			{
				EdMode->GetToolManager()->DeactivateTool(EToolSide::Left, EToolShutdownType::Accept);
				return true;
			}
		}
	}

	return false;
}

FInputRayHit UDrawRoadTool::IsHitByClick(const FInputDeviceRay& ClickPos)
{
	FMouseTraceResult HitResult;
	if (MouseTrace(ClickPos.WorldRay, HitResult))
	{
		return FInputRayHit(HitResult.HitT);
	}
	return FInputRayHit();
}

void UDrawRoadTool::InitRoadProfile(URoadSplineComponent* TargetSpline) const
{
	using namespace DrawSplineToolLocals;
	static const FRoadLaneSectionProfile EmptyProfile{};
	
	if (!StartLaneConnection.IsValid())
	{
		const auto* Profile = Settings->LanesSource == ERoadLanesProfileSource::RoadProfile ? Settings->DrawProfile.GetProfile() : &EmptyProfile;
		SetRoadProfile(TargetSpline, Profile ? *Profile : EmptyProfile);
	}
	else
	{
		switch (Settings->LanesSource)
		{
		case ERoadLanesProfileSource::RoadProfile:
			{
				const auto* Profile = Settings->DrawProfile.GetProfile();
				SetRoadProfile(TargetSpline, Profile ? *Profile : EmptyProfile);
			}
			break;
		case ERoadLanesProfileSource::OneLane:
		case ERoadLanesProfileSource::RightSide:
			CopyRoadProfileFromConnection_OneSide(StartLaneConnection.Get(), TargetSpline, Settings->LanesSource == ERoadLanesProfileSource::RightSide);
			break;
		case ERoadLanesProfileSource::BothSides:
			if (FMath::Abs(StartLaneConnection->GetLaneIndex()) == 1)
			{
				CopyRoadProfileFromConnection_BothSides(StartLaneConnection.Get(), TargetSpline);
			}
			else
			{
				SetRoadProfile(TargetSpline, EmptyProfile);
			}
			break;
		}
	}

	TargetSpline->GetRoadLayout().FilledInstance = Settings->FilledInstance;
}

void UDrawRoadTool::OnClicked(const FInputDeviceRay& ClickPos)
{
	using namespace DrawSplineToolLocals;

	if (!WorkingSpline.IsValid())
	{
		return;
	}

	FMouseTraceResult HitResult;
	if (!MouseTrace(ClickPos.WorldRay, HitResult))
	{
		return;
	}

	AddSplinePoint(HitResult.Location, HitResult.UpVector);


	if (WorkingSpline->GetNumberOfSplinePoints() == 1)
	{
		if (HitResult.Connection.IsValid())
		{
			StartLaneConnection = HitResult.Connection;
		}
		InitRoadProfile(WorkingSpline.Get());
		bConnectionsCashIsDirty = true;
	}

	if (WorkingSpline->GetNumberOfSplinePoints() == 2)
	{
		FitFirstSplinePoint(WorkingSpline.Get(), StartLaneConnection.Get(), true);
	}

	int32 PointIndex = WorkingSpline->GetNumberOfSplinePoints() - 1;

	if (HitResult.Connection.IsValid() || Settings->DrawMode == EDrawRoadDrawMode::TangentDrag)
	{
		WorkingSpline->SetTangentAtSplinePoint(PointIndex, HitResult.ForwardVector, ESplineCoordinateSpace::World, true);

		GetToolManager()->EmitObjectChange(this,
			MakeUnique<FTangentPointInsertionChange>(
				HitResult.Location,
				WorkingSpline->GetUpVectorAtSplinePoint(PointIndex, ESplineCoordinateSpace::World),
				HitResult.ForwardVector),
			AddPointTransactionName);
	}
	else
	{
		GetToolManager()->EmitObjectChange(this,
			MakeUnique<FSimplePointInsertionChange>(
				HitResult.Location,
				WorkingSpline->GetUpVectorAtSplinePoint(PointIndex, ESplineCoordinateSpace::World)),
			AddPointTransactionName);
	}

	if (HitResult.Connection.IsValid() && WorkingSpline->GetNumberOfSplinePoints() > 1)
	{
		EndLaneConnection = HitResult.Connection.Get();
		FinishDraw();
	}

	bNeedToRerunConstructionScript = true;
}

FInputRayHit UDrawRoadTool::CanBeginClickDragSequence(const FInputDeviceRay& PressPos)
{
	FMouseTraceResult HitResult;
	if (MouseTrace(PressPos.WorldRay, HitResult))
	{
		return FInputRayHit(HitResult.HitT);
	}
	return FInputRayHit();
}

void UDrawRoadTool::OnClickPress(const FInputDeviceRay& PressPos)
{
	using namespace DrawSplineToolLocals;

	FMouseTraceResult HitResult;

	LongTransactions.Open(DrawSplineToolLocals::AddPointTransactionName, GetToolManager());

	// Regardless of DrawMode, start by placing a point, though don't emit a transaction until mouse up
	if (ensure(MouseTrace(PressPos.WorldRay, HitResult)))
	{
		AddSplinePoint(HitResult.Location, HitResult.UpVector);

		if (Settings->DrawMode == EDrawRoadDrawMode::TangentDrag)
		{
			if (HitResult.Connection.IsValid())
			{
				if (WorkingSpline->GetNumberOfSplinePoints() == 1)
				{
					StartLaneConnection = HitResult.Connection;
				}
				else if (WorkingSpline->GetNumberOfSplinePoints() > 1)
				{
					EndLaneConnection = HitResult.Connection;
				}
			}
		}

		if (WorkingSpline->GetNumberOfSplinePoints() == 2)
		{
			FitFirstSplinePoint(WorkingSpline.Get(), StartLaneConnection.Get(), true);
		}
		
		bNeedToRerunConstructionScript = bNeedToRerunConstructionScript || Settings->bRerunConstructionScriptOnDrag;
	}
}

void UDrawRoadTool::OnClickDrag(const FInputDeviceRay& DragPos)
{
	using namespace DrawSplineToolLocals;

	int32 NumSplinePoints = WorkingSpline->GetNumberOfSplinePoints();
	if (!ensure(NumSplinePoints > 0))
	{
		return;
	}

	FMouseTraceResult HitResult;
	if (!MouseTrace(DragPos.WorldRay, HitResult))
	{
		return;
	}
	
	const int LastPointIndex = NumSplinePoints - 1;

	switch (Settings->DrawMode)
	{
	case EDrawRoadDrawMode::ClickAutoTangent:
	{
		// Drag the last placed point
		WorkingSpline->SetLocationAtSplinePoint(LastPointIndex, HitResult.Location, ESplineCoordinateSpace::World, false);
		auto& Point = WorkingSpline->SplineCurves.Position.Points[LastPointIndex];
		if (HitResult.Connection.IsValid())
		{
			FitLastSplinePoint(WorkingSpline.Get(), HitResult.Connection.Get(), false);
		}
		else
		{
			FVector3d UpVector = GetUpVectorToUse(HitResult.Location, HitResult.UpVector, NumSplinePoints);
			WorkingSpline->SetUpVectorAtSplinePoint(LastPointIndex, UpVector, ESplineCoordinateSpace::World, false);
			Point.InterpMode = EInterpCurveMode::CIM_CurveAuto;
		}
		WorkingSpline->UpdateSpline();
		break;
	}
	case EDrawRoadDrawMode::TangentDrag:
	{
		// Set the tangent
		FVector3d LastPoint = WorkingSpline->GetLocationAtSplinePoint(LastPointIndex, ESplineCoordinateSpace::World);
		FVector3d Tangent = (HitResult.Location - LastPoint) / GetTangentScale();
		WorkingSpline->SetTangentAtSplinePoint(LastPointIndex, Tangent, ESplineCoordinateSpace::World, true);
		bDrawTangentForLastPoint = true;
		break;
	}
	}

	if (WorkingSpline->GetNumberOfSplinePoints() == 2)
	{
		FitFirstSplinePoint(WorkingSpline.Get(), StartLaneConnection.Get(), true);
	}
	
	bNeedToRerunConstructionScript = bNeedToRerunConstructionScript || Settings->bRerunConstructionScriptOnDrag;
}

void UDrawRoadTool::OnClickRelease(const FInputDeviceRay& ReleasePos)
{
	OnClickDrag(ReleasePos);

	FMouseTraceResult HitResult;
	if (!MouseTrace(ReleasePos.WorldRay, HitResult))
	{
		return;
	}

	if (HitResult.Connection.IsValid())
	{
		if (Settings->DrawMode == EDrawRoadDrawMode::ClickAutoTangent)
		{
			if (WorkingSpline->GetNumberOfSplinePoints() > 1)
			{
				EndLaneConnection = HitResult.Connection;
			}
		}

		if (WorkingSpline->GetNumberOfSplinePoints() == 1)
		{
			StartLaneConnection = HitResult.Connection;
		}
	}

	if (WorkingSpline->GetNumberOfSplinePoints() == 1)
	{
		InitRoadProfile(WorkingSpline.Get());
		bConnectionsCashIsDirty = true;
	}

	if (WorkingSpline->GetNumberOfSplinePoints() > 1 && EndLaneConnection.IsValid())
	{
		FinishDraw();
		return;
	}

	OnTerminateDragSequence();
}

void UDrawRoadTool::OnTerminateDragSequence()
{
	using namespace DrawSplineToolLocals;

	bDrawTangentForLastPoint = false;
	bNeedToRerunConstructionScript = true;

	int32 NumSplinePoints = WorkingSpline->GetNumberOfSplinePoints();

	// Emit the appropriate undo transaction
	switch (Settings->DrawMode)
	{
	case EDrawRoadDrawMode::ClickAutoTangent:
	{
		GetToolManager()->EmitObjectChange(this,
			MakeUnique<FSimplePointInsertionChange>(
				WorkingSpline->GetLocationAtSplinePoint(NumSplinePoints - 1, ESplineCoordinateSpace::World),
				WorkingSpline->GetUpVectorAtSplinePoint(NumSplinePoints - 1, ESplineCoordinateSpace::World)),
			AddPointTransactionName);
		break;
	}
	case EDrawRoadDrawMode::TangentDrag:
	{
		GetToolManager()->EmitObjectChange(this,
			MakeUnique<FTangentPointInsertionChange>( 
				WorkingSpline->GetLocationAtSplinePoint(NumSplinePoints - 1, ESplineCoordinateSpace::World),
				WorkingSpline->GetUpVectorAtSplinePoint(NumSplinePoints - 1, ESplineCoordinateSpace::World),
				WorkingSpline->GetTangentAtSplinePoint(NumSplinePoints - 1, ESplineCoordinateSpace::World)),
			AddPointTransactionName);
		break;
	}
	}

	LongTransactions.Close(GetToolManager());
}

void UDrawRoadTool::OnTick(float DeltaTime)
{
	if (PlaneMechanic)
	{
		PlaneMechanic->Tick(DeltaTime);
	}

	// check if we've invalidated the WorkingSpline
	if (PreviewActor && !WorkingSpline.IsValid())
	{
		bNeedToRerunConstructionScript = true;
	}

	if (bNeedToRerunConstructionScript)
	{ 
		bNeedToRerunConstructionScript = false;
		if (PreviewActor)
		{
			PreviewActor->RerunConstructionScripts();

			// Rerunning the construction script can make us lose our reference to the spline, so try to
			// recapture.
			// TODO: This might be avoidable with FComponentReference?
			if (!WorkingSpline.IsValid())
			{
				TInlineComponentArray<URoadSplineComponent*> SplineComponents;
				PreviewActor->GetComponents<URoadSplineComponent>(SplineComponents);

				if (ensure(SplineRecaptureIndex >= 0 && SplineRecaptureIndex < SplineComponents.Num()))
				{
					WorkingSpline = SplineComponents[SplineRecaptureIndex];
				}
			}
		}
	}

	if (!WorkingSpline.IsValid())
	{
		GetToolManager()->PostActiveToolShutdownRequest(this, EToolShutdownType::Cancel, true, LOCTEXT("LostWorkingSpline", "The Draw Spline tool must close because the in-progress spline has been unexpectedly deleted."));
	}

	if (bConnectionsCashIsDirty)
	{
		GetTargetWorld()->GetSubsystem<UUnrealDriveSubsystem>()->CaptureConnections(
			WorkingSpline->GetNumberOfSplinePoints() == 0 ? WorkingSpline->GetPredecessorConnection() : WorkingSpline->GetSuccessorConnection(),
			UUnrealDriveSubsystem::FViewCameraState{
				CashedViewToProj,
				CashedViewRect,
				CameraState.Position,
				CameraState.bIsOrthographic,
				CameraState.OrthoWorldCoordinateWidth },
			GetDefault<UUnrealDriveEditorSettings>()->RoadConnectionsMaxViewDistance,
			GetDefault<UUnrealDriveEditorSettings>()->RoadConnectionMaxViewOrthoWidth);
		bConnectionsCashIsDirty = false;
	}

	ConnectionUnderCursor = nullptr;
	if (IToolsContextQueriesAPI* ContextAPI = GetToolManager()->GetContextQueriesAPI())
	{
		if (FViewport* Viewport = ContextAPI->GetFocusedViewport())
		{
			if (auto HitProx = CompVisUtils::GetHitProxy<HRoadLaneConnectionProxy>(Viewport, Viewport->GetMouseX(), Viewport->GetMouseY(), 5))
			{
				ConnectionUnderCursor = HitProx->Connection;
			}
		}
	}

}

void UDrawRoadTool::Render(IToolsContextRenderAPI* RenderAPI)
{
	using namespace DrawSplineToolLocals;

	Super::Render(RenderAPI);

	CashedViewToProj = RenderAPI->GetSceneView()->ViewMatrices.GetViewProjectionMatrix();
	CashedViewRect = RenderAPI->GetSceneView()->UnconstrainedViewRect;

	FViewCameraState NewCameraState;
	GetToolManager()->GetContextQueriesAPI()->GetCurrentViewState(NewCameraState);

	if (!NewCameraState.Position.Equals(CameraState.Position, 50.0) ||
		!NewCameraState.Orientation.Equals(CameraState.Orientation, 0.1) ||
		!FMath::IsNearlyEqual(NewCameraState.OrthoWorldCoordinateWidth, CameraState.OrthoWorldCoordinateWidth, 1.0) ||
		!FMath::IsNearlyEqual(NewCameraState.HorizontalFOVDegrees, CameraState.HorizontalFOVDegrees, 1.0) ||
		!FMath::IsNearlyEqual(NewCameraState.AspectRatio, CameraState.AspectRatio, 0.01) ||
		NewCameraState.bIsOrthographic != CameraState.bIsOrthographic)
	{
		bConnectionsCashIsDirty = true;
		CameraState = NewCameraState;
	}

	if (PlaneMechanic)
	{
		PlaneMechanic->Render(RenderAPI);
	}

	if (WorkingSpline.IsValid())
	{
		if (bDrawTangentForLastPoint)
		{
			DrawTangent(*WorkingSpline, WorkingSpline->GetNumberOfSplinePoints() - 1, *RenderAPI);
		}

		SplineUtil::FDrawSplineSettings DrawSettings;
		SplineUtil::DrawSpline(*WorkingSpline, *RenderAPI, DrawSettings);

		GetTargetWorld()->GetSubsystem<UUnrealDriveSubsystem>()->ForEachObservedConnection([&RenderAPI, this](const ULaneConnection* Connection, UUnrealDriveSubsystem::FConnectionInfo& Info)
		{
			FPrimitiveDrawInterface* PDI = RenderAPI->GetPrimitiveDrawInterface();

			auto & Lane = Connection->GetOwnedRoadLane();

			PDI->SetHitProxy(new HRoadLaneConnectionProxy(const_cast<ULaneConnection*>(Connection), Lane.GetStartSectionIndex(), Lane.GetLaneIndex()));

			DrawUtils::DrawRoadLaneConnection(
				Connection->IsSuccessorConnection(),
				Info.Transform,
				(ConnectionUnderCursor.IsValid() && ConnectionUnderCursor == Connection)
					? GetDefault<UUnrealDriveEditorSettings>()->GetLaneConnectionSelectedMaterialDyn()->GetRenderProxy()
					: GetDefault<UUnrealDriveEditorSettings>()->GetLaneConnectionMaterialDyn()->GetRenderProxy(),
				PDI,
				RenderAPI->GetSceneView(),
				SDPG_Foreground);

			PDI->SetHitProxy(nullptr);
		});
	}
}

void UDrawRoadTool::OnPropertyModified(UObject* PropertySet, FProperty* Property)
{
	//UE_LOG(LogUnrealDrive, Log, TEXT("*** %s"), *Property->GetName());
	const FName PropertyName = Property->GetFName();

	if (PropertyName == GET_MEMBER_NAME_CHECKED(FRoadDrawProfilePicker, ProfileName))
	{
		ReCreatePreview();
	}

}

bool UDrawRoadTool::CanAccept() const
{
	return WorkingSpline.IsValid() && WorkingSpline->GetNumberOfSplinePoints() > 0;
}

// To be called by builder
void UDrawRoadTool::SetSelectedActor(AActor* Actor)
{
	if (IsValid(Actor))
	{
		TargetActor = Actor;
		ToolMode = EDrawRoadToolMode::ExistingActor;
	}
	else
	{
		TargetActor = nullptr;
		ToolMode = EDrawRoadToolMode::NewActor;
	}
}
void UDrawRoadTool::SetWorld(UWorld* World)
{
	TargetWorld = World;
}

void UDrawRoadTool::FSplineChange::Apply(UObject* Object)
{
	UDrawRoadTool* Tool = Cast<UDrawRoadTool>(Object);
	if (!ensure(Tool))
	{
		return;
	}
	TWeakObjectPtr<URoadSplineComponent> Spline = Tool->WorkingSpline;
	if (!ensure(Spline.IsValid()))
	{
		return;
	}

	Apply(*Spline);

	Tool->bNeedToRerunConstructionScript = true;
}

void UDrawRoadTool::FSplineChange::Revert(UObject* Object)
{
	UDrawRoadTool* Tool = Cast<UDrawRoadTool>(Object);
	if (!ensure(Tool))
	{
		return;
	}
	TWeakObjectPtr<URoadSplineComponent> Spline = Tool->WorkingSpline;
	if (!ensure(Spline.IsValid()))
	{
		return;
	}

	Revert(*Spline);

	Tool->bNeedToRerunConstructionScript = true;
}

/// ----------------------------------------------------- Tool builder ------------------------------------------------------------------

bool UDrawNewRoadToolBuilder::CanBuildTool(const FToolBuilderState& SceneState) const
{
	return true;
}

UInteractiveTool* UDrawNewRoadToolBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	UDrawRoadTool* NewTool = NewObject<UDrawRoadTool>(SceneState.ToolManager);
	NewTool->SetWorld(SceneState.World);

	// May be null
	NewTool->SetSelectedActor(nullptr);

	return NewTool;
}

bool UDrawInnerRoadToolBuilder::CanBuildTool(const FToolBuilderState& SceneState) const
{
	if (AActor* Actor = ToolBuilderUtil::FindFirstActor(SceneState, [](AActor*) { return true; }))
	{

		TInlineComponentArray<URoadSplineComponent*> SplineComponent;
		Actor->GetComponents(SplineComponent);

		return SplineComponent.Num() > 0;
	}
	else
	{
		return false;
	}
}

UInteractiveTool* UDrawInnerRoadToolBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	UDrawRoadTool* NewTool = NewObject<UDrawRoadTool>(SceneState.ToolManager);
	NewTool->SetWorld(SceneState.World);

	// May be null
	NewTool->SetSelectedActor(ToolBuilderUtil::FindFirstActor(SceneState, [](AActor*) { return true; }));

	return NewTool;
}


#undef LOCTEXT_NAMESPACE
