/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "ModelingModeToolExtensions.h"
#include "UnrealDriveEditorStyle.h"
#include "StructUtils/InstancedStruct.h"
#include "DefaultRoadLaneAttributes.h"
#include "RoadLaneAttributeEntries.h"
//#include "UnrealDriveEditorModule.generated.h"

class FUICommandList;

//class FRoadSectionComponentVisualizer;
//class FRoadSplineComponentVisualizer;

class FComponentVisualizer;

enum class ERoadSelectionMode
{
	None,
	Spline,
	Section,
	Offset,
	Width,
	Attribute
};

class IRoadOpCompute;
class UTriangulateRoadTool;

namespace UnrealDrive
{
	struct FRoadActorComputeScope;
}

class UNREALDRIVEEDITOR_API FUnrealDriveEditorModule :
	public IModuleInterface, 
	public IModelingModeToolExtension
{
public:
		
	static inline FUnrealDriveEditorModule& Get() { return FModuleManager::LoadModuleChecked< FUnrealDriveEditorModule >("UnrealDriveEditor"); }

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	void SetSplineEditorMode();
	void SetSectionEditorMode();
	void SetOffsetEditorMode();
	void SetWidthEditorMode();
	void SetAttributeEditorMode(FName RoadLaneAttributName);

	ERoadSelectionMode GetRoadSelectionMode() const { return RoadSelectionMode; }
	FName GetSelectionRoadLaneAttribute() const { return SelectionRoadLaneAttribute; }

	TSharedPtr<FUICommandList> GetCommandList() const { return CommandList; }

	TSharedPtr<FComponentVisualizer> GetComponentVisualizer() const { return ComponentVisualizer; }

	bool RegisterRoadLaneAttribute(FName EntryName, const TInstancedStruct<FRoadLaneAttributeEntry>& RoadLaneAttributeEntry);
	void UnregisterRoadLaneAttribute(FName EntryName);;

	// if RoadLaneAttributEntrie return true the the loop will stoped and function return current RoadLaneAttributEntrie
	const TInstancedStruct<FRoadLaneAttributeEntry>* ForEachRoadLaneAttributEntries(TFunctionRef<bool(FName, const TInstancedStruct<FRoadLaneAttributeEntry>*)> RoadLaneAttributEntrie);

	

	static bool IsTileRendersVisibleInEditor() { return bIsTileRendersVisibleInEditor; }
	static void SetIsTileRendersVisibleInEditor(bool bInbIsTileRendersVisibleInEditor) { bIsTileRendersVisibleInEditor = bInbIsTileRendersVisibleInEditor; }

	DECLARE_DELEGATE_RetVal_TwoParams(IRoadOpCompute*, FRoadComputeFactory, UTriangulateRoadTool*, TWeakPtr<UnrealDrive::FRoadActorComputeScope>);
	void RegisterRoadComputeFactory(FName FactoryName, FRoadComputeFactory&& Factory);
	void UnregisterRoadComputeFactory(FName FactoryName);
	const TMap<FName, FRoadComputeFactory>& GetRoadComputeFactories() const { return RoadComputeFactories; }


protected:
	/* IModelingModeToolExtension implementation */
	virtual FText GetExtensionName() override;
	virtual FText GetToolSectionName() override;
	virtual void GetExtensionTools(const FExtensionToolQueryInfo& QueryInfo, TArray<FExtensionToolDescription>& ToolsOut) override;
	virtual bool GetExtensionExtendedInfo(FModelingModeExtensionExtendedInfo& InfoOut) override;
	virtual bool GetExtensionToolTargets(TArray<TSubclassOf<UToolTargetFactory>>& ToolTargetFactoriesOut) override;
	/* IModelingModeToolExtension implementation */

	void AddToolbarExtension(FToolBarBuilder& builder);
	void BindCommands();
	void RegisterRoadLaneAttributes();
	void RegisterRoadComputeFactories();
	void OnPreExit();
	void OnPostEngineInit();

	void SetComponentVisualizer(TSharedRef<FComponentVisualizer> Visualizer);

	TSharedPtr<FUICommandList> CommandList;

	ERoadSelectionMode RoadSelectionMode = ERoadSelectionMode::None;
	FName SelectionRoadLaneAttribute = NAME_None;

	TSharedPtr<FComponentVisualizer> ComponentVisualizer;

	TMap<FName, TInstancedStruct<FRoadLaneAttributeEntry>> RoadLaneAttributEntries;

	//FOnRoadEditorModeChanged OnRoadEditorModeChanged;

	struct FComboBoxContent
	{
		FText Lable;
		FText Tooltip;
		FSlateIcon Icon;
	};
	FComboBoxContent ComboBoxContent;

	
	TMap<FName, FRoadComputeFactory> RoadComputeFactories;

	static bool bIsTileRendersVisibleInEditor;
};
