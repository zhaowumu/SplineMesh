/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "CoreMinimal.h"

#include "BaseBehaviors/BehaviorTargetInterfaces.h"
#include "InteractiveTool.h"
#include "InteractiveToolBuilder.h"
#include "InteractiveToolChange.h"
#include "ToolContextInterfaces.h" // FViewCameraState
#include "TransactionUtil.h"
#include "UnrealDriveTypes.h"
#include "UnrealDriveEditorSettings.h"
#include "UnrealDrivePreset.h"
#include "Utils/DrawUtils.h"
#include "DrawRoadTool.generated.h"

class AActor;
class APreviewGeometryActor;
class UConstructionPlaneMechanic;
class USingleClickOrDragInputBehavior;
class URoadSplineComponent;
class UWorld;


UENUM()
enum class EDrawRoadToolMode : uint8
{
	NewActor,
	ExistingActor,
};

UENUM()
enum class EDrawRoadDrawMode : uint8
{
	/** Click to place a point and then drag to set its tangent.Clicking without dragging will create sharp corners. */
	TangentDrag,

	/** Click and drag new points, with the tangent set automatically */
	ClickAutoTangent,

	//TODO: Add AutoArc mode as a road drawing style in the Mathwork RoadRunner - sequence of arcs and straight lines
	// AutoArc
};

UENUM()
enum class ERoadOffsetMethod : uint8
{
	/** Spline points will be offset along the normal direction of the clicked surface */
	HitNormal,

	/** Spline points will be offset along a manually-chosen direction */
	Custom
};

UENUM()
enum class ENewRoadActorType : uint8
{
	/** Create a new empty actor with the URoadSpline inside it */
	CreateEmptyActor,

	/** 
	 * Create the blueprint specified by Blueprint To Create, and either attach
	 * the spline to that, or replace an existing spline if Existing Spline Index to Replace is valid. */
	CreateBlueprint,
};

/**
 *  Defines the rules for automatic detection of road lanes profile for drawing  of the RoadSplineComponent if the spline originates at the LaneConnection 
 */
UENUM()
enum class ERoadLanesProfileSource : uint8
{
	/** Copy only one road lane from the LaneSuccessorConnection. Only valid if the spline is drawn from the LaneSuccessorConnection.*/
	OneLane,

	/** Copy the road lanes from the LaneSuccessorConnection to the last right lane in the source road section. Only valid if the spline is drawn from  the LaneSuccessorConnection. */
	RightSide,

	//LeftLanes, //TODO

	/** Copy all road lanes from the LaneSuccessorConnection. Only valid if the spline is drawn from  the LaneSuccessorConnection. */
	BothSides,

	/** Copy road lanes from the profile. */
	RoadProfile
};

UENUM()
enum class EDrawRoadUpVectorMode : uint8
{
	/** Pick the first up vector based on the hit normal, and then align subsequent up vectors with the previous ones. */
	AlignToPrevious,
	/** Base the up vector off the hit normal. */
	UseHitNormal,
};

/*
UENUM()
enum class EDrawRoad: uint8
{
	OneLane,
	Border
};
*/

USTRUCT()
struct UNREALDRIVEEDITOR_API FRoadDrawProfilePicker
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category=RoadDrawProfilePicker);
	FName ProfileName;

	FRoadLaneSectionProfile* GetProfile() const;
};

UCLASS()
class UNREALDRIVEEDITOR_API UDrawRoadToolProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()

