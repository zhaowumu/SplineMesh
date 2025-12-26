/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "RoadSectionComponentVisualizer.h"


struct HRoadLaneWidthSegmentVisProxy : public HRoadLaneVisProxy
{
	DECLARE_HIT_PROXY();

	HRoadLaneWidthSegmentVisProxy(const URoadSplineComponent* InComponent, int InSectionIndex, int InLaneIndex, int InWidthIndex, EHitProxyPriority InPriority = HPP_Wireframe)
		: HRoadLaneVisProxy(InComponent, InSectionIndex, InLaneIndex, InPriority)
		, WidthIndex(InWidthIndex)
	{
	}

	virtual EMouseCursor::Type GetMouseCursor() override
	{
		return EMouseCursor::CardinalCross;
	}

	int WidthIndex;
};

struct HRoadLaneWidthKeyVisProxy : public HRoadLaneWidthSegmentVisProxy
{
	DECLARE_HIT_PROXY();

	HRoadLaneWidthKeyVisProxy(const URoadSplineComponent* InComponent, int InSectionIndex, int InLaneIndex, int InWidthIndex, EHitProxyPriority InPriority = HPP_Foreground)
		: HRoadLaneWidthSegmentVisProxy(InComponent, InSectionIndex, InLaneIndex, InWidthIndex, InPriority)
	{
	}

	virtual EMouseCursor::Type GetMouseCursor() override
	{
		return EMouseCursor::CardinalCross;
	}
};

struct HRoadLaneWidthTangentVisProxy : public HRoadLaneWidthKeyVisProxy
{
	DECLARE_HIT_PROXY();

	HRoadLaneWidthTangentVisProxy(const URoadSplineComponent* InComponent, int InSectionIndex, int InLaneIndex, int InWidthIndex, bool bInArriveTangent, EHitProxyPriority InPriority = HPP_Wireframe)
		: HRoadLaneWidthKeyVisProxy(InComponent, InSectionIndex, InLaneIndex, InWidthIndex, InPriority)
		, bArriveTangent(bInArriveTangent)
	{
	}

	bool bArriveTangent;

	virtual EMouseCursor::Type GetMouseCursor() override
	{
		return EMouseCursor::CardinalCross;
	}
};

class  FRoadWidthComponentVisualizer : public FRoadSectionComponentVisualizer
{
public:
	FRoadWidthComponentVisualizer();
	virtual ~FRoadWidthComponentVisualizer();

	virtual void OnRegister() override;
	virtual void DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI) override;
	virtual bool VisProxyHandleClick(FEditorViewportClient* InViewportClient, HComponentVisProxy* VisProxy, const FViewportClick& Click) override;
	virtual bool HandleInputDelta(FEditorViewportClient* ViewportClient, FViewport* Viewport, FVector& DeltaTranslate, FRotator& DeltaRotate, FVector& DeltaScale) override;
	virtual void TrackingStarted(FEditorViewportClient* InViewportClient) override;
	virtual FString GetReferencerName() const override { return GetReferencerNameStatic(); }
	static FString GetReferencerNameStatic() { return TEXT("FRoadWidthComponentVisualizer"); }

protected:
	virtual void GenerateChildContextMenuSections(FMenuBuilder& InMenuBuilder) const;

	void OnAddWidthKey();
	void OnDeleteWidthKey();
};
