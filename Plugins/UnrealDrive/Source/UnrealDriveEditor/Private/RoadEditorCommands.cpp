/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "RoadEditorCommands.h"
#include "UnrealDriveEditorStyle.h"
#include "Framework/Commands/InputChord.h"
#include "Framework/Commands/UICommandInfo.h"
#include "GenericPlatform/GenericApplication.h"
#include "HAL/Platform.h"
#include "InputCoreTypes.h"
#include "Internationalization/Internationalization.h"
#include "Styling/AppStyle.h"
#include "UObject/NameTypes.h"
#include "UObject/UnrealNames.h"
#include "DefaultRoadLaneAttributes.h"
#include "UnrealDriveEditorModule.h"

#define LOCTEXT_NAMESPACE "FRoadEditorCommands"

FName FRoadEditorCommands::RoadContext = TEXT("RoadEditor");

FRoadEditorCommands::FRoadEditorCommands()
	: TCommands<FRoadEditorCommands>(
		FRoadEditorCommands::RoadContext, // Context name for fast lookup
		NSLOCTEXT("Contexts", "RoadEditor", "Road Editor"), // Localized context name for displaying
		NAME_None, //"LevelEditor" // Parent
		FUnrealDriveEditorStyle::Get().GetStyleSetName() // Icon Style Set
	)
{
}

void FRoadEditorCommands::RegisterCommands()
{
	UI_COMMAND(RoadSplineMode, "Spline", "Road spline editing mode.", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(RoadSectionMode, "Section", "Road section editing mode", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(RoadOffsetMode, "Offset", "Center line offset editing mode", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(RoadLaneWidthMode, "Width", "Road lane width editing mode", EUserInterfaceActionType::RadioButton, FInputChord());

	UI_COMMAND(UnrealDriveToolsTabButton, "Road", "Unreal Drive Toolset", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(BeginRoadToMeshTool, "Build Mesh", "Generate mesh for selected actors with RoadSplineComponent(s)", EUserInterfaceActionType::ToggleButton, FInputChord());
	UI_COMMAND(BeginDrawNewRoad, "New Spline", "Create a new actor and draw a new RoadSplineComponent inside", EUserInterfaceActionType::ToggleButton, FInputChord());
	UI_COMMAND(BeginDrawNewInnerRoad, "Add Spline", "Draw a new RoadSplineComponent inside a selected actor with RoadSplineComponent(s). Mainly used to create intersections and junctions", EUserInterfaceActionType::ToggleButton, FInputChord());

	UI_COMMAND(RoadSplineVisibility, "Roads Visibility", "Whether to visualize URoadSplineComponent(s).", EUserInterfaceActionType::ToggleButton, FInputChord());
	UI_COMMAND(TileMapWindowVisibility, "Tiles Visibility", "Whether to visualize UTileMapWindowComponent().", EUserInterfaceActionType::ToggleButton, FInputChord());

	UI_COMMAND(HideSelectedSpline, "Hide", "Hide selected road spline.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(UnhideAllSpline, "Unhide All", "Unhide all road splines for current actor.", EUserInterfaceActionType::Button, FInputChord());

	UI_COMMAND(About, "About", "About UnrealDrive plugin.", EUserInterfaceActionType::Button, FInputChord());
	
}


#undef LOCTEXT_NAMESPACE
