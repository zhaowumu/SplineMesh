/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "UnrealDrivePresetBase.h"

#include <iostream>
#include <string>


TArray<FName> UUnrealDrivePresetBase::GetLaneMarkProfileNames()
{
	auto Profiles = GetAllProfiles(&UUnrealDrivePresetBase::LaneMarkProfiles);
	TArray<FName> Keys;
	Profiles.GetKeys(Keys);
	Keys.Remove(NAME_None);
	return Keys;
}

TArray<FName> UUnrealDrivePresetBase::GetDriveableMaterialProfiles()
{
	auto Profiles = GetAllProfiles(&UUnrealDrivePresetBase::DriveableMaterialProfiles);
	TArray<FName> Keys;
	Profiles.GetKeys(Keys);
	Keys.Remove(NAME_None);
	return Keys;
}

TArray<FName> UUnrealDrivePresetBase::GetSidewalkMaterialProfiles()
{
	auto Profiles = GetAllProfiles(&UUnrealDrivePresetBase::SidewalkMaterialProfiles);
	TArray<FName> Keys;
	Profiles.GetKeys(Keys);
	Keys.Remove(NAME_None);
	return Keys;
}

TArray<FName> UUnrealDrivePresetBase::GetCurbProfiles()
{
	auto Profiles = GetAllProfiles(&UUnrealDrivePresetBase::CurbProfiles);
	TArray<FName> Keys;
	Profiles.GetKeys(Keys);
	Keys.Remove(NAME_None);
	return Keys;
}

TArray<FName> UUnrealDrivePresetBase::GetLaneMarkMaterialProfiles()
{
	auto Profiles = GetAllProfiles(&UUnrealDrivePresetBase::LaneMarkMaterialProfiles);
	TArray<FName> Keys;
	Profiles.GetKeys(Keys);
	Keys.Remove(NAME_None);
	return Keys;
}
