/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "CoreMinimal.h"
#include "ModelingObjectsCreationAPI.h"


namespace ObjectsCreationHelper
{
	UNREALDRIVEEDITOR_API FString GenerateValidComponentName(const FString& DesierName, AActor* ComponentOwner);

	UNREALDRIVEEDITOR_API FCreateMeshObjectResult CreateMeshObject(FCreateMeshObjectParams&& CreateMeshParams, USceneComponent* Parent, const FString& DesierComponentName);

	UNREALDRIVEEDITOR_API FCreateMeshObjectResult CreateStaticMeshAsset(FCreateMeshObjectParams&& CreateMeshParams, USceneComponent* Parent, const FString& DesierComponentName);
	UNREALDRIVEEDITOR_API FCreateMeshObjectResult CreateDynamicMeshActor(FCreateMeshObjectParams&& CreateMeshParams, USceneComponent* Parent, const FString& DesierComponentName);

	UNREALDRIVEEDITOR_API ECreateModelingObjectResult GetNewAssetPath(FString& OutNewAssetPath, const FString& BaseName, const UObject* StoreRelativeToObject, const UWorld* World);
};
