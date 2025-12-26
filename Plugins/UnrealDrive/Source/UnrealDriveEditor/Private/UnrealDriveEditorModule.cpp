/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "UnrealDriveEditorModule.h"
#include "UnrealDriveVersion.h"
#include "UnrealDriveEditorStyle.h"
#include "RoadSplineComponent.h"
#include "ComponentVisualizers/RoadSplineComponentVisualizer.h"
#include "ComponentVisualizers/RoadSectionComponentVisualizer.h"
#include "ComponentVisualizers/RoadOffsetComponentVisualizer.h"
#include "ComponentVisualizers/RoadWidthComponentVisualizer.h"
#include "ComponentVisualizers/RoadAttributeComponentVisualizer.h"
#include "DetailCustomizations/RoadSplineComponentDetails.h"
#include "DetailCustomizations/RoadLaneAttributeProfileDetails.h"
#include "DetailCustomizations/DrawRoadToolPropertiesDetails.h"
#include "RoadEditorCommands.h"
#include "LevelEditor.h"
#include "Features/IModularFeatures.h"
#include "UnrealEdGlobals.h"
#include "Selection.h"
#include "Editor/UnrealEdEngine.h"
#include "EditorModeRegistry.h"
#include "Editor/UnrealEdEngine.h"
#include "EditorModeRegistry.h"
#include "PropertyEditorModule.h"
#include "IDetailCustomization.h"
#include "DefaultRoadLaneAttributes.h"
#include "EditorModeManager.h"
#include "UnrealDrive.h"
#include "EngineUtils.h"
#include "UnrealDriveEditorSettings.h"
#include "UnrealDrivePreset.h"
#include "RoadLaneAttributeEntries.h"
#include "TileMapWindowComponent.h"
#include "ModelingTools/TriangulateRoadTool.h"
#include "ModelingTools/DrawRoadTool.h"
#include "ModelingTools/OpFactories.h"
#include "Widgets/Input/SHyperlink.h"

#define LOCTEXT_NAMESPACE "FUnrealDriveEditorModule"

using namespace UnrealDrive;

bool FUnrealDriveEditorModule::bIsTileRendersVisibleInEditor = true;


