/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "UnrealDriveSettings.h"
#include "Engine/Texture.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Styling/StyleColors.h"

#define LOCTEXT_NAMESPACE "UUnrealDriveSettings"

#include UE_INLINE_GENERATED_CPP_BY_NAME(UnrealDriveSettings)


UUnrealDriveSettings::UUnrealDriveSettings()
	: DefaultSolidMaterial(FSoftObjectPath(TEXT("/UnrealDrive/EditorAssets/Materials/SolidMaterial.SolidMaterial")))
	, DefaultDirectLaneMaterial(FSoftObjectPath(TEXT("/UnrealDrive/EditorAssets/Materials/DirectLaneMaterial.DirectLaneMaterial")))
	, DefaultDirectLaneTransparentMaterial(FSoftObjectPath(TEXT("/UnrealDrive/EditorAssets/Materials/DirectLaneTransparentMaterial.DirectLaneTransparentMaterial")))
	, DefaultDirectLaneGridMaterial(FSoftObjectPath(TEXT("/UnrealDrive/EditorAssets/Materials/DirectLaneGridMaterial.DirectLaneGridMaterial")))
{
	checkf(DefaultSolidMaterial.LoadSynchronous(), TEXT("Can't load DefaultSolidMaterial"));
	checkf(DefaultDirectLaneMaterial.LoadSynchronous(), TEXT("Can't load DefaultDirectLaneMaterial"));
	checkf(DefaultDirectLaneTransparentMaterial.LoadSynchronous(), TEXT("Can't load DirectLaneTransparentMaterial"));
	checkf(DefaultDirectLaneGridMaterial.LoadSynchronous(), TEXT("Can't load DefaultDirectLaneGridMaterial"));

	auto CreateSolidMaterial = [this](const FLinearColor& Color)
	{
		UMaterialInstanceDynamic* Mat = UMaterialInstanceDynamic::Create(DefaultSolidMaterial.Get(), GetTransientPackage());
		Mat->SetVectorParameterValue(TEXT("BaseColor"), Color);
		return Mat;
	};

	auto CreateDirectMaterial = [this](const FLinearColor& Color)
	{
		UMaterialInstanceDynamic* Mat = UMaterialInstanceDynamic::Create(DefaultDirectLaneMaterial.Get(), GetTransientPackage());
		Mat->SetVectorParameterValue(TEXT("BaseColor"), Color);
		return Mat;
	};

	auto CreateDirectLaneTransparentMateria = [this](const FLinearColor& Color)
	{
		UMaterialInstanceDynamic* Mat = UMaterialInstanceDynamic::Create(DefaultDirectLaneTransparentMaterial.Get(), GetTransientPackage());
		Mat->SetVectorParameterValue(TEXT("BaseColor"), Color);
		return Mat;
	};

	auto CreateDirectLaneGridMaterial = [this](const FLinearColor& Color)
	{
		UMaterialInstanceDynamic* Mat = UMaterialInstanceDynamic::Create(DefaultDirectLaneGridMaterial.Get(), GetTransientPackage());
		Mat->SetVectorParameterValue(TEXT("BaseColor"), Color);
		return Mat;
	};

	const FLinearColor NormalDrive(FColor(0, 96, 153));
	const FLinearColor RestrictedDrive(FColor(1, 87, 138));

	DriveableLaneMatrtials.Add(EDriveableRoadLaneType::Driving, CreateDirectMaterial(NormalDrive));
	//DriveableLaneMatrtials.Add(EDriveableRoadLaneType::Bidirectional, CreateDirectMaterial(NormalDrive));
	DriveableLaneMatrtials.Add(EDriveableRoadLaneType::Shoulder, CreateDirectLaneGridMaterial(RestrictedDrive));
	DriveableLaneMatrtials.Add(EDriveableRoadLaneType::Border, CreateDirectLaneGridMaterial(RestrictedDrive));
	DriveableLaneMatrtials.Add(EDriveableRoadLaneType::Stop, CreateDirectLaneGridMaterial(RestrictedDrive));
	DriveableLaneMatrtials.Add(EDriveableRoadLaneType::Biking, CreateDirectLaneGridMaterial(RestrictedDrive));
	DriveableLaneMatrtials.Add(EDriveableRoadLaneType::Restricted, CreateDirectLaneGridMaterial(RestrictedDrive));
	DriveableLaneMatrtials.Add(EDriveableRoadLaneType::Parking, CreateDirectLaneGridMaterial(RestrictedDrive));
	DriveableLaneMatrtials.Add(EDriveableRoadLaneType::Median, CreateDirectLaneGridMaterial(RestrictedDrive));
	DriveableLaneMatrtials.Add(EDriveableRoadLaneType::RoadWorks, CreateDirectLaneGridMaterial(RestrictedDrive));
	//DriveableLaneMatrtials.Add(EDriveableRoadLaneType::Tram, CreateDirectLaneGridMaterial(RestrictedDrive));
	//DriveableLaneMatrtials.Add(EDriveableRoadLaneType::Exit, CreateDirectMaterial(NormalDrive));
	//DriveableLaneMatrtials.Add(EDriveableRoadLaneType::OffRamp, CreateDirectMaterial(NormalDrive));
	//DriveableLaneMatrtials.Add(EDriveableRoadLaneType::OnRamp, CreateDirectMaterial(NormalDrive));

	//CurbLaneMatrtial = CreateDirectMaterial(FColor(26, 64, 41));
	SidewalkMatrtial = CreateDirectMaterial(FColor(33, 82, 53));
	EmptyLaneMatrtial = CreateDirectLaneGridMaterial(FColor(87, 35, 35));

	HiddenLaneMatrtial = CreateDirectLaneTransparentMateria(FColor(50, 50, 50));
	SelectedLaneMatrtial = CreateDirectMaterial(FStyleColors::AccentOrange.GetSpecifiedColor() * 0.5);

	SplineArrowMatrtial = CreateSolidMaterial(FColor::White);
}

FName UUnrealDriveSettings::GetCategoryName() const
{
	return TEXT("Plugins");
}

#if WITH_EDITOR
FText UUnrealDriveSettings::GetSectionText() const
{
	return LOCTEXT("UnrealDriveEditorSettings_Section", "UnrealDrive");
}

FText UUnrealDriveSettings::GetSectionDescription() const
{
	return LOCTEXT("UnrealDriveEditorSettings_Description", "UnrealDrive Settings");
}
#endif


UMaterialInstanceDynamic* UUnrealDriveSettings::GetLaneMatrtial(const TInstancedStruct<FRoadLaneInstance>& LaneInstance)
{
	const UUnrealDriveSettings * Settings = GetDefault<UUnrealDriveSettings>();

	if (auto* DrivingLane = LaneInstance.GetPtr<FRoadLaneDriving>())
	{
		if (auto* Found = Settings->DriveableLaneMatrtials.Find(DrivingLane->DriveableLaneType))
		{
			return *Found;
		}
	}
	else if (auto SidewalkLane = LaneInstance.GetPtr<FRoadLaneSidewalk>())
	{
		return Settings->SidewalkMatrtial;
	}
	return Settings->EmptyLaneMatrtial;
}

#undef LOCTEXT_NAMESPACE