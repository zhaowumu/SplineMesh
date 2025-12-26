/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Engine/TextureDefines.h"
#include "Engine/DeveloperSettings.h"
#include "UnrealDriveTypes.h"
#include "DefaultRoadLaneAttributes.h"
#include "UnrealDriveSettings.generated.h"



/*
USTRUCT()
struct FDriveLaneAttributeDescription
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, NoClear, Export, Category = RoadLane, Meta = (MetaStruct = "/Script/UnrealDrive.RoadLaneAttributeBase"))
	TObjectPtr<UScriptStruct> Attribute;
};

inline uint32 GetTypeHash(const FDriveLaneAttributeDescription& Desc)
{
	return GetTypeHash(Desc.Attribute);
}
*/

UCLASS(config = Plugins, defaultconfig, meta=(DisplayName="Unreal Drive"))
class UNREALDRIVE_API UUnrealDriveSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UUnrealDriveSettings();

public:
	//~ Begin UDeveloperSettings interface
	virtual FName GetCategoryName() const;
#if WITH_EDITOR
	virtual FText GetSectionText() const override;
	virtual FText GetSectionDescription() const override;
#endif
	//~ End UDeveloperSettings interface

	UPROPERTY(EditAnywhere, config, Category = Brush)
	TSoftObjectPtr<UMaterialInterface> DefaultSolidMaterial;

	UPROPERTY(EditAnywhere, config, Category = Brush)
	TSoftObjectPtr<UMaterialInterface> DefaultDirectLaneMaterial;

	UPROPERTY(EditAnywhere, config, Category = Brush)
	TSoftObjectPtr<UMaterialInterface> DefaultDirectLaneTransparentMaterial;

	UPROPERTY(EditAnywhere, config, Category = Brush)
	TSoftObjectPtr<UMaterialInterface> DefaultDirectLaneGridMaterial;

	UPROPERTY(EditAnywhere, config, Category = LookAndFeel, AdvancedDisplay, meta = (ClampMin = "2", ClampMax = "100"))
	int NumPointPerSegmaent = 20;

	UPROPERTY(EditAnywhere, config, Category = LookAndFeel, AdvancedDisplay, meta = (ClampMin = "2", ClampMax = "100"))
	int NumPointPerSection = 20;


public:
	TMap<EDriveableRoadLaneType, TObjectPtr<UMaterialInstanceDynamic>> DriveableLaneMatrtials;
	TObjectPtr<UMaterialInstanceDynamic> SidewalkMatrtial;
	TObjectPtr<UMaterialInstanceDynamic> EmptyLaneMatrtial;
	TObjectPtr<UMaterialInstanceDynamic> HiddenLaneMatrtial;
	TObjectPtr<UMaterialInstanceDynamic> SelectedLaneMatrtial;
	TObjectPtr<UMaterialInstanceDynamic> SplineArrowMatrtial;

	static UMaterialInstanceDynamic* GetLaneMatrtial(const TInstancedStruct<FRoadLaneInstance>& LaneInstance);
};
