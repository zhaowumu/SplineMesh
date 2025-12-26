/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "CoreMinimal.h"
#include "RoadSplineComponent.h"
#include "Geometry/DynamicGraph2.h"
#include "DynamicMesh/DynamicMesh3.h"

//#include "Kismet/BlueprintFunctionLibrary.h"
//#include "OpUtils.generated.h"


namespace OpUtils 
{
	using namespace UE::Geometry;

	UNREALDRIVEEDITOR_API void AppendMesh(FDynamicMesh3& Dst, FDynamicMesh3& Src);
	UNREALDRIVEEDITOR_API void EnableDefaultAttributes(FDynamicMesh3& DynamicMesh, bool bNormals, bool bColors, bool bMaterialIDs, bool TriangleGroups, int NumUVs);

	using TGIDFilter = TFunction<bool(int)>;
	UNREALDRIVEEDITOR_API bool FindBoundary(const UnrealDrive::FDynamicGraph2d& Graph, const TArray<FIndex2i> & SkipEdges, TArray<FIndex2i>& Boundary, const TGIDFilter& GIDFilter);
	UNREALDRIVEEDITOR_API int FindBoundaries(const UnrealDrive::FDynamicGraph2d& Graph, const TArray<FIndex2i>& SkipEdges, TArray<TArray<FIndex2i>>& Boundaries, TGIDFilter GIDFilter);
	inline int FindBoundaries(const UnrealDrive::FDynamicGraph2d& Graph, const TArray<FIndex2i>& SkipEdges, TArray<TArray<FIndex2i>>& Boundaries, int GID)
	{ 
		return FindBoundaries(Graph, SkipEdges, Boundaries, [AllowedGID = GID](int GID) { return AllowedGID == -1 || AllowedGID == GID; });
	}
	UNREALDRIVEEDITOR_API TArray<FIndex2i> MergeBoundaries(const TArray<TArray<FIndex2i>>& Boundary);
	UNREALDRIVEEDITOR_API TArray<FIndex2i> MergeBoundaries(const TArray<TArray<FIndex2i>>& Boundary, const TArray<TArray<FIndex2i>>& Holes);
	UNREALDRIVEEDITOR_API bool IsSameBoundary(const TArray<FIndex2i>& BoundaryA, const TArray<FIndex2i>& BoundaryB);
	// Remove the probe boundaries from the target boundaries
	UNREALDRIVEEDITOR_API void RemoveBoundaries(const TArray<TArray<FIndex2i>>& ProbeBoundaries, TArray<TArray<FIndex2i>>& TargetBoundaries);
	UNREALDRIVEEDITOR_API TArray<FIndex2i> ReverseBoundary(const TArray<FIndex2i>& Boundary);

	UNREALDRIVEEDITOR_API void RemoveTriangles(const TArray<FIndex3i>& Probes, TArray<FIndex3i>& Targets);

	inline bool IsTriangleValid(const FIndex3i& T)
	{
		return T.A != IndexConstants::InvalidID && T.B != IndexConstants::InvalidID && T.C != IndexConstants::InvalidID;
	}

	inline double AngleBetweenNormals(const FVector& V1, const FVector& V2)
	{
		double s = FVector::CrossProduct(V1, V2).Size();
		double c = FVector::DotProduct(V1, V2);
		return FMath::Atan2(s, c);
	}


	UNREALDRIVEEDITOR_API void RemovedPolylineSelfIntersection(TArray<FVector2D>& Polyline, bool bParallel = true);

} // OpUtils

// For debug only
/*
UCLASS()
class UNREALDRIVEEDITOR_API UOpUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable)
	static bool BakeCurve(const UCurveFloat* SrcCurve, UCurveFloat* TargetCurve, float TimeStart, float TimeEnd, float MaxSquareDistanceFromCurve, float Tolerance, int ReparamSteps = 200);
};
*/

