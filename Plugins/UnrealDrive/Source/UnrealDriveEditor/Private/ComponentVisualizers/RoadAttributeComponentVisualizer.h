/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "RoadSectionComponentVisualizer.h"

struct HRoadLaneAttributeVisProxy : public HRoadLaneVisProxy
{
	DECLARE_HIT_PROXY();

	HRoadLaneAttributeVisProxy(const URoadSplineComponent* InComponent, int InSectionIndex, int InLaneIndex, FName InAttributeName, EHitProxyPriority InPriority = HPP_Wireframe)
		: HRoadLaneVisProxy(InComponent, InSectionIndex, InLaneIndex, InPriority)
		, AttributeName(InAttributeName)
	{
	}

	virtual EMouseCursor::Type GetMouseCursor() override
	{
		return EMouseCursor::Crosshairs;
	}
	FName AttributeName;
};

struct HRoadLaneAttributeSegmentVisProxy : public HRoadLaneAttributeVisProxy
{
	DECLARE_HIT_PROXY();

	HRoadLaneAttributeSegmentVisProxy(const URoadSplineComponent* InComponent, int InSectionIndex, int InLaneIndex, FName InAttributeName, int InAttributeIndex, EHitProxyPriority InPriority = HPP_Wireframe)
		: HRoadLaneAttributeVisProxy(InComponent, InSectionIndex, InLaneIndex, InAttributeName, InPriority)
		, AttributeIndex(InAttributeIndex)
	{
	}

	virtual EMouseCursor::Type GetMouseCursor() override
	{
		return EMouseCursor::CardinalCross;
	}

	int AttributeIndex;
};

struct HRoadLaneAttributeKeyVisProxy : public HRoadLaneAttributeSegmentVisProxy
{
	DECLARE_HIT_PROXY();

	HRoadLaneAttributeKeyVisProxy(const URoadSplineComponent* InComponent, int InSectionIndex, int InLaneIndex, FName InAttributeName, int InAttributeIndex, EHitProxyPriority InPriority = HPP_Foreground)
		: HRoadLaneAttributeSegmentVisProxy(InComponent, InSectionIndex, InLaneIndex, InAttributeName, InAttributeIndex, InPriority)
	{
	}

	virtual EMouseCursor::Type GetMouseCursor() override
	{
		return EMouseCursor::CardinalCross;
	}
};


class  FRoadAttributeComponentVisualizer : public FRoadSectionComponentVisualizer
{
public:
	FRoadAttributeComponentVisualizer();
	virtual ~FRoadAttributeComponentVisualizer();

	virtual void OnRegister() override;
	virtual void DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI) override;
	virtual bool VisProxyHandleClick(FEditorViewportClient* InViewportClient, HComponentVisProxy* VisProxy, const FViewportClick& Click) override;
	virtual bool HandleInputDelta(FEditorViewportClient* ViewportClient, FViewport* Viewport, FVector& DeltaTranslate, FRotator& DeltaRotate, FVector& DeltaScale) override;
	virtual FString GetReferencerName() const override { return GetReferencerNameStatic(); }
	static FString GetReferencerNameStatic() { return TEXT("FRoadAttributeComponentVisualizer"); }

protected:
	virtual void GenerateChildContextMenuSections(FMenuBuilder& InMenuBuilder) const;

protected:
	void OnCreateAttribute();
	void OnDeleteAttribute();
	void OnAddKey();
	void OnDeleteKey();
};
