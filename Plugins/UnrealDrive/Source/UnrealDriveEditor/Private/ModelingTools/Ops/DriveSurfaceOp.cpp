/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "TriangulateRoadOp.h"
#include "DynamicMesh/MeshNormals.h"
#include "Algo/AnyOf.h"
#include "Utils/MeshUtils.h"


#define LOCTEXT_NAMESPACE "DriveSurfaceOp"

using namespace UnrealDrive;

static bool IsTriIsIsland(const TSharedPtr<FRoadBaseOperatorData>& BaseData, int TID)
{
	for (auto& Poly : BaseData->Polygons)
	{
		if (Poly->TrianglesIDs.Find(TID) != INDEX_NONE)
		{
			return false;
		}
	}
	return true;
}


static TArray<int> FindAllIslandTri(const TSharedPtr<FRoadBaseOperatorData>& BaseData)
{
	TArray<int> Ret;
	for (int TID = 0; TID < BaseData->Triangles.Num(); ++TID)
	{
		if (IsTriIsIsland(BaseData, TID))
		{
			Ret.Add(TID);
		}
	}
	return Ret;
}

static bool IsContain(const TArray<FRoadVertexInfo>& Infos, const UScriptStruct* LaneInstance)
{
	auto * Found = Infos.FindByPredicate([LaneInstance](const FRoadVertexInfo& Info) 
	{ 
		auto* ScriptSctruct = Info.Poly->GetLaneInstance().GetScriptStruct();
		return ScriptSctruct && ScriptSctruct->IsChildOf(LaneInstance);
	});
	return Found != nullptr;
}

static bool IsNeighbours(const FIndex3i& TriA, const FIndex3i& TriB)
{
	int NumNeighbours =
		(TriA.Contains(TriB.A) ? 1 : 0) +
		(TriA.Contains(TriB.B) ? 1 : 0) +
		(TriA.Contains(TriB.C) ? 1 : 0);
	return NumNeighbours > 1;
}


static TArray<int> GetFilledsIslandTri(const TSharedPtr<FRoadBaseOperatorData>& BaseData, const UScriptStruct* LaneInstance)
{
	auto& Triangles = BaseData->Triangles;

	TArray<int> IslandTriangles = FindAllIslandTri(BaseData);
	TArray<int> FilledTriangles;

	for (int& TID : IslandTriangles)
	{
		auto& T = BaseData->Triangles[TID];
		int NumTargetEdge =
			(IsContain(BaseData->Vertices3d[T.A].Infos, LaneInstance) ? 1 : 0) +
			(IsContain(BaseData->Vertices3d[T.B].Infos, LaneInstance) ? 1 : 0) +
			(IsContain(BaseData->Vertices3d[T.C].Infos, LaneInstance) ? 1 : 0);

		if (NumTargetEdge > 1)
		{
			FilledTriangles.Add(TID);
			TID = INDEX_NONE;
		}
	}

	bool bWasAdded;
	do
	{
		bWasAdded = false;
		for (int& TID : IslandTriangles)
		{
			if (TID != INDEX_NONE)
			{
				for (auto FilledTID =  FilledTriangles.CreateIterator(); FilledTID; ++FilledTID)
				{
					if (IsNeighbours(Triangles[*FilledTID], Triangles[TID]))
					{
						bWasAdded = true;
						FilledTriangles.Add(TID);
						TID = INDEX_NONE;
						break;
					}
				}
			}
		}
	} while (bWasAdded);

	return FilledTriangles;
}



