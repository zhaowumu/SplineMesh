/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "RoadSplineComponentVisualizer.h"
#include "RoadOffsetComponentVisualizer.generated.h"

struct HRoadOffsetLineVisProxy : public HRoadSplineVisProxy
{
	DECLARE_HIT_PROXY();

	HRoadOffsetLineVisProxy(const URoadSplineComponent* InComponent, EHitProxyPriority InPriority = HPP_Wireframe)
		: HRoadSplineVisProxy(InComponent, InPriority)
	{
	}

	virtual EMouseCursor::Type GetMouseCursor() override
	{
		return EMouseCursor::CardinalCross;
	}
};

struct HRoadOffsetKeyVisProxy : public HRoadSplineVisProxy
{
	DECLARE_HIT_PROXY();

	HRoadOffsetKeyVisProxy(const URoadSplineComponent* InComponent, int OffsetKey, EHitProxyPriority InPriority = HPP_Foreground)
		: HRoadSplineVisProxy(InComponent, InPriority)
		, OffsetKey(OffsetKey)
	{}

	virtual EMouseCursor::Type GetMouseCursor() override
	{
		return EMouseCursor::CardinalCross;
	}

	int OffsetKey;
};

struct HRoadOffsetTangentVisProxy : public HRoadOffsetKeyVisProxy
{
	DECLARE_HIT_PROXY();

	HRoadOffsetTangentVisProxy(const URoadSplineComponent* InComponent, int OffsetKey, bool bInArriveTangent, EHitProxyPriority InPriority = HPP_Wireframe)
		: HRoadOffsetKeyVisProxy(InComponent, OffsetKey, InPriority)
		, bArriveTangent(bInArriveTangent)
	{
	}

	bool bArriveTangent;

	virtual EMouseCursor::Type GetMouseCursor() override
	{
		return EMouseCursor::CardinalCross;
	}
};


/** Selection state data that will be captured by scoped transactions.*/
UCLASS(Transient)
class URoadOffsetComponentVisualizerSelectionState : public UObject
{
	GENERATED_BODY()

public:
	void ResetSelection(bool bSaveSplineSelection);
	void SetSelectedSpline(FComponentPropertyPath& SplinePropertyPath);
	void SetSelectedKey(int32 SelectedKey);
	void SetSelectedTangent(ESelectedTangentHandle Tangent);

	void SetCashedData(const FVector& Position, const FQuat& Rotation, float SplineKey);
	void SetCashedDataAtSplineDistance(float S);
	void SetCashedDataAtSplineInputKey(float Key);
	void ResetCahedData();

	const FComponentPropertyPath GetSplinePropertyPath() const { return SplinePropertyPath; }
	URoadSplineComponent* GetSelectedSpline() const { return SplinePropertyPath.IsValid() ? Cast<URoadSplineComponent>(SplinePropertyPath.GetComponent()) : nullptr; }
	int32 GetSelectedKey() const { return SelectedKey; }
	int32 GetSelectedKeyVerified() const;
	ESelectedTangentHandle GetSelectedTangent() const { return SelectedTangentType; }

	FVector GetCashedPosition() const { return CahedPosition; }
	FQuat GetCachedRotation() const { return CachedRotation; }
	float GetCachedSplineKey() const { return CashedSplineKey; }

protected:
	UPROPERTY()
	FComponentPropertyPath SplinePropertyPath;

	UPROPERTY()
	int32 SelectedKey = INDEX_NONE;

	UPROPERTY()
	ESelectedTangentHandle SelectedTangentType = ESelectedTangentHandle::None;

	UPROPERTY()
	FVector CahedPosition;

	UPROPERTY()
	FQuat CachedRotation;

	UPROPERTY()
	float CashedSplineKey = 0;
};

/** SplineComponent visualizer/edit functionality */
class  FRoadOffsetComponentVisualizer : public FComponentVisualizer, public FGCObject
{
public:
	FRoadOffsetComponentVisualizer();
	virtual ~FRoadOffsetComponentVisualizer();

	//~ Begin FComponentVisualizer Interface
	virtual void OnRegister() override;
	virtual void DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI) override;
	virtual bool VisProxyHandleClick(FEditorViewportClient* InViewportClient, HComponentVisProxy* VisProxy, const FViewportClick& Click) override;
	virtual void DrawVisualizationHUD(const UActorComponent* Component, const FViewport* Viewport, const FSceneView* View, FCanvas* Canvas) override;
	virtual void EndEditing() override;
	virtual bool GetWidgetLocation(const FEditorViewportClient* ViewportClient, FVector& OutLocation) const override;
	virtual bool GetCustomInputCoordinateSystem(const FEditorViewportClient* ViewportClient, FMatrix& OutMatrix) const override;
	virtual bool HandleInputDelta(FEditorViewportClient* ViewportClient, FViewport* Viewport, FVector& DeltaTranslate, FRotator& DeltaRotate, FVector& DeltaScale) override;
	virtual bool HandleInputKey(FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event) override;
	/** Handle click modified by Alt, Ctrl and/or Shift. The input HitProxy may not be on this component. */
	virtual bool HandleModifiedClick(FEditorViewportClient* InViewportClient, HHitProxy* HitProxy, const FViewportClick& Click) override;
	/** Return whether focus on selection should focus on bounding box defined by active visualizer */
	virtual bool HasFocusOnSelectionBoundingBox(FBox& OutBoundingBox) override;
	/** Pass snap input to active visualizer */
	virtual bool HandleSnapTo(const bool bInAlign, const bool bInUseLineTrace, const bool bInUseBounds, const bool bInUsePivot, AActor* InDestination) override;
	/** Gets called when the mouse tracking has stopped (dragging behavior) */
	virtual void TrackingStopped(FEditorViewportClient* InViewportClient, bool bInDidMove) override;
	/** Get currently edited component, this is needed to reset the active visualizer after undo/redo */
	virtual UActorComponent* GetEditedComponent() const override;
	virtual TSharedPtr<SWidget> GenerateContextMenu() const override;
	virtual bool IsVisualizingArchetype() const override;
	//~ End FComponentVisualizer Interface

	//~ FGCObject interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override { return GetReferencerNameStatic(); }
	//~ End of FGCObject interface

	URoadSplineComponent* GetEditedSplineComponent() const;
	URoadOffsetComponentVisualizerSelectionState* GetSelectionState() const { return SelectionState; }

	static FString GetReferencerNameStatic() { return TEXT("FRoadOffsetComponentVisualizer"); }

protected:
	virtual void GenerateContextMenuSections(FMenuBuilder& InMenuBuilder) const;
	const URoadSplineComponent* UpdateSelectedSpline(HComponentVisProxy* VisProxy);

protected:
	void OnAddKey();
	void OnDeleteKey();

protected:
	/** Output log commands */
	TSharedPtr<FUICommandList> RoadOffsetComponentVisualizerActions;

	/** Current selection state */
	TObjectPtr<URoadOffsetComponentVisualizerSelectionState> SelectionState;
};
