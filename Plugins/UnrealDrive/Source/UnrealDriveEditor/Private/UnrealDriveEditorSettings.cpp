/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "UnrealDriveEditorSettings.h"
#include "Engine/Texture.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Styling/StyleColors.h"
#include "UnrealDriveEditorModule.h"
#include "Utils/DrawUtils.h"

#define LOCTEXT_NAMESPACE "UUnrealDriveEditorSettings"

#include UE_INLINE_GENERATED_CPP_BY_NAME(UnrealDriveEditorSettings)


UUnrealDriveEditorSettings::UUnrealDriveEditorSettings()
	: LaneConnectionMaterial(FSoftObjectPath(TEXT("/Engine/EngineMaterials/GizmoMaterial.GizmoMaterial")))
{
	TileSources.Add(TEXT("Google Satellite Only"), { TEXT("http://mt0.google.com/vt/lyrs=s&hl=en&x={x}&y={y}&z={z}"), ETileMapProjection::WebMercator });
	TileSources.Add(TEXT("Google Roadmap"), { TEXT("http://mt0.google.com/vt/lyrs=m&hl=en&x={x}&y={y}&z={z}"), ETileMapProjection::WebMercator });
	TileSources.Add(TEXT("Google Terrain"), { TEXT("http://mt0.google.com/vt/lyrs=p&hl=en&x={x}&y={y}&z={z}"), ETileMapProjection::WebMercator });
	TileSources.Add(TEXT("Google Altered Roadmap"), { TEXT("http://mt0.google.com/vt/lyrs=r&hl=en&x={x}&y={y}&z={z}"), ETileMapProjection::WebMercator });
	TileSources.Add(TEXT("Google Terrain Only"), { TEXT("http://mt0.google.com/vt/lyrs=t&hl=en&x={x}&y={y}&z={z}"), ETileMapProjection::WebMercator });
	TileSources.Add(TEXT("Google Hybrid"), { TEXT("http://mt0.google.com/vt/lyrs=y&hl=en&x={x}&y={y}&z={z}"), ETileMapProjection::WebMercator });
	TileSources.Add(TEXT("OSM"), { TEXT("https://tile.openstreetmap.org/{z}/{x}/{y}.png"), ETileMapProjection::WebMercator });
	TileSources.Add(TEXT("Yandex Satellite Only"), { TEXT("https://sat01.maps.yandex.net/tiles?l=sat&v=1.22.0&x={x}&y={y}&z={z}&g=Gagari"), ETileMapProjection::WorldMercator });
}

FName UUnrealDriveEditorSettings::GetCategoryName() const
{
	return TEXT("Plugins");
}

FText UUnrealDriveEditorSettings::GetSectionText() const
{
	return LOCTEXT("UnrealDriveEditorSettings_Section", "UnrealDrive Editor");
}

FText UUnrealDriveEditorSettings::GetSectionDescription() const
{
	return LOCTEXT("UnrealDriveEditorSettings_Description", "UnrealDrive Editor Settings");
}

const UMaterialInstanceDynamic* UUnrealDriveEditorSettings::GetLaneConnectionMaterialDyn() const
{
	if (!LaneConnectionMaterialDyn)
	{
		auto Mat = LaneConnectionMaterial.Get();
		check(Mat);
		LaneConnectionMaterialDyn = UMaterialInstanceDynamic::Create(Mat, nullptr);
		LaneConnectionMaterialDyn->SetVectorParameterValue(TEXT("GizmoColor"), FColor(255, 255, 255));
	}
	return LaneConnectionMaterialDyn;
}

const UMaterialInstanceDynamic* UUnrealDriveEditorSettings::GetLaneConnectionSelectedMaterialDyn() const
{
	if (!LaneConnectionSelectedMaterialDyn)
	{
		auto Mat = LaneConnectionMaterial.Get();
		check(Mat);
		LaneConnectionSelectedMaterialDyn = UMaterialInstanceDynamic::Create(Mat, nullptr);
		LaneConnectionSelectedMaterialDyn->SetVectorParameterValue(TEXT("GizmoColor"), FStyleColors::AccentOrange.GetSpecifiedColor());
	}
	return LaneConnectionSelectedMaterialDyn;
}

const FColor FUnrealDriveColors::EmptyColor(106, 145, 196);
const FColor FUnrealDriveColors::SelectedColor = FStyleColors::AccentOrange.GetSpecifiedColor().ToFColor(true);
const FColor FUnrealDriveColors::ReadOnlyColor(255, 0, 255, 255);
const FColor FUnrealDriveColors::ErrColor(184, 15, 10, 255);
const FColor FUnrealDriveColors::RestrictedColor(104, 151, 187, 255);
const FColor FUnrealDriveColors::AccentColorHi(129, 106, 196);
const FColor FUnrealDriveColors::AccentColorLow = DrawUtils::MakeLowAccent(FUnrealDriveColors::AccentColorHi).ToFColor(true);

const FColor FUnrealDriveColors::SplineColor = FStyleColors::AccentPink.GetSpecifiedColor().ToFColor(true);
const FColor FUnrealDriveColors::CrossSplineColor = FColor::Yellow;
const FColor FUnrealDriveColors::TangentColor = FLinearColor(0.718f, 0.589f, 0.921f).ToFColor(true);

#undef LOCTEXT_NAMESPACE
