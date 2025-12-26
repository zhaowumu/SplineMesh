/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "Utils/OpUtils.h"
#include "DynamicMeshEditor.h"
#include "Curve/CurveUtil.h"
#include "Intersection/IntrSegment2Segment2.h"
#include "UnrealDrive.h"

using namespace UnrealDrive;

void OpUtils::AppendMesh(FDynamicMesh3& Dst, FDynamicMesh3& Src)
{
	Src.CompactInPlace();
	FDynamicMeshEditor MeshEditor(&Dst);
	FMeshIndexMappings IndexMaps;
	MeshEditor.AppendMesh(&Src, IndexMaps);
}

void OpUtils::EnableDefaultAttributes(FDynamicMesh3& DynamicMesh, bool bNormals, bool bColors, bool bMaterialIDs, bool bTriangleGroups, int NumUVs)
{
	DynamicMesh.EnableAttributes();

	if (bTriangleGroups)
	{
		DynamicMesh.EnableTriangleGroups();
	}

	if (bNormals)
	{
		DynamicMesh.EnableVertexNormals(FVector3f::ZAxisVector);
	}

	if (bColors)
	{
		DynamicMesh.EnableVertexColors(FVector3f::ZeroVector);
		DynamicMesh.Attributes()->EnablePrimaryColors();
	}

	if (NumUVs)
	{
		DynamicMesh.EnableVertexUVs(FVector2f::Zero());
		DynamicMesh.Attributes()->SetNumUVLayers(NumUVs);
	}

	if (bMaterialIDs)
	{
		DynamicMesh.Attributes()->EnableMaterialID();
	}
}

TArray<FIndex2i> OpUtils::MergeBoundaries(const TArray<TArray<FIndex2i>>& Boundary)
{
	TArray<FIndex2i> Ret;
	for (auto& It : Boundary) Ret.Append(It);
	return Ret;
}

TArray<FIndex2i> OpUtils::MergeBoundaries(const TArray<TArray<FIndex2i>>& Boundary, const TArray<TArray<FIndex2i>>& Holes)
{
	TArray<FIndex2i> Edges = MergeBoundaries(Boundary);
	Edges.Append(ReverseBoundary(MergeBoundaries(Holes)));
	return Edges;
}

bool OpUtils::IsSameBoundary(const TArray<FIndex2i>& BoundaryA, const TArray<FIndex2i>& BoundaryB)
{
	if (BoundaryA.Num() == 0 && BoundaryB.Num() == 0)
	{
		return true;
	}

	if (BoundaryA.Num() != BoundaryB.Num())
	{
		return false;
	}

	const int Num = BoundaryA.Num();

	int FirstIndB = INDEX_NONE;
	for (int i = 0; i < Num; ++i)
	{
		if (BoundaryB[i] == BoundaryA[0])
		{
			FirstIndB = i;
			break;
		}
	}

	if (FirstIndB == INDEX_NONE)
	{
		return false;
	}

	for (int i = 0; i < Num; ++i)
	{
		if (BoundaryA[i] != BoundaryB[(i + FirstIndB) % Num])
		{
			return false;
		}
	}

	return true;
}

void OpUtils::RemoveBoundaries(const TArray<TArray<FIndex2i>>& Probes, TArray<TArray<FIndex2i>>& Targets)
{
	for (auto& Probe : Probes)
	{
		for (auto It = Targets.CreateIterator(); It; ++It)
		{
			if (IsSameBoundary(Probe, *It))
			{
				It.RemoveCurrent();
			}
		}
	}
}

TArray<FIndex2i> OpUtils::ReverseBoundary(const TArray<FIndex2i>& Boundary)
{
	TArray<FIndex2i> Edges;
	for (int i = Boundary.Num() - 1; i >= 0; --i)
	{
		Edges.Add({ Boundary[i].B, Boundary[i].A });
	}
	return Edges;
}

