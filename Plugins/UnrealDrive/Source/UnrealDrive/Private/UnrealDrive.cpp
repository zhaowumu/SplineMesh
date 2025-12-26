/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "UnrealDrive.h"
#include "UnrealDriveVersion.h"

#define LOCTEXT_NAMESPACE "FUnrealDriveModule"

bool FUnrealDriveModule::bIsRoadSplinesVisibleInEditor = true;

void FUnrealDriveModule::StartupModule()
{
	UE_LOG(LogUnrealDrive, Log, TEXT("UnreadDrive version: " UNREALDRIVE_VERSION_STRING));
}

void FUnrealDriveModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE
	
DEFINE_LOG_CATEGORY(LogUnrealDrive);

IMPLEMENT_MODULE(FUnrealDriveModule, UnrealDrive)