public:
	UDrawRoadToolProperties();

	UPROPERTY()
	EDrawRoadToolMode ToolMode = EDrawRoadToolMode::NewActor;

	/** Defines the rules for detection of road lanes profile for spline drawing  */
	UPROPERTY(EditAnywhere, Category = RoadSpline)
	ERoadLanesProfileSource LanesSource = ERoadLanesProfileSource::RoadProfile;

	/** Road draw profile presets  */
	UPROPERTY(EditAnywhere, Category = RoadSpline, meta = (EditCondition="LanesSource == ERoadLanesProfileSource::RoadProfile"))
	FRoadDrawProfilePicker DrawProfile{};

	/** Determines whether the created spline is a loop. This can be toggled using "Closed Loop" in the detail panel after spline creation. */
	UPROPERTY(EditAnywhere, Category = RoadSpline)
	bool bLoop = false;

	/** Set FRoadLaneInstance to fill the the looped spline. */
	UPROPERTY(EditAnywhere, Category = RoadSpline)
	TInstancedStruct<FRoadLaneInstance> FilledInstance;

	/** How the spline is drawn in the tool. */
	UPROPERTY(EditAnywhere, Category = Drawing)
	EDrawRoadDrawMode DrawMode = EDrawRoadDrawMode::ClickAutoTangent;

	/** How far to offset spline points from the clicked surface, along the surface normal */
	UPROPERTY(EditAnywhere, Category = Drawing, meta = (UIMin = 0, UIMax = 100))
	double ClickOffset = 20;

	/** How to choose the direction to offset points from the clicked surface */
	UPROPERTY(EditAnywhere, Category = Road, meta = (EditCondition="ClickOffset > 0", EditConditionHides))
	ERoadOffsetMethod OffsetMethod = ERoadOffsetMethod::HitNormal;

	/** Manually-specified click offset direction. Note: Will be normalized. If it is a zero vector, a default Up vector will be used instead. */
	UPROPERTY(EditAnywhere, Category = Drawing, meta = (EditCondition="ClickOffset > 0 && OffsetMethod == ERoadOffsetMethod::Custom", EditConditionHides))
	FVector OffsetDirection = FVector::UpVector;

	/**
	 * How the spline rotation is set. It is suggested to use a nonzero FrameVisualizationWidth to see the effects.
	 */
	UPROPERTY(EditAnywhere, Category = Drawing)
	EDrawRoadUpVectorMode UpVectorMode = EDrawRoadUpVectorMode::UseHitNormal;

	/** Whether to place spline points on the surface of objects in the world */
	UPROPERTY(EditAnywhere, Category = RaycastTargets, meta = (DisplayName = "World Objects"))
	bool bHitWorld = true;

	/** Whether to place spline points on a custom, user-adjustable plane */
	UPROPERTY(EditAnywhere, Category = RaycastTargets, meta = (DisplayName = "Custom Plane"))
	bool bHitCustomPlane = false;

	/** Whether to place spline points on a plane through the origin aligned with the Z axis in perspective views, or facing the camera in othographic views */
	UPROPERTY(EditAnywhere, Category = RaycastTargets, meta = (DisplayName = "Ground Planes"))
	bool bHitGroundPlanes = true;

	/**  Determines how the resulting spline is emitted on tool accept. */
	UPROPERTY(EditAnywhere, Category = Output, meta = (EditCondition="ToolMode == EDrawRoadToolMode::NewActor", EditConditionHides))
	ENewRoadActorType OutputMode = ENewRoadActorType::CreateEmptyActor;

	/** Blueprint to create when Output Mode is "Create Blueprint"  */
	UPROPERTY(EditAnywhere, Category = Output, meta = (EditCondition="ToolMode == EDrawRoadToolMode::NewActor && OutputMode == ENewRoadActorType::CreateBlueprint", EditConditionHides))
	TWeakObjectPtr<UBlueprint> BlueprintToCreate;

	/**
	 * If modifying a blueprint actor, whether to run the construction script while dragging or only at the end of a drag. Can be toggled off for expensive construction scripts.
	 */
	UPROPERTY(EditAnywhere, Category = Output, meta = (EditCondition="ToolMode == EDrawRoadToolMode::NewActor && OutputMode == ENewRoadActorType::CreateBlueprint"))
	bool bRerunConstructionScriptOnDrag = true;
};

