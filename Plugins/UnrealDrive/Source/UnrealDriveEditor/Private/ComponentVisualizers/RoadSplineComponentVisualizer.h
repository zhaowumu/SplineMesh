/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "SplineComponentVisualizer.h"
#include "RoadSplineComponent.h"
#include "RoadSceneProxy.h"
#include "RoadSplineComponentVisualizer.generated.h"

 /** HRoadSplineKeyProxy */
struct HRoadSplineKeyProxy : public HRoadSplineVisProxy
{
	DECLARE_HIT_PROXY();

	HRoadSplineKeyProxy(const URoadSplineComponent* InComponent, int32 InKeyIndex, EHitProxyPriority InPriority = HPP_Foreground)
		: HRoadSplineVisProxy(InComponent, InPriority)
		, KeyIndex(InKeyIndex)
	{
	}

	int32 KeyIndex;

	virtual EMouseCursor::Type GetMouseCursor() override
	{
		return EMouseCursor::CardinalCross;
	}
};

/** HRoadSplineSegmentProxy */
struct HRoadSplineSegmentProxy : public HRoadSplineVisProxy
{
	DECLARE_HIT_PROXY();

	HRoadSplineSegmentProxy(const URoadSplineComponent* InComponent, int32 InSegmentIndex, EHitProxyPriority InPriority = HPP_Wireframe)
		: HRoadSplineVisProxy(InComponent, InPriority)
		, SegmentIndex(InSegmentIndex)
	{
	}

	int32 SegmentIndex;

	virtual EMouseCursor::Type GetMouseCursor() override
	{
		return EMouseCursor::CardinalCross;
	}
};

/** HRoadSplineTangentHandleProxy */
struct HRoadSplineTangentHandleProxy : public HRoadSplineVisProxy
{
	DECLARE_HIT_PROXY();

	HRoadSplineTangentHandleProxy(const URoadSplineComponent* InComponent, int32 InKeyIndex, bool bInArriveTangent, EHitProxyPriority InPriority = HPP_Wireframe)
		: HRoadSplineVisProxy(InComponent, InPriority)
		, KeyIndex(InKeyIndex)
		, bArriveTangent(bInArriveTangent)
	{
	}

	int32 KeyIndex;
	bool bArriveTangent;

	virtual EMouseCursor::Type GetMouseCursor() override
	{
		return EMouseCursor::CardinalCross;
	}
};



/** Selection state data that will be captured by scoped transactions.*/
UCLASS(Transient)
class  URoadSplineComponentVisualizerSelectionState : public UObject
{
	GENERATED_BODY()

public:

	/** Checks LastKeyIndexSelected is valid given the number of splint points and returns its value. */
	int32 GetVerifiedLastKeyIndexSelected(const int32 InNumSplinePoints) const;

	/** Checks TangentHandle and TangentHandleType are valid and sets relevant output parameters. */
	void GetVerifiedSelectedTangentHandle(const int32 InNumSplinePoints, int32& OutSelectedTangentHandle, ESelectedTangentHandle& OutSelectedTangentHandleType) const;

	const FComponentPropertyPath GetSplinePropertyPath() const { return SplinePropertyPath; }
	void SetSplinePropertyPath(const FComponentPropertyPath& InSplinePropertyPath) { SplinePropertyPath = InSplinePropertyPath; }

	const TSet<int32>& GetSelectedKeys() const { return SelectedKeys; }
	TSet<int32>& ModifySelectedKeys() { return SelectedKeys; }

	int32 GetLastKeyIndexSelected() const { return LastKeyIndexSelected; }
	void SetLastKeyIndexSelected(const int32 InLastKeyIndexSelected) { LastKeyIndexSelected = InLastKeyIndexSelected; }

	int32 GetSelectedSegmentIndex() const { return SelectedSegmentIndex; }
	void SetSelectedSegmentIndex(const int32 InSelectedSegmentIndex) { SelectedSegmentIndex = InSelectedSegmentIndex; }