void OpUtils::RemoveTriangles(const TArray<FIndex3i>& Probes, TArray<FIndex3i>& Targets)
{
	for (auto& Probe : Probes)
	{
		for (auto It = Targets.CreateIterator(); It; ++It)
		{
			if (It->Contains(Probe.A) && It->Contains(Probe.B) && It->Contains(Probe.C))
			{
				It.RemoveCurrent();
			}
		}
	}
}

static bool IsExist(const TArray<FIndex2i>& Edges, FIndex2i Edge)
{
	for (auto& It : Edges)
	{
		if (It.A == Edge.A && It.B == Edge.B || It.A == Edge.B && It.B == Edge.A)
		{
			return true;
		}
	}
	return false;
}

static bool VertexHasGID(const UnrealDrive::FDynamicGraph2d& Graph, int VID, const TArray<FIndex2i>& SkipEdges, const OpUtils::TGIDFilter& GIDFilter)
{
	if (!Graph.IsVertex(VID))
	{
		return false;
	}

	for (int EID : Graph.VtxEdgesItr(VID))
	{
		auto Edge = Graph.GetEdgeCopy(EID);

		if (GIDFilter(Edge.Group) && !IsExist(SkipEdges, { Edge.A, Edge.B }))
		{
			return true;
		}
	}

	return false;
}

static void RemoveVertex(UnrealDrive::FDynamicGraph2d& Graph, int VID)
{
	if (Graph.IsVertex(VID))
	{
		for (int EID : Graph.VtxEdgesItr(VID))
		{
			if (Graph.IsEdge(EID))
			{
				Graph.RemoveEdge(EID, true);
			}
		}
	}
}

template <typename T>
double AngleBetweenTwoNormals2D(const T& BaseVectorNormal, const T& RelativeVectorNorma)
{
	return FMath::UnwindRadians(std::atan2(RelativeVectorNorma.Y, RelativeVectorNorma.X) - std::atan2(BaseVectorNormal.Y, BaseVectorNormal.X));
}

bool OpUtils::FindBoundary(const UnrealDrive::FDynamicGraph2d& Graph, const TArray<FIndex2i>& SkipEdges, TArray<FIndex2i>& Boundary, const TGIDFilter& GIDFilter)
{
	Boundary.Empty();

	if (Graph.VertexCount() == 0)
	{
		return false;
	}

	FVector2d V;
	int VID = -1;
	FVector2d VDir = FVector2d(1, 0);

	// Find first vertex
	for (int CheckVID : Graph.VertexIndices())
	{
		if (VertexHasGID(Graph, CheckVID, SkipEdges, GIDFilter))
		{
			FVector2d CheckV = Graph.GetVertex(CheckVID);
			if (VID < 0 || CheckV.Y < V.Y || (CheckV.Y == V.Y && CheckV.X < V.X))
			{
				V = CheckV;
				VID = CheckVID;
			}
		}
	}

	if (VID == -1)
	{
		return false;
	}

	bool bContoureFound = false;

	while (true)
	{
		struct FCandidat
		{
			FVector2d V{};
			int VID = -1;
			double Angle = 0;
			FVector2d Dir{};
		};

		FCandidat Candidate{};

		for (int NbrVID : Graph.VtxVerticesItr(VID))
		{
			if (VertexHasGID(Graph, NbrVID, SkipEdges, GIDFilter) && (Boundary.Num() == 0 || Boundary.Last().A != NbrVID))
			{
				FVector2d NbrV = Graph.GetVertex(NbrVID);
				FVector2d Dir = (NbrV - V).GetSafeNormal();
				double Ang = AngleBetweenTwoNormals2D(VDir, Dir);
				if (Ang < 0) Ang += PI * 2.0;
				if (Ang < Candidate.Angle || Candidate.VID < 0)
				{
					Candidate.V = NbrV;
					Candidate.Angle = Ang;
					Candidate.Dir = -Dir;
					Candidate.VID = NbrVID;
				}
				//UE_LOG(LogUnrealDrive, Log, TEXT("*** angle: %f; Pos: %i, Dir: %s, PrevDir: %s"), Ang / PI * 180, int(Edge.PosSide), *Dir.ToString(), *VertexDir.ToString());
			}
		}

		if (Candidate.VID >= 0)
		{
			Boundary.Add({ VID, Candidate.VID });

			V = Candidate.V;
			VID = Candidate.VID;
			VDir = Candidate.Dir;

			//UE_LOG(LogUnrealDrive, Warning, TEXT("*** %f"), CandidateAngle / PI * 180);

			bool bLoopDetected = false;
			for (int i = 0; i < Boundary.Num() - 1; ++i)
			{
				auto& It = Boundary[i];
				if (It.Contains(VID))
				{
					bLoopDetected = true;
				}
			}

			if (bLoopDetected)
			{
				if (Boundary[0].A == VID)
				{
					bContoureFound = true;
					break;
				}
				else
				{
					bContoureFound = false;
					//UE_LOG(LogUnrealDrive, Warning, TEXT("FArrangement25D::FindBoundaries(); Wrong loop detected"));
					break;
				}
			}
		}
		else
		{
			bContoureFound = false;
			//UE_LOG(LogUnrealDrive, Warning, TEXT("FArrangement25D::FindBoundaries(); Can't find contour"));
			break;
		}
	}

	/*
	std::reverse(Boundary.begin(), Boundary.end());
	for (auto& It : Boundary)
	{
		It = { It.B, It.A };
	}
	*/


	return bContoureFound;
}

