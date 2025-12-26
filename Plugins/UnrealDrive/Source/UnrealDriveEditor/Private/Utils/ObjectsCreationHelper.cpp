/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "Utils/ObjectsCreationHelper.h"
#include "InteractiveToolsContext.h"
#include "InteractiveToolManager.h"
#include "ContextObjectStore.h"

#include "AssetUtils/CreateStaticMeshUtil.h"
#include "Physics/ComponentCollisionUtil.h"

#include "ConversionUtils/DynamicMeshToVolume.h"
#include "MeshDescriptionToDynamicMesh.h"

#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "MaterialDomain.h"
#include "Materials/Material.h"

#include "ToolTargets/VolumeComponentToolTarget.h"  // for CVarModelingMaxVolumeTriangleCount
#include "Engine/BlockingVolume.h"
#include "Components/BrushComponent.h"
#include "Engine/Polys.h"
#include "Model.h"
//#include "BSPOps.h"		// in UnrealEd
#include "Editor/EditorEngine.h"		// for FActorLabelUtilities

#include "DynamicMeshActor.h"
#include "Components/DynamicMeshComponent.h"

#include "ActorFactories/ActorFactory.h"
#include "AssetSelection.h"
#include "ModelingModeAssetUtils.h"
#include "Kismet2/ComponentEditorUtils.h"

//extern UNREALED_API UEditorEngine* GEditor;

FString ObjectsCreationHelper::GenerateValidComponentName(const FString& DesierName, AActor* ComponentOwner)
{
	check(ComponentOwner);

	FString ComponentTypeName = DesierName;

	// Strip off 'Component' if the class ends with that.  It just looks better in the UI.
	FString SuffixToStrip(TEXT("Component"));
	if (ComponentTypeName.EndsWith(SuffixToStrip))
	{
		ComponentTypeName.LeftInline(ComponentTypeName.Len() - SuffixToStrip.Len(), EAllowShrinking::No);
	}

	// Strip off 'Actor' if the class ends with that so as not to confuse actors with components
	SuffixToStrip = TEXT("Actor");
	if (ComponentTypeName.EndsWith(SuffixToStrip))
	{
		ComponentTypeName.LeftInline(ComponentTypeName.Len() - SuffixToStrip.Len(), EAllowShrinking::No);
	}

	// Try to create a name without any numerical suffix first
	int32 Counter = 1;
	FString ComponentInstanceName = ComponentTypeName;
	while (!FComponentEditorUtils::IsComponentNameAvailable(ComponentInstanceName, ComponentOwner))
	{
		// Assign the lowest possible numerical suffix
		ComponentInstanceName = FString::Printf(TEXT("%s%d"), *ComponentTypeName, Counter++);
	}

	return ComponentInstanceName;
}


FCreateMeshObjectResult ObjectsCreationHelper::CreateMeshObject(FCreateMeshObjectParams&& CreateMeshParams, USceneComponent* Parent, const FString& DesierComponentName)
{
	FCreateMeshObjectResult ResultOut;

	if (CreateMeshParams.TypeHint == ECreateObjectTypeHint::DynamicMeshActor)
	{
		ResultOut = CreateDynamicMeshActor(MoveTemp(CreateMeshParams), Parent, DesierComponentName);
	}
	else if (CreateMeshParams.TypeHint == ECreateObjectTypeHint::StaticMesh)
	{
		ResultOut = CreateStaticMeshAsset(MoveTemp(CreateMeshParams), Parent, DesierComponentName);
	}
	else
	{
		ResultOut.ResultCode = ECreateModelingObjectResult::Failed_Unknown;
	}
	return ResultOut;
}

