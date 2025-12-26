/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "RoadSplineComponent.h"
#include "VertexFactory.h"
#include "PrimitiveSceneProxy.h"
#include "LocalVertexFactory.h"
#include "DynamicMeshBuilder.h"
//#include "PrimitiveViewRelevance.h"
//#include "Engine/Engine.h"
//#include "MaterialShared.h"
//#include "Materials/Material.h"

#if WITH_EDITOR
#include "ComponentVisualizer.h"
#endif

namespace UnrealDrive
{
	struct FTriProxy;
	struct FLaneProxy;
}


#if WITH_EDITOR

struct UNREALDRIVE_API HRoadSplineVisProxy : public HComponentVisProxy
{
	DECLARE_HIT_PROXY();

	HRoadSplineVisProxy(const URoadSplineComponent* InComponent, EHitProxyPriority InPriority = HPP_Wireframe)
		: HComponentVisProxy(InComponent, InPriority)
	{}

	virtual EMouseCursor::Type GetMouseCursor() override
	{
		return EMouseCursor::Crosshairs;
	}
};

struct UNREALDRIVE_API HRoadSectionVisProxy : public HRoadSplineVisProxy
{
	DECLARE_HIT_PROXY();

	HRoadSectionVisProxy(const URoadSplineComponent* InComponent, int InSectionIndex, EHitProxyPriority InPriority = HPP_Wireframe)
		: HRoadSplineVisProxy(InComponent, InPriority)
		, SectionIndex(InSectionIndex)
	{}

	int SectionIndex;

	virtual EMouseCursor::Type GetMouseCursor() override
	{
		return EMouseCursor::Crosshairs;
	}
};

struct UNREALDRIVE_API HRoadLaneVisProxy : public HRoadSectionVisProxy
{
	DECLARE_HIT_PROXY();

	HRoadLaneVisProxy(const URoadSplineComponent* InComponent, int InSectionIndex, int InLaneIndex, EHitProxyPriority InPriority = HPP_Wireframe)
		: HRoadSectionVisProxy(InComponent, InSectionIndex, InPriority)
		, LaneIndex(InLaneIndex)
	{}

	int LaneIndex;

	virtual EMouseCursor::Type GetMouseCursor() override
	{
		return EMouseCursor::Crosshairs;
	}

	/*
	inline bool operator == (const FRoadLaneConnectionInfo& LaneInfo)
	{
		return Component.IsValid() &&
			Component == LaneInfo.OwnedRoadSpline &&
			SectionIndex == LaneInfo.SectionIndex &&
			LaneIndex == LaneInfo.LaneIndex;

	}
	*/
};

#endif //WITH_EDITOR

class UNREALDRIVE_API FRoadSplineSceneProxy final : public FPrimitiveSceneProxy
{

public:
	FRoadSplineSceneProxy(URoadSplineComponent* Component);
	FRoadSplineSceneProxy(const FRoadSplineSceneProxy& Component) = delete;
	virtual ~FRoadSplineSceneProxy();

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override;
	virtual void DrawStaticElements(FStaticPrimitiveDrawInterface* PDI) override;
	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override;
	virtual bool CanBeOccluded() const override;
	virtual uint32 GetMemoryFootprint(void) const override { return(sizeof(*this) + GetAllocatedSize()); }
	virtual SIZE_T GetTypeHash() const override
	{
		static size_t UniquePointer;
		return reinterpret_cast<size_t>(&UniquePointer);
	}

	uint32 GetAllocatedSize(void) const { return(FPrimitiveSceneProxy::GetAllocatedSize()); }

	virtual HHitProxy* CreateHitProxies(UPrimitiveComponent* Component, TArray<TRefCountPtr<HHitProxy> >& OutHitProxies) override;

private:
	URoadSplineComponent* RoadSpline = nullptr;
	TArray<TSharedPtr<UnrealDrive::FLaneProxy>> LanesProxies;
	TSharedPtr<UnrealDrive::FTriProxy> TriProxy;
	FMaterialRelevance MaterialRelevance{};
	bool bIsMultiRoad;
	//TArray<TPair<FVector, FVector>> ArrawLines;
	const class UUnrealDriveSubsystem* Subsystem = nullptr;
};