	int32 GetSelectedTangentHandle() const { return SelectedTangentHandle; }
	void SetSelectedTangentHandle(const int32 InSelectedTangentHandle) { SelectedTangentHandle = InSelectedTangentHandle; }

	ESelectedTangentHandle GetSelectedTangentHandleType() const { return SelectedTangentHandleType; }
	void SetSelectedTangentHandleType(const ESelectedTangentHandle InSelectedTangentHandle) { SelectedTangentHandleType = InSelectedTangentHandle; }

	FVector GetSelectedSplinePosition() const { return SelectedSplinePosition; }
	void SetSelectedSplinePosition(const FVector& InSelectedSplinePosition) { SelectedSplinePosition = InSelectedSplinePosition; }

	FQuat GetCachedRotation() const { return CachedRotation; }
	void SetCachedRotation(const FQuat& InCachedRotation) { CachedRotation = InCachedRotation; }

	void Reset();
	void ClearSelectedSegmentIndex();
	void ClearSelectedTangentHandle();

	bool IsSplinePointSelected(const int32 InIndex) const;

protected:
	/** Property path from the parent actor to the component */
	UPROPERTY()
	FComponentPropertyPath SplinePropertyPath;

	/** Indices of keys we have selected */
	UPROPERTY()
	TSet<int32> SelectedKeys;

	/** Index of the last key we selected */
	UPROPERTY()
	int32 LastKeyIndexSelected = INDEX_NONE;

	/** Index of segment we have selected */
	UPROPERTY()
	int32 SelectedSegmentIndex = INDEX_NONE;

	/** Index of tangent handle we have selected */
	UPROPERTY()
	int32 SelectedTangentHandle = INDEX_NONE;

	/** The type of the selected tangent handle */
	UPROPERTY()
	ESelectedTangentHandle SelectedTangentHandleType = ESelectedTangentHandle::None;

	/** Position on spline we have selected */
	UPROPERTY()
	FVector SelectedSplinePosition;

	/** Cached rotation for this point */
	UPROPERTY()
	FQuat CachedRotation;
};


/** SplineComponent visualizer/edit functionality */
class  FRoadSplineComponentVisualizer : public FComponentVisualizer, public FGCObject
{
public:
	FRoadSplineComponentVisualizer();
	virtual ~FRoadSplineComponentVisualizer();