void FUnrealDriveEditorModule::StartupModule()
{
	FUnrealDriveEditorStyle::Register();
	
	FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomClassLayout("RoadSplineComponent", FOnGetDetailCustomizationInstance::CreateStatic(&FRoadSplineComponentDetails::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout("RoadLaneAttributeProfile", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FRoadLaneAttributeProfileDetails::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout("RoadDrawProfilePicker", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FDrawRoadToolPropertiesDetails::MakeInstance));

	RegisterRoadLaneAttributes();

	FRoadEditorCommands::Register();
	CommandList = MakeShared<FUICommandList>();
	BindCommands();

	IModularFeatures::Get().RegisterModularFeature(IModelingModeToolExtension::GetModularFeatureName(), this);

	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
	TSharedPtr<FExtender> MyExtender = MakeShareable(new FExtender);
	MyExtender->AddToolBarExtension("Play", EExtensionHook::After, CommandList, FToolBarExtensionDelegate::CreateRaw(this, &FUnrealDriveEditorModule::AddToolbarExtension));
	LevelEditorModule.GetToolBarExtensibilityManager()->AddExtender(MyExtender);

	RegisterRoadComputeFactories();

	FCoreDelegates::OnPostEngineInit.AddRaw(this, &FUnrealDriveEditorModule::OnPostEngineInit);
	FCoreDelegates::OnEnginePreExit.AddRaw(this, &FUnrealDriveEditorModule::OnPreExit);
}

void FUnrealDriveEditorModule::ShutdownModule()
{
	if (GUnrealEd)
	{
		GUnrealEd->UnregisterComponentVisualizer(URoadSplineComponent::StaticClass()->GetFName());
	}

	IModularFeatures::Get().UnregisterModularFeature(IModelingModeToolExtension::GetModularFeatureName(), this);

	FCoreDelegates::OnPostEngineInit.RemoveAll(this);
	FCoreDelegates::OnEnginePreExit.RemoveAll(this);

	//PropertyModule.UnregisterCustomPropertyTypeLayout("RoadLaneAttributeProfile");
}

void FUnrealDriveEditorModule::OnPostEngineInit()
{
	if (GUnrealEd)
	{
		SetSplineEditorMode();
	}

	// Unregistering the CustomClassLayout for USplineComponent is necessary because a new ComponentVisualizer (FRoadSectionComponentVisualizer, FRoadSplineComponentVisualizer) 
	// is assigned to URoadSplinneComponent. The CustomClassLayout for USplineComponent gets pointer for FSplineComponentVisualizer, but it is not created, what causes the fall.
	// TODO: Think how to fix it
	FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.UnregisterCustomClassLayout("SplineComponent");

}

void FUnrealDriveEditorModule::OnPreExit()
{
	RoadLaneAttributEntries.Empty();

	if (GUnrealEd)
	{
		GUnrealEd->UnregisterComponentVisualizer(URoadSplineComponent::StaticClass()->GetFName());
	}
	ComponentVisualizer.Reset();
}

void FUnrealDriveEditorModule::RegisterRoadLaneAttributes()
{
	RoadLaneAttributEntries.Add(LaneAttributes::Mark, TInstancedStruct<FRoadLaneAttributeEntry>::Make<FRoadLaneAttributeEntry>(
		TInstancedStruct<FRoadLaneMark>::Make(),
		LOCTEXT("RoadLaneAttributeEntry_Mark", "Mark"),
		FText(),
		"RoadEditor.RoadLaneMarkMode"
	));

	RoadLaneAttributEntries.Add(LaneAttributes::Speed, TInstancedStruct<FRoadLaneAttributeEntry>::Make<FRoadLaneAttributeEntry>(
		TInstancedStruct<FRaodLaneSpeed>::Make(),
		LOCTEXT("RoadLaneAttributeEntry_Speed", "Speed"),
		FText(),
		"RoadEditor.RoadLaneSpeedMode"
	));
}

void FUnrealDriveEditorModule::AddToolbarExtension(FToolBarBuilder& ToolbarBuilder)
{
	ToolbarBuilder.AddSeparator();
	ToolbarBuilder.SetLabelVisibility(EVisibility::Visible);
	ToolbarBuilder.AddComboButton(
		FUIAction(),
		FOnGetContent::CreateLambda([this]() 
		{
			auto& Commands = FRoadEditorCommands::Get();

			FMenuBuilder MenuBuilder(true, CommandList, TSharedPtr<FExtender>(), false, &FAppStyle::Get());

			MenuBuilder.BeginSection(NAME_None, LOCTEXT("ToolbarBase", "Lane Base Modes"));
			MenuBuilder.AddMenuEntry(Commands.RoadSplineMode);
			MenuBuilder.AddMenuEntry(Commands.RoadSectionMode);
			MenuBuilder.AddMenuEntry(Commands.RoadOffsetMode);
			MenuBuilder.AddMenuEntry(Commands.RoadLaneWidthMode);
			MenuBuilder.EndSection();

			MenuBuilder.BeginSection(NAME_None, LOCTEXT("ToolbarAttr", "Lane Attributes Modes"));

			ForEachRoadLaneAttributEntries([&MenuBuilder, this](FName Key, const TInstancedStruct<FRoadLaneAttributeEntry>* Value)
			{ 
				auto& Entry = Value->Get<FRoadLaneAttributeEntry>();
				MenuBuilder.AddMenuEntry(
					!Entry.LabelOverride.IsEmpty() ? Entry.LabelOverride : FText::FromName(Key),
					Entry.ToolTip.IsEmpty() && Entry.AttributeValueTemplate.GetScriptStruct() ? Entry.AttributeValueTemplate.GetScriptStruct()->GetToolTipText() : Entry.ToolTip,
					Entry.GetIcon(),
					FUIAction(
						FExecuteAction::CreateLambda([this, Key]()
						{
							SetAttributeEditorMode(Key);
						}),
						FCanExecuteAction(),
						FIsActionChecked::CreateLambda([this, Key]()
						{
							return GetSelectionRoadLaneAttribute() == Key;
						})
					),
					NAME_None,
					EUserInterfaceActionType::RadioButton
				);
				return false;
			});
			MenuBuilder.EndSection();

			MenuBuilder.BeginSection(NAME_None, LOCTEXT("ToolbarVisibility", "Visibility"));
			MenuBuilder.AddMenuEntry(Commands.RoadSplineVisibility);
			MenuBuilder.AddMenuEntry(Commands.TileMapWindowVisibility);
			MenuBuilder.EndSection();

			MenuBuilder.BeginSection(NAME_None);
			MenuBuilder.AddSeparator();
			MenuBuilder.AddMenuEntry(Commands.About);
			MenuBuilder.EndSection();

			return MenuBuilder.MakeWidget();
		}),
		TAttribute<FText>::CreateLambda([this]()
		{
			return ComboBoxContent.Lable;
		}),
		TAttribute<FText>::CreateLambda([this]() 
		{
			return ComboBoxContent.Tooltip;
		}),
		TAttribute<FSlateIcon>::CreateLambda([this]() 
		{
			return ComboBoxContent.Icon;
		})
	);
}

void FUnrealDriveEditorModule::SetSplineEditorMode()
{
	if (!GUnrealEd)
	{
		return;
	}

	if (RoadSelectionMode != ERoadSelectionMode::Spline)
	{
		SelectionRoadLaneAttribute = NAME_None;
		RoadSelectionMode = ERoadSelectionMode::Spline;
		SetComponentVisualizer(MakeShared<FRoadSplineComponentVisualizer>());
		auto& Command = FRoadEditorCommands::Get().RoadSplineMode;
		ComboBoxContent.Lable = Command->GetLabel();
		ComboBoxContent.Tooltip = Command->GetDescription();
		ComboBoxContent.Icon = Command->GetIcon();
	}
}

void FUnrealDriveEditorModule::SetSectionEditorMode()
{
	if (!GUnrealEd)
	{
		return;
	}

	if (RoadSelectionMode != ERoadSelectionMode::Section)
	{
		SelectionRoadLaneAttribute = NAME_None;
		RoadSelectionMode = ERoadSelectionMode::Section;
		SetComponentVisualizer(MakeShared<FRoadSectionComponentVisualizer>());
		auto& Command = FRoadEditorCommands::Get().RoadSectionMode;
		ComboBoxContent.Lable = Command->GetLabel();
		ComboBoxContent.Tooltip = Command->GetDescription();
		ComboBoxContent.Icon = Command->GetIcon();
	}
}

void FUnrealDriveEditorModule::SetOffsetEditorMode()
{
	if (!GUnrealEd)
	{
		return;
	}

	if (RoadSelectionMode != ERoadSelectionMode::Offset)
	{
		SelectionRoadLaneAttribute = NAME_None;
		RoadSelectionMode = ERoadSelectionMode::Offset;
		SetComponentVisualizer(MakeShared<FRoadOffsetComponentVisualizer>());
		auto& Command = FRoadEditorCommands::Get().RoadOffsetMode;
		ComboBoxContent.Lable = Command->GetLabel();
		ComboBoxContent.Tooltip = Command->GetDescription();
		ComboBoxContent.Icon = Command->GetIcon();
	}
}

void FUnrealDriveEditorModule::SetWidthEditorMode()
{
	if (!GUnrealEd)
	{
		return;
	}

	if (RoadSelectionMode != ERoadSelectionMode::Width)
	{
		SelectionRoadLaneAttribute = NAME_None;
		RoadSelectionMode = ERoadSelectionMode::Width;
		SetComponentVisualizer(MakeShared<FRoadWidthComponentVisualizer>());
		auto& Command = FRoadEditorCommands::Get().RoadLaneWidthMode;
		ComboBoxContent.Lable = Command->GetLabel();
		ComboBoxContent.Tooltip = Command->GetDescription();
		ComboBoxContent.Icon = Command->GetIcon();
	}
}

void FUnrealDriveEditorModule::SetAttributeEditorMode(FName RoadLaneAttributName)
{
	if (!GUnrealEd)
	{
		return;
	}

	SelectionRoadLaneAttribute = RoadLaneAttributName;

	if (RoadSelectionMode != ERoadSelectionMode::Attribute)
	{
		RoadSelectionMode = ERoadSelectionMode::Attribute;
		SetComponentVisualizer(MakeShared<FRoadAttributeComponentVisualizer>());
	}

	StaticCastSharedPtr<FRoadAttributeComponentVisualizer>(ComponentVisualizer)->GetSelectionState()->SetSelectedAttributeName(RoadLaneAttributName);


	auto* Attribute = ForEachRoadLaneAttributEntries([RoadLaneAttributName](FName Key, const TInstancedStruct<FRoadLaneAttributeEntry>*) { return RoadLaneAttributName == Key; });
	check(Attribute);
	auto& Entry = Attribute->Get<FRoadLaneAttributeEntry>();

	ComboBoxContent.Lable = !Entry.LabelOverride.IsEmpty() ? Entry.LabelOverride : FText::FromName(RoadLaneAttributName);
	ComboBoxContent.Tooltip = Entry.ToolTip.IsEmpty() && Entry.AttributeValueTemplate.GetScriptStruct() ? Entry.AttributeValueTemplate.GetScriptStruct()->GetToolTipText() : Entry.ToolTip;
	ComboBoxContent.Icon = Entry.GetIcon();
}

void FUnrealDriveEditorModule::SetComponentVisualizer(TSharedRef<FComponentVisualizer> Visualizer)
{
	GUnrealEd->UnregisterComponentVisualizer(URoadSplineComponent::StaticClass()->GetFName());
	ComponentVisualizer.Reset();
	ComponentVisualizer = Visualizer;
	GUnrealEd->RegisterComponentVisualizer(URoadSplineComponent::StaticClass()->GetFName(), ComponentVisualizer);
	ComponentVisualizer->OnRegister();

	FModuleManager::Get().GetModuleChecked<FPropertyEditorModule>("PropertyEditor").NotifyCustomizationModuleChanged();

	const bool bComponentSelectionChanged = GEditor->GetSelectedComponentCount() > 0;
	USelection* Selection = bComponentSelectionChanged ? GEditor->GetSelectedComponents() : GEditor->GetSelectedActors();
	if (UTypedElementSelectionSet* SelectionSet = Selection->GetElementSelectionSet())
	{
		SelectionSet->OnChanged().Broadcast(SelectionSet);
	}
	GEditor->NoteSelectionChange();
}

void FUnrealDriveEditorModule::BindCommands()
{
	auto& Commands = FRoadEditorCommands::Get();

	CommandList->MapAction(
		Commands.RoadSplineVisibility,
		FExecuteAction::CreateLambda([this]() 
		{
			FUnrealDriveModule::SetIsRoadSplinesVisibleInEditor(!FUnrealDriveModule::IsRoadSplinesVisibleInEditor());
			
			for (TActorIterator<AActor> It(GEditor->GetEditorWorldContext().World()); It; ++It)
			{
				TArray<URoadSplineComponent*> Components;
				It->GetComponents(Components);
				for (auto* Component : Components)
				{
					Component->MarkRenderStateDirty();
				}
			}
			GEditor->RedrawLevelEditingViewports(true);

		}),
		FCanExecuteAction(),
		FIsActionChecked::CreateLambda([this]()
		{
			return FUnrealDriveModule::IsRoadSplinesVisibleInEditor();
		})
	);

	CommandList->MapAction(
		Commands.TileMapWindowVisibility,
		FExecuteAction::CreateLambda([this]() 
		{
			FUnrealDriveEditorModule::SetIsTileRendersVisibleInEditor(!FUnrealDriveEditorModule::IsTileRendersVisibleInEditor());
			
			for (TActorIterator<AActor> It(GEditor->GetEditorWorldContext().World()); It; ++It)
			{
				TArray<UTileMapWindowComponent*> Components;
				It->GetComponents(Components);
				for (auto* Component : Components)
				{
					Component->MarkRenderStateDirty();
				}
			}
			GEditor->RedrawLevelEditingViewports(true);

		}),
		FCanExecuteAction(),
		FIsActionChecked::CreateLambda([this]()
		{
			return FUnrealDriveEditorModule::IsTileRendersVisibleInEditor();
		})
	);

	CommandList->MapAction(
		Commands.HideSelectedSpline,
		FExecuteAction::CreateLambda([this]() 
		{
			for (FSelectionIterator It(GEditor->GetSelectedComponentIterator()); It; ++It)
			{
				if (auto* Component = Cast<URoadSplineComponent>(*It))
				{
					Component->SetVisibility(false);
				}
			}
		})
	);

	CommandList->MapAction(
		Commands.UnhideAllSpline,
		FExecuteAction::CreateLambda([this]() 
		{
			for (TActorIterator<AActor> It(GEditor->GetEditorWorldContext().World()); It; ++It)
			{
				TArray<URoadSplineComponent*> Components;
				It->GetComponents(Components);
				for (auto* Component : Components)
				{
					Component->SetVisibility(true);
				}
			}
		})
	);
		
	CommandList->MapAction(
		Commands.RoadSplineMode,
		FExecuteAction::CreateLambda([this]() 
		{
			SetSplineEditorMode();
		}),
		FCanExecuteAction(), 
		FIsActionChecked::CreateLambda([this]()
		{
			return GetRoadSelectionMode() == ERoadSelectionMode::Spline;
		})
	);

	CommandList->MapAction(
		Commands.RoadSectionMode,
		FExecuteAction::CreateLambda([this]() 
		{
			SetSectionEditorMode();
		}),
		FCanExecuteAction(), 
		FIsActionChecked::CreateLambda([this]()
		{
			return GetRoadSelectionMode() == ERoadSelectionMode::Section && GetSelectionRoadLaneAttribute().IsNone();
		})
	);

	CommandList->MapAction(
		Commands.RoadOffsetMode,
		FExecuteAction::CreateLambda([this]() 
		{
				SetOffsetEditorMode();
		}),
		FCanExecuteAction(), 
		FIsActionChecked::CreateLambda([this]()
		{
			return GetRoadSelectionMode() == ERoadSelectionMode::Offset;
		})
	);

	CommandList->MapAction(
		Commands.RoadLaneWidthMode,
		FExecuteAction::CreateLambda([this]() 
		{
				SetWidthEditorMode();
		}),
		FCanExecuteAction(), 
		FIsActionChecked::CreateLambda([this]()
		{
			return GetRoadSelectionMode() == ERoadSelectionMode::Width;
		})
	);

	CommandList->MapAction(
		Commands.About,
		FExecuteAction::CreateLambda([this]() 
		{
			auto AboutWindow = SNew(SWindow)
				.SizingRule(ESizingRule::UserSized)
				.ClientSize(FVector2D(640, 300))
				.AutoCenter(EAutoCenter::PreferredWorkArea)
				.Title(FText::FromString("About UnrealDrive")) 
				[
					SNew(SBox)
					.Padding(12.f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.AutoHeight()
						.VAlign(VAlign_Center)
						.HAlign(HAlign_Center)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.AutoWidth()
							[
								SNew(SBox)
								.WidthOverride(64)
								.HeightOverride(64)
								[
									SNew(SImage)
									.Image(FUnrealDriveEditorStyle::Get().GetBrush("UnrealDriveLogo.Image"))
								]
							]
							+ SHorizontalBox::Slot()
							.FillWidth(1.0)
							.VAlign(VAlign_Center)
							[
								SNew(STextBlock)
								.Text(LOCTEXT("AboutWindow_Name", "UnrealDrive"))
								.TextStyle(&FUnrealDriveEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>("UnrealDriveLogo.Text"))
							]
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(SBorder)
							.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
							.BorderBackgroundColor(FLinearColor(.1f, .1f, .1f))
							.Padding(1)
						]
						+ SVerticalBox::Slot()
						[
							SNew(SBox)
							.Padding(12.f)
							[
								SNew(SVerticalBox)
								+SVerticalBox::Slot()
								.AutoHeight()
								.Padding(0.0, 10.0, 0.0, 2.0)
								[
									SNew(STextBlock)
									.Text(LOCTEXT("AboutWindow_Name", "UnrealDrive Plugin"))
								]
								+SVerticalBox::Slot()
								.AutoHeight()
								.Padding(2.0)
								[
									SNew(STextBlock)
									.Text(FText::Format(LOCTEXT("AboutWindow_Version", "Version {0}"), FText::FromString(UNREALDRIVE_VERSION_STRING)))
								]
								+SVerticalBox::Slot()
								.AutoHeight()
								.Padding(2.0)
								[
									SNew(STextBlock)
									.Text(LOCTEXT("AboutWindow_copyright", "(c) 2025 Ivan Zhukov. All Rights Reserved."))
								]
								+ SVerticalBox::Slot()
								.AutoHeight()
								.Padding(2.0)
								[
									SNew(SHorizontalBox)
									+ SHorizontalBox::Slot()
									.AutoWidth()
									[
										SNew(STextBlock)
										.Text(LOCTEXT("AboutWindow_copyrightText", "This product is licensed under the "))
									]
									+ SHorizontalBox::Slot()
									.AutoWidth()
									[
										SNew(SHyperlink)
										.Text(LOCTEXT("AboutWindow_copyrightLink", "Fab End User License Agreement"))
										.Style(FAppStyle::Get(), "Common.GotoNativeCodeHyperlink")
										.OnNavigate_Lambda([]()
										{
											 FPlatformProcess::LaunchURL(TEXT("https://www.fab.com/eula"), nullptr, nullptr);
										})
									]

								]
								+ SVerticalBox::Slot()
								.AutoHeight()
								.Padding(0, 12, 0, 2)
								[
									SNew(SHorizontalBox)
									+ SHorizontalBox::Slot()
									.AutoWidth()
									.Padding(0, 0, 4, 0)
									[
										SNew(SImage)
										.Image(FAppStyle::Get().GetBrush("Icons.Documentation"))
									]
									+ SHorizontalBox::Slot()
									.AutoWidth()
									[
										SNew(SHyperlink)
										.Text(LOCTEXT("AboutWindow_docLink", "Documentation"))
										.Style(FAppStyle::Get(), "Common.GotoNativeCodeHyperlink")
										.OnNavigate_Lambda([]()
										{
											 FPlatformProcess::LaunchURL(TEXT("https://unrealdrive.readthedocs.io/en/latest/index.html"), nullptr, nullptr);
										})
									]
								]
								+ SVerticalBox::Slot()
								.AutoHeight()
								.Padding(2.0)
								[
									SNew(SHorizontalBox)
									+ SHorizontalBox::Slot()
									.AutoWidth()
									.Padding(0, 0, 4, 0)
									[
										SNew(SImage)
										.Image(FUnrealDriveEditorStyle::Get().GetBrush("Icons.YouTube"))
									]
									+ SHorizontalBox::Slot()
									.AutoWidth()
									[
										SNew(SHyperlink)
										.Text(LOCTEXT("AboutWindow_youTube", "YouTube"))
										.Style(FAppStyle::Get(), "Common.GotoNativeCodeHyperlink")
										.OnNavigate_Lambda([]()
										{
											 FPlatformProcess::LaunchURL(TEXT("https://www.youtube.com/@UnrealDrivePlugin"), nullptr, nullptr);
										})
									]
								]
								+ SVerticalBox::Slot()
								.AutoHeight()
								.Padding(2.0)
								[
									SNew(SHorizontalBox)
									+ SHorizontalBox::Slot()
									.AutoWidth()
									.Padding(0, 0, 4, 0)
									[
										SNew(SImage)
										.Image(FUnrealDriveEditorStyle::Get().GetBrush("Icons.Discord"))
									]
									+ SHorizontalBox::Slot()
									.AutoWidth()
									[
										SNew(SHyperlink)
										.Text(LOCTEXT("AboutWindow_discord", "Discord"))
										.Style(FAppStyle::Get(), "Common.GotoNativeCodeHyperlink")
										.OnNavigate_Lambda([]()
										{
											 FPlatformProcess::LaunchURL(TEXT("https://discord.gg/xedqRcbC"), nullptr, nullptr);
										})
									]
								]
								+ SVerticalBox::Slot()
								.AutoHeight()
								.Padding(2.0)
								[
									SNew(SHorizontalBox)
									+ SHorizontalBox::Slot()
									.AutoWidth()
									.Padding(0, 0, 4, 0)
									[
										SNew(SImage)
										.Image(FUnrealDriveEditorStyle::Get().GetBrush("Icons.Email"))
									]
									+ SHorizontalBox::Slot()
									.AutoWidth()
									[
										SNew(SHyperlink)
										.Text(FText::FromString(TEXT("ivzhuk7@gmail.com")))
										.Style(FAppStyle::Get(), "Common.GotoNativeCodeHyperlink")
										.OnNavigate_Lambda([]()
										{
											 FPlatformProcess::LaunchURL(TEXT("ivzhuk7@gmail.com"), nullptr, nullptr);
										})
									]
								]
							]
						]
						+ SVerticalBox::Slot()
						.FillHeight(1)
					]
				];
			FSlateApplication::Get().AddWindow(AboutWindow, true);
		})
	);
}

FText FUnrealDriveEditorModule::GetExtensionName()
{
	return LOCTEXT("ExtensionName", "UnrealDrive");
}

FText FUnrealDriveEditorModule::GetToolSectionName()
{
	return LOCTEXT("SectionName", "UnrealDrive");
}

void FUnrealDriveEditorModule::GetExtensionTools(const FExtensionToolQueryInfo& QueryInfo, TArray<FExtensionToolDescription>& ToolsOut)
{
	FExtensionToolDescription RoadToMeshToolInfo;
	RoadToMeshToolInfo.ToolName = LOCTEXT("UnrealDriveToMeshTool", "Road To Mesh");
	RoadToMeshToolInfo.ToolCommand = FRoadEditorCommands::Get().BeginRoadToMeshTool;
	RoadToMeshToolInfo.ToolBuilder = NewObject<UTriangulateRoadToolBuilder>();
	ToolsOut.Add(RoadToMeshToolInfo);

	FExtensionToolDescription DrawNewRoadToolInfo;
	DrawNewRoadToolInfo.ToolName = LOCTEXT("UnrealDriveDrawRoadSpline", "Draw New Road");
	DrawNewRoadToolInfo.ToolCommand = FRoadEditorCommands::Get().BeginDrawNewRoad;
	DrawNewRoadToolInfo.ToolBuilder = NewObject<UDrawNewRoadToolBuilder>();
	ToolsOut.Add(DrawNewRoadToolInfo);

	FExtensionToolDescription DrawInnerRoadToolInfo;
	DrawInnerRoadToolInfo.ToolName = LOCTEXT("UnrealDriveDrawRoadSpline", "Draw New Inner Road");
	DrawInnerRoadToolInfo.ToolCommand = FRoadEditorCommands::Get().BeginDrawNewInnerRoad;
	DrawInnerRoadToolInfo.ToolBuilder = NewObject<UDrawInnerRoadToolBuilder>();
	ToolsOut.Add(DrawInnerRoadToolInfo);
}

bool FUnrealDriveEditorModule::GetExtensionExtendedInfo(FModelingModeExtensionExtendedInfo& InfoOut)
{
	InfoOut.ExtensionCommand = FRoadEditorCommands::Get().UnrealDriveToolsTabButton;
	InfoOut.ToolPaletteButtonTooltip = LOCTEXT("UnrealDriveExtensionTooltip", "Tools for working with UnrealDrive splines");
	return true;
}

bool FUnrealDriveEditorModule::GetExtensionToolTargets(TArray<TSubclassOf<UToolTargetFactory>>& ToolTargetFactoriesOut)
{
	return false;
}

bool FUnrealDriveEditorModule::RegisterRoadLaneAttribute(FName EntryName, const TInstancedStruct<FRoadLaneAttributeEntry>& RoadLaneAttributeEntry)
{
	if (EntryName.IsNone())
	{
		UE_LOG(LogUnrealDrive, Error, TEXT("FUnrealDriveEditorModule::RegisterRoadLaneAttribute(%s); EntryName is not allowed"), *EntryName.ToString());
		return false;
	}
	if (RoadLaneAttributeEntry.GetPtr<FRoadLaneAttributeEntry>() == nullptr)
	{
		UE_LOG(LogUnrealDrive, Error, TEXT("FUnrealDriveEditorModule::RegisterRoadLaneAttribute(%s); Entry is not child of FRoadLaneAttributeEntry"), *EntryName.ToString());
		return false;
	}

	if (RoadLaneAttributEntries.Find(EntryName))
	{
		UE_LOG(LogUnrealDrive, Error, TEXT("FUnrealDriveEditorModule::RegisterRoadLaneAttribute(%s); Entry is already register"), *EntryName.ToString());
		return false;
	}

	RoadLaneAttributEntries.Add(EntryName, RoadLaneAttributeEntry);
	return true;
}

void FUnrealDriveEditorModule::UnregisterRoadLaneAttribute(FName EntryName)
{
	RoadLaneAttributEntries.Remove(EntryName);
}

const TInstancedStruct<FRoadLaneAttributeEntry>* FUnrealDriveEditorModule::ForEachRoadLaneAttributEntries(TFunctionRef<bool(FName, const TInstancedStruct<FRoadLaneAttributeEntry>*)> RoadLaneAttributEntrie)
{
	TMap<FName, FName> UsedNames;

	for (const auto& [Key, Value] : RoadLaneAttributEntries)
	{
		if (RoadLaneAttributEntrie(Key, &Value))
		{
			return &Value;
		}
		UsedNames.Add(Key, NAME_None);
	}

	UAssetManager& AssetManager = UAssetManager::Get();
	//AssetManager.ScanPathForPrimaryAssets(FPrimaryAssetType("UnrealDrivePreset"), TEXT("/Game"), UUnrealDrivePreset::StaticClass(), false, false, true);
	TArray<FSoftObjectPath> AssetPathList;
	AssetManager.GetPrimaryAssetPathList(FPrimaryAssetType("UnrealDrivePreset"), AssetPathList);

	for (auto& Asset : AssetPathList)
	{
		if (UClass* AssetObjectClass = Cast<UClass>(Asset.TryLoad()))
		{
			auto* PrimaryAsset = CastChecked<UUnrealDrivePreset>(AssetObjectClass->GetDefaultObject());
			for (auto& [Key, Value] : PrimaryAsset->RoadAttributeEntries)
			{
				if (Value.GetScriptStruct() && Value.GetScriptStruct()->IsChildOf<FRoadLaneAttributeEntry>())
				{
					if (auto* Found = UsedNames.Find(Key))
					{
						UE_LOG(LogUnrealDrive, Error, TEXT("FUnrealDriveEditorModule::ForEachRoadLaneAttributEntries(); RoadEntryName \"%s\" is duplicated in \"%s\", previus defenition in \"%s\""),
							*Key.ToString(),
							*Asset.GetAssetPathString(),
							*Found == NAME_None ? TEXT("CPP") : *Found->ToString()
						);
					}
					else
					{
						if (RoadLaneAttributEntrie(Key, &Value))
						{
							return &Value;
						}
						UsedNames.Add(Key, Asset.GetAssetPath().ToFName());
					}
				}
			}
		}
	}
	return nullptr;
}




void FUnrealDriveEditorModule::RegisterRoadComputeFactory(FName FactoryName, FRoadComputeFactory&& Factory)
{
	RoadComputeFactories.Add(FactoryName, MoveTemp(Factory));
}

void FUnrealDriveEditorModule::UnregisterRoadComputeFactory(FName FactoryName)
{
	RoadComputeFactories.Remove(FactoryName);
}

template<class TOpFactory, class TProperties>
IRoadOpCompute* MakeRoadMeshOpComputeHelper(
	UTriangulateRoadTool* RoadTool,
	TWeakPtr<UnrealDrive::FRoadActorComputeScope> RoadComputeScope,
	const FString::ElementType* BaseAssetName,
	TSet<FName> RebuildTags
)
{
	auto* RoadCompute = NewObject<URoadMeshOpPreviewWithBackgroundCompute>(RoadTool);
	auto Properties = RoadTool->SetupPropertySet(TProperties::StaticClass());
	auto OpFactory = MakeUnique<TOpFactory>();
	OpFactory->RoadTool = RoadTool;
	OpFactory->RoadComputeScope = RoadComputeScope;
	OpFactory->Properties = Cast<TProperties>(Properties);
	RoadCompute->BaseAssetName = BaseAssetName;
	if (Cast<IInteractiveToolPropertyMaterialInterface>(Properties))
	{
		RoadCompute->MaterialGetter = TScriptInterface<IInteractiveToolPropertyMaterialInterface>(Properties);
	}
	RoadCompute->Setup(RoadTool, RoadComputeScope, OpFactory.Get());
	RoadCompute->RebuildTags = RebuildTags;
	RoadComputeScope.Pin()->OpFactories.Add(MoveTemp(OpFactory));
	return RoadCompute;
};

void FUnrealDriveEditorModule::RegisterRoadComputeFactories()
{
	RoadComputeFactories.Add("RoadSurface", FRoadComputeFactory::CreateStatic(MakeRoadMeshOpComputeHelper<FDriveSurfaceOperatorFactory, URoadSurfaceToolProperties>, TEXT("RoadSurface"), TSet<FName>{ "RebuilDriveSurface" }));
	RoadComputeFactories.Add("RoadDecals", FRoadComputeFactory::CreateStatic(MakeRoadMeshOpComputeHelper<FRoadDecalsOperatorFactory, URoadDecalToolProperties>, TEXT("RoadDecals"), TSet<FName>{ "RebuildDecales" }));
	RoadComputeFactories.Add("RoadSidewalks", FRoadComputeFactory::CreateStatic(MakeRoadMeshOpComputeHelper<FRoadSidewalksOperatorFactory, URoadSidewalkToolProperties>, TEXT("RoadSidewalks"), TSet<FName>{ "RebuildSidewalks" }));
	RoadComputeFactories.Add("RoadCurbs", FRoadComputeFactory::CreateStatic(MakeRoadMeshOpComputeHelper<FRoadCurbsOperatorFactory, URoadCertbToolProperties>, TEXT("RoadCurbs"), TSet<FName>{ "RebuildCurbs" }));
	RoadComputeFactories.Add("RoadMarks", FRoadComputeFactory::CreateStatic(MakeRoadMeshOpComputeHelper<FRoadMarksOperatorFactory, URoadMarkToolProperties>, TEXT("RoadMarks"), TSet<FName>{ "RebuildMarks" }));
	RoadComputeFactories.Add("RoadSplineMeshes", FRoadComputeFactory::CreateLambda([](UTriangulateRoadTool* RoadTool, TWeakPtr<UnrealDrive::FRoadActorComputeScope> RoadComputeScope)
	{
		USplineMeshOpPreviewWithBackgroundCompute* RoadCompute = NewObject<USplineMeshOpPreviewWithBackgroundCompute>(RoadTool);
		auto Properties = RoadTool->SetupPropertySet(URoadAttributesToolProperties::StaticClass());
		auto OpFactory = MakeUnique<FRoadSplineMeshOperatorFactory>();
		OpFactory->RoadTool = RoadTool;
		OpFactory->RoadComputeScope = RoadComputeScope;
		OpFactory->Properties = Cast<URoadAttributesToolProperties>(Properties);
		RoadCompute->Setup(RoadTool, RoadComputeScope, OpFactory.Get());
		RoadCompute->RebuildTags.Add("RebuilAttributes");
		RoadComputeScope.Pin()->OpFactories.Add(MoveTemp(OpFactory));
		return RoadCompute;
	}));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FUnrealDriveEditorModule, UnrealDriveEditor)