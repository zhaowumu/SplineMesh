/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "TriangulateRoadOp.h"
#include "DefaultRoadLaneAttributes.h"
#include "DynamicMesh/MeshNormals.h"
#include "SmoothingOps/CotanSmoothingOp.h"
#include "UnrealDrive.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(TriangulateRoadOp)

#define LOCTEXT_NAMESPACE "RoadBaseOperator"

using namespace UnrealDrive;

bool MakeDelaunay2(const UnrealDrive::FDynamicGraph2d Graph, FDelaunay2& Delaunay, TArray<int32>* SkippedEdges)
{

	// A flat array of the vertices, copied out of the graph
	TArray<FVector2d> InputVertices;

	check(Graph.MaxVertexID() == Graph.VertexCount());

	TArray<int> InputIndices, OutputIndices;

	for (int i = 0; i < Graph.MaxVertexID(); i++)
	{
		FVector2d Vertex = FVector2d(Graph.GetVertex(i));
		InputVertices.Add(Vertex);
	}

	Delaunay.bAutomaticallyFixEdgesToDuplicateVertices = false; // Arrangement will remove duplicates already

	if (!Delaunay.Triangulate(InputVertices))
	{
		return false;
	}

	Delaunay.bValidateEdges = false;
	Delaunay.bKeepFastEdgeAdjacencyData = true;

	//bool bInsertConstraintFailure = false;

	TArray<FIndex2i> AllEdges;

	for (int EdgeIdx : Graph.EdgeIndices())
	{
		auto& Edge = Graph.GetEdgeRef(EdgeIdx);
		AllEdges.Emplace(Edge.A, Edge.B);
	}

	Delaunay.ConstrainEdges(InputVertices, AllEdges);

	// Verify all edges after all constraints are in -- to ensure that inserted edges were also not removed by subsequent edge insertion
	for (int EdgeIdx : Graph.EdgeIndices())
	{
		auto& Edge = Graph.GetEdgeRef(EdgeIdx);
		if (!Delaunay.HasEdge(FIndex2i(Edge.A, Edge.B), false))
		{
			//bInsertConstraintFailure = true;
			if (SkippedEdges)
			{
				SkippedEdges->Add(EdgeIdx);
			}
		}
	}

	//return !bInsertConstraintFailure;
	return true;

}

static bool IsSameTri(const FIndex3i& A, const FIndex3i& B)
{
	return A.Contains(B.A) && A.Contains(B.B) && A.Contains(B.C);
}


//------------------------------------------------------------------------------------------------------------------------------------------------------

void FRoadBaseOperatorData::AddDebugLines(const TArray<FIndex2i>& InBoundaries, const FColor& Color, float Thickness)
{
	auto & DebugLine = DebugLines.Add_GetRef({});
	DebugLine.Color = Color;
	DebugLine.Thickness = Thickness;

	auto& Graph = Arrangement->Graph;
	for (auto& Ind : InBoundaries)
	{
		DebugLine.Lines.Add({ Vertices3d[Ind.A].Vertex, Vertices3d[Ind.B].Vertex });
	}

}

void FRoadBaseOperatorData::AddDebugLines(int GID, const FColor& Color, float Thickness)
{
	auto& DebugLine = DebugLines.Add_GetRef({});
	DebugLine.Color = Color;
	DebugLine.Thickness = Thickness;

	auto& Graph = Arrangement->Graph;

	for (int EID : Graph.EdgeIndices())
	{
		auto& Edge = Graph.GetEdgeRef(EID);

		//if(Edge.PolylinesID.Contains(3))
		if (GID == -1 || Graph.GetEdgeGroup(EID) == GID )
		{
			FVector2d A = Graph.GetVertex(Edge.A);
			FVector2d B = Graph.GetVertex(Edge.B);

			//DebugLine.Lines.Add({ Vertices3d[Edge.A].Vertex, Vertices3d[Edge.B].Vertex });
			DebugLine.Lines.Add({ FVector{A.X, A.Y, 50}, FVector{B.X, B.Y, 50} });
		}
	}
}

bool FRoadBaseOperatorData::IsBoundaryVertex(int VID) const
{
	for (auto& Boundary : Boundaries)
	{
		if (Boundary.FindByPredicate([VID](const FIndex2i& Index) { return Index.A == VID || Index.B == VID; }))
		{
			return true;
		}
	}
	return false;
}