	//~ Begin FComponentVisualizer Interface
	virtual void OnRegister() override;
	virtual void DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI) override;
	virtual bool VisProxyHandleClick(FEditorViewportClient* InViewportClient, HComponentVisProxy* VisProxy, const FViewportClick& Click) override;
	/** Draw HUD on viewport for the supplied component */
	virtual void DrawVisualizationHUD(const UActorComponent* Component, const FViewport* Viewport, const FSceneView* View, FCanvas* Canvas) override;
	virtual void EndEditing() override;
	virtual bool GetWidgetLocation(const FEditorViewportClient* ViewportClient, FVector& OutLocation) const override;
	virtual bool GetCustomInputCoordinateSystem(const FEditorViewportClient* ViewportClient, FMatrix& OutMatrix) const override;
	virtual bool HandleInputDelta(FEditorViewportClient* ViewportClient, FViewport* Viewport, FVector& DeltaTranslate, FRotator& DeltaRotate, FVector& DeltaScale) override;
	virtual bool HandleInputKey(FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event) override;
	/** Handle click modified by Alt, Ctrl and/or Shift. The input HitProxy may not be on this component. */
	virtual bool HandleModifiedClick(FEditorViewportClient* InViewportClient, HHitProxy* HitProxy, const FViewportClick& Click) override;
	/** Handle box select input */
	virtual bool HandleBoxSelect(const FBox& InBox, FEditorViewportClient* InViewportClient, FViewport* InViewport) override;
	/** Handle frustum select input */
	virtual bool HandleFrustumSelect(const FConvexVolume& InFrustum, FEditorViewportClient* InViewportClient, FViewport* InViewport) override;
	/** Return whether focus on selection should focus on bounding box defined by active visualizer */
	virtual bool HasFocusOnSelectionBoundingBox(FBox& OutBoundingBox) override;
	/** Pass snap input to active visualizer */
	virtual bool HandleSnapTo(const bool bInAlign, const bool bInUseLineTrace, const bool bInUseBounds, const bool bInUsePivot, AActor* InDestination) override;
	/** Gets called when the mouse tracking has started (dragging behavior) */
	virtual void TrackingStarted(FEditorViewportClient* InViewportClient);
	/** Gets called when the mouse tracking has stopped (dragging behavior) */
	virtual void TrackingStopped(FEditorViewportClient* InViewportClient, bool bInDidMove) override;
	/** Get currently edited component, this is needed to reset the active visualizer after undo/redo */
	virtual UActorComponent* GetEditedComponent() const override;
	virtual TSharedPtr<SWidget> GenerateContextMenu() const override;
	virtual bool IsVisualizingArchetype() const override;
	//~ End FComponentVisualizer Interface

	/** Add menu sections to the context menu */
	virtual void GenerateContextMenuSections(FMenuBuilder& InMenuBuilder) const;

	/** Get the spline component we are currently editing */
	URoadSplineComponent* GetEditedSplineComponent() const;

	const TSet<int32>& GetSelectedKeys() const { check(SelectionState); return SelectionState->GetSelectedKeys(); }

	/** Select first or last spline point, returns true if the spline component being edited has changed */
	bool HandleSelectFirstLastSplinePoint(URoadSplineComponent* InSplineComponent, bool bFirstPoint);

	/** Select all spline points, , returns true if the spline component being edited has changed */
	bool HandleSelectAllSplinePoints(URoadSplineComponent* InSplineComponent);

	/** Select next or prev spline point, loops when last point is currently selected */
	void OnSelectPrevNextSplinePoint(bool bNextPoint, bool bAddToSelection);

	/** Sets the new cached rotation on the visualizer */
	void SetCachedRotation(const FQuat& NewRotation);

	/*
	struct FExternalConnection
	{
		FExternalConnection(ULaneConnection* Connection)
			: Connection(Connection)
		{}
		bool Init(const FIntRect& ViewRect, const FMatrix& ViewToProj);
		bool Update(const FIntRect& ViewRect, const FMatrix& ViewToProj);
		void Draw(class FPrimitiveDrawInterface* PDI, const FSceneView* View, uint8 DepthPriorityGroup) const;

		TWeakObjectPtr<ULaneConnection> Connection;
		FVector2D ScreenPos{};
		FTransform TransformOrigin{};
		FTransform TransformVis{};
		bool bIsCaptured{};
		bool bIsValid{};
	};
	*/

	// FGCObject interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector);
	virtual FString GetReferencerName() const override { return GetReferencerNameStatic(); }
	// End of FGCObject interface

	static FString GetReferencerNameStatic() { return TEXT("FRoadSplineComponentVisualizer"); }