FCreateMeshObjectResult ObjectsCreationHelper::CreateDynamicMeshActor(FCreateMeshObjectParams&& CreateMeshParams, USceneComponent* Parent, const FString& DesierComponentName)
{
	check(Parent);

	AActor* Actor = Parent->GetOwner();
	check(Actor);

	UDynamicMeshComponent* NewComponent = NewObject<UDynamicMeshComponent>(Parent, *GenerateValidComponentName(DesierComponentName, Actor), RF_Transactional);
	NewComponent->SetupAttachment(Parent);
	NewComponent->OnComponentCreated();
	Actor->AddInstanceComponent(NewComponent);
	NewComponent->RegisterComponent();
	NewComponent->ResetRelativeTransform();
	NewComponent->SetMobility(EComponentMobility::Static);


	// assume that DynamicMeshComponent always has tangents on it's internal UDynamicMesh
	NewComponent->SetTangentsType(EDynamicMeshComponentTangentsMode::ExternallyProvided);

	if (CreateMeshParams.MeshType == ECreateMeshObjectSourceMeshType::DynamicMesh)
	{
		FDynamicMesh3 SetMesh = MoveTemp(CreateMeshParams.DynamicMesh.GetValue());
		if (SetMesh.IsCompact() == false)
		{
			SetMesh.CompactInPlace();
		}
		NewComponent->SetMesh(MoveTemp(SetMesh));
		NewComponent->NotifyMeshUpdated();
	}
	else if (CreateMeshParams.MeshType == ECreateMeshObjectSourceMeshType::MeshDescription)
	{
		const FMeshDescription* MeshDescription = &CreateMeshParams.MeshDescription.GetValue();
		FDynamicMesh3 Mesh(UE::Geometry::EMeshComponents::FaceGroups);
		Mesh.EnableAttributes();
		FMeshDescriptionToDynamicMesh Converter;
		Converter.Convert(MeshDescription, Mesh, true);
		NewComponent->SetMesh(MoveTemp(Mesh));
	}
	else
	{
		return FCreateMeshObjectResult{ ECreateModelingObjectResult::Failed_InvalidMesh };
	}

	//NewActor->SetActorTransform(CreateMeshParams.Transform);
	//FActorLabelUtilities::SetActorLabelUnique(NewActor, CreateMeshParams.BaseName);

	// set materials
	TArray<UMaterialInterface*> ComponentMaterials = CreateMeshParams.Materials;
	for (int32 k = 0; k < ComponentMaterials.Num(); ++k)
	{
		NewComponent->SetMaterial(k, ComponentMaterials[k]);
	}

	// configure collision
	if (CreateMeshParams.bEnableCollision)
	{
		if (CreateMeshParams.CollisionShapeSet.IsSet())
		{
			UE::Geometry::SetSimpleCollision(NewComponent, CreateMeshParams.CollisionShapeSet.GetPtrOrNull());
		}

		NewComponent->CollisionType = CreateMeshParams.CollisionMode;
		// enable complex collision so that raycasts can hit this object
		NewComponent->bEnableComplexCollision = true;

		// force collision update
		NewComponent->UpdateCollision(false);
	}

	// configure raytracing
	NewComponent->SetEnableRaytracing(CreateMeshParams.bEnableRaytracingSupport);

	Actor->PostEditChange();

	// emit result
	FCreateMeshObjectResult ResultOut;
	ResultOut.ResultCode = ECreateModelingObjectResult::Ok;
	ResultOut.NewActor = Actor;
	ResultOut.NewComponent = NewComponent;
	ResultOut.NewAsset = nullptr;
	return ResultOut;
}

