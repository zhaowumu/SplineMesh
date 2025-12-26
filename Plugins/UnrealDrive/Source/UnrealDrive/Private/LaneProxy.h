/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "RHI.h"
#include "DynamicMeshBuilder.h"
#include "StaticMeshResources.h"

class URoadSplineComponent;
class FMaterialRenderProxy;
struct FRoadLaneSection;
struct HRoadLaneVisProxy;
struct HRoadSplineVisProxy;

namespace UnrealDrive
{

struct FLaneProxy
{
	FLaneProxy(int InSectionIndex, int InLaneIndex, ERHIFeatureLevel::Type InFeatureLevel) 
		: SectionIndex(InSectionIndex)
		, LaneIndex(InLaneIndex)
		, VertexFactory(InFeatureLevel, "RoadMesh")
	{
	}
	//FLaneProxy(const FLaneProxy& Other) = default;
	//FLaneProxy(FLaneProxy&& Other) = default;

	virtual ~FLaneProxy() { ReleaseResources(); }

	virtual void InitMesh(TArray<FDynamicMeshVertex>& Vertices, TArray<uint32>& Indices);
	virtual void ReleaseResources();
	virtual FMeshBatch * GetDynamicMeshElements(const class FPrimitiveSceneProxy & SceneProxy, const FSceneView*& Views, const FSceneViewFamily& ViewFamily, FPrimitiveDrawInterface* PDI, FMeshElementCollector& Collector) const;
	virtual void DrawLines(const FMatrix& LocalToWorld, FPrimitiveDrawInterface* PDI, bool bIsSelected) const;

#if WITH_EDITOR
	virtual HRoadSplineVisProxy* CreateHitProxy(const URoadSplineComponent* Component);
#endif

	int SectionIndex = -1;
	int LaneIndex = 0;
	TArray<FVector> LanePoints;
	FStaticMeshVertexBuffers VertexBuffers;
	FDynamicMeshIndexBuffer32 IndexBuffer;
	FLocalVertexFactory VertexFactory;
	FMaterialRenderProxy* Material = nullptr;
	FLinearColor LineColor{ FColor(255, 255, 255) };

#if WITH_EDITOR
	TRefCountPtr<HRoadSplineVisProxy> HitProxy;
#endif

	static TArray<TSharedPtr<FLaneProxy>> MakeLaneProxysFromSpline(URoadSplineComponent* InComponent, ERHIFeatureLevel::Type InFeatureLevel);
	static TSharedPtr<FLaneProxy> MakeLoopProxyFromSpline(URoadSplineComponent* InComponent, ERHIFeatureLevel::Type InFeatureLevel);
};

} // UnrealDrive


