/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Engine/TextureDefines.h"
#include "UnrealDriveEditorSettings.generated.h"

UENUM()
enum class ETileMapProjection: uint8
{
	/** EPSG:3857 - Spherical Mercator, also known as WGS84 Web Mercator or WGS84/Pseudo-Mercator */
	WebMercator,

	/** EPSG:3395 - True Elliptical Mercator WGS84 */
	WorldMercator,
};

USTRUCT()
struct FTileMapSource
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, config, Category = TileMapSource)
	FString URL;

	UPROPERTY(EditAnywhere, config, Category = TileMapSource)
	ETileMapProjection Projection = ETileMapProjection::WebMercator;
};


UCLASS(config = Plugins, defaultconfig, meta=(DisplayName="Unreal Drive Editor"))
class UNREALDRIVEEDITOR_API UUnrealDriveEditorSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UUnrealDriveEditorSettings();

	//~ Begin UDeveloperSettings interface
	virtual FName GetCategoryName() const;
	virtual FText GetSectionText() const override;
	virtual FText GetSectionDescription() const override;
	//~ End UDeveloperSettings interface

public:
	UPROPERTY(EditAnywhere, config, Category = TileMapSources)
	TMap<FName, FTileMapSource> TileSources;

	UPROPERTY(EditAnywhere, config, Category = LookAndFeel, AdvancedDisplay)
	TSoftObjectPtr<UMaterialInterface> LaneConnectionMaterial;

	/** The size adjustment to apply to spline line thickness which increases the spline's hit tolerance. */
	UPROPERTY(EditAnywhere, config, Category = LookAndFeel, AdvancedDisplay, meta = (ClampMin = "0.00"))
	double CenterSplineLineThicknessAdjustment = 4.0f;

	/** The scale to apply to spline tangent lengths */
	UPROPERTY(EditAnywhere, config, Category = LookAndFeel, AdvancedDisplay, meta = (ClampMin = "0.00"))
	double SplineTangentScale = 0.5f;

	/** The size adjustment to apply to selected spline points (in screen space units). */
	UPROPERTY(EditAnywhere, config, Category = LookAndFeel, AdvancedDisplay, meta = (ClampMin = "-5.0", ClampMax = "20.0"))
	double SelectedSplinePointSizeAdjustment = 0.0f;

	UPROPERTY(EditAnywhere, config, Category = LookAndFeel, AdvancedDisplay, meta = (ClampMin = "-5.0", ClampMax = "20.0"))
	double SplineTangentHandleSizeAdjustment = 0.0f;

	UPROPERTY(EditAnywhere, config, Category = LookAndFeel, AdvancedDisplay, meta = (ClampMin = "100.0", ClampMax = "100000.0"))
	double RoadConnectionsMaxViewDistance = 30000.0;

	UPROPERTY(EditAnywhere, config, Category = LookAndFeel, AdvancedDisplay, meta = (ClampMin = "100.0", ClampMax = "200000.0"))
	double RoadConnectionMaxViewOrthoWidth = 50000.0;

	const UMaterialInstanceDynamic* GetLaneConnectionMaterialDyn() const;
	const UMaterialInstanceDynamic* GetLaneConnectionSelectedMaterialDyn() const;


private:
	UPROPERTY(Transient, NonPIEDuplicateTransient)
	mutable TObjectPtr<UMaterialInstanceDynamic> LaneConnectionMaterialDyn;

	UPROPERTY(Transient, NonPIEDuplicateTransient)
	mutable TObjectPtr<UMaterialInstanceDynamic> LaneConnectionSelectedMaterialDyn;
};

struct FUnrealDriveColors
{
	static UNREALDRIVEEDITOR_API const FColor EmptyColor;
	static UNREALDRIVEEDITOR_API const FColor SelectedColor;
	static UNREALDRIVEEDITOR_API const FColor ReadOnlyColor;
	static UNREALDRIVEEDITOR_API const FColor ErrColor;
	static UNREALDRIVEEDITOR_API const FColor RestrictedColor;

	static UNREALDRIVEEDITOR_API const FColor SplineColor;
	static UNREALDRIVEEDITOR_API const FColor CrossSplineColor;
	static UNREALDRIVEEDITOR_API const FColor TangentColor;
	static UNREALDRIVEEDITOR_API const FColor AccentColorHi;
	static UNREALDRIVEEDITOR_API const FColor AccentColorLow;
};