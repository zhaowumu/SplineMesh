
/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Copyright Epic Games, Inc. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "BackgroundModelingComputeSource.h"
#include "MeshOpPreviewHelpers.h"
#include "IRoadOpCompute.h"
#include "RoadMeshOpHelper.generated.h"

class UTriangulateRoadTool;

namespace UnrealDrive
{
	struct FRoadActorComputeScope;
}

class IInteractiveToolPropertyMaterialInterface;

/**
 * URoadMeshOpPreviewWithBackgroundCompute
 */
UCLASS(Transient)
class UNREALDRIVEEDITOR_API URoadMeshOpPreviewWithBackgroundCompute
	: public UObject
	, public IRoadOpCompute
{
	GENERATED_BODY()

public:
	void Setup(UTriangulateRoadTool* RoadTool, TWeakPtr<UnrealDrive::FRoadActorComputeScope> RoadComputeScope, UE::Geometry::IDynamicMeshOperatorFactory* OpFactory);
	virtual void ShutdownAndGenerateAssets(AActor* TargetActor, const FTransform3d& ActorToWorld) override;
	virtual void InvalidateResult() override { BackgroundCompute->InvalidateResult(); }
	virtual void CancelCompute() override { BackgroundCompute->CancelCompute(); }
	virtual void SetVisibility(bool bVisible) override { BackgroundCompute->SetVisibility(bVisible); }
	virtual void Tick(float DeltaTime) override { BackgroundCompute->Tick(DeltaTime); };
	virtual void EnableWireframe(bool bEnable) override { BackgroundCompute->PreviewMesh->EnableWireframe(bEnable); }
	virtual void Cancel() override { BackgroundCompute->Cancel(); }
	virtual bool HaveValidNonEmptyResult() const override { return BackgroundCompute->HaveValidNonEmptyResult(); }
	virtual UE::Geometry::EBackgroundComputeTaskStatus GetLastComputeStatus() const override;
	virtual bool IsRoadAttribute() const { return bIsRoadAttribute; }
	virtual int GetNumVertices() const;
	virtual int GetNumTriangles() const;

	UPROPERTY()
	bool bIsRoadAttribute = false;

	UPROPERTY()
	TObjectPtr<UMeshOpPreviewWithBackgroundCompute> BackgroundCompute;

	UPROPERTY()
	FString BaseAssetName;

	UPROPERTY()
	TScriptInterface<IInteractiveToolPropertyMaterialInterface> MaterialGetter;

	UPROPERTY()
	TArray<FName> ResultMaterialSlots;

	UPROPERTY()
	TWeakObjectPtr<UTriangulateRoadTool> RoadTool;
};


UINTERFACE()
class UNREALDRIVEEDITOR_API UInteractiveToolPropertyMaterialInterface : public UInterface
{
	GENERATED_BODY()
};

class UNREALDRIVEEDITOR_API IInteractiveToolPropertyMaterialInterface
{
	GENERATED_BODY()

public:
	virtual TMap<FName, TObjectPtr<UMaterialInterface>> GetMaterialsMap() const = 0;
};

namespace UnrealDrive
{
	template <typename TMaterialProfile>
	TMap<FName, TObjectPtr<UMaterialInterface>> MakeMaterialsMap(const TMap<FName, TMaterialProfile>& Profiles, const TMap<FName, TObjectPtr<UMaterialInterface>>& OverrideMaterials)
	{
		TMap<FName, TObjectPtr<UMaterialInterface>> Ret;
		for (auto& Profile : Profiles)
		{
			if (auto* OverrideMaterial = OverrideMaterials.Find(Profile.Key))
			{
				Ret.Add(Profile.Key, *OverrideMaterial);
			}
			else 
			{
				Ret.Add(Profile.Key, Profile.Value.DefaultMaterial);
			}
		}
		return Ret;
	}
}