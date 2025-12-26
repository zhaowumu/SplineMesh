
/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Copyright Epic Games, Inc. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */


#include "RoadMeshTools/RoadMeshOpHelper.h"
#include "Utils/ObjectsCreationHelper.h"
#include "ModelingTools/TriangulateRoadTool.h"
#include "ModelingTools/Ops/TriangulateRoadOp.h"
#include "DynamicMesh/MeshTransforms.h"
#include "ModelingObjectsCreationAPI.h"
#include "ToolSetupUtil.h"
#include "UnrealDrive.h"
#include "DynamicSubmesh3.h"

using namespace UE::Geometry;

namespace
{
	class UMeshOpPreviewWithBackgroundCompute_Private : public UMeshOpPreviewWithBackgroundCompute
	{
	public:
		UE::Geometry::EBackgroundComputeTaskStatus GetLastComputeStatus() const { return this->BackgroundCompute->CheckStatus().TaskStatus; }
		void OnlyInvalidateResult() { bResultValid = false; }
	};

	int32 FillComponentTriIndicesFromTriIDs(const FDynamicMesh3& SourceMesh, TFunctionRef<int32(int32)> TIDtoID, TArray<TArray<int32>>& ComponentTriIndices)
	{
		TMap<int32, int32> ComponentIDMap;
		for (int32 TID : SourceMesh.TriangleIndicesItr())
		{
			int32 CompID = TIDtoID(TID);
			int32* FoundIdx = ComponentIDMap.Find(CompID);
			int32 UseIdx = -1;
			if (FoundIdx)
			{
				UseIdx = *FoundIdx;
			}
			else
			{
				UseIdx = ComponentTriIndices.AddDefaulted();
				ComponentIDMap.Add(CompID, UseIdx);
			}
			ComponentTriIndices[UseIdx].Add(TID);
		}
		//int32 NumComponents = ComponentTriIndices.Num();
		return ComponentTriIndices.Num();
	};

	struct FSplitedComponentResult
	{
		UE::Geometry::FDynamicMesh3 Mesh;
		TArray<UMaterialInterface*> Materials;
		TArray<FName> MaterialSlots;
		FVector3d Origins;
		//int GroupID;
	};

	bool SplitMeshesByGroupID(
		const FDynamicMesh3& SourceMesh, 
		const TArray<TObjectPtr<UMaterialInterface>>& Materials,
		const TArray<FName>& MaterialSlots,
		bool bCenterPivots, 
		TArray<FSplitedComponentResult>& SplitInfo)
	{
		check(Materials.Num() == MaterialSlots.Num());

		TArray<TArray<int32>> ComponentTriIndices;
		int32 NumComponents = FillComponentTriIndicesFromTriIDs(SourceMesh, [SourceMesh](int32 TID) { return SourceMesh.GetTriangleGroup(TID); }, ComponentTriIndices);

		if (NumComponents < 2)
		{
			return false;
		}

		SplitInfo.SetNum(NumComponents);

		for (int32 k = 0; k < NumComponents; ++k)
		{
			FDynamicSubmesh3 SubmeshCalc;

			// if statement should always be true- components should always have been calculated & populated when there's no geometry selection
			if (ensure(!ComponentTriIndices.IsEmpty()))
			{
				SubmeshCalc = FDynamicSubmesh3(&SourceMesh, ComponentTriIndices[k]);
			}
			
			FDynamicMesh3& Submesh = SubmeshCalc.GetSubmesh();
			TArray<UMaterialInterface*> NewMaterials;
			TArray<FName> NewMaterialSlots;

			// remap materials
			FDynamicMeshMaterialAttribute* MaterialIDs = Submesh.HasAttributes() ? Submesh.Attributes()->GetMaterialID() : nullptr;
			if (MaterialIDs)
			{
				TArray<int32> UniqueIDs;
				for (int32 tid : Submesh.TriangleIndicesItr())
				{
					int32 MaterialID = MaterialIDs->GetValue(tid);
					int32 Index = UniqueIDs.IndexOfByKey(MaterialID);
					if (Index == INDEX_NONE)
					{
						int32 NewMaterialID = UniqueIDs.Num();
						UniqueIDs.Add(MaterialID);
						NewMaterials.Add(Materials[MaterialID]);
						NewMaterialSlots.Add(MaterialSlots[MaterialID]);
						MaterialIDs->SetValue(tid, NewMaterialID);
					}
					else
					{
						MaterialIDs->SetValue(tid, Index);
					}
				}
			}

			FVector3d Origin = FVector3d::ZeroVector;
			if (bCenterPivots)
			{
				// reposition mesh
				FAxisAlignedBox3d Bounds = Submesh.GetBounds();
				Origin = Bounds.Center();
				MeshTransforms::Translate(Submesh, -Origin);
			}

			SplitInfo[k].Mesh = MoveTemp(Submesh);
			SplitInfo[k].Materials = MoveTemp(NewMaterials);
			SplitInfo[k].MaterialSlots = MoveTemp(NewMaterialSlots);
			SplitInfo[k].Origins = Origin;
			//SplitInfo[k].GroupID = ;
		}

		return true;
	}
}

