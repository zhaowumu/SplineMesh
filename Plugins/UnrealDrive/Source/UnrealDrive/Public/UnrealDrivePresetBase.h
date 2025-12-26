/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "Engine/DataAsset.h"
#include "Engine/AssetManagerTypes.h"
#include "Engine/AssetManager.h"
#include "Engine/EngineTypes.h"
#include "UnrealDriveTypes.h"
#include "DefaultRoadLaneAttributes.h"
#include "UObject/Package.h"
#include "Misc/PackageName.h"
#include "UnrealDrivePresetBase.generated.h"


USTRUCT(BlueprintType, Blueprintable)
struct FRoadLaneMaterialProfile
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, NonTransactional, Category = MaterialProfile)
	TObjectPtr<UMaterialInterface> DefaultMaterial;
};

USTRUCT(BlueprintType, Blueprintable)
struct FSurfaceProfile: public FRoadLaneMaterialProfile
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = MaterialProfile)
	int Priority = 0;

	UPROPERTY(EditAnywhere, NonTransactional, Category = MaterialProfile)
	TObjectPtr<UMaterialInterface> DecaltMaterial;
};

USTRUCT(BlueprintType, Blueprintable)
struct FCurblProfile : public FRoadLaneMaterialProfile
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = CurblProfile)
	FRuntimeFloatCurve CurbCurve;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = CurblProfile)
	float Width = 15;
};


/**
 * UUnrealDrivePresetBase
 */
UCLASS()
class UNREALDRIVE_API UUnrealDrivePresetBase : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = Preset)
	TMap<FName, FCurblProfile> CurbProfiles;

	UPROPERTY(EditAnywhere, Category = Preset, Meta = (ExcludeBaseStruct))
	TMap<FName, TInstancedStruct<FRoadLaneMarkProfile>> LaneMarkProfiles;

	UPROPERTY(EditAnywhere, Category = MaterialProfiles)
	TMap<FName, FSurfaceProfile> DriveableMaterialProfiles;

	//UPROPERTY(EditAnywhere, Category = MaterialProfiles)
	//TMap<FName, FRoadLaneMaterialProfile> DecalMaterialProfiles;

	UPROPERTY(EditAnywhere, Category = MaterialProfiles)
	TMap<FName, FSurfaceProfile> SidewalkMaterialProfiles;

	UPROPERTY(EditAnywhere, Category = MaterialProfiles)
	TMap<FName, FRoadLaneMaterialProfile> LaneMarkMaterialProfiles;

	template<typename T=UUnrealDrivePresetBase>
	static void ForEachPreset(TFunctionRef<void(const T*)> Preset)
	{
		UAssetManager& AssetManager = UAssetManager::Get();
		TArray<FSoftObjectPath> AssetPathList;
		AssetManager.GetPrimaryAssetPathList(FPrimaryAssetType("UnrealDrivePreset"), AssetPathList);

		for (auto& Asset : AssetPathList)
		{
			if (UClass* AssetObjectClass = Cast<UClass>(Asset.TryLoad()))
			{
				if (auto* Obj = Cast<T>(AssetObjectClass->GetDefaultObject()))
				{
					Preset(Obj);
				}
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("UUnrealDrivePresetBase::ForEachPreset(); Can't load \"%s\" asset"), *Asset.ToString());
			}
		}
	}

	template<typename T, typename MemberType>
	static auto GetAllProfiles(MemberType T::* member_ptr)
	{
		MemberType Ret;
		ForEachPreset<T>([&Ret, member_ptr](const T* Preset) {
			Ret.Append(Preset->*member_ptr);
		});
		return Ret;
	}

public:
	UFUNCTION()
	static TArray<FName> GetLaneMarkProfileNames();

	UFUNCTION()
	static TArray<FName> GetDriveableMaterialProfiles();

	UFUNCTION()
	static TArray<FName> GetSidewalkMaterialProfiles();

	UFUNCTION()
	static TArray<FName> GetCurbProfiles();

	UFUNCTION()
	static TArray<FName> GetLaneMarkMaterialProfiles();
	
public:
	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(FName("UnrealDrivePreset"), FPackageName::GetShortFName(GetOutermost()->GetName()));
	}
};