protected:

	/** Determine if any selected key index is out of range (perhaps because something external has modified the spline) */
	bool IsAnySelectedKeyIndexOutOfRange(const URoadSplineComponent* Comp) const;

	/** Whether a single spline key is currently selected */
	bool IsSingleKeySelected() const;

	/** Whether a multiple spline keys are currently selected */
	bool AreMultipleKeysSelected() const;

	/** Whether any keys are currently selected */
	bool AreKeysSelected() const;

	/** Select spline point at specified index */
	void SelectSplinePoint(int32 SelectIndex, bool bAddToSelection);

	/** Transforms selected tangent by given translation */
	bool TransformSelectedTangent(EPropertyChangeType::Type InPropertyChangeType, const FVector& InDeltaTranslate);

	/** Transforms selected tangent by given translate, rotate and scale */
	bool TransformSelectedKeys(EPropertyChangeType::Type InPropertyChangeType, FEditorViewportClient* InViewportClient, FViewport* Viewport, const FVector& InDeltaTranslate, const FRotator& InDeltaRotate = FRotator::ZeroRotator, const FVector& InDeltaScale = FVector::ZeroVector);

	/** Update the key selection state of the visualizer */
	virtual void ChangeSelectionState(int32 Index, bool bIsCtrlHeld);

	/** Alt-drag: duplicates the selected spline key */
	virtual bool DuplicateKeyForAltDrag(const FVector& InDrag);

	/** Alt-drag: updates duplicated selected spline key */
	virtual bool UpdateDuplicateKeyForAltDrag(const FVector& InDrag);

	/** Return spline data for point on spline closest to input point */
	virtual float FindNearest(const FVector& InLocalPos, int32 InSegmentStartIndex, FVector& OutSplinePos, FVector& OutSplineTangent) const;

	/** Split segment using given world position */
	virtual void SplitSegment(const FVector& InWorldPos, int32 InSegmentIndex, bool bCopyFromSegmentBeginIndex = true);

	/** Update split segment based on drag offset */
	virtual void UpdateSplitSegment(const FVector& InDrag);

	/** Add segment to beginning or end of spline */
	virtual void AddSegment(const FVector& InWorldPos, bool bAppend);

	/** Add segment to beginning or end of spline */
	virtual void UpdateAddSegment(const FVector& InWorldPos);

	/** Alt-drag: duplicates the selected spline key */
	virtual void ResetAllowDuplication();

	/** Snapping: snap keys to axis position of last selected key */
	virtual void SnapKeysToLastSelectedAxisPosition(const EAxis::Type InAxis, TArray<int32> InSnapKeys);

	/** Snapping: snap key to selected actor */
	virtual void SnapKeyToActor(const AActor* InActor, const ESplineComponentSnapMode SnapMode);

	/** Snapping: generic method for snapping selected keys to given transform */
	virtual void SnapKeyToTransform(const ESplineComponentSnapMode InSnapMode,
		const FVector& InWorldPos,
		const FVector& InWorldUpVector,
		const FVector& InWorldForwardVector,
		const FVector& InScale,
		const USplineMetadata* InCopySplineMetadata = nullptr,
		const int32 InCopySplineMetadataKey = 0);

	/** Snapping: set snap to actor temporary mode */
	virtual void SetSnapToActorMode(const bool bInIsSnappingToActor, const ESplineComponentSnapMode InSnapMode = ESplineComponentSnapMode::Snap);

	/** Snapping: get snap to actor temporary mode */
	virtual bool GetSnapToActorMode(ESplineComponentSnapMode& OutSnapMode) const;

	/** Reset temporary modes after inputs are handled. */
	virtual void ResetTempModes();

	/** Updates the component and selected properties if the component has changed */
	const URoadSplineComponent* UpdateSelectedSplineComponent(HComponentVisProxy* VisProxy);

	void OnDeleteKey();
	bool CanDeleteKey() const;

	void OnDisconnect();
	bool CanDisconnect() const;

	void OnDisconnectAll();

	/** Duplicates selected spline keys in place */
	void OnDuplicateKey();
	bool IsKeySelectionValid() const;

	void OnAddKeyToSegment();
	bool CanAddKeyToSegment() const;

	void OnSnapKeyToNearestSplinePoint(ESplineComponentSnapMode InSnapMode);

	void OnSnapKeyToActor(const ESplineComponentSnapMode InSnapMode);

	void OnSnapAllToAxis(EAxis::Type InAxis);

	void OnSnapSelectedToAxis(EAxis::Type InAxis);

	void OnLockAxis(EAxis::Type InAxis);
	bool IsLockAxisSet(EAxis::Type InAxis) const;

	void OnSetKeyType(ERoadSplinePointType Mode);
	bool IsKeyTypeSet(ERoadSplinePointType Mode) const;

	void OnSetVisualizeRollAndScale();
	bool IsVisualizingRollAndScale() const;

	void OnResetToDefault();
	bool CanResetToDefault() const;

	/** Select first or last spline point */
	void OnSelectFirstLastSplinePoint(bool bFirstPoint);

	/** Select all spline points, if no spline points selected yet the currently edited spline component will be set as well */
	void OnSelectAllSplinePoints();

	bool CanSelectSplinePoints() const;

	/** Generate the submenu containing available selection actions */
	void GenerateSelectSplinePointsSubMenu(FMenuBuilder& MenuBuilder) const;

	/** Generate the submenu containing the available point types */
	void GenerateSplinePointTypeSubMenu(FMenuBuilder& MenuBuilder) const;

	/** Generate the submenu containing the available auto tangent types */
	//void GenerateTangentTypeSubMenu(FMenuBuilder& MenuBuilder) const;

	/** Generate the submenu containing the available snap/align actions */
	void GenerateSnapAlignSubMenu(FMenuBuilder& MenuBuilder) const;

	/** Generate the submenu containing the lock axis types */
	void GenerateLockAxisSubMenu(FMenuBuilder& MenuBuilder) const;

	/** Helper function to set edited component we are currently editing */
	void SetEditedSplineComponent(const URoadSplineComponent* InSplineComponent);

	void CreateSplineGeneratorPanel();

	//void OnDeselectedInEditor(TObjectPtr<USplineComponent> SplineComponent);

	URoadConnection* GetSelectedConnection(int KeyIndex = INDEX_NONE) const;

