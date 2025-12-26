/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "CoreMinimal.h"
#include "RoadSplineComponent.h"
#include "CompGeom/Delaunay2.h"
#include "ModelingOperators.h"
#include "Curve/GeneralPolygon2.h"
#include "RoadMeshTools/RoadSplineCache.h"
#include "Geometry/Arrangement2d.h"
#include "Utils/OpUtils.h"
#include "RoadMeshTools/RoadLanePolygone.h"
#include "RoadMeshTools/SplineMeshOpHelpers.h"
#include "TriangulateRoadOp.generated.h"


class URoadSplineComponent;

UENUM()
enum class ERoadOverlapStrategy : uint8
{
	UseMaxZ = 0,
	UseMinZ = 1,
};

namespace UnrealDrive 
{

using namespace UE::Geometry;

/**
 * FRoadBaseOperatorData 
 */
struct FRoadBaseOperatorData
{
	struct FDebugLines
	{
		TArray<TPair<FVector, FVector>> Lines;
		FColor Color;
		float Thickness;
	};

	FTransform ActorTransform;
	TArray< UnrealDrive::FRoadSplineCache> RoadSplinesCache;
	FGeometryResult ResultInfo;
	FAxisAlignedBox3d Bounds;
	TUniquePtr<UnrealDrive::FArrangement2d> Arrangement;
	TArray<FArrangementVertex3d> Vertices3d; // Vertices  matched with Arrangement by ID
	FDelaunay2 Delaunay;
	TArray<TArray<FIndex2i>> Boundaries;
	TArray<FIndex3i> Triangles;
	TArray<FDebugLines> DebugLines;
	TArray<TSharedPtr<FRoadPolygoneBase>> Polygons;
	double UV0ScaleFactor;
	double UV1ScaleFactor;
	double UV2ScaleFactor;

	FCriticalSection RenderAPIMutex;

	FDynamicMesh3 FullMesh3d; // Used for AABBTree3d
	UE::Geometry::FDynamicMeshAABBTree3 AABBTree3d;

	FDynamicMesh3 FullMesh2d; // Similar FullMesh3d, but with Z=0, Used for AABBTree3d
	UE::Geometry::FDynamicMeshAABBTree3 AABBTree2d;

	void AddDebugLines(const TArray<FIndex2i>& Boundaries, const FColor& Color, float Thickness);
	void AddDebugLines(int GID, const FColor& Color, float Thickness);
	bool IsBoundaryVertex(int VID) const;
	bool FindRayIntersection(const FVector2D& Point, FHitResult& HitOut) const;

};

/**
 * FRoadBaseOperator
 */
class FRoadBaseOperator : public TGenericDataOperator<FRoadBaseOperatorData>
{
public:
	FRoadBaseOperator() = default;
	virtual ~FRoadBaseOperator();

	ERoadOverlapStrategy OverlapStrategy = ERoadOverlapStrategy::UseMaxZ;
	double OverlapRadius = 500;
	double MaxSquareDistanceFromSpline = 1.0;
	double MaxSquareDistanceFromCap = 1.0;
	double MinSegmentLength = 375;
	double VertexSnapTol = 0.01;
	double UV0ScaleFactor = 0.0025;
	double UV1ScaleFactor = 0.001;
	double UV2ScaleFactor = 0.001;
	bool bSmooth = true;
	float SmoothSpeed = 0.1f;
	float Smoothness = 0.5f;
	bool bDrawBoundaries = false;

public:
	void SetActorWithRoads(const AActor* Actors);

public:
	virtual void CalculateResult(FProgressCancel* Progress) override;
};

class FDynamicMeshWithMaterialsOperator : public FDynamicMeshOperator
{
public:
	FDynamicMeshWithMaterialsOperator() = default;

	TArray<FName> ResultMaterialSlots;
};

/**
 * FDriveSurfaceOp
 */
class FDriveSurfaceOp : public FDynamicMeshWithMaterialsOperator
{
public:
	FDriveSurfaceOp() = default;

	TSharedPtr<FRoadBaseOperatorData> BaseData;
	FName DriveSurfaceIslandMaterial = "Default";
	bool bComputVertexColor = true;
	double VertexColorSmoothRadius = 200;
	FColor DefaultVertexColor = FColor::White;
	FColor EdgeVertexColor = FColor::Black;
	bool bSplitBySections = false;
	double MergeSectionsAreaThreshold = 25 * 100 * 100;

	virtual void CalculateResult(FProgressCancel* Progress) override;
};

/**
 * FDecalsOp
 */
class FDecalsOp : public FDynamicMeshWithMaterialsOperator
{
public:
	FDecalsOp() = default;

	TSharedPtr<FRoadBaseOperatorData> BaseData;
	//double VScaleFactor = 0.0025;
	double DecalOffset = 3;
	bool bSplitBySections = false;
	double MergeSectionsAreaThreshold = 25 * 100 * 100;

	virtual void CalculateResult(FProgressCancel* Progress) override;
};

/**
 * FSidewalksOp
 */
class FSidewalksOp : public FDynamicMeshWithMaterialsOperator
{
public:
	FSidewalksOp() = default;

	TSharedPtr<FRoadBaseOperatorData> BaseData;
	double SidewalkHeight = 10;
	bool bSplitBySections = false;
	double MergeSectionsAreaThreshold = 25 * 100 * 100;

	virtual void CalculateResult(FProgressCancel* Progress) override;
};

/**
 * FCurbsOp
 */
class FCurbsOp : public FDynamicMeshWithMaterialsOperator
{
public:
	FCurbsOp() = default;

	TSharedPtr<FRoadBaseOperatorData> BaseData;
	double MarkOffset = 3;
	double CurbsHeight = 10;
	double UV0Scale = 0.001;

	virtual void CalculateResult(FProgressCancel* Progress) override;
};


/**
 * FMarksOp
 */
class FMarksOp : public FDynamicMeshWithMaterialsOperator
{
public:
	FMarksOp() = default;

	TSharedPtr<FRoadBaseOperatorData> BaseData;
	double MarkOffset = 3;

	virtual void CalculateResult(FProgressCancel* Progress) override;
};


/**
 * FRefSplineOp
 */

class FSplineMeshOp : public UnrealDrive::FSplineMeshOperator
{
public:
	FSplineMeshOp();

	TSharedPtr<FRoadBaseOperatorData> BaseData;
	bool bDrawRefSplines = false;
	
	virtual void CalculateResult(FProgressCancel* Progress) override;
};


} // UnrealDrive