int OpUtils::FindBoundaries(const FDynamicGraph2d& Graph, const TArray<FIndex2i>& SkipEdges, TArray<TArray<FIndex2i>>& Boundaries, TGIDFilter GIDFilter)
{
	Boundaries.Empty();

	auto GrapCopy = Graph;

	int FoundBoundariesNum = 0;
	TArray<FIndex2i> Contour;
	while (true)
	{
		const bool BoundaryFound = FindBoundary(GrapCopy, SkipEdges, Contour, GIDFilter);

		if (BoundaryFound)
		{
			check(Contour.Num() > 2);
		}

		//UE_LOG(LogUnrealDrive, Warning, TEXT("FArrangement25D::FindBoundaries(); valid: %i;  num: %i"), BoundaryFound, Contour.Num());

		if (Contour.Num() == 0)
		{
			return FoundBoundariesNum;
		}

		//UE_LOG(LogUnrealDrive, Warning, TEXT("FArrangement25D::FindBoundaries();  all %i"), GrapCopy.VertexCount());

		//int k = 0;
		// Remove all vertex inside boundary
		if (BoundaryFound)
		{
			TArray<FVector2d> Poly;
			Poly.Add(GrapCopy.GetVertex(Contour[0].A));
			for (auto& Ind : Contour)
			{
				Poly.Add(GrapCopy.GetVertex(Ind.B));
			}
			Poly.RemoveAt(Poly.Num() - 1);


			for (int VID = 0; VID < GrapCopy.MaxVertexID(); ++VID)
			{
				if (GrapCopy.IsVertex(VID))
				{
					auto V = GrapCopy.GetVertex(VID);
					if (CurveUtil::Contains2<double, FVector2d>(Poly, V))
					{
						RemoveVertex(GrapCopy, VID);
						//++k;
						//UE_LOG(LogUnrealDrive, Warning, TEXT("FArrangement25D::FindBoundaries(); remove %i"), VID);
					}
					else
					{
						//UE_LOG(LogUnrealDrive, Warning, TEXT("FArrangement25D::FindBoundaries(); skip %i"), VID);
					}
				}
			}

			Boundaries.Add(Contour);
			++FoundBoundariesNum;
		}
		//UE_LOG(LogUnrealDrive, Warning, TEXT("FArrangement25D::FindBoundaries(); in %i %i"), GrapCopy.VertexCount(), k);


		// Remove boundary 
		for (auto& Ind : Contour)
		{
			RemoveVertex(GrapCopy, Ind.A);
			RemoveVertex(GrapCopy, Ind.B);
		}

		//UE_LOG(LogUnrealDrive, Warning, TEXT("FArrangement25D::FindBoundaries(); out %i"), GrapCopy.VertexCount());


		Contour.Empty();
	}

	return FoundBoundariesNum;
}


