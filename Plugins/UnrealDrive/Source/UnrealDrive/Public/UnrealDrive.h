/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

UNREALDRIVE_API DECLARE_LOG_CATEGORY_EXTERN(LogUnrealDrive, Log, All);


class UNREALDRIVE_API FUnrealDriveModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	static bool IsRoadSplinesVisibleInEditor() { return bIsRoadSplinesVisibleInEditor; }
	static void SetIsRoadSplinesVisibleInEditor(bool bInIsRoadSplinesVisibleInEditor) { bIsRoadSplinesVisibleInEditor = bInIsRoadSplinesVisibleInEditor; }

private:
	static bool bIsRoadSplinesVisibleInEditor;
};