protected:

	/** Output log commands */
	TSharedPtr<FUICommandList> SplineComponentVisualizerActions;

	/** Current selection state */
	TObjectPtr<URoadSplineComponentVisualizerSelectionState> SelectionState;

	/** Whether we currently allow duplication when dragging */
	bool bAllowDuplication;

	/** Alt-drag: True when in process of duplicating a spline key. */
	bool bDuplicatingSplineKey;

	/** Alt-drag: True when in process of adding end segment. */
	bool bUpdatingAddSegment;

	/** Alt-drag: Delays duplicating control point to accumulate sufficient drag input offset. */
	uint32 DuplicateDelay;

	/** Alt-drag: Accumulates delayed drag offset. */
	FVector DuplicateDelayAccumulatedDrag;

	/** Alt-drag: Cached segment parameter for split segment at new control point */
	float DuplicateCacheSplitSegmentParam;

	/** Axis to fix when adding new spline points. Uses the value of the currently
		selected spline point's X, Y, or Z value when fix is not equal to none. */
	EAxis::Type AddKeyLockedAxis;

	/** Snap: True when in process of snapping to actor which needs to be Ctrl-Selected. */
	bool bIsSnappingToActor;

	/** Snap: Snap to actor mode. */
	ESplineComponentSnapMode SnapToActorMode;

	FProperty* SplineCurvesProperty;
	FProperty* SplinePointTypesProperty;

	//FDelegateHandle DeselectedInEditorDelegateHandle;

	//TSharedPtr<SSplineGeneratorPanel> SplineGeneratorPanel;
	static TWeakPtr<SWindow> WeakExistingWindow;

	// ~ Begin moving connection data 
	bool bIsMovingConnection = false;
	FVector WidgetLocationForMovingConnection;
	//TArray<FExternalConnection> ExternalConnections;
	FMatrix CashedViewToProj;
	FIntRect CashedViewRect;
	FVector CashedViewLocation;
	FQuat CashedConnectionQuat;
	FVector CashedConnectionArrivalTangent;
	FVector CashedConnectionLeaveTangent;
	// ~ End moving connection data 
};
