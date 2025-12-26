/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "RoadSplineComponentVisualizer.h"
#include "RoadSectionComponentVisualizer.generated.h"


struct HRoadSectionKeyVisProxy : public HRoadLaneVisProxy
{
	DECLARE_HIT_PROXY();

	HRoadSectionKeyVisProxy(const URoadSplineComponent* InComponent, int InSectionIndex, EHitProxyPriority InPriority = HPP_Foreground)
		: HRoadLaneVisProxy(InComponent, InSectionIndex, LANE_INDEX_NONE, InPriority)
	{}

	virtual EMouseCursor::Type GetMouseCursor() override
	{
		return EMouseCursor::CardinalCross;
	}
};

UENUM()
enum class ERoadSectionSelectionState : uint8
{
	None = 0,
	Component,
	Section,
	Lane,
	Key,
	KeyTangent,
};

/** Selection state data that will be captured by scoped transactions.*/
UCLASS(Transient)
class URoadSectionComponentVisualizerSelectionState : public UObject
{
	GENERATED_BODY()

public:
	void ResetSelection(bool bSaveSplineSelection);
	void SetSelectedSpline(FComponentPropertyPath& SplinePropertyPath);
	void SetSelectedSection(int32 SectionIndex);
	void SetSelectedLane(int32 LaneIndex);
	void SetSelectedAttributeName(FName AttributeName);
	void SetSelectedKeyIndex(int32 KeyIndex);
	void SetSelectedTangent(ESelectedTangentHandle TangentHandle);

	const FComponentPropertyPath GetSplinePropertyPath() const { return SplinePropertyPath; }
	URoadSplineComponent* GetSelectedSpline() const { return SplinePropertyPath.IsValid() ? Cast<URoadSplineComponent>(SplinePropertyPath.GetComponent()) : nullptr; }
	int32 GetSelectedSectionIndex() const { return SelectedSectionIndex; }
	int32 GetSelectedLaneIndex() const { return SelectedLaneIndex; }
	FName GetSelectedAttributeName() const { return SelectedAttributeName; }
	int32 GetSelectedKeyIndex() const { return SelectedKeyIndex; }
	ESelectedTangentHandle GetSelectedTangent() const { return SelectedTangentHandleType; }

	void SetCashedData(const FVector& Position, const FQuat& Rotation, float SplineKey);
	void SetCashedDataAtSplineDistance(float S);
	void SetCashedDataAtSplineInputKey(float Key);
	void SetCashedDataAtLane(int SectionIndex, int LaneIndex, double SOffset, double Aplha);
	void ResetCahedData();

	FVector GetCashedPosition() const { return CahedPosition; }
	FQuat GetCachedRotation() const { return CachedRotation; }
	float GetCachedSplineKey() const { return CashedSplineKey; }

	ERoadSectionSelectionState GetState() const { return State; }
	ERoadSectionSelectionState GetStateVerified() const;

	inline bool IsSelected(const URoadSplineComponent* Spline, int SectionIndex) const { return Spline == GetSelectedSpline() && SectionIndex == SelectedSectionIndex; }
	inline bool IsSelected(const URoadSplineComponent* Spline, int SectionIndex, int LaneIndex) const { return Spline == GetSelectedSpline() && SectionIndex == SelectedSectionIndex && LaneIndex == SelectedLaneIndex; }
	inline bool IsSelected(const URoadSplineComponent* Spline, int SectionIndex, int LaneIndex, int KeyIndex) const { return Spline == GetSelectedSpline() && SectionIndex == SelectedSectionIndex && LaneIndex == SelectedLaneIndex && KeyIndex == SelectedKeyIndex; }
	inline bool IsSelected(const URoadSplineComponent* Spline, int SectionIndex, int LaneIndex, FName AttributeName, int KeyIndex) const { return Spline == GetSelectedSpline() && SectionIndex == SelectedSectionIndex && LaneIndex == SelectedLaneIndex && AttributeName == SelectedAttributeName && KeyIndex == SelectedKeyIndex;  }

	void FixState();

	void UpdateSplineSelection() const;

	DECLARE_DELEGATE_RetVal(bool, FIsKeyValid);

	// Used in GetStateVerified() to check validation of the selected key
	FIsKeyValid IsKeyValid;

protected:
	/** Property path from the parent actor to the component */
	UPROPERTY()
	FComponentPropertyPath SplinePropertyPath;

	UPROPERTY()
	int32 SelectedSectionIndex = INDEX_NONE;

	UPROPERTY()
	int32 SelectedLaneIndex = LANE_INDEX_NONE; 

	UPROPERTY()
	FName SelectedAttributeName = NAME_None;

	UPROPERTY()
	int32 SelectedKeyIndex = INDEX_NONE;

	UPROPERTY()
	ESelectedTangentHandle SelectedTangentHandleType = ESelectedTangentHandle::None;

	/** Position on spline we have selected */
	UPROPERTY()
	FVector CahedPosition;

	/** Cached rotation for this point */
	UPROPERTY()
	FQuat CachedRotation;

	UPROPERTY()
	float CashedSplineKey = 0;

	UPROPERTY()
	ERoadSectionSelectionState State;
};

/** SplineComponent visualizer/edit functionality */
class  FRoadSectionComponentVisualizer : public FComponentVisualizer, public FGCObject
{
public:
	FRoadSectionComponentVisualizer();
	virtual ~FRoadSectionComponentVisualizer();

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
	URoadSectionComponentVisualizerSelectionState* GetSelectionState() const { return SelectionState; }

	static FString GetReferencerNameStatic() { return TEXT("FRoadSectionComponentVisualizer"); }


protected:
	virtual void GenerateContextMenuSections(FMenuBuilder& InMenuBuilder) const;
	virtual void GenerateChildContextMenuSections(FMenuBuilder& InMenuBuilder) const {}
	const URoadSplineComponent* UpdateSelectedComponentAndSectionAndLane(HComponentVisProxy* VisProxy);

	bool ShouldDraw(const UActorComponent* Component) const;

protected:
	void OnSplitSection(bool bFull);
	void OnDeleteSection();
	void OnAddLane(bool bLeft);
	void OnDeleteLane();


protected:
	/** Output log commands */
	TSharedPtr<FUICommandList> RoadScetionComponentVisualizerActions;

	/** Current selection state */
	TObjectPtr<URoadSectionComponentVisualizerSelectionState> SelectionState;
};
