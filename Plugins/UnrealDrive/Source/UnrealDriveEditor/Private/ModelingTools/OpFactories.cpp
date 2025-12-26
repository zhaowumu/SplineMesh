/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "ModelingTools/OpFactories.h"
#include "ModelingTools/Ops/TriangulateRoadOp.h"
#include "UnrealDrivePreset.h"

using namespace UnrealDrive;

class FDynamicMeshOperatorDummy : public FDynamicMeshOperator
{
	virtual void CalculateResult(FProgressCancel* Progress) override
	{
		SetResultInfo(FGeometryResult::Cancelled());
	}
};

class FSplineMeshOperatorDummy : public FSplineMeshOperator
{
	virtual void CalculateResult(FProgressCancel* Progress) override
	{
		SetResultInfo(FGeometryResult::Cancelled());
	}
};

TMap<FName, TObjectPtr<UMaterialInterface>> URoadSurfaceToolProperties::GetMaterialsMap() const
{
	return MakeMaterialsMap(UUnrealDrivePresetBase::GetAllProfiles(&UUnrealDrivePresetBase::DriveableMaterialProfiles), Materials);
}

TMap<FName, TObjectPtr<UMaterialInterface>> URoadDecalToolProperties::GetMaterialsMap() const
{
	auto DriveableProfiles = UUnrealDrivePresetBase::GetAllProfiles(&UUnrealDrivePresetBase::DriveableMaterialProfiles);

	TMap<FName, TObjectPtr<UMaterialInterface>> Ret;
	for (auto& Profile : DriveableProfiles)
	{
		if (auto* OverrideMaterial = Materials.Find(Profile.Key))
		{
			Ret.Add(Profile.Key, *OverrideMaterial);
		}
		else
		{
			Ret.Add(Profile.Key, Profile.Value.DecaltMaterial);
		}
	}
	return Ret;
}

TMap<FName, TObjectPtr<UMaterialInterface>> URoadSidewalkToolProperties::GetMaterialsMap() const
{
	return MakeMaterialsMap(UUnrealDrivePresetBase::GetAllProfiles(&UUnrealDrivePresetBase::SidewalkMaterialProfiles), Materials);
}

TMap<FName, TObjectPtr<UMaterialInterface>> URoadCertbToolProperties::GetMaterialsMap() const
{
	return MakeMaterialsMap(UUnrealDrivePresetBase::GetAllProfiles(&UUnrealDrivePresetBase::CurbProfiles), Materials);
}

TMap<FName, TObjectPtr<UMaterialInterface>> URoadMarkToolProperties::GetMaterialsMap() const
{
	return MakeMaterialsMap(UUnrealDrivePresetBase::GetAllProfiles(&UUnrealDrivePresetBase::LaneMarkMaterialProfiles), Materials);
}

TUniquePtr<UE::Geometry::TGenericDataOperator<UnrealDrive::FRoadBaseOperatorData>> FRoadBaseOperatorFactory::MakeNewOperator()
{
	TUniquePtr<UnrealDrive::FRoadBaseOperator> Op = MakeUnique<UnrealDrive::FRoadBaseOperator>();

	const auto& TriangulateProperties = RoadTool->TriangulateProperties;
	Op->OverlapStrategy = TriangulateProperties->OverlapStrategy;
	Op->OverlapRadius = TriangulateProperties->OverlapRadius;
	Op->MaxSquareDistanceFromSpline = TriangulateProperties->ErrorTolerance * TriangulateProperties->ErrorTolerance;
	Op->MaxSquareDistanceFromCap = TriangulateProperties->SidewalkCapErrorTolerance * TriangulateProperties->SidewalkCapErrorTolerance;
	Op->MinSegmentLength = TriangulateProperties->MinSegmentLength;
	Op->VertexSnapTol = TriangulateProperties->VertexSnapTol;
	Op->UV0ScaleFactor = TriangulateProperties->UV0VScale;
	Op->UV1ScaleFactor = TriangulateProperties->UV1VScale;
	Op->UV2ScaleFactor = TriangulateProperties->UV2VScale;
	Op->bSmooth = TriangulateProperties->bSmooth;
	Op->SmoothSpeed = TriangulateProperties->SmoothSpeed;
	Op->Smoothness = TriangulateProperties->Smoothness;
	Op->bDrawBoundaries = TriangulateProperties->bDrawBoundaries;
	Op->SetActorWithRoads(RoadComputeScope.Pin()->TargetActor.Get());
	return Op;
}

