/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "UnrealDriveEditorStyle.h"
#include "Interfaces/IPluginManager.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateStyleMacros.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/SlateTypes.h"
#include "Styling/StyleColors.h"

#define EDITOR_IMAGE_BRUSH_SVG( RelativePath, ... ) FSlateVectorImageBrush(FPaths::EngineContentDir() / "Editor/Slate" / RelativePath + TEXT(".svg"), __VA_ARGS__ )
#define EDITOR_IMAGE_BRUSH( RelativePath, ... ) FSlateImageBrush(FPaths::EngineContentDir() / "Editor/Slate" / RelativePath + TEXT(".png"), __VA_ARGS__ )

FUnrealDriveEditorStyle::FUnrealDriveEditorStyle() :
    FSlateStyleSet("UnrealDriveEditor")
{
	SetParentStyleName(FAppStyle::GetAppStyleSetName());

	static const FVector2D IconSize10x10(10.0f, 10.0f);
	static const FVector2D IconSize16x12(16.0f, 12.0f);
	static const FVector2D IconSize16x16(16.0f, 16.0f);
	static const FVector2D IconSize20x20(20.0f, 20.0f);
	static const FVector2D IconSize24x24(24.0f, 24.0f);
	static const FVector2D IconSize32x32(32.0f, 32.0f);
	static const FVector2D IconSize64x64(64.0f, 64.0f);

	static const FSlateColor DefaultForeground(FLinearColor(0.72f, 0.72f, 0.72f, 1.f));
	static const FSlateColor LogoColor(FLinearColor(0.5f, 0.5f, 0.5f, 1.f));


	//FSlateStyleSet::SetCoreContentRoot(FPaths::EngineContentDir() / TEXT("Editor/Slate"));
	FSlateStyleSet::SetCoreContentRoot(FPaths::EngineContentDir() / TEXT("Slate"));
	FSlateStyleSet::SetContentRoot(IPluginManager::Get().FindPlugin(TEXT("UnrealDrive"))->GetBaseDir() / TEXT("Resources"));

	FTextBlockStyle NormalText = FCoreStyle::Get().GetWidgetStyle<FTextBlockStyle>("NormalText");

	Set("UnrealDriveLogo.Image", new IMAGE_BRUSH_SVG("Icons/Logo", IconSize64x64, LogoColor));

	Set("UnrealDriveLogo.Text", FTextBlockStyle(NormalText)
		.SetFont(DEFAULT_FONT("Bold", 32))
		.SetColorAndOpacity(LogoColor)
	);

	Set("RoadEditor.RoadSplineVisibility", new IMAGE_BRUSH_SVG("Icons/Road", IconSize24x24, DefaultForeground));
	Set("RoadEditor.TileMapWindowVisibility", new IMAGE_BRUSH_SVG("Icons/TileRenderer", IconSize24x24, DefaultForeground));

	Set("RoadEditor.RoadSplineMode", new IMAGE_BRUSH_SVG("Icons/RoadSpline", IconSize24x24, DefaultForeground));
	Set("RoadEditor.RoadSectionMode", new IMAGE_BRUSH_SVG("Icons/RoadLaneSections", IconSize24x24, DefaultForeground));
	Set("RoadEditor.RoadOffsetMode", new IMAGE_BRUSH_SVG("Icons/RoadOffset", IconSize24x24, DefaultForeground));
	Set("RoadEditor.RoadLaneWidthMode", new IMAGE_BRUSH_SVG("Icons/RoadLaneWidth", IconSize24x24, DefaultForeground));
	Set("RoadEditor.RoadLaneMarkMode", new IMAGE_BRUSH_SVG("Icons/RoadLaneMark", IconSize24x24, DefaultForeground));
	Set("RoadEditor.RoadLaneSpeedMode", new IMAGE_BRUSH_SVG("Icons/RoadLaneSpeed", IconSize24x24, DefaultForeground));
	Set("RoadEditor.RoadLaneBuildMode", new IMAGE_BRUSH_SVG("Icons/RoadLaneBuild", IconSize24x24, DefaultForeground));
	
	Set("RoadEditor.UnrealDriveToolsTabButton", new IMAGE_BRUSH_SVG("Icons/Road", IconSize20x20));
	Set("RoadEditor.UnrealDriveToolsTabButton.Small", new IMAGE_BRUSH_SVG("Icons/Road", IconSize16x16));

	Set("RoadEditor.BeginRoadToMeshTool", new IMAGE_BRUSH_SVG("Icons/Road", IconSize20x20));
	Set("RoadEditor.BeginRoadToMeshTool.Small", new IMAGE_BRUSH_SVG("Icons/Road", IconSize20x20));

	Set("RoadEditor.BeginDrawNewRoad", new IMAGE_BRUSH_SVG("Icons/DrawSpline", IconSize20x20));
	Set("RoadEditor.BeginDrawNewRoad.Small", new IMAGE_BRUSH_SVG("Icons/DrawSpline", IconSize16x16));

	Set("RoadEditor.BeginDrawNewPoly", new IMAGE_BRUSH_SVG("Icons/Polygon", IconSize20x20));
	Set("RoadEditor.BeginDrawNewPoly.Small", new IMAGE_BRUSH_SVG("Icons/Polygon", IconSize16x16));

	Set("RoadEditor.BeginDrawNewInnerRoad", new IMAGE_BRUSH_SVG("Icons/AddSpline", IconSize20x20));
	Set("RoadEditor.BeginDrawNewInnerRoad.Small", new IMAGE_BRUSH_SVG("Icons/AddSpline", IconSize16x16));

	Set("RoadEditor.BeginDrawNewInnerRoad", new IMAGE_BRUSH_SVG("Icons/AddSpline", IconSize20x20));
	Set("RoadEditor.BeginDrawNewInnerRoad.Small", new IMAGE_BRUSH_SVG("Icons/AddSpline", IconSize16x16));

	Set("RoadEditor.HideSelectedSpline", new CORE_IMAGE_BRUSH_SVG("Starship/Common/hidden", IconSize20x20));
	Set("RoadEditor.HideSelectedSpline.Small", new CORE_IMAGE_BRUSH_SVG("Starship/Common/hidden", IconSize16x16));

	Set("RoadEditor.UnhideAllSpline", new CORE_IMAGE_BRUSH_SVG("Starship/Common/visible", IconSize20x20));
	Set("RoadEditor.UnhideAllSpline.Small", new CORE_IMAGE_BRUSH_SVG("Starship/Common/visible", IconSize16x16));

	Set("RoadEditor.About", new CORE_IMAGE_BRUSH_SVG("Starship/Common/Info", IconSize24x24, DefaultForeground));


	// RoadOffsetComponentVisualize
	Set("RoadOffsetComponentVisualize.AddKey", new CORE_IMAGE_BRUSH_SVG("Starship/Common/plus", IconSize16x16));
	Set("RoadOffsetComponentVisualize.DeleteKey", new CORE_IMAGE_BRUSH_SVG("Starship/Common/Delete", IconSize16x16));

	// RoadAttributeComponentVisualizerCommands
	Set("RoadAttributeComponentVisualizerCommands.CreateAttribute", new CORE_IMAGE_BRUSH_SVG("Starship/Common/plus", IconSize16x16));
	Set("RoadAttributeComponentVisualizerCommands.DeleteAttribute", new CORE_IMAGE_BRUSH_SVG("Starship/Common/Delete", IconSize16x16));
	Set("RoadAttributeComponentVisualizerCommands.AddAttributeKey", new CORE_IMAGE_BRUSH_SVG("Starship/Common/plus", IconSize16x16));
	Set("RoadAttributeComponentVisualizerCommands.DeleteAttributeKey", new CORE_IMAGE_BRUSH_SVG("Starship/Common/Delete", IconSize16x16));

	// RoadSectionComponentVisualizer
	Set("RoadSectionComponentVisualizer.SplitFullSection", new IMAGE_BRUSH_SVG("Icons/SplitFullSection", IconSize16x16));
	Set("RoadSectionComponentVisualizer.SplitSideSection", new IMAGE_BRUSH_SVG("Icons/SplitLeftSection", IconSize16x16));
	Set("RoadSectionComponentVisualizer.SplitLeftSection", new IMAGE_BRUSH_SVG("Icons/SplitLeftSection", IconSize16x16));
	Set("RoadSectionComponentVisualizer.SplitRightSection", new IMAGE_BRUSH_SVG("Icons/SplitRightSection", IconSize16x16));
	Set("RoadSectionComponentVisualizer.DeleteSection", new CORE_IMAGE_BRUSH_SVG("Starship/Common/Delete", IconSize16x16)); 
	Set("RoadSectionComponentVisualizer.AddLaneToLeft", new IMAGE_BRUSH_SVG("Icons/AddLeft", IconSize16x16));
	Set("RoadSectionComponentVisualizer.AddLaneToRight", new IMAGE_BRUSH_SVG("Icons/AddRight", IconSize16x16));
	Set("RoadSectionComponentVisualizer.DeleteLane", new CORE_IMAGE_BRUSH_SVG("Starship/Common/Delete", IconSize16x16));

	// RoadWidthComponentVisualizerCommands
	Set("RoadWidthComponentVisualizerCommands.AddWidthKey", new CORE_IMAGE_BRUSH_SVG("Starship/Common/plus", IconSize16x16));
	Set("RoadWidthComponentVisualizerCommands.DeleteWidthKey", new CORE_IMAGE_BRUSH_SVG("Starship/Common/Delete", IconSize16x16));

	//TODO: add icons for DriveSplineComponentVisualizer

	Set("Icons.YouTube", new IMAGE_BRUSH_SVG("Icons/YouTube", IconSize16x16));
	Set("Icons.Discord", new IMAGE_BRUSH_SVG("Icons/Discord", IconSize16x16));
	Set("Icons.Email", new IMAGE_BRUSH_SVG("Icons/Email", IconSize16x16));
}

void FUnrealDriveEditorStyle::Register()
{
	FSlateStyleRegistry::RegisterSlateStyle(Get());
}


void FUnrealDriveEditorStyle::Unregister()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(Get());
}


FUnrealDriveEditorStyle& FUnrealDriveEditorStyle::Get()
{
	static FUnrealDriveEditorStyle Instance;
	return Instance;
}

#undef EDITOR_IMAGE_BRUSH_SVG
#undef EDITOR_IMAGE_BRUSH
