/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

using System.IO;
using UnrealBuildTool;

public class UnrealDriveEditor : ModuleRules
{
	public UnrealDriveEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
			}
			);
				

		/*
		PrivateIncludePaths.AddRange(
			new string[] {
				//"Runtime/../../Plugins/Runtime/GeometryProcessing/Source/GeometryAlgorithms/Private",
				Path.Combine(GetModuleDirectory("GeometryAlgorithms"), "Private")
			}
		);
		*/
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"ComponentVisualizers",
				"UnrealDrive",
				"InteractiveToolsFramework",
				"GeometryCore",
				"GeometryFramework",
				"GeometryAlgorithms",
				"DynamicMesh",
				"MeshConversion",
				"MeshDescription",
				"StaticMeshDescription",
				"ModelingComponents",
				"ModelingOperators",
				"ModelingOperatorsEditorOnly",
				"MeshModelingToolsExp",
				"ModelingToolsEditorMode",
				"SceneOutliner",
				"MeshModelingTools",
				"DeveloperSettings"
			}
		);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"UnrealEd",
				"LevelEditor",
				"InputCore",
				"EditorFramework",
				"StructUtils",
				"Kismet",
				"Projects",
				"ApplicationCore",
				"DetailCustomizations",
				"EditorInteractiveToolsFramework",
				"ModelingComponentsEditorOnly",
				"StructUtilsEditor",
				"PropertyEditor",
				"ImageCore",
				"HTTP",
				"RHI",
				"RenderCore",
				"CurveEditor"
			}
		);

		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
		);
	}
}
