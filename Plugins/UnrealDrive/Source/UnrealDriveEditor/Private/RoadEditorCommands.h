/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "Containers/Map.h"
#include "Framework/Commands/Commands.h"
#include "Templates/SharedPointer.h"

class FName;
class FUICommandInfo;


class FRoadEditorCommands : public TCommands<FRoadEditorCommands>
{

public:
	FRoadEditorCommands();
	virtual void RegisterCommands() override;

public:
	static FName RoadContext;

	// Modes
	TSharedPtr<FUICommandInfo> RoadSplineMode;
	TSharedPtr<FUICommandInfo> RoadSectionMode;
	TSharedPtr<FUICommandInfo> RoadOffsetMode;
	TSharedPtr<FUICommandInfo> RoadLaneWidthMode;

	//Misks
	TSharedPtr<FUICommandInfo> RoadSplineVisibility;
	TSharedPtr<FUICommandInfo> TileMapWindowVisibility;
	//TSharedPtr<FUICommandInfo> UnhideAllRoadSplines;
	TSharedPtr<FUICommandInfo> HideSelectedSpline;
	TSharedPtr<FUICommandInfo> UnhideAllSpline;
	TSharedPtr<FUICommandInfo> About;

	// Tools
	TSharedPtr<FUICommandInfo> UnrealDriveToolsTabButton;
	TSharedPtr<FUICommandInfo> BeginRoadToMeshTool;
	TSharedPtr<FUICommandInfo> BeginDrawNewRoad;
	TSharedPtr<FUICommandInfo> BeginDrawNewInnerRoad;
};