UE::Geometry::EBackgroundComputeTaskStatus URoadMeshOpPreviewWithBackgroundCompute::GetLastComputeStatus() const
{
	return static_cast<UMeshOpPreviewWithBackgroundCompute_Private*>(BackgroundCompute)->GetLastComputeStatus();
}

void URoadMeshOpPreviewWithBackgroundCompute::Setup(UTriangulateRoadTool* InRoadTool, TWeakPtr<UnrealDrive::FRoadActorComputeScope> RoadComputeScope, IDynamicMeshOperatorFactory* OpFactory)
{
	RoadTool = InRoadTool;

	//OpFactory->RoadTool = this;
	//OpFactory->ActorData = RoadComputeScope;
	BackgroundCompute = NewObject<UMeshOpPreviewWithBackgroundCompute>(this);
	BackgroundCompute->Setup(RoadTool->GetTargetWorld(), OpFactory);
	BackgroundCompute->PreviewMesh->EnableWireframe(RoadTool->TriangulateProperties->bShowWireframe);
	BackgroundCompute->PreviewMesh->SetTangentsMode(EDynamicMeshComponentTangentsMode::AutoCalculated);
	ToolSetupUtil::ApplyRenderingConfigurationToPreview(BackgroundCompute->PreviewMesh, nullptr);

//#if (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6)
	BackgroundCompute->OnOpCompleted.AddLambda(
//#else
//	BackgroundCompute->OnOpDynamicMeshCompleted.AddLambda(
//#endif
		[this, RoadComputeScope, RoadTool = TWeakObjectPtr<UTriangulateRoadTool>(RoadTool)](const FDynamicMeshOperator* Op)
		{
			if (!Op)
			{
				return;
			}

			auto& ActorData = *RoadComputeScope.Pin().Get();
			ActorData.AppendResultInfo(Op->GetResultInfo());

			if (!Op->GetResultInfo().HasResult())
			{
				return;
			}

			if (MaterialGetter.GetInterface())
			{
				const auto* OpWithMat = static_cast<const UnrealDrive::FDynamicMeshWithMaterialsOperator*>(Op);
				TArray<TObjectPtr<UMaterialInterface>> Materials;
				Materials.SetNum(OpWithMat->ResultMaterialSlots.Num());
				for (int i = 0; i < OpWithMat->ResultMaterialSlots.Num(); ++i)
				{
					FName SlotName = OpWithMat->ResultMaterialSlots[i];
					if (auto* Mat = MaterialGetter.GetInterface()->GetMaterialsMap().Find(SlotName))
					{
						Materials[i] = *Mat;
					}
					else
					{
						Materials[i] = nullptr;
					}
				}
				ResultMaterialSlots = OpWithMat->ResultMaterialSlots;
				BackgroundCompute->ConfigureMaterials(Materials, ToolSetupUtil::GetDefaultWorkingMaterial(RoadTool->GetToolManager()));
			}
		}
	);
	BackgroundCompute->OnMeshUpdated.AddLambda(
		[RoadTool = TWeakObjectPtr<UTriangulateRoadTool>(RoadTool)](const UMeshOpPreviewWithBackgroundCompute* UpdatedPreview)
		{
			RoadTool->GetToolManager()->PostInvalidation();
			RoadTool->NotifyOpWasUpdated();
		}
	);
}