UCLASS()
class UNREALDRIVEEDITOR_API UDrawRoadTool 
	: public UInteractiveTool
	, public IClickBehaviorTarget
	, public IClickDragBehaviorTarget
	//, public IHoverBehaviorTarget
{
	GENERATED_BODY()

public:

	virtual void SetSelectedActor(AActor* Actor);

	virtual void SetWorld(UWorld* World);
	virtual UWorld* GetTargetWorld() { return TargetWorld.Get(); }

	// UInteractiveTool
	virtual void Setup() override;
	virtual void Shutdown(EToolShutdownType ShutdownType) override;
	virtual void OnTick(float DeltaTime) override;
	virtual bool HasCancel() const override { return true; }
	virtual bool HasAccept() const override { return true; }
	virtual bool CanAccept() const override;
	virtual void Render(IToolsContextRenderAPI* RenderAPI) override;
	virtual void OnPropertyModified(UObject* PropertySet, FProperty* Property) override;

	// IClickBehaviorTarget
	virtual FInputRayHit IsHitByClick(const FInputDeviceRay& ClickPos);
	virtual void OnClicked(const FInputDeviceRay& ClickPos);

	// IClickDragBehaviorTarget
	virtual FInputRayHit CanBeginClickDragSequence(const FInputDeviceRay& PressPos);
	virtual void OnClickPress(const FInputDeviceRay& PressPos);
	virtual void OnClickDrag(const FInputDeviceRay& DragPos);
	virtual void OnClickRelease(const FInputDeviceRay& ReleasePos);
	virtual void OnTerminateDragSequence();

	// IHoverBehaviorTarget
	//virtual FInputRayHit BeginHoverSequenceHitTest(const FInputDeviceRay& PressPos) override;
	//virtual void OnBeginHover(const FInputDeviceRay& DevicePos) override;
	//virtual bool OnUpdateHover(const FInputDeviceRay& DevicePos) override;
	//virtual void OnEndHover() override;

private:

	UPROPERTY()
	TObjectPtr<UDrawRoadToolProperties> Settings = nullptr;

	UPROPERTY()
	TObjectPtr<USingleClickOrDragInputBehavior> ClickOrDragBehavior = nullptr;

	UPROPERTY()
	TObjectPtr<UConstructionPlaneMechanic> PlaneMechanic = nullptr;

	TWeakObjectPtr<UWorld> TargetWorld = nullptr;

	// This is only used to initialize TargetActor in the settings object
	TWeakObjectPtr<AActor> TargetActor = nullptr;

	EDrawRoadToolMode ToolMode = EDrawRoadToolMode::NewActor;

	// The preview actor is either a APreviewGeometryActor with a spline, or a duplicate of 
	// some target blueprint actor so that we can see the effects of the drawn spline immediately.
	UPROPERTY()
	TObjectPtr<AActor> PreviewActor = nullptr;

	// Used for recapturing the spline when rerunning construction scripts
	int32 SplineRecaptureIndex = 0;

	// This is the spline we add points to. It points to a component nested somewhere under 
	// PreviewRootActor.
	TWeakObjectPtr<URoadSplineComponent> WorkingSpline = nullptr;

	bool bDrawTangentForLastPoint = false;

	struct FMouseTraceResult
	{
		FVector3d Location;
		FVector3d UpVector;
		FVector3d ForwardVector;
		double HitT;
		TWeakObjectPtr<ULaneConnection> Connection;
	};

	bool Raycast(const FRay& WorldRay, FVector3d& HitLocationOut, FVector3d& HitNormalOut, double& HitTOut) const;
	bool MouseTrace(const FRay& WorldRay, FMouseTraceResult& Result) const;
	void AddSplinePoint(const FVector3d& HitLocation, const FVector3d& UpVector);
	FVector3d GetUpVectorToUse(const FVector3d& HitLocation, const FVector3d& HitNormal, int32 NumSplinePointsBeforehand);
	bool FinishDraw();
	void InitRoadProfile(URoadSplineComponent * TargetSpline) const;

	void ReCreatePreview();
	void GenerateAsset();

	int32 TargetActorWatcherID = -1;
	bool PreviousTargetActorVisibility = true;
	bool bPreviousSplineVisibility = true;

	bool bNeedToRerunConstructionScript = false;

	FViewCameraState CameraState;

	TWeakObjectPtr<ULaneConnection> ConnectionUnderCursor;
	TWeakObjectPtr<ULaneConnection> StartLaneConnection;
	TWeakObjectPtr<ULaneConnection> EndLaneConnection;

	bool bCahedEnableRenderingDuringHitProxyPass = false;
	FMatrix CashedViewToProj;
	FIntRect CashedViewRect;
	bool bConnectionsCashIsDirty = false;

private:
	UE::TransactionUtil::FLongTransactionTracker LongTransactions;

public:
	// Helper class for making undo/redo transactions, to avoid friending all the variations.
	class FSplineChange : public FToolCommandChange
	{
	public:
		// These pass the working spline to the overloads below
		virtual void Apply(UObject* Object) override;
		virtual void Revert(UObject* Object) override;

	protected:
		virtual void Apply(URoadSplineComponent& Spline) = 0;
		virtual void Revert(URoadSplineComponent& Spline) = 0;
	};
};

UCLASS(Transient)
class UNREALDRIVEEDITOR_API UDrawNewRoadToolBuilder : public UInteractiveToolBuilder
{
	GENERATED_BODY()

public:
	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override;

	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;
};

UCLASS(Transient)
class UNREALDRIVEEDITOR_API UDrawInnerRoadToolBuilder : public UInteractiveToolBuilder
{
	GENERATED_BODY()

public:
	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override;

	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;
};