FCreateMeshObjectResult ObjectsCreationHelper::CreateStaticMeshAsset(FCreateMeshObjectParams&& CreateMeshParams, USceneComponent* Parent, const FString& DesierComponentName)
{

	UE::AssetUtils::FStaticMeshAssetOptions AssetOptions;
	
	ECreateModelingObjectResult AssetPathResult = GetNewAssetPath(
		AssetOptions.NewAssetPath,
		CreateMeshParams.BaseName,
		nullptr,
		CreateMeshParams.TargetWorld);
		
	if (AssetPathResult != ECreateModelingObjectResult::Ok)
	{
		return FCreateMeshObjectResult{ AssetPathResult };
	}

	AssetOptions.NumSourceModels = 1;
	AssetOptions.NumMaterialSlots = CreateMeshParams.Materials.Num();
	AssetOptions.AssetMaterials = (CreateMeshParams.AssetMaterials.Num() == AssetOptions.NumMaterialSlots) ? CreateMeshParams.AssetMaterials : CreateMeshParams.Materials;

	AssetOptions.bEnableRecomputeNormals = CreateMeshParams.bEnableRecomputeNormals;
	AssetOptions.bEnableRecomputeTangents = CreateMeshParams.bEnableRecomputeTangents;
	AssetOptions.bGenerateNaniteEnabledMesh = CreateMeshParams.bEnableNanite;
	AssetOptions.NaniteSettings = CreateMeshParams.NaniteSettings;
	AssetOptions.bGenerateLightmapUVs = CreateMeshParams.bGenerateLightmapUVs;

	AssetOptions.bCreatePhysicsBody = CreateMeshParams.bEnableCollision;
	AssetOptions.CollisionType = CreateMeshParams.CollisionMode;

	if (CreateMeshParams.MeshType == ECreateMeshObjectSourceMeshType::DynamicMesh)
	{
		FDynamicMesh3* DynamicMesh = &CreateMeshParams.DynamicMesh.GetValue();
		AssetOptions.SourceMeshes.DynamicMeshes.Add(DynamicMesh);
	}
	else if (CreateMeshParams.MeshType == ECreateMeshObjectSourceMeshType::MeshDescription)
	{
		FMeshDescription* MeshDescription = &CreateMeshParams.MeshDescription.GetValue();
		AssetOptions.SourceMeshes.MoveMeshDescriptions.Add(MeshDescription);
	}
	else
	{
		return FCreateMeshObjectResult{ ECreateModelingObjectResult::Failed_InvalidMesh };
	}

	UE::AssetUtils::FStaticMeshResults ResultData;
	UE::AssetUtils::ECreateStaticMeshResult AssetResult = UE::AssetUtils::CreateStaticMeshAsset(AssetOptions, ResultData);

	if (AssetResult != UE::AssetUtils::ECreateStaticMeshResult::Ok)
	{
		return FCreateMeshObjectResult{ ECreateModelingObjectResult::Failed_AssetCreationFailed };
	}

	UStaticMesh* NewStaticMesh = ResultData.StaticMesh;


	check(Parent);
	AActor* Actor = Parent->GetOwner();
	check(Actor);

	UStaticMeshComponent* NewComponent = NewObject<UStaticMeshComponent>(Parent, *GenerateValidComponentName(DesierComponentName, Actor), RF_Transactional);
	NewComponent->SetupAttachment(Parent);
	NewComponent->OnComponentCreated();
	Actor->AddInstanceComponent(NewComponent);
	NewComponent->RegisterComponent();
	NewComponent->ResetRelativeTransform();
	NewComponent->SetMobility(EComponentMobility::Static);

	//return FCreateMeshObjectResult{ECreateModelingObjectResult::Failed_ActorCreationFailed};
	

	// this disconnects the component from various events
	NewComponent->UnregisterComponent();
	// replace the UStaticMesh in the component
	NewComponent->SetStaticMesh(NewStaticMesh);

	// set materials
	TArray<UMaterialInterface*> ComponentMaterials = CreateMeshParams.Materials;
	for (int32 k = 0; k < ComponentMaterials.Num(); ++k)
	{
		NewComponent->SetMaterial(k, ComponentMaterials[k]);
	}

	// set simple collision geometry
	if (CreateMeshParams.CollisionShapeSet.IsSet())
	{
		UE::Geometry::SetSimpleCollision(NewComponent, CreateMeshParams.CollisionShapeSet.GetPtrOrNull(),
			UE::Geometry::GetCollisionSettings(NewComponent));
	}

	// re-connect the component (?)
	NewComponent->RegisterComponent();

	NewStaticMesh->PostEditChange();

	NewComponent->RecreatePhysicsState();
	NewComponent->SetRelativeTransform(CreateMeshParams.Transform);

	// update transform
	//StaticMeshActor->SetActorTransform(CreateMeshParams.Transform);

	// emit result
	FCreateMeshObjectResult ResultOut;
	ResultOut.ResultCode = ECreateModelingObjectResult::Ok;
	ResultOut.NewActor = Actor;
	ResultOut.NewComponent = NewComponent;
	ResultOut.NewAsset = NewStaticMesh;
	return ResultOut;
}

ECreateModelingObjectResult ObjectsCreationHelper::GetNewAssetPath(FString& OutNewAssetPath, const FString& BaseName, const UObject* StoreRelativeToObject, const UWorld* TargetWorld)
{
	FString RelativeToObjectFolder;
	if (StoreRelativeToObject != nullptr)
	{
		// find path to asset
		UPackage* AssetOuterPackage = CastChecked<UPackage>(StoreRelativeToObject->GetOuter());
		if (ensure(AssetOuterPackage))
		{
			FString AssetPackageName = AssetOuterPackage->GetName();
			RelativeToObjectFolder = FPackageName::GetLongPackagePath(AssetPackageName);
		}
	}
	else
	{
		if (!ensure(TargetWorld)) {
			return ECreateModelingObjectResult::Failed_InvalidWorld;
		}
	}

	if (/*GetNewAssetPathNameCallback.IsBound()*/ true)
	{
		OutNewAssetPath = UE::Modeling::GetNewAssetPathName(BaseName, TargetWorld, RelativeToObjectFolder);

		//OutNewAssetPath = GetNewAssetPathNameCallback.Execute(BaseName, TargetWorld, RelativeToObjectFolder);
		if (OutNewAssetPath.Len() == 0)
		{
			return ECreateModelingObjectResult::Cancelled;
		}
	}
	else
	{
		FString UseBaseFolder = (RelativeToObjectFolder.Len() > 0) ? RelativeToObjectFolder : TEXT("/Game");
		OutNewAssetPath = FPaths::Combine(UseBaseFolder, BaseName);
	}

	return ECreateModelingObjectResult::Ok;
}