void URoadMeshOpPreviewWithBackgroundCompute::ShutdownAndGenerateAssets(AActor* TargetActor, const FTransform3d& ActorToWorld)
{
	check(RoadTool.IsValid());

	if (!HaveValidNonEmptyResult())
	{
		//UE_LOG(LogUnrealDrive, Error, TEXT(" UTriangulateRoadTool::Shutdown(); Can't generate asset \"%s\" for actor \"%s\", got empty result"), *BaseAssetName, *TargetActor->GetActorLabel());
		Cancel();
		return;
	}

	const FString BaseNameWithPrefix = (RoadTool->TriangulateProperties->ObjectType == ECreateRoadObjectType::StaticMesh ? TEXT("SM_") : TEXT("DM_")) + BaseAssetName;
	auto OpResult = BackgroundCompute->Shutdown();
	if (!OpResult.Mesh.Get())
	{
		UE_LOG(LogUnrealDrive, Error, TEXT(" UTriangulateRoadTool::Shutdown(); Can't generate asset \"%s\" for actor \"%s\""), *BaseNameWithPrefix, *TargetActor->GetActorLabel());
		BackgroundCompute = nullptr;
		return;
	}

	MeshTransforms::ApplyTransform(*OpResult.Mesh, OpResult.Transform, true);
	MeshTransforms::ApplyTransformInverse(*OpResult.Mesh, ActorToWorld, true);

	TArray<FSplitedComponentResult> SplitInfo;
	if (OpResult.Mesh->HasTriangleGroups())
	{
		SplitMeshesByGroupID(*OpResult.Mesh, BackgroundCompute->StandardMaterials, ResultMaterialSlots, true, SplitInfo);
	}
	if (SplitInfo.Num() == 0)
	{
		SplitInfo.Add({ *OpResult.Mesh.Get(), BackgroundCompute->StandardMaterials, ResultMaterialSlots, FVector::ZeroVector});
	}

	for (int i = 0; i < SplitInfo.Num(); ++i)
	{
		const FString BaseNameWithPrefixID = BaseNameWithPrefix + TEXT("_") + FString::FromInt(i);
		FCreateMeshObjectParams NewMeshObjectParams;
		NewMeshObjectParams.TargetWorld = TargetActor->GetWorld();
		NewMeshObjectParams.Transform = FTransform(FRotator::ZeroRotator, SplitInfo[i].Origins);
		NewMeshObjectParams.BaseName = BaseAssetName;
		NewMeshObjectParams.Materials = SplitInfo[i].Materials;
		NewMeshObjectParams.SetMesh(&SplitInfo[i].Mesh);
		NewMeshObjectParams.bEnableCollision = true;
		NewMeshObjectParams.CollisionMode = ECollisionTraceFlag::CTF_UseComplexAsSimple;
		NewMeshObjectParams.TypeHint = RoadTool->TriangulateProperties->ObjectType == ECreateRoadObjectType::StaticMesh ? ECreateObjectTypeHint::StaticMesh : ECreateObjectTypeHint::DynamicMeshActor;

		FCreateMeshObjectResult Res = ObjectsCreationHelper::CreateMeshObject(MoveTemp(NewMeshObjectParams), TargetActor->GetRootComponent(), BaseNameWithPrefixID);
		if (!Res.IsOK())
		{
			UE_LOG(LogUnrealDrive, Error, TEXT(" UTriangulateRoadTool::Shutdown(); Can't generate asset \"%s\" for actor \"%s\" code:\"%i\""), *BaseNameWithPrefixID, *TargetActor->GetActorLabel(), Res.ResultCode);
			continue;
		}

		if (UStaticMesh* StaticMesh = Cast<UStaticMesh>(Res.NewAsset))
		{
			auto& StaticMaterials = StaticMesh->GetStaticMaterials();
			check(StaticMaterials.Num() == SplitInfo[i].MaterialSlots.Num());
			for (int j = 0; j < StaticMaterials.Num(); ++j)
			{
				StaticMaterials[j].MaterialSlotName = SplitInfo[i].MaterialSlots[j];
			}
		}
	}

	BackgroundCompute = nullptr;

}

int URoadMeshOpPreviewWithBackgroundCompute::GetNumVertices() const
{
	if (BackgroundCompute->HaveValidNonEmptyResult())
	{
		auto* DynamicMesh = BackgroundCompute->PreviewMesh->GetMesh();
		return DynamicMesh->VertexCount();
	}
	return 0;
}

int URoadMeshOpPreviewWithBackgroundCompute::GetNumTriangles() const
{
	if (BackgroundCompute->HaveValidNonEmptyResult())
	{
		auto* DynamicMesh = BackgroundCompute->PreviewMesh->GetMesh();
		return DynamicMesh->TriangleCount();
	}
	return 0;
}