/*
int FArrangement2d::MinimalCycleBasis(TArray<FIndex2i>& Boundaries, int GID) const
{
	Boundaries.Empty();

	check(Graph.MaxVertexID() == Graph.VertexCount());

	std::vector<std::array<double, 2>> Positions;
	std::vector<std::array<int, 2>> Edges;


	for (int VID : Graph.VertexIndices())
	{
		auto V = Graph.GetVertex(VID);
		Positions.push_back({ V.X, V.Y });

		//for (int EID : Graph.VtxEdgesItr(VID))
		for (int NbrVID : Graph.VtxVerticesItr(VID))
		{
			int EID = Graph.FindEdge(VID, NbrVID);
			if (EID != FDynamicGraph::InvalidID)
			{
				auto Edge = Graph.GetEdge(EID);
				if (Edge.Group == 1)
				{
					Edges.push_back({ VID, NbrVID });
				}
			}
		}
	}

	std::vector<std::shared_ptr<gte::MinimalCycleBasis<double>::Tree>> Forest;
	gte::MinimalCycleBasis<double> MinimalCycleBasis{ Positions, Edges, Forest };

	return 0;
}
*/

template<typename RealType>
static bool FindPolylineSelfIntersection(
	const TArray<UE::Math::TVector2<RealType>>& Polyline,
	UE::Math::TVector2<RealType>& IntersectionPointOut,
	FIndex2i& IntersectionIndexOut,
	bool bParallel = true)
{
	std::atomic<bool> bSelfIntersects(false);
	bool IsLoop = (Polyline[0] - Polyline.Last()).IsNearlyZero(UE_KINDA_SMALL_NUMBER);
	int32 N = Polyline.Num() - (int)IsLoop;

	if (N == 0)
	{
		return false;
	}

	ParallelFor(N - 1, [&](int32 i)
	{
		TSegment2<RealType> SegA(Polyline[i], Polyline[i + 1]);
		for (int32 j = i + 2; j < N - 1 && bSelfIntersects == false; ++j)
		{
			TSegment2<RealType> SegB(Polyline[j], Polyline[j + 1]);
			if (SegA.Intersects(SegB) && bSelfIntersects == false)
			{
				bool ExpectedValue = false;
				if (std::atomic_compare_exchange_strong(&bSelfIntersects, &ExpectedValue, true))
				{
					UE::Geometry::TIntrSegment2Segment2<RealType> Intersection(SegA, SegB);
					Intersection.Find();
					IntersectionPointOut = Intersection.Point0;
					IntersectionIndexOut = FIndex2i(i, j);
					return;
				}
			}
		}
	}, (bParallel) ? EParallelForFlags::None : EParallelForFlags::ForceSingleThread);

	return bSelfIntersects;
}

void OpUtils::RemovedPolylineSelfIntersection(TArray<FVector2D>& Polyline, bool bParallel )
{
	// Remove self intersections
	FVector2D IntersectionPoint;
	FIndex2i IntersectionIndex;
	while (FindPolylineSelfIntersection(Polyline, IntersectionPoint, IntersectionIndex, true))
	{
		FVector2D PtA = Polyline[IntersectionIndex.A];
		FVector2D PtB = Polyline[IntersectionIndex.B];
		Polyline.RemoveAt(IntersectionIndex.A + 1, IntersectionIndex.B - IntersectionIndex.A, EAllowShrinking::No);
		Polyline.Insert(IntersectionPoint, IntersectionIndex.A + 1);
	}
};
