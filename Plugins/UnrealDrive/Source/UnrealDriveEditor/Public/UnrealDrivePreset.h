/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "Engine/DataAsset.h"
#include "Engine/AssetManagerTypes.h"
#include "Engine/EngineTypes.h"
#include "UnrealDriveEditorModule.h"
#include "UnrealDriveTypes.h"
#include "RoadLaneAttributeEntries.h"
#include "UnrealDrivePresetBase.h"
#include "UnrealDrivePreset.generated.h"

class UActorComponent;
class AActor;

/**
 * FRoadLaneAttributeProfile
 */
USTRUCT()
struct UNREALDRIVEEDITOR_API FRoadLaneAttributeProfile
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = AttributeProfile);
	FName AttributeName;

	UPROPERTY(EditAnywhere, Category = AttributeProfile);
	TInstancedStruct<FRoadLaneAttributeValue> AttributeValueTemplate;

	friend uint32 GetTypeHash(const FRoadLaneAttributeProfile& Profile)
	{
		return GetTypeHash(Profile.AttributeName);
	}

	inline bool operator==(const FRoadLaneAttributeProfile& Other) const
	{
		return Other.AttributeName == AttributeName;
	}
};

/**
 * FRoadLaneProfile
 */
USTRUCT()
struct UNREALDRIVEEDITOR_API FRoadLaneProfile
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = LaneProfile)
	double Width = UnrealDrive::DefaultRoadLaneWidth;

	UPROPERTY(EditAnywhere, NoClear, Category = RoadLane, Export)
	TInstancedStruct<FRoadLaneInstance> LaneInstance;

	UPROPERTY(EditAnywhere, NoClear, Category = RoadLane, Export)
	TSet<FRoadLaneAttributeProfile> Attributes;

	UPROPERTY(EditAnywhere, Category = RoadLane)
	ERoadLaneDirection Direction = ERoadLaneDirection::Default;

	UPROPERTY(EditAnywhere, Category = RoadLane)
	bool bSkipProcrdureGeneration = false;
};

/**
 * FRoadLaneSectionProfile
 */
USTRUCT()
struct UNREALDRIVEEDITOR_API FRoadLaneSectionProfile
{
	GENERATED_USTRUCT_BODY();

	// Profile name for UI only
	UPROPERTY(EditAnywhere, Category = UI)
	FString ProfileName;

	// Profile category for UI only
	UPROPERTY(EditAnywhere, Category = UI)
	FString Category;

	// Tooltip for UI only
	UPROPERTY(EditAnywhere, Category = UI)
	FString Tooltip;

	UPROPERTY(EditAnywhere, Category = LaneSection)
	TArray<FRoadLaneProfile> Left;

	UPROPERTY(EditAnywhere, Category = LaneSection)
	TArray<FRoadLaneProfile> Right;

	UPROPERTY(EditAnywhere, NoClear, Category = RoadLane, Export)
	TSet<FRoadLaneAttributeProfile> CenterAttributes;

	inline FString GetFullName() const { return Category + TEXT(".") + ProfileName; }

	static const FRoadLaneSectionProfile EmptyProfile;
};


/**
 * UUnrealDrivePreset
 */
UCLASS()
class UNREALDRIVEEDITOR_API UUnrealDrivePreset : public UUnrealDrivePresetBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, NoClear, Category = Preset)
	TMap<FName, TInstancedStruct<FRoadLaneAttributeEntry>> RoadAttributeEntries;

	UPROPERTY(EditAnywhere, NoClear, Category = Preset)
	TArray<FRoadLaneSectionProfile> RoadLanesProfiles;
};