// --------------------------------------------------------------------------------------------------------------------------------
void FDriveSurfaceOp::CalculateResult(FProgressCancel* Progress)
{
#define CHECK_CANCLE() if (Progress && Progress->Cancelled()) { ResultInfo.Result = EGeometryResultType::Cancelled; return; }

	ResultInfo.Result = EGeometryResultType::InProgress;

	if (!BaseData || BaseData->ResultInfo.HasFailed())
	{
		ResultInfo.SetFailed();
		return;
	}

	bool bSurfaceIsPresent = false;
	for (auto& Poly : BaseData->Polygons)
	{
		if (Poly->GetLaneInstance().GetPtr<FRoadLaneDriving>())
		{
			bSurfaceIsPresent = true;
			break;
		}
	}

	if (!bSurfaceIsPresent)
	{
		ResultInfo.SetSuccess();
		return;
	}

	auto& Graph = BaseData->Arrangement->Graph;
	OpUtils::EnableDefaultAttributes(*ResultMesh, true, true, true, true, 3);

	// ========================== GetFilledsIslandTri ==========================
	TArray<int> FilledsIslandTri = GetFilledsIslandTri(BaseData, FRoadLaneDriving::StaticStruct());
	CHECK_CANCLE();

	// ========================== Copy all vertexes from Graph to DynamicMesh ==========================
	for (int VID = 0; VID < Graph.VertexCount(); ++VID)
	{
		const auto Pt = Graph.GetVertex(VID);
		const auto& Verticex3d = BaseData->Vertices3d[VID];
		//const auto LocalNormal = FVector3f(Info[0].Pos.Quat.GetUpVector());
		int32 NewVID = ResultMesh->AppendVertex(ResultTransform.InverseTransformPosition(Verticex3d.Vertex));
		check(NewVID == VID);
	}

	CHECK_CANCLE();

	

	// ========================== Create sorted LanesPoly by MaterialPriority ==========================
	TArray<TSharedPtr<const FRoadPolygoneBase>> LanesPolySorted;
	for (auto& Poly : BaseData->Polygons)
	{
		if (Poly->GetLaneInstance().GetPtr<FRoadLaneDriving>() && !Poly->IsPolyline())
		{
			LanesPolySorted.Add(Poly);
		}
	}
	LanesPolySorted.Sort([](const TSharedPtr<const FRoadPolygoneBase>& A, const TSharedPtr<const FRoadPolygoneBase>& B)
	{
		return A->GetPriority() > B->GetPriority();
	});

	// ========================== Create VerticesColorAlpha ==========================
	
	TArray<double> VerticesColorAlpha;
	VerticesColorAlpha.SetNum(BaseData->Vertices3d.Num());
	for (auto& It : VerticesColorAlpha)
	{
		It = 0.0; // Set VerticesColorAlpha to 0.0 by default
	}

	if (bComputVertexColor)
	{
		// Set VerticesColorAlpha to 0.5 for all islands
		for (auto& TID : FilledsIslandTri)
		{
			auto& T = BaseData->Triangles[TID];
			VerticesColorAlpha[T.A] = 0.5;
			VerticesColorAlpha[T.B] = 0.5;
			VerticesColorAlpha[T.C] = 0.5;
		}

		for (int VID = 0; VID < BaseData->Vertices3d.Num(); ++VID)
		{
			auto& Verticex3d = BaseData->Vertices3d[VID];

			if (BaseData->IsBoundaryVertex(VID) && Algo::AnyOf(Verticex3d.Infos, [](const FRoadVertexInfo& Info) { return Info.Poly->GetLaneInstance().GetPtr<FRoadLaneDriving>() != nullptr; }))
			{
				// Set  VerticesColorAlpha to 1.0 for boundaries
				VerticesColorAlpha[VID] = 1.0;
			}
			else
			{
				TSet<int> DriveSplineIndexes;
				for (auto& Info : Verticex3d.Infos)
				{
					if (Info.Poly->GetLaneInstance().GetPtr<FRoadLaneDriving>())
					{
						DriveSplineIndexes.Add(Info.Poly->SplineIndex);
					}
				}
				if (DriveSplineIndexes.Num() > 1)
				{
					// Set  VerticesColorAlpha to 0.5 for intersections
					VerticesColorAlpha[VID] = 0.5;
				}
			}
		}

		// Smooth VerticesColorAlpha
		if (VertexColorSmoothRadius > KINDA_SMALL_NUMBER)
		{
			for (int VID = 0; VID < BaseData->Vertices3d.Num(); ++VID)
			{
				double& Aplha1 = VerticesColorAlpha[VID];

				auto DistanceSqFunc = [VID_A = VID, &Graph](int VID_B)
				{
					return DistanceSquared(Graph.GetVertex(VID_A), Graph.GetVertex(VID_B));
				};

				auto Points = BaseData->Arrangement->PointHash.FindAllInRadius(Graph.GetVertex(VID), VertexColorSmoothRadius, DistanceSqFunc);
				for (auto& [NearVID, DistSq] : Points)
				{
					double& Aplha2 = VerticesColorAlpha[NearVID];
					double Alpha = FMath::CubicInterp(Aplha1, 0.0, Aplha2, 0.0, FMath::Sqrt(DistSq) / VertexColorSmoothRadius);
					Aplha2 = FMath::Min(Aplha2, Alpha);
				}

				CHECK_CANCLE();

			}
		}
		CHECK_CANCLE();
	}


	// ========================== Create Mesh ==========================
	//auto* UVLayer0 = ResultMesh->Attributes()->GetUVLayer(0);
	//auto* UVLayer1 = ResultMesh->Attributes()->GetUVLayer(1);
	auto* MaterialID = ResultMesh->Attributes()->GetMaterialID();
	auto* ColorOverlay = ResultMesh->Attributes()->PrimaryColors();

	int MaxMaterialId = 0;
	TMap<FName, int> MaterialIDMap;
	MaterialIDMap.Add(DriveSurfaceIslandMaterial, MaxMaterialId++);

	const FLinearColor DefaultVertexColor_linear = DefaultVertexColor;
	const FLinearColor EdgeVertexColor_linear = EdgeVertexColor;

	const int MAX_SECTIONS = 1024;
	const int MAX_SPLINES = 1024;

	for (auto& Poly : LanesPolySorted)
	{
		for (int TID : Poly->TrianglesIDs)
		{
			if (ResultMesh->IsTriangle(TID) && OpUtils::IsTriangleValid(ResultMesh->GetTriangleRef(TID)))
			{
				// Tringle is already set
				continue;
			}

			auto& T = BaseData->Triangles[TID];
			EMeshResult Res = ResultMesh->InsertTriangle(TID, T);
			check(Res == EMeshResult::Ok);

			auto& LaneDriving = Poly->GetLaneInstance().Get<FRoadLaneDriving>();
			if (int* FoundMaterialId = MaterialIDMap.Find(LaneDriving.MaterialProfile))
			{
				MaterialID->SetValue(TID, *FoundMaterialId);
			}
			else
			{
				MaterialIDMap.Add(LaneDriving.MaterialProfile, MaxMaterialId);
				MaterialID->SetValue(TID, MaxMaterialId);
				++MaxMaterialId;
			}

			Poly->SetUVLayers(*ResultMesh, TID, BaseData->UV0ScaleFactor, BaseData->UV1ScaleFactor, BaseData->UV2ScaleFactor);


			auto ColorA = FLinearColor(FMath::Lerp(DefaultVertexColor_linear, EdgeVertexColor_linear, VerticesColorAlpha[T.A]));
			auto ColorB = FLinearColor(FMath::Lerp(DefaultVertexColor_linear, EdgeVertexColor_linear, VerticesColorAlpha[T.B]));
			auto ColorC = FLinearColor(FMath::Lerp(DefaultVertexColor_linear, EdgeVertexColor_linear, VerticesColorAlpha[T.C]));

			ColorOverlay->SetTriangle(TID, FIndex3i{ 
				ColorOverlay->AppendElement(ColorA),
				ColorOverlay->AppendElement(ColorB),
				ColorOverlay->AppendElement(ColorC),
			});

			if (bSplitBySections)
			{
				int GroupID = Poly->SplineIndex * MAX_SECTIONS;
				if (Poly->GetType() == ERoadPolygoneType::RoadLane)
				{
					auto LanePoly = StaticCastSharedPtr<const FRoadLanePolygone>(Poly);
					GroupID += LanePoly->SectionIndex;
				}
				ResultMesh->SetTriangleGroup(TID, GroupID);
			}
		}

		CHECK_CANCLE();
	}

	// Resolve materials slot name
	ResultMaterialSlots.SetNum(MaxMaterialId);
	for (int i = 0; i < MaxMaterialId; ++i)
	{
		ResultMaterialSlots[i] = *MaterialIDMap.FindKey(i);
	}

	CHECK_CANCLE();

	// ========================== Fill the "islands" triangles ==========================
	
	for (int TID : FilledsIslandTri)
	{
		auto& T = BaseData->Triangles[TID];
		ResultMesh->InsertTriangle(TID, T);

		ColorOverlay->SetTriangle(TID, FIndex3i{
			ColorOverlay->AppendElement(FVector4f(VerticesColorAlpha[T.A], VerticesColorAlpha[T.A], VerticesColorAlpha[T.A], 1.0)),
			ColorOverlay->AppendElement(FVector4f(VerticesColorAlpha[T.B], VerticesColorAlpha[T.B], VerticesColorAlpha[T.B], 1.0)),
			ColorOverlay->AppendElement(FVector4f(VerticesColorAlpha[T.C], VerticesColorAlpha[T.C], VerticesColorAlpha[T.C], 1.0)),
		});
	}
	CHECK_CANCLE();


	// ========================== Compute groups for Islands ==========================
	if (bSplitBySections)
	{
		TArray<int> Sections;
		MeshUtils::FindMeshSections(*ResultMesh, FilledsIslandTri, Sections);
		for (int i = 0; i < FilledsIslandTri.Num(); ++i)
		{
			int TID = FilledsIslandTri[i];
			int GroupID = Sections[i] + MAX_SPLINES * MAX_SECTIONS + 1;
			ResultMesh->SetTriangleGroup(TID, GroupID);
		}
		CHECK_CANCLE();
	}

	// ========================== Split groups by mesh sections ==========================
	if (bSplitBySections)
	{
		MeshUtils::SplitMeshGroupsBySections(*ResultMesh);
		CHECK_CANCLE();
	}

	// ========================== Merge groups by arias ==========================
	if (bSplitBySections && MergeSectionsAreaThreshold > 0)
	{
		// TODO: instead of grouping "by area" make a grouping under the length of the common line
		MeshUtils::MergeGroupByArea(*ResultMesh, MergeSectionsAreaThreshold);
		CHECK_CANCLE();
	}

	// ========================== Compact mesh ==========================
	ResultMesh->CompactInPlace();
	CHECK_CANCLE();

	// ========================== Compute Normals ==========================
	FMeshNormals::QuickComputeVertexNormals(*ResultMesh);
	FMeshNormals::InitializeOverlayToPerVertexNormals(ResultMesh->Attributes()->PrimaryNormals(), true);
	FMeshNormals::QuickRecomputeOverlayNormals(*ResultMesh);

	ResultInfo.SetSuccess();

#undef CHECK_CANCLE
}

#undef LOCTEXT_NAMESPACE
