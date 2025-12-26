/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "TriangulateRoadOp.h"
#include "DynamicMesh/MeshNormals.h"
#include "UnrealDrivePresetBase.h"

#define LOCTEXT_NAMESPACE "DecalsOp"

using namespace UnrealDrive;


void FDecalsOp::CalculateResult(FProgressCancel* Progress)
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
	OpUtils::EnableDefaultAttributes(*ResultMesh, true, true, true, true, 2);

	auto DriveableMaterialProfiles = UUnrealDrivePresetBase::GetAllProfiles(&UUnrealDrivePresetBase::DriveableMaterialProfiles);


	// ========================== Create Mesh ==========================

	int MaxMaterialId = 0;
	TMap<FName, int> MaterialIDMap;

	for (auto& Poly : BaseData->Polygons)
	{
		const auto* LaneDriving = Poly->GetLaneInstance().GetPtr<FRoadLaneDriving>();
		if (!LaneDriving)
		{
			continue;
		}

		FName MaterialProfile{};

		if (!LaneDriving->MaterialProfile.IsNone())
		{
			if (auto* Profile = DriveableMaterialProfiles.Find(LaneDriving->MaterialProfile))
			{
				if (IsValid(Profile->DecaltMaterial))
				{
					MaterialProfile = LaneDriving->MaterialProfile;
				}
			}
		}

		//if (!LaneDriving->DecalMaterialProfile.IsNone())
		//{
		//	MaterialProfile = LaneDriving->DecalMaterialProfile;
		//}

		if (MaterialProfile.IsNone())
		{
			continue;
		}

		FDynamicMesh3 DynamicMesh;
		OpUtils::EnableDefaultAttributes(DynamicMesh, true, true, true, true, 2);

		// Copy all vertexes from Graph to DynamicMesh 
		for (int VID = 0; VID < Graph.VertexCount(); ++VID)
		{
			const auto Pt = Graph.GetVertex(VID);
			const auto& Verticex3d = BaseData->Vertices3d[VID];
			int32 NewVID = DynamicMesh.AppendVertex(ResultTransform.InverseTransformPosition(Verticex3d.Vertex + Verticex3d.Normal * DecalOffset));
			check(NewVID == VID);
		}

		CHECK_CANCLE();

		auto* MaterialID = DynamicMesh.Attributes()->GetMaterialID();
		auto* ColorOverlay = DynamicMesh.Attributes()->PrimaryColors();

		// Creat triangles
		for (auto& TID : Poly->TrianglesIDs)
		{
			auto& T = BaseData->Triangles[TID];
			EMeshResult Res = DynamicMesh.InsertTriangle(TID, T);
			check(Res == EMeshResult::Ok);

			if (int* FoundMaterialId = MaterialIDMap.Find(MaterialProfile))
			{
				MaterialID->SetValue(TID, *FoundMaterialId);
			}
			else
			{
				MaterialIDMap.Add(MaterialProfile, MaxMaterialId);
				MaterialID->SetValue(TID, MaxMaterialId);
				++MaxMaterialId;
			}

			Poly->SetUVLayers(DynamicMesh, TID, BaseData->UV0ScaleFactor, BaseData->UV1ScaleFactor, BaseData->UV2ScaleFactor);

			ColorOverlay->SetTriangle(TID, FIndex3i{
				ColorOverlay->AppendElement(FVector4f(1.0, 1.0, 1.0, 1.0)),
				ColorOverlay->AppendElement(FVector4f(1.0, 1.0, 1.0, 1.0)),
				ColorOverlay->AppendElement(FVector4f(1.0, 1.0, 1.0, 1.0)) 
			});

			if (bSplitBySections && BaseData->RoadSplinesCache.Num() == 1 && Poly->GetType() == ERoadPolygoneType::RoadLane)
			{
				auto LanePoly = StaticCastSharedPtr<const FRoadLanePolygone>(Poly);
				DynamicMesh.SetTriangleGroup(TID, LanePoly->SectionIndex);
			}
		}


		CHECK_CANCLE()

		// Compact mesh 
		DynamicMesh.CompactInPlace();

		CHECK_CANCLE()

		// Compute Normals
		FMeshNormals::QuickComputeVertexNormals(DynamicMesh);
		FMeshNormals::InitializeOverlayToPerVertexNormals(DynamicMesh.Attributes()->PrimaryNormals(), true);
		FMeshNormals::QuickRecomputeOverlayNormals(DynamicMesh);

		CHECK_CANCLE();

		// Append DynamicMesh to ResultMesh
		OpUtils::AppendMesh(*ResultMesh, DynamicMesh);

		
	}

	// Resolve materials slot name
	ResultMaterialSlots.SetNum(MaxMaterialId);
	for (int i = 0; i < MaxMaterialId; ++i)
	{
		ResultMaterialSlots[i] = *MaterialIDMap.FindKey(i);
	}

	ResultInfo.SetSuccess();

#undef CHECK_CANCLE

}


#undef LOCTEXT_NAMESPACE