bool FRoadBaseOperatorData::FindRayIntersection(const FVector2D& Point, FHitResult& HitOut) const
{
	const FVector Point3d{ Point.X, Point.Y, 0.0 };
	AABBTree2d.FindNearestPoint(Point3d);
	double NearestDistSqr;
	int32 NearTriID = AABBTree2d.FindNearestTriangle(Point3d, NearestDistSqr);
	if (NearTriID >= 0)
	{
		FTriangle3d Triangle;
		FullMesh3d.GetTriVertices(NearTriID, Triangle.V[0], Triangle.V[1], Triangle.V[2]);

		HitOut.FaceIndex = NearTriID;
		HitOut.Normal = Triangle.Normal();
		HitOut.ImpactPoint = FMath::RayPlaneIntersection(FVector{ Point.X, Point.Y, Bounds.Max.Z }, FVector{ 0.0, 0.0, -1.0 }, FPlane{ Triangle.V[0], Triangle.V[1], Triangle.V[2] });
		return true;
	}

	return false;
}

FRoadBaseOperator::~FRoadBaseOperator()
{
}

void FRoadBaseOperator::SetActorWithRoads(const AActor* Actor)
{
	Result = MakeUnique<FRoadBaseOperatorData>();

	TArray<const URoadSplineComponent*> Splines;
	Actor->GetComponents(Splines);

	Result->RoadSplinesCache.Reserve(Splines.Num());

	for (auto& Spline : Splines)
	{
		Result->RoadSplinesCache.Emplace(Spline);
	}

	Result->ActorTransform = Actor->GetTransform();
}

