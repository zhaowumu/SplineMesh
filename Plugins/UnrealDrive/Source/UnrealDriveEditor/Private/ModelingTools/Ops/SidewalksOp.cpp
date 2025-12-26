/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "TriangulateRoadOp.h"
//#include "Operations/ExtrudeMesh.h"
//#include "Operations/OffsetMeshRegion.h"
#include "DynamicMesh/MeshNormals.h"
#include "Utils/MeshUtils.h"

#define LOCTEXT_NAMESPACE "SidewalksOp"

using namespace UnrealDrive;

void FSidewalksOp::CalculateResult(FProgressCancel* Progress)
{

#define CHECK_CANCLE() if (Progress && Progress->Cancelled()) { ResultInfo.Result = EGeometryResultType::Cancelled; return; }

	ResultInfo.Result = EGeometryResultType::InProgress;

	if (!BaseData || BaseData->ResultInfo.HasFailed())
	{
		ResultInfo.SetFailed();
		return;
	}

	/*
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
	*/

	auto& Graph = BaseData->Arrangement->Graph;
	OpUtils::EnableDefaultAttributes(*ResultMesh, true, true, true, true, 3);


	// ========================== Copy all vertexes from Graph to DynamicMesh ==========================
	for (int VID = 0; VID < Graph.VertexCount(); ++VID)
	{
		const auto Pt = Graph.GetVertex(VID);
		const auto& Verticex3d = BaseData->Vertices3d[VID];
		//const auto LocalNormal = FVector3f(Info[0].Pos.Quat.GetUpVector());
		int32 NewVID = ResultMesh->AppendVertex(ResultTransform.InverseTransformPosition(Verticex3d.Vertex + Verticex3d.Normal * SidewalkHeight));
		check(NewVID == VID);
	}

	CHECK_CANCLE();

	// ========================== Create Mesh ==========================

	// Create sorted LanesPoly by MaterialPriority
	TArray<TSharedPtr<const FRoadPolygoneBase>> LanesPolySorted;
	for (auto& Poly : BaseData->Polygons)
	{
		if (Poly->GetLaneInstance().GetPtr<FRoadLaneSidewalk>() && !Poly->IsPolyline())
		{
			LanesPolySorted.Add(Poly);
		}
	}
	LanesPolySorted.Sort([](const TSharedPtr<const FRoadPolygoneBase>& A, const TSharedPtr<const FRoadPolygoneBase>& B)
	{
		return A->GetPriority() > B->GetPriority();
	});

	auto* MaterialID = ResultMesh->Attributes()->GetMaterialID();
	auto* ColorOverlay = ResultMesh->Attributes()->PrimaryColors();

	int MaxMaterialId = 0;
	TMap<FName, int> MaterialIDMap;

	for (auto& Poly : LanesPolySorted)
	{
		for (auto& TID : Poly->TrianglesIDs)
		{
			if (ResultMesh->IsTriangle(TID) && OpUtils::IsTriangleValid(ResultMesh->GetTriangleRef(TID)))
			{
				continue;
			}

			auto& T = BaseData->Triangles[TID];
			EMeshResult Res = ResultMesh->InsertTriangle(TID, T);
			check(Res == EMeshResult::Ok);

			auto& LaneSidewalk = Poly->GetLaneInstance().Get<FRoadLaneSidewalk>();
			if (int* FoundMaterialId = MaterialIDMap.Find(LaneSidewalk.MaterialProfile))
			{
				MaterialID->SetValue(TID, *FoundMaterialId);
			}
			else
			{
				MaterialIDMap.Add(LaneSidewalk.MaterialProfile, MaxMaterialId);
				MaterialID->SetValue(TID, MaxMaterialId);
				++MaxMaterialId;
			}

			Poly->SetUVLayers(*ResultMesh, TID, BaseData->UV0ScaleFactor, BaseData->UV1ScaleFactor, BaseData->UV2ScaleFactor);
				
			ColorOverlay->SetTriangle(TID, FIndex3i{ 
				ColorOverlay->AppendElement(FVector4f(1.0, 1.0, 1.0, 1.0)),
				ColorOverlay->AppendElement(FVector4f(1.0, 1.0, 1.0, 1.0)),
				ColorOverlay->AppendElement(FVector4f(1.0, 1.0, 1.0, 1.0)) 
			});

			if (bSplitBySections && BaseData->RoadSplinesCache.Num() == 1 && Poly->GetType() == ERoadPolygoneType::RoadLane)
			{
				auto LanePoly = StaticCastSharedPtr<const FRoadLanePolygone>(Poly);
				ResultMesh->SetTriangleGroup(TID, LanePoly->SectionIndex);
			}
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

	// ========================== Resolve materials slot name ==========================
	ResultMaterialSlots.SetNum(MaxMaterialId);
	for (int i = 0; i < MaxMaterialId; ++i)
	{
		ResultMaterialSlots[i] = *MaterialIDMap.FindKey(i);
	}
	CHECK_CANCLE();

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