TUniquePtr<UE::Geometry::FDynamicMeshOperator> FDriveSurfaceOperatorFactory::MakeNewOperator()
{
	check(RoadTool.IsValid());
	check(Properties.IsValid());

	if (!Properties->bBuild)
	{
		return MakeUnique<FDynamicMeshOperatorDummy>();
	}

	TUniquePtr<UnrealDrive::FDriveSurfaceOp> Op = MakeUnique<UnrealDrive::FDriveSurfaceOp>();
	const auto& TriangulateProperties = RoadTool->TriangulateProperties;
	Op->BaseData = RoadComputeScope.Pin()->BaseData;
	Op->DriveSurfaceIslandMaterial = Properties->DriveSurfaceIslandMaterial;
	Op->bComputVertexColor = Properties->bComputVertexColor;
	Op->VertexColorSmoothRadius = Properties->VertexColorSmoothRadius;
	Op->DefaultVertexColor = Properties->DefaultVertexColor;
	Op->EdgeVertexColor = Properties->EdgeVertexColor;
	Op->bSplitBySections = TriangulateProperties->bSplitBySections;
	Op->MergeSectionsAreaThreshold = TriangulateProperties->MergeSectionsAreaThreshold * 100 * 100;
	return Op;
}

TUniquePtr<UE::Geometry::FDynamicMeshOperator> FRoadDecalsOperatorFactory::MakeNewOperator()
{
	check(RoadTool.IsValid());
	check(Properties.IsValid());

	if (!Properties->bBuild)
	{
		return MakeUnique<FDynamicMeshOperatorDummy>();
	}

	TUniquePtr<UnrealDrive::FDecalsOp> Op = MakeUnique<UnrealDrive::FDecalsOp>();
	const auto& TriangulateProperties = RoadTool->TriangulateProperties;
	Op->BaseData = RoadComputeScope.Pin()->BaseData;
	Op->DecalOffset = Properties->DecalOffset;
	Op->bSplitBySections = TriangulateProperties->bSplitBySections;
	Op->MergeSectionsAreaThreshold = TriangulateProperties->MergeSectionsAreaThreshold * 100 * 100;
	return Op;
}

TUniquePtr<UE::Geometry::FDynamicMeshOperator> FRoadSidewalksOperatorFactory::MakeNewOperator()
{
	check(RoadTool.IsValid());
	check(Properties.IsValid());

	if (!Properties->bBuild)
	{
		return MakeUnique<FDynamicMeshOperatorDummy>();
	}

	TUniquePtr<UnrealDrive::FSidewalksOp> Op = MakeUnique<UnrealDrive::FSidewalksOp>();
	const auto& TriangulateProperties = RoadTool->TriangulateProperties;
	Op->BaseData = RoadComputeScope.Pin()->BaseData;
	Op->SidewalkHeight = Properties->SidewalkHeight;
	Op->bSplitBySections = TriangulateProperties->bSplitBySections;
	Op->MergeSectionsAreaThreshold = TriangulateProperties->MergeSectionsAreaThreshold * 100 * 100;
	return Op;
}

TUniquePtr<UE::Geometry::FDynamicMeshOperator> FRoadCurbsOperatorFactory::MakeNewOperator()
{
	check(RoadTool.IsValid());
	check(Properties.IsValid());

	if (!Properties->bBuild)
	{
		return MakeUnique<FDynamicMeshOperatorDummy>();
	}

	TUniquePtr<UnrealDrive::FCurbsOp> Op = MakeUnique<UnrealDrive::FCurbsOp>();
	const auto& TriangulateProperties = RoadTool->TriangulateProperties;
	Op->BaseData = RoadComputeScope.Pin()->BaseData;
	Op->CurbsHeight = Properties->CurbsHeight;
	Op->UV0Scale = Properties->CurbsUV0Scale;
	return Op;
}

TUniquePtr<UE::Geometry::FDynamicMeshOperator> FRoadMarksOperatorFactory::MakeNewOperator()
{
	check(RoadTool.IsValid());
	check(Properties.IsValid());

	if (!Properties->bBuild)
	{
		return MakeUnique<FDynamicMeshOperatorDummy>();
	}

	TUniquePtr<UnrealDrive::FMarksOp> Op = MakeUnique<UnrealDrive::FMarksOp>();
	const auto& TriangulateProperties = RoadTool->TriangulateProperties;
	Op->BaseData = RoadComputeScope.Pin()->BaseData;
	Op->MarkOffset = Properties->MarkOffset;
	return Op;
}

TUniquePtr<UnrealDrive::FSplineMeshOperator> FRoadSplineMeshOperatorFactory::MakeNewOperator()
{
	check(RoadTool.IsValid());
	check(Properties.IsValid());

	if (!Properties->bBuild)
	{
		return MakeUnique<FSplineMeshOperatorDummy>();
	}

	TUniquePtr<UnrealDrive::FSplineMeshOp> Op = MakeUnique<UnrealDrive::FSplineMeshOp>();
	//const auto& TriangulateProperties = RoadTool->TriangulateProperties;
	Op->BaseData = RoadComputeScope.Pin()->BaseData;
	Op->bDrawRefSplines = Properties->bDrawRefSplines;
	return Op;

}