void FRoadBaseOperator::CalculateResult(FProgressCancel* Progress)
{
	//SCOPE_LOG_TIME_IN_SECONDS(TEXT("    Full time"), nullptr);

#define CHECK_CANCLE() if (Progress && Progress->Cancelled()) { Result->ResultInfo.Result = EGeometryResultType::Cancelled; return; }

	Result->ResultInfo.Result = EGeometryResultType::InProgress;
	Result->UV0ScaleFactor = UV0ScaleFactor;
	Result->UV1ScaleFactor = UV1ScaleFactor;
	Result->UV2ScaleFactor = UV2ScaleFactor;
	Result->Bounds = {};
	for (auto& RoadSplineCache : Result->RoadSplinesCache)
	{
		auto Bound = RoadSplineCache.CalcBounds(FTransform::Identity);
		Result->Bounds.Contain( -Bound.BoxExtent );
		Result->Bounds.Contain( Bound.BoxExtent );
	}
	Result->Arrangement = MakeUnique<UnrealDrive::FArrangement2d>(FAxisAlignedBox2d{ FVector2d{Result->Bounds.Min}, FVector2d{Result->Bounds.Max} });
	Result->Arrangement->VertexSnapTol = VertexSnapTol;

	const auto& Graph = Result->Arrangement->Graph;

	CHECK_CANCLE();

	// ========================== Prepare SplinesCurves2d ==========================
	for (auto& RoadSplineCache : Result->RoadSplinesCache)
	{
		RoadSplineCache.UpdateSplinesCurves2d();
	}

	// ========================== Make LanesPoly arrangement ==========================
	{
		//SCOPE_LOG_TIME_IN_SECONDS(TEXT("Make lanes poly"), nullptr);
		for (int SplineIndex = 0; SplineIndex < Result->RoadSplinesCache.Num(); ++SplineIndex)
		{
			auto& RoadSplineCache = Result->RoadSplinesCache[SplineIndex];
			if (RoadSplineCache.bSkipProcrdureGeneration)
			{
				continue;
			}

			for (int SectionIndex = 0; SectionIndex < RoadSplineCache.RoadLayout.Sections.Num(); ++SectionIndex)
			{
				auto& Section = RoadSplineCache.RoadLayout.Sections[SectionIndex];
				for (int LaneIndex = -Section.Left.Num(); LaneIndex <= Section.Right.Num(); ++LaneIndex)
				{
					if (LaneIndex != LANE_INDEX_NONE && Section.GetLaneByIndex(LaneIndex).bSkipProcrdureGeneration)
					{
						continue;
					}

					auto Poly = MakeShared<FRoadLanePolygone>(*Result, SplineIndex, SectionIndex, LaneIndex, MaxSquareDistanceFromSpline, MaxSquareDistanceFromCap, MinSegmentLength);
					if (!Poly->GetResult().HasResult())
					{
						Result->ResultInfo.Warnings.Append(Poly->GetResult().Warnings);
						Result->ResultInfo.Errors.Append(Poly->GetResult().Errors);
						Result->ResultInfo.SetFailed(LOCTEXT("CalculateResultFail_LanePoly", "Base: Can't make the lane polygone"));
						return;
					}
					Result->Polygons.Add(MoveTemp(Poly));
					CHECK_CANCLE();
				}
			}

			if (RoadSplineCache.bIsClosedLoop && RoadSplineCache.RoadLayout.FilledInstance.IsValid())
			{
				auto Poly = MakeShared<FRoadSimplePolygone>(*Result, SplineIndex, MaxSquareDistanceFromSpline, MinSegmentLength);
				if (!Poly->GetResult().HasResult())
				{
					Result->ResultInfo.Warnings.Append(Poly->GetResult().Warnings);
					Result->ResultInfo.Errors.Append(Poly->GetResult().Errors);
					Result->ResultInfo.SetFailed(LOCTEXT("CalculateResultFail_SimplePoly", "Base: Can't make the simple polygone"));
					return;
				}
				Result->Polygons.Add(MoveTemp(Poly));
				CHECK_CANCLE();
			}
		}
	}

	// ========================== Complete LanesPoly arrangement  ==========================
	{
		//Result->AddDebugLines(-1, FColor(255, 255, 0, 50), 4.0);

		Result->Vertices3d.SetNum(Graph.MaxVertexID());

		for (auto& Poly : Result->Polygons)
		{
			bool bSucces = Poly->CompleteArrangement();
			Result->ResultInfo.Warnings.Append(Poly->GetResult().Warnings);
			Result->ResultInfo.Errors.Append(Poly->GetResult().Errors);
			if (!bSucces)
			{
				Result->ResultInfo.SetFailed(LOCTEXT("CalculateResultFail_PolyComplete", "Base: Can't complete the arrangement for one of the PolyLane"));
				return;
			}
		}

		/*
		int Layer = 0;
		for (auto& Poly : Result->Polygons)
		{
			//auto& DebugLine = Result->DebugLines.Add_GetRef({});
			//DebugLine.Color = FColor(0, 255, 0, 50);
			//DebugLine.Thickness = 4;
			//for (int i = 0; i < Poly.Poly2d.VertexCount(); ++i)
			//{
			//	auto& PtA = Poly.Poly2d[i];
			//	auto& PtB = Poly.Poly2d[(i + 1) % Poly.Poly2d.VertexCount()];
			//	DebugLine.Lines.Add({ FVector{PtA.X, PtA.Y, 200.0 + Layer * 100.0},  FVector{PtB.X, PtB.Y, 200.0 + Layer * 100.0} });
			//}
			//++Layer;
			

			auto& DebugLine = Result->DebugLines.Add_GetRef({});
			DebugLine.Color = FColor(0, 255, 0, 50);
			DebugLine.Thickness = 4;
			for (auto& It : Poly->Boundary)
			{
				auto PtA = Graph.GetVertex(It.A);
				auto PtB = Graph.GetVertex(It.B);
				DebugLine.Lines.Add({ FVector{PtA.X, PtA.Y, 200.0 + Layer * 100.0},  FVector{PtB.X, PtB.Y, 200.0 + Layer * 100.0} });
			}
			++Layer;
		}
		*/
	}

	CHECK_CANCLE();

	// ========================== Find boundaries ==========================
	{
		//SCOPE_LOG_TIME_IN_SECONDS(TEXT("Find boundaries"), nullptr);
		int FoundBoundariesNum = OpUtils::FindBoundaries(Graph, {}, Result->Boundaries, [](int GID) { return GID != GUIFlags::CenterLine; });
		//UE_LOG(LogUnrealDrive, Log, TEXT("%32s - %6i"), TEXT("Boundaries num"), FoundBoundariesNum);
	}
	if (!Result->Boundaries.Num())
	{
		Result->ResultInfo.SetFailed(LOCTEXT("CalculateResultFail_Boundaries", "Base: Can't find boundaries"));
		return;
	}

	CHECK_CANCLE();

	// ========================== Triangulate ==========================
	{
		TArray<int32> SkippedEdges;
		if (!MakeDelaunay2(Result->Arrangement->Graph, Result->Delaunay, &SkippedEdges))
		{
			Result->ResultInfo.SetFailed(LOCTEXT("CalculateResultFail_Triangulate", "Base: Can't triangulate"));
			return;
		}
	}

	CHECK_CANCLE();

	// ========================== Get all triangles ==========================

	Result->Triangles = Result->Delaunay.GetFilledTriangles(OpUtils::MergeBoundaries(Result->Boundaries), FDelaunay2::EFillMode::NonZeroWinding);
	if (Result->Triangles.Num() == 0)
	{
		Result->ResultInfo.SetFailed(LOCTEXT("CalculateResultFail_NoTriangles", "Base: No triangles"));
		return;
	}

	for (FIndex3i& T : Result->Triangles)
	{
		T = FIndex3i(T.C, T.B, T.A);
	}

	CHECK_CANCLE();


	// ========================== Find LanesPoly triangles  ==========================

	for (auto& Poly : Result->Polygons)
	{
		if (!Poly->IsPolyline())
		{
			const auto& SplineBounds = Result->RoadSplinesCache[Poly->SplineIndex].SplineBounds;

			TArray<FIndex3i> Triangles = Result->Delaunay.GetFilledTriangles(OpUtils::MergeBoundaries({ Poly->Boundary }, Poly->Holse), FDelaunay2::EFillMode::NonZeroWinding);
			if (Triangles.Num() == 0)
			{
				Result->ResultInfo.SetFailed(FText::Format(LOCTEXT("CalculateResultFail_PolyTry", "Base: Can't get filled triangles for {0}"), Poly->GetDescription()));
				continue;
			}
			for (const FIndex3i& T : Triangles)
			{
				int TID = Result->Triangles.Find(FIndex3i(T.C, T.B, T.A));
				if (TID != INDEX_NONE)
				{
					Poly->TrianglesIDs.Add(TID);
				}

				Poly->AddVertexInfo(T.A, &SplineBounds, ERoadVertexInfoFlags::OverlapPoly);
				Poly->AddVertexInfo(T.B, &SplineBounds, ERoadVertexInfoFlags::OverlapPoly);
				Poly->AddVertexInfo(T.C, &SplineBounds, ERoadVertexInfoFlags::OverlapPoly);
			}
			CHECK_CANCLE();
		}
	}

	CHECK_CANCLE();

	// ========================== Compute height ==========================
	
	// Set max or min z value for all Vertices3d
	for (int VID = 0; VID < Result->Vertices3d.Num(); ++VID)
	{
		auto& Verticex3d = Result->Vertices3d[VID];
		if (ensure(Verticex3d.Infos.Num()))
		{
			Verticex3d.Vertex = Verticex3d.Infos[0].Pos.Location;
			for (int i = 1; i < Verticex3d.Infos.Num(); ++i)
			{
				if (OverlapStrategy == ERoadOverlapStrategy::UseMaxZ)
				{
					Verticex3d.Vertex.Z = FMath::Max(Verticex3d.Vertex.Z, Verticex3d.Infos[i].Pos.Location.Z);
				}
				else // OverlapStrategy == ERoadOverlapStrategy::UseMinZ
				{
					Verticex3d.Vertex.Z = FMath::Min(Verticex3d.Vertex.Z, Verticex3d.Infos[i].Pos.Location.Z);
				}
			}
		}
		else
		{
			Result->ResultInfo.SetFailed({LOCTEXT("CalculateResultFail_MeshBroken", "Base: Mesh is broken") });
			return;
		}
	}

	CHECK_CANCLE();

	// Smooth z by kernal OverlapRadius
	if (OverlapRadius > KINDA_SMALL_NUMBER)
	{
		for (int VID = 0; VID < Result->Vertices3d.Num(); ++VID)
		{
			auto& Verticex3d = Result->Vertices3d[VID];
			auto DistanceSqFunc = [VID_A = VID, &Graph](int VID_B)
			{
				return DistanceSquared(Graph.GetVertex(VID_A), Graph.GetVertex(VID_B));
			};
			auto IgnoreFunc = [this, CurVID = VID](const int& VID)
			{
				if (CurVID == VID)
				{
					return true;
				}
				if (Result->IsBoundaryVertex(VID))
				{
					return true;
				}
				return false;
			};
			auto Points = Result->Arrangement->PointHash.FindAllInRadius(Graph.GetVertex(VID), OverlapRadius, DistanceSqFunc, IgnoreFunc);
			for (auto& [NearVID, DistSq] : Points)
			{
				double Alpha = FMath::Sqrt(DistSq) / OverlapRadius;
				double& Z = Result->Vertices3d[NearVID].Vertex.Z;
				if (OverlapStrategy == ERoadOverlapStrategy::UseMaxZ)
				{
					double MinZ = FMath::CubicInterp(Verticex3d.Vertex.Z, 0.0, Verticex3d.Vertex.Z - OverlapRadius, 0.0, Alpha);
					Z = FMath::Max(Z, MinZ);
				}
				else // OverlapStrategy == ERoadOverlapStrategy::UseMinZ
				{
					double MaxZ = FMath::CubicInterp(Verticex3d.Vertex.Z, 0.0, Verticex3d.Vertex.Z + OverlapRadius, 0.0, Alpha);
					Z = FMath::Min(Z, MaxZ);
				}
			}

			CHECK_CANCLE();
		}
	}
	
	CHECK_CANCLE();

	// ========================== Create DynamicMesh and Compute normals ==========================
	FDynamicMesh3 DynamicMesh(true, false, false, false);
	for (int VID = 0; VID < Graph.VertexCount(); ++VID)
	{
		const auto& Verticex3d = Result->Vertices3d[VID];
		int32 NewVID = DynamicMesh.AppendVertex(Verticex3d.Vertex);
		check(NewVID == VID);
	}
	for (int TID = 0; TID < Result->Triangles.Num(); ++TID)
	{
		auto& T = Result->Triangles[TID];
		DynamicMesh.InsertTriangle(TID, T);
	}
	FMeshNormals::QuickComputeVertexNormals(DynamicMesh);
	CHECK_CANCLE();

	// ========================== CotanSmoothingOp ==========================

	if(Result->RoadSplinesCache.Num() > 1 && bSmooth)
	{
		FSmoothingOpBase::FOptions SmoothingOptions;
		SmoothingOptions.SmoothAlpha = SmoothSpeed;
		SmoothingOptions.BoundarySmoothAlpha = 0.0;
		double NonlinearT = FMathd::Pow(Smoothness, 2.0);
		// this is an empirically-determined hack that seems to work OK to normalize the smoothing result for variable vertex count...
		double ScaledPower = (NonlinearT / 50.0) * Graph.VertexCount();
		SmoothingOptions.SmoothPower = ScaledPower;
		SmoothingOptions.bUniform = false;
		SmoothingOptions.bUseImplicit = true;
		SmoothingOptions.NormalOffset = 0.0f;

		FCotanSmoothingOp SmoothingOp(&DynamicMesh, SmoothingOptions);
		SmoothingOp.CalculateResult(Progress);
		CHECK_CANCLE();

		auto SmoothedMesh = SmoothingOp.ExtractResult();
		if (SmoothedMesh && SmoothedMesh->VertexCount() == DynamicMesh.VertexCount())
		{
			for (int VID = 0; VID < Graph.VertexCount(); ++VID)
			{
				auto& Verticex3d = Result->Vertices3d[VID];
				Verticex3d.Vertex.Z = SmoothedMesh->GetVertexRef(VID).Z;
				//Verticex3d.Vertex = SmoothedMesh->GetVertexRef(VID);
			}

			FMeshNormals::QuickComputeVertexNormals(DynamicMesh);
			CHECK_CANCLE();
		}
		else
		{
			Result->ResultInfo.AddWarning({ 0, LOCTEXT("CalculateResultFail_Smoothing", "Base: Can't smooth mesh") });
		}
	}

	// ========================== Set vertex normals ==========================
	
	for (int VID = 0; VID < Graph.VertexCount(); ++VID)
	{
		auto& Verticex3d = Result->Vertices3d[VID];
		auto Normal = DynamicMesh.GetVertexNormal(VID);
		Verticex3d.Normal = FVector{ Normal.X, Normal.Y, Normal.Z};
	}

	CHECK_CANCLE();

	// ========================== AABBTree ==========================

	for (int VID = 0; VID < Graph.VertexCount(); ++VID)
	{
		const auto& Vertex = Result->Vertices3d[VID].Vertex;
		Result->FullMesh3d.AppendVertex(Vertex);
		Result->FullMesh2d.AppendVertex({ Vertex.X, Vertex.Y, 0.0});
	}
	for (int TID = 0; TID < Result->Triangles.Num(); ++TID)
	{
		Result->FullMesh3d.AppendTriangle(Result->Triangles[TID]);
		Result->FullMesh2d.AppendTriangle(Result->Triangles[TID]);
	}
	CHECK_CANCLE();

	Result->AABBTree3d.SetMesh(&Result->FullMesh3d, true);
	Result->AABBTree2d.SetMesh(&Result->FullMesh2d, true);

	// ========================== Debug ==========================
	// 
	//Result->AddDebugLines((int)EArrangementGID::DrivingSurface, FColor(255, 255, 0, 128), 4.0);
	//Result->AddDebugLines((int)EArrangementGID::SidewalksSoft, FColor(255, 255, 0, 128), 4.0);
	//Result->AddDebugLines((int)EArrangementGID::SidewalksHard, FColor(255, 255, 0, 128), 4.0);

	if (bDrawBoundaries)
	{
		for (auto& It : Result->Boundaries)
		{
			Result->AddDebugLines(It, FColor::Blue, 4.0);
		}
	}


	Result->ResultInfo.SetSuccess();

#undef CHECK_CANCLE

}

#undef LOCTEXT_NAMESPACE
