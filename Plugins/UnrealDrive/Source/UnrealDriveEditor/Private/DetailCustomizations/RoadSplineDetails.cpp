/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "RoadSplineDetails.h"

#include "BlueprintEditor.h"
#include "BlueprintEditorModule.h"
#include "ComponentVisualizer.h"
#include "ComponentVisualizerManager.h"
//#include "Components/SplineComponent.h"
#include "Containers/Array.h"
#include "Containers/BitArray.h"
#include "Containers/EnumAsByte.h"
#include "Containers/Set.h"
#include "Containers/SparseArray.h"
#include "Containers/UnrealString.h"
#include "Delegates/Delegate.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "Editor/UnrealEdEngine.h"
#include "Engine/Blueprint.h"
#include "Fonts/SlateFontInfo.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/Commands/UICommandInfo.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "GameFramework/Actor.h"
#include "HAL/PlatformApplicationMisc.h"
#include "HAL/PlatformCrt.h"
#include "HAL/PlatformMisc.h"
#include "IDetailChildrenBuilder.h"
#include "IDetailCustomNodeBuilder.h"
#include "Input/Reply.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Text.h"
#include "Layout/Clipping.h"
#include "Layout/Margin.h"
#include "Layout/Visibility.h"
#include "LevelEditorViewport.h"
#include "Logging/LogCategory.h"
#include "Logging/LogMacros.h"
#include "Math/Axis.h"
#include "Math/InterpCurve.h"
#include "Math/InterpCurvePoint.h"
#include "Math/Quat.h"
#include "Math/Rotator.h"
#include "Math/Transform.h"
#include "Math/UnrealMathSSE.h"
#include "Math/Vector.h"
#include "Math/VectorRegister.h"
#include "Misc/AssertionMacros.h"
#include "Misc/Attribute.h"
#include "Misc/MessageDialog.h"
#include "Misc/Optional.h"
#include "Modules/ModuleManager.h"
#include "PropertyHandle.h"
#include "ScopedTransaction.h"
#include "Serialization/Archive.h"
#include "SlotBase.h"
#include "SplineComponentVisualizer.h"
#include "SplineMetadataDetailsFactory.h"
#include "Styling/AppStyle.h"
#include "Styling/SlateColor.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Templates/Casts.h"
#include "Templates/TypeHash.h"
#include "Templates/UnrealTemplate.h"
#include "Textures/SlateIcon.h"
#include "Trace/Detail/Channel.h"
#include "Types/SlateEnums.h"
#include "UObject/Class.h"
#include "UObject/NameTypes.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ReflectedTypeAccessors.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealNames.h"
#include "UObject/UnrealType.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "UnrealEdGlobals.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Input/SRotatorInputBox.h"
#include "Widgets/Input/SVectorInputBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"

#include "UnrealDriveEditorModule.h"
#include "RoadSplineComponent.h"

class FObjectInitializer;
class IDetailGroup;
class SWidget;

#define LOCTEXT_NAMESPACE "RoadSplineDetails"
DEFINE_LOG_CATEGORY_STATIC(LogRoadSplineDetails, Log, All)

bool FRoadSplineDetails::bAlreadyWarnedInvalidIndex = false;

FRoadSplineDetails::FRoadSplineDetails(URoadSplineComponent* InOwningSplineComponent)
	: SplineComp(nullptr)
{
	TSharedPtr<FComponentVisualizer> Visualizer = GUnrealEd->FindComponentVisualizer(InOwningSplineComponent->GetClass());
	SplineVisualizer = StaticCastSharedPtr<FRoadSplineComponentVisualizer>(FUnrealDriveEditorModule::Get().GetComponentVisualizer());
	check(SplineVisualizer.IsValid());
	check(SplineVisualizer->GetReferencerName() == TEXT("FRoadSplineComponentVisualizer"));

	SplineCurvesProperty = FindFProperty<FProperty>(URoadSplineComponent::StaticClass(), GET_MEMBER_NAME_CHECKED(URoadSplineComponent, SplineCurves));

	const TArray<ESplinePointType::Type> EnabledSplinePointTypes = InOwningSplineComponent->GetEnabledSplinePointTypes();

	UEnum* SplinePointTypeEnum = StaticEnum<ESplinePointType::Type>();
	check(SplinePointTypeEnum);
	for (int32 EnumIndex = 0; EnumIndex < SplinePointTypeEnum->NumEnums() - 1; ++EnumIndex)
	{
		const int32 Value = SplinePointTypeEnum->GetValueByIndex(EnumIndex);
		if (EnabledSplinePointTypes.Contains(Value))
		{
			SplinePointTypes.Add(MakeShareable(new FString(SplinePointTypeEnum->GetNameStringByIndex(EnumIndex))));
		}
	}

	check(InOwningSplineComponent);
	if (InOwningSplineComponent->IsTemplate())
	{
		// For blueprints, SplineComp will be set to the preview actor in UpdateValues().
		SplineComp = nullptr;
		SplineCompArchetype = InOwningSplineComponent;
	}
	else
	{
		SplineComp = InOwningSplineComponent;
		SplineCompArchetype = nullptr;
	}

	bAlreadyWarnedInvalidIndex = false;
}

void FRoadSplineDetails::SetOnRebuildChildren(FSimpleDelegate InOnRegenerateChildren)
{
	OnRegenerateChildren = InOnRegenerateChildren;
}

void FRoadSplineDetails::GenerateHeaderRowContent(FDetailWidgetRow& NodeRow)
{
}

void FRoadSplineDetails::GenerateSplinePointSelectionControls(IDetailChildrenBuilder& ChildrenBuilder)
{
	FMargin ButtonPadding(2.f, 0.f);

	ChildrenBuilder.AddCustomRow(LOCTEXT("SelectSplinePoints", "Select Spline Points"))
	.RowTag("SelectSplinePoints")
	.NameContent()
	[
		SNew(STextBlock)
		.Font(IDetailLayoutBuilder::GetDetailFont())
		.Text(LOCTEXT("SelectSplinePoints", "Select Spline Points"))
	]
	.ValueContent()
	.VAlign(VAlign_Fill)
	.MaxDesiredWidth(170.f)
	.MinDesiredWidth(170.f)
	[
		SNew(SHorizontalBox)
		.Clipping(EWidgetClipping::ClipToBounds)

		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.AutoWidth()
		.Padding(ButtonPadding)
		[
			SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "SplineComponentDetails.SelectFirst")
			.ContentPadding(2.0f)
			.ToolTipText(LOCTEXT("SelectFirstSplinePointToolTip", "Select first spline point."))
			.OnClicked(this, &FRoadSplineDetails::OnSelectFirstLastSplinePoint, true)
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(ButtonPadding)
		[
			SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "SplineComponentDetails.AddPrev")
			.ContentPadding(2.f)
			.ToolTipText(LOCTEXT("SelectAddPrevSplinePointToolTip", "Add previous spline point to current selection."))
			.OnClicked(this, &FRoadSplineDetails::OnSelectPrevNextSplinePoint, false, true)
			.IsEnabled(this, &FRoadSplineDetails::ArePointsSelected)
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(ButtonPadding)
		[
			SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "SplineComponentDetails.SelectPrev")
			.ContentPadding(2.f)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			.ToolTipText(LOCTEXT("SelectPrevSplinePointToolTip", "Select previous spline point."))
			.OnClicked(this, &FRoadSplineDetails::OnSelectPrevNextSplinePoint, false, false)
			.IsEnabled(this, &FRoadSplineDetails::ArePointsSelected)
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(ButtonPadding)
		[
			SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "SplineComponentDetails.SelectAll")
			.ContentPadding(2.f)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			.ToolTipText(LOCTEXT("SelectAllSplinePointToolTip", "Select all spline points."))
			.OnClicked(this, &FRoadSplineDetails::OnSelectAllSplinePoints)
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(ButtonPadding)
		[
			SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "SplineComponentDetails.SelectNext")
			.ContentPadding(2.f)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			.ToolTipText(LOCTEXT("SelectNextSplinePointToolTip", "Select next spline point."))
			.OnClicked(this, &FRoadSplineDetails::OnSelectPrevNextSplinePoint, true, false)
			.IsEnabled(this, &FRoadSplineDetails::ArePointsSelected)
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(ButtonPadding)
		[
			SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "SplineComponentDetails.AddNext")
			.ContentPadding(2.f)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			.ToolTipText(LOCTEXT("SelectAddNextSplinePointToolTip", "Add next spline point to current selection."))
			.OnClicked(this, &FRoadSplineDetails::OnSelectPrevNextSplinePoint, true, true)
			.IsEnabled(this, &FRoadSplineDetails::ArePointsSelected)
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(ButtonPadding)
		[
			SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "SplineComponentDetails.SelectLast")
			.ContentPadding(2.f)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			.ToolTipText(LOCTEXT("SelectLastSplinePointToolTip", "Select last spline point."))
			.OnClicked(this, &FRoadSplineDetails::OnSelectFirstLastSplinePoint, false)
		]
	];
}

void FRoadSplineDetails::GenerateChildContent(IDetailChildrenBuilder& ChildrenBuilder)
{
	// Select spline point buttons
	GenerateSplinePointSelectionControls(ChildrenBuilder);

	// Message which is shown when no points are selected
	ChildrenBuilder.AddCustomRow(LOCTEXT("NoneSelected", "None selected"))
		.RowTag(TEXT("NoneSelected"))
		.Visibility(TAttribute<EVisibility>(this, &FRoadSplineDetails::IsDisabled))
		[
			SNew(SBox)
			.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("NoPointsSelected", "No spline points are selected."))
		.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		];

	if (!SplineComp)
	{
		return;
	}

	// Input key
	ChildrenBuilder.AddCustomRow(LOCTEXT("InputKey", "Input Key"))
		.RowTag(TEXT("InputKey"))
		.Visibility(TAttribute<EVisibility>(this, &FRoadSplineDetails::IsEnabled))
		.NameContent()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("InputKey", "Input Key"))
		.Font(IDetailLayoutBuilder::GetDetailFont())
		]
	.ValueContent()
		.MinDesiredWidth(125.0f)
		.MaxDesiredWidth(125.0f)
		[
			SNew(SNumericEntryBox<float>)
			.IsEnabled(TAttribute<bool>(this, &FRoadSplineDetails::IsOnePointSelected))
			.Value(this, &FRoadSplineDetails::GetInputKey)
			.UndeterminedString(LOCTEXT("Multiple", "Multiple"))
			.OnValueCommitted(this, &FRoadSplineDetails::OnSetInputKey)
			.Font(IDetailLayoutBuilder::GetDetailFont())
		];

	IDetailCategoryBuilder& ParentCategory = ChildrenBuilder.GetParentCategory();
	TSharedPtr<FOnPasteFromText> PasteFromTextDelegate = ParentCategory.OnPasteFromText();
	const bool bUsePasteFromText = PasteFromTextDelegate.IsValid();	

	// Position
	if (SplineComp->AllowsSpinePointLocationEditing())
	{
		PasteFromTextDelegate->AddSP(this, &FRoadSplineDetails::OnPasteFromText, ESplinePointProperty::Location);
		
		ChildrenBuilder.AddCustomRow(LOCTEXT("Location", "Location"))
			.RowTag(TEXT("Location"))
			.CopyAction(CreateCopyAction(ESplinePointProperty::Location))
			.PasteAction(CreatePasteAction(ESplinePointProperty::Location))
			.Visibility(TAttribute<EVisibility>(this, &FRoadSplineDetails::IsEnabled))
			.NameContent()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			[
				BuildSplinePointPropertyLabel(ESplinePointProperty::Location)
			]
		.ValueContent()
			.MinDesiredWidth(375.0f)
			.MaxDesiredWidth(375.0f)
			[
				SNew(SVectorInputBox)
				.X(this, &FRoadSplineDetails::GetPositionX)
				.Y(this, &FRoadSplineDetails::GetPositionY)
				.Z(this, &FRoadSplineDetails::GetPositionZ)
				.AllowSpin(true)
				.bColorAxisLabels(true)
				.SpinDelta(1.f)
				.OnXChanged(this, &FRoadSplineDetails::OnSetPosition, ETextCommit::Default, EAxis::X)
				.OnYChanged(this, &FRoadSplineDetails::OnSetPosition, ETextCommit::Default, EAxis::Y)
				.OnZChanged(this, &FRoadSplineDetails::OnSetPosition, ETextCommit::Default, EAxis::Z)
				.OnXCommitted(this, &FRoadSplineDetails::OnSetPosition, EAxis::X)
				.OnYCommitted(this, &FRoadSplineDetails::OnSetPosition, EAxis::Y)
				.OnZCommitted(this, &FRoadSplineDetails::OnSetPosition, EAxis::Z)
				.OnBeginSliderMovement(this, &FRoadSplineDetails::OnBeginPositionSlider)
				.OnEndSliderMovement(this, &FRoadSplineDetails::OnEndSlider)
				.Font(IDetailLayoutBuilder::GetDetailFont())
			];
	}

	// Rotation
	if (SplineComp->AllowsSplinePointRotationEditing())
	{
		PasteFromTextDelegate->AddSP(this, &FRoadSplineDetails::OnPasteFromText, ESplinePointProperty::Rotation);	
		
		ChildrenBuilder.AddCustomRow(LOCTEXT("Rotation", "Rotation"))
			.RowTag(TEXT("Rotation"))
			.CopyAction(CreateCopyAction(ESplinePointProperty::Rotation))
			.PasteAction(CreatePasteAction(ESplinePointProperty::Rotation))
			.Visibility(TAttribute<EVisibility>(this, &FRoadSplineDetails::IsEnabled))
			.NameContent()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			[
				BuildSplinePointPropertyLabel(ESplinePointProperty::Rotation)
			]
		.ValueContent()
			.MinDesiredWidth(375.0f)
			.MaxDesiredWidth(375.0f)
			[
				SNew(SRotatorInputBox)
				.Roll(this, &FRoadSplineDetails::GetRotationRoll)
				.Pitch(this, &FRoadSplineDetails::GetRotationPitch)
				.Yaw(this, &FRoadSplineDetails::GetRotationYaw)
				.AllowSpin(false)
				.bColorAxisLabels(false)
				.OnRollCommitted(this, &FRoadSplineDetails::OnSetRotation, EAxis::X)
				.OnPitchCommitted(this, &FRoadSplineDetails::OnSetRotation, EAxis::Y)
				.OnYawCommitted(this, &FRoadSplineDetails::OnSetRotation, EAxis::Z)
				.Font(IDetailLayoutBuilder::GetDetailFont())
			];
	}

	// Scale
	if (SplineComp->AllowsSplinePointScaleEditing())
	{
		PasteFromTextDelegate->AddSP(this, &FRoadSplineDetails::OnPasteFromText, ESplinePointProperty::Scale);
		
		ChildrenBuilder.AddCustomRow(LOCTEXT("Scale", "Scale"))
			.RowTag(TEXT("Scale"))
			.Visibility(TAttribute<EVisibility>(this, &FRoadSplineDetails::IsEnabled))
			.CopyAction(CreateCopyAction(ESplinePointProperty::Scale))
			.PasteAction(CreatePasteAction(ESplinePointProperty::Scale))
			.NameContent()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("ScaleLabel", "Scale"))
			.Font(IDetailLayoutBuilder::GetDetailFont())
			]
		.ValueContent()
			.MinDesiredWidth(375.0f)
			.MaxDesiredWidth(375.0f)
			[
				SNew(SVectorInputBox)
				.X(this, &FRoadSplineDetails::GetScaleX)
			.Y(this, &FRoadSplineDetails::GetScaleY)
			.Z(this, &FRoadSplineDetails::GetScaleZ)
			.AllowSpin(true)
			.bColorAxisLabels(true)
			.OnXChanged(this, &FRoadSplineDetails::OnSetScale, ETextCommit::Default, EAxis::X)
			.OnYChanged(this, &FRoadSplineDetails::OnSetScale, ETextCommit::Default, EAxis::Y)
			.OnZChanged(this, &FRoadSplineDetails::OnSetScale, ETextCommit::Default, EAxis::Z)
			.OnXCommitted(this, &FRoadSplineDetails::OnSetScale, EAxis::X)
			.OnYCommitted(this, &FRoadSplineDetails::OnSetScale, EAxis::Y)
			.OnZCommitted(this, &FRoadSplineDetails::OnSetScale, EAxis::Z)
			.OnBeginSliderMovement(this, &FRoadSplineDetails::OnBeginScaleSlider)
			.OnEndSliderMovement(this, &FRoadSplineDetails::OnEndSlider)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			];
	}

	// ArriveTangent
	if (SplineComp->AllowsSplinePointArriveTangentEditing())
	{
		PasteFromTextDelegate->AddSP(this, &FRoadSplineDetails::OnPasteFromText, ESplinePointProperty::ArriveTangent);
	
		ChildrenBuilder.AddCustomRow(LOCTEXT("ArriveTangent", "Arrive Tangent"))
			.RowTag(TEXT("ArriveTangent"))
			.Visibility(TAttribute<EVisibility>(this, &FRoadSplineDetails::IsEnabled))
			.CopyAction(CreateCopyAction(ESplinePointProperty::ArriveTangent))
			.PasteAction(CreatePasteAction(ESplinePointProperty::ArriveTangent))
			.NameContent()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("ArriveTangent", "Arrive Tangent"))
				.Font(IDetailLayoutBuilder::GetDetailFont())
			]
		.ValueContent()
			.MinDesiredWidth(375.0f)
			.MaxDesiredWidth(375.0f)
			[
				SNew(SVectorInputBox)
				.X(this, &FRoadSplineDetails::GetArriveTangentX)
				.Y(this, &FRoadSplineDetails::GetArriveTangentY)
				.Z(this, &FRoadSplineDetails::GetArriveTangentZ)
				.AllowSpin(false)
				.bColorAxisLabels(false)
				.OnXCommitted(this, &FRoadSplineDetails::OnSetArriveTangent, EAxis::X)
				.OnYCommitted(this, &FRoadSplineDetails::OnSetArriveTangent, EAxis::Y)
				.OnZCommitted(this, &FRoadSplineDetails::OnSetArriveTangent, EAxis::Z)
				.Font(IDetailLayoutBuilder::GetDetailFont())
			];
	}

	// LeaveTangent
	if (SplineComp->AllowsSplinePointLeaveTangentEditing())
	{
		PasteFromTextDelegate->AddSP(this, &FRoadSplineDetails::OnPasteFromText, ESplinePointProperty::LeaveTangent);
	
		ChildrenBuilder.AddCustomRow(LOCTEXT("LeaveTangent", "Leave Tangent"))
			.RowTag(TEXT("LeaveTangent"))
			.Visibility(TAttribute<EVisibility>(this, &FRoadSplineDetails::IsEnabled))
			.CopyAction(CreateCopyAction(ESplinePointProperty::LeaveTangent))
			.PasteAction(CreatePasteAction(ESplinePointProperty::LeaveTangent))
			.NameContent()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("LeaveTangent", "Leave Tangent"))
				.Font(IDetailLayoutBuilder::GetDetailFont())
			]
		.ValueContent()
			.MinDesiredWidth(375.0f)
			.MaxDesiredWidth(375.0f)
			[
				SNew(SVectorInputBox)
				.X(this, &FRoadSplineDetails::GetLeaveTangentX)
				.Y(this, &FRoadSplineDetails::GetLeaveTangentY)
				.Z(this, &FRoadSplineDetails::GetLeaveTangentZ)
				.AllowSpin(false)
				.bColorAxisLabels(false)
				.OnXCommitted(this, &FRoadSplineDetails::OnSetLeaveTangent, EAxis::X)
				.OnYCommitted(this, &FRoadSplineDetails::OnSetLeaveTangent, EAxis::Y)
				.OnZCommitted(this, &FRoadSplineDetails::OnSetLeaveTangent, EAxis::Z)
				.Font(IDetailLayoutBuilder::GetDetailFont())
			];
	}

	// Type
	if (SplineComp->GetEnabledSplinePointTypes().Num() > 1)
	{
		ChildrenBuilder.AddCustomRow(LOCTEXT("Type", "Type"))
			.RowTag(TEXT("Type"))
			.Visibility(TAttribute<EVisibility>(this, &FRoadSplineDetails::IsEnabled))
			.NameContent()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Type", "Type"))
			.Font(IDetailLayoutBuilder::GetDetailFont())
			]
		.ValueContent()
			.MinDesiredWidth(125.0f)
			.MaxDesiredWidth(125.0f)
			[
				SNew(SComboBox<TSharedPtr<FString>>)
				.OptionsSource(&SplinePointTypes)
				.OnGenerateWidget(this, &FRoadSplineDetails::OnGenerateComboWidget)
				.OnSelectionChanged(this, &FRoadSplineDetails::OnSplinePointTypeChanged)
				[
					SNew(STextBlock)
					.Font(IDetailLayoutBuilder::GetDetailFont())
					.Text(this, &FRoadSplineDetails::GetPointType)
				]
			];
	}

	if (SplineVisualizer.IsValid() && SplineVisualizer->GetSelectedKeys().Num() > 0)
	{
		for (TObjectIterator<UClass> ClassIterator; ClassIterator; ++ClassIterator)
		{
			if (ClassIterator->IsChildOf(USplineMetadataDetailsFactoryBase::StaticClass()) && !ClassIterator->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
			{
				USplineMetadataDetailsFactoryBase* Factory = ClassIterator->GetDefaultObject<USplineMetadataDetailsFactoryBase>();
				const USplineMetadata* SplineMetadata = SplineComp->GetSplinePointsMetadata();
				if (SplineMetadata && SplineMetadata->GetClass() == Factory->GetMetadataClass())
				{
					SplineMetaDataDetails = Factory->Create();
					IDetailGroup& Group = ChildrenBuilder.AddGroup(SplineMetaDataDetails->GetName(), SplineMetaDataDetails->GetDisplayName());
					SplineMetaDataDetails->GenerateChildContent(Group);
					break;
				}
			}
		}
	}
}

void FRoadSplineDetails::Tick(float DeltaTime)
{
	UpdateValues();
}

void FRoadSplineDetails::UpdateValues()
{
	// If this is a blueprint spline, always update the spline component based on 
	// the spline component visualizer's currently edited spline component.
	if (SplineCompArchetype)
	{
		URoadSplineComponent* EditedSplineComp = SplineVisualizer.IsValid() ? SplineVisualizer->GetEditedSplineComponent() : nullptr;

		if (!EditedSplineComp || (EditedSplineComp->GetArchetype() != SplineCompArchetype))
		{
			return;
		}

		SplineComp = EditedSplineComp;
	}

	if (!SplineComp || !SplineVisualizer.IsValid())
	{
		return;
	}

	bool bNeedsRebuild = false;
	const TSet<int32>& NewSelectedKeys = SplineVisualizer->GetSelectedKeys();

	if (NewSelectedKeys.Num() != SelectedKeys.Num())
	{
		bNeedsRebuild = true;
	}
	SelectedKeys = NewSelectedKeys;

	// Cache values to be shown by the details customization.
	// An unset optional value represents 'multiple values' (in the case where multiple points are selected).
	InputKey.Reset();
	Position.Reset();
	ArriveTangent.Reset();
	LeaveTangent.Reset();
	Rotation.Reset();
	Scale.Reset();
	PointType.Reset();

	// Only display point details when there are selected keys
	if (SelectedKeys.Num() > 0)
	{
		bool bValidIndices = true;
		for (int32 Index : SelectedKeys)
		{
			if (Index < 0 ||
				Index >= SplineComp->GetSplinePointsPosition().Points.Num() ||
				Index >= SplineComp->GetSplinePointsRotation().Points.Num() ||
				Index >= SplineComp->GetSplinePointsScale().Points.Num())
			{
				bValidIndices = false;
				if (!bAlreadyWarnedInvalidIndex)
				{
					UE_LOG(LogRoadSplineDetails, Error, TEXT("Spline component details selected keys contains invalid index %d for spline %s with %d points, %d rotations, %d scales"),
						Index,
						*SplineComp->GetPathName(),
						SplineComp->GetSplinePointsPosition().Points.Num(),
						SplineComp->GetSplinePointsRotation().Points.Num(),
						SplineComp->GetSplinePointsScale().Points.Num());
					bAlreadyWarnedInvalidIndex = true;
				}
				break;
			}
		}

		if (bValidIndices)
		{
			for (int32 Index : SelectedKeys)
			{
				const FTransform SplineToWorld = SplineComp->GetComponentToWorld();

				if (bEditingLocationAbsolute)
				{
					const FVector AbsoluteLocation = SplineToWorld.TransformPosition(SplineComp->GetSplinePointsPosition().Points[Index].OutVal);
					Position.Add(AbsoluteLocation);
				}
				else
				{
					Position.Add(SplineComp->GetSplinePointsPosition().Points[Index].OutVal);
				}

				if (bEditingRotationAbsolute)
				{
					const FQuat AbsoluteRotation = SplineToWorld.TransformRotation(SplineComp->GetSplinePointsRotation().Points[Index].OutVal);
					Rotation.Add(AbsoluteRotation.Rotator());
				}
				else
				{
					Rotation.Add(SplineComp->GetSplinePointsRotation().Points[Index].OutVal.Rotator());
				}

				InputKey.Add(SplineComp->GetSplinePointsPosition().Points[Index].InVal);
				Scale.Add(SplineComp->GetSplinePointsScale().Points[Index].OutVal);
				ArriveTangent.Add(SplineComp->GetSplinePointsPosition().Points[Index].ArriveTangent);
				LeaveTangent.Add(SplineComp->GetSplinePointsPosition().Points[Index].LeaveTangent);
				PointType.Add(ConvertInterpCurveModeToSplinePointType(SplineComp->GetSplinePointsPosition().Points[Index].InterpMode));
			}

			if (SplineMetaDataDetails)
			{
				SplineMetaDataDetails->Update(SplineComp, SelectedKeys);
			}
		}
	}

	if (bNeedsRebuild)
	{
		OnRegenerateChildren.ExecuteIfBound();
	}
}

FName FRoadSplineDetails::GetName() const
{
	static const FName Name("SplinePointDetails");
	return Name;
}

void FRoadSplineDetails::OnSetInputKey(float NewValue, ETextCommit::Type CommitInfo)
{
	if ((CommitInfo != ETextCommit::OnEnter && CommitInfo != ETextCommit::OnUserMovedFocus) || !SplineComp)
	{
		return;
	}

	check(SelectedKeys.Num() == 1);
	const int32 Index = *SelectedKeys.CreateConstIterator();
	TArray<FInterpCurvePoint<FVector>>& Positions = SplineComp->GetSplinePointsPosition().Points;

	const int32 NumPoints = Positions.Num();

	bool bModifyOtherPoints = false;
	if ((Index > 0 && NewValue <= Positions[Index - 1].InVal) ||
		(Index < NumPoints - 1 && NewValue >= Positions[Index + 1].InVal))
	{
		const FText Title(LOCTEXT("InputKeyTitle", "Input key out of range"));
		const FText Message(LOCTEXT("InputKeyMessage", "Spline input keys must be numerically ascending. Would you like to modify other input keys in the spline in order to be able to set this value?"));

		// Ensure input keys remain ascending
		if (FMessageDialog::Open(EAppMsgType::YesNo, Message, Title) == EAppReturnType::No)
		{
			return;
		}

		bModifyOtherPoints = true;
	}

	// Scope the transaction to only include the value change and none of the derived data changes that might arise from NotifyPropertyModified
	{
		const FScopedTransaction Transaction(LOCTEXT("SetSplinePointInputKey", "Set spline point input key"));
		SplineComp->Modify();

		TArray<FInterpCurvePoint<FQuat>>& Rotations = SplineComp->GetSplinePointsRotation().Points;
		TArray<FInterpCurvePoint<FVector>>& Scales = SplineComp->GetSplinePointsScale().Points;

		if (bModifyOtherPoints)
		{
			// Shuffle the previous or next input keys down or up so the input value remains in sequence
			if (Index > 0 && NewValue <= Positions[Index - 1].InVal)
			{
				float Delta = (NewValue - Positions[Index].InVal);
				for (int32 PrevIndex = 0; PrevIndex < Index; PrevIndex++)
				{
					Positions[PrevIndex].InVal += Delta;
					Rotations[PrevIndex].InVal += Delta;
					Scales[PrevIndex].InVal += Delta;
				}
			}
			else if (Index < NumPoints - 1 && NewValue >= Positions[Index + 1].InVal)
			{
				float Delta = (NewValue - Positions[Index].InVal);
				for (int32 NextIndex = Index + 1; NextIndex < NumPoints; NextIndex++)
				{
					Positions[NextIndex].InVal += Delta;
					Rotations[NextIndex].InVal += Delta;
					Scales[NextIndex].InVal += Delta;
				}
			}
		}

		Positions[Index].InVal = NewValue;
		Rotations[Index].InVal = NewValue;
		Scales[Index].InVal = NewValue;
	}

	SplineComp->UpdateSpline();
	SplineComp->bSplineHasBeenEdited = true;
	FComponentVisualizer::NotifyPropertyModified(SplineComp, SplineCurvesProperty);
	if (AActor* Owner = SplineComp->GetOwner())
	{
		Owner->PostEditMove(true);
	}
	UpdateValues();

	GEditor->RedrawLevelEditingViewports(true);
}

void FRoadSplineDetails::OnSetPosition(float NewValue, ETextCommit::Type CommitInfo, EAxis::Type Axis)
{
	if (!SplineComp)
	{
		return;
	}

	// Scope the transaction to only include the value change and none of the derived data changes that might arise from NotifyPropertyModified
	{
		const FScopedTransaction Transaction(LOCTEXT("SetSplinePointPosition", "Set spline point position"), !bInSliderTransaction);
		SplineComp->Modify();

		for (int32 Index : SelectedKeys)
		{
			if (Index < 0 || Index >= SplineComp->GetSplinePointsPosition().Points.Num())
			{
				UE_LOG(LogRoadSplineDetails, Error, TEXT("Set spline point location: invalid index %d in selected points for spline component %s which contains %d spline points."),
					Index, *SplineComp->GetPathName(), SplineComp->GetSplinePointsPosition().Points.Num());
				continue;
			}

			if (bEditingLocationAbsolute)
			{
				const FTransform SplineToWorld = SplineComp->GetComponentToWorld();
				const FVector RelativePos = SplineComp->GetSplinePointsPosition().Points[Index].OutVal;
				FVector AbsolutePos = SplineToWorld.TransformPosition(RelativePos);
				AbsolutePos.SetComponentForAxis(Axis, NewValue);
				FVector PointPosition = SplineToWorld.InverseTransformPosition(AbsolutePos);

				SplineComp->GetSplinePointsPosition().Points[Index].OutVal = PointPosition;
			}
			else
			{
				FVector PointPosition = SplineComp->GetSplinePointsPosition().Points[Index].OutVal;
				PointPosition.SetComponentForAxis(Axis, NewValue);
				SplineComp->GetSplinePointsPosition().Points[Index].OutVal = PointPosition;
			}
		}
	}

	if (CommitInfo == ETextCommit::OnEnter || CommitInfo == ETextCommit::OnUserMovedFocus)
	{
		SplineComp->UpdateSpline();
		SplineComp->bSplineHasBeenEdited = true;
		FComponentVisualizer::NotifyPropertyModified(SplineComp, SplineCurvesProperty, EPropertyChangeType::ValueSet);
		if (AActor* Owner = SplineComp->GetOwner())
		{
			Owner->PostEditMove(true);
		}
		UpdateValues();
	}

	GEditor->RedrawLevelEditingViewports(true);
}

void FRoadSplineDetails::OnSetArriveTangent(float NewValue, ETextCommit::Type CommitInfo, EAxis::Type Axis)
{
	if (!SplineComp)
	{
		return;
	}

	// Scope the transaction to only include the value change and none of the derived data changes that might arise from NotifyPropertyModified
	{
		const FScopedTransaction Transaction(LOCTEXT("SetSplinePointTangent", "Set spline point tangent"));
		SplineComp->Modify();

		for (int32 Index : SelectedKeys)
		{
			if (Index < 0 || Index >= SplineComp->GetSplinePointsPosition().Points.Num())
			{
				UE_LOG(LogRoadSplineDetails, Error, TEXT("Set spline point arrive tangent: invalid index %d in selected points for spline component %s which contains %d spline points."),
					Index, *SplineComp->GetPathName(), SplineComp->GetSplinePointsPosition().Points.Num());
				continue;
			}

			FVector PointTangent = SplineComp->GetSplinePointsPosition().Points[Index].ArriveTangent;
			PointTangent.SetComponentForAxis(Axis, NewValue);
			SplineComp->GetSplinePointsPosition().Points[Index].ArriveTangent = PointTangent;
			SplineComp->GetSplinePointsPosition().Points[Index].InterpMode = CIM_CurveUser;
		}
	}

	if (CommitInfo == ETextCommit::OnEnter || CommitInfo == ETextCommit::OnUserMovedFocus)
	{
		SplineComp->UpdateSpline();
		SplineComp->bSplineHasBeenEdited = true;
		FComponentVisualizer::NotifyPropertyModified(SplineComp, SplineCurvesProperty, EPropertyChangeType::ValueSet);
		if (AActor* Owner = SplineComp->GetOwner())
		{
			Owner->PostEditMove(true);
		}
		UpdateValues();
	}

	GEditor->RedrawLevelEditingViewports(true);
}

void FRoadSplineDetails::OnSetLeaveTangent(float NewValue, ETextCommit::Type CommitInfo, EAxis::Type Axis)
{
	if (!SplineComp)
	{
		return;
	}

	// Scope the transaction to only include the value change and none of the derived data changes that might arise from NotifyPropertyModified
	{
		const FScopedTransaction Transaction(LOCTEXT("SetSplinePointTangent", "Set spline point tangent"));
		SplineComp->Modify();

		for (int32 Index : SelectedKeys)
		{
			if (Index < 0 || Index >= SplineComp->GetSplinePointsPosition().Points.Num())
			{
				UE_LOG(LogRoadSplineDetails, Error, TEXT("Set spline point leave tangent: invalid index %d in selected points for spline component %s which contains %d spline points."),
					Index, *SplineComp->GetPathName(), SplineComp->GetSplinePointsPosition().Points.Num());
				continue;
			}

			FVector PointTangent = SplineComp->GetSplinePointsPosition().Points[Index].LeaveTangent;
			PointTangent.SetComponentForAxis(Axis, NewValue);
			SplineComp->GetSplinePointsPosition().Points[Index].LeaveTangent = PointTangent;
			SplineComp->GetSplinePointsPosition().Points[Index].InterpMode = CIM_CurveUser;
		}
	}

	if (CommitInfo == ETextCommit::OnEnter || CommitInfo == ETextCommit::OnUserMovedFocus)
	{
		SplineComp->UpdateSpline();
		SplineComp->bSplineHasBeenEdited = true;
		FComponentVisualizer::NotifyPropertyModified(SplineComp, SplineCurvesProperty, EPropertyChangeType::ValueSet);
		if (AActor* Owner = SplineComp->GetOwner())
		{
			Owner->PostEditMove(true);
		}
		UpdateValues();
	}

	GEditor->RedrawLevelEditingViewports(true);
}

void FRoadSplineDetails::OnSetRotation(float NewValue, ETextCommit::Type CommitInfo, EAxis::Type Axis)
{
	if (!SplineComp)
	{
		return;
	}
	
	FQuat NewRotationRelative;
	// Scope the transaction to only include the value change and none of the derived data changes that might arise from NotifyPropertyModified
	{
		const FScopedTransaction Transaction(LOCTEXT("SetSplinePointRotation", "Set spline point rotation"));
		SplineComp->Modify();
		FQuat SplineComponentRotation = SplineComp->GetComponentQuat();
		for (int32 Index : SelectedKeys)
		{
			if (Index < 0 || Index >= SplineComp->GetSplinePointsRotation().Points.Num())
			{
				UE_LOG(LogRoadSplineDetails, Error, TEXT("Set spline point rotation: invalid index %d in selected points for spline component %s which contains %d spline points."),
					Index, *SplineComp->GetPathName(), SplineComp->GetSplinePointsRotation().Points.Num());
				continue;
			}

			FInterpCurvePoint<FVector>& EditedPoint = SplineComp->GetSplinePointsPosition().Points[Index];
			FInterpCurvePoint<FQuat>& EditedRotPoint = SplineComp->GetSplinePointsRotation().Points[Index];
			const FQuat CurrentRotationRelative = EditedRotPoint.OutVal;

			if (bEditingRotationAbsolute)
			{
				FRotator AbsoluteRot = (SplineComponentRotation * CurrentRotationRelative).Rotator();

				switch (Axis)
				{
				case EAxis::X: AbsoluteRot.Roll = NewValue; break;
				case EAxis::Y: AbsoluteRot.Pitch = NewValue; break;
				case EAxis::Z: AbsoluteRot.Yaw = NewValue; break;
				}

				NewRotationRelative = SplineComponentRotation.Inverse() * AbsoluteRot.Quaternion();
			}
			else
			{
				FRotator NewRotationRotator(CurrentRotationRelative);

				switch (Axis)
				{
				case EAxis::X: NewRotationRotator.Roll = NewValue; break;
				case EAxis::Y: NewRotationRotator.Pitch = NewValue; break;
				case EAxis::Z: NewRotationRotator.Yaw = NewValue; break;
				}

				NewRotationRelative = NewRotationRotator.Quaternion();
			}

			SplineComp->GetSplinePointsRotation().Points[Index].OutVal = NewRotationRelative;

			FQuat DeltaRotate(NewRotationRelative * CurrentRotationRelative.Inverse());
			// Rotate tangent according to delta rotation
			FVector NewTangent = SplineComponentRotation.RotateVector(EditedPoint.LeaveTangent); // convert local-space tangent vector to world-space
			NewTangent = DeltaRotate.RotateVector(NewTangent); // apply world-space delta rotation to world-space tangent
			NewTangent = SplineComponentRotation.Inverse().RotateVector(NewTangent); // convert world-space tangent vector back into local-space
			EditedPoint.LeaveTangent = NewTangent;
			EditedPoint.ArriveTangent = NewTangent;
		}
	}

	SplineVisualizer->SetCachedRotation(NewRotationRelative);

	if (CommitInfo == ETextCommit::OnEnter || CommitInfo == ETextCommit::OnUserMovedFocus)
	{
		SplineComp->UpdateSpline();
		SplineComp->bSplineHasBeenEdited = true;
		FComponentVisualizer::NotifyPropertyModified(SplineComp, SplineCurvesProperty, EPropertyChangeType::ValueSet);
		if (AActor* Owner = SplineComp->GetOwner())
		{
			Owner->PostEditMove(true);
		}
		UpdateValues();
	}
	GEditor->RedrawLevelEditingViewports(true);
}

void FRoadSplineDetails::OnSetScale(float NewValue, ETextCommit::Type CommitInfo, EAxis::Type Axis)
{
	if (!SplineComp)
	{
		return;
	}

	// Scope the transaction to only include the value change and none of the derived data changes that might arise from NotifyPropertyModified
	{
		const FScopedTransaction Transaction(LOCTEXT("SetSplinePointScale", "Set spline point scale"));
		SplineComp->Modify();

		for (int32 Index : SelectedKeys)
		{
			if (Index < 0 || Index >= SplineComp->GetSplinePointsScale().Points.Num())
			{
				UE_LOG(LogRoadSplineDetails, Error, TEXT("Set spline point scale: invalid index %d in selected points for spline component %s which contains %d spline points."),
					Index, *SplineComp->GetPathName(), SplineComp->GetSplinePointsScale().Points.Num());
				continue;
			}

			FVector PointScale = SplineComp->GetSplinePointsScale().Points[Index].OutVal;
			PointScale.SetComponentForAxis(Axis, NewValue);
			SplineComp->GetSplinePointsScale().Points[Index].OutVal = PointScale;
		}
	}

	if (CommitInfo == ETextCommit::OnEnter || CommitInfo == ETextCommit::OnUserMovedFocus)
	{
		SplineComp->UpdateSpline();
		SplineComp->bSplineHasBeenEdited = true;
		FComponentVisualizer::NotifyPropertyModified(SplineComp, SplineCurvesProperty, EPropertyChangeType::ValueSet);
		if (AActor* Owner = SplineComp->GetOwner())
		{
			Owner->PostEditMove(true);
		}
		UpdateValues();
	}

	GEditor->RedrawLevelEditingViewports(true);
}

FText FRoadSplineDetails::GetPointType() const
{
	if (PointType.Value.IsSet())
	{
		const UEnum* SplinePointTypeEnum = StaticEnum<ESplinePointType::Type>();
		check(SplinePointTypeEnum);
		return SplinePointTypeEnum->GetDisplayNameTextByValue(PointType.Value.GetValue());
	}

	return LOCTEXT("MultipleTypes", "Multiple Types");
}

void FRoadSplineDetails::OnSplinePointTypeChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo)
{
	if (!SplineComp)
	{
		return;
	}

	// Scope the transaction to only include the value change and none of the derived data changes that might arise from NotifyPropertyModified
	{
		const FScopedTransaction Transaction(LOCTEXT("SetSplinePointType", "Set spline point type"));
		SplineComp->Modify();

		EInterpCurveMode Mode = CIM_Unknown;
		if (NewValue.IsValid() && SplinePointTypes.Contains(NewValue))
		{
			const UEnum* SplinePointTypeEnum = StaticEnum<ESplinePointType::Type>();
			check(SplinePointTypeEnum);
			const int64 SplinePointType = SplinePointTypeEnum->GetValueByNameString(*NewValue);

			Mode = ConvertSplinePointTypeToInterpCurveMode(static_cast<ESplinePointType::Type>(SplinePointType));
		}

		for (int32 Index : SelectedKeys)
		{
			if (Index < 0 || Index >= SplineComp->GetSplinePointsPosition().Points.Num())
			{
				UE_LOG(LogRoadSplineDetails, Error, TEXT("Set spline point type: invalid index %d in selected points for spline component %s which contains %d spline points."),
					Index, *SplineComp->GetPathName(), SplineComp->GetSplinePointsPosition().Points.Num());
				continue;
			}

			SplineComp->GetSplinePointsPosition().Points[Index].InterpMode = Mode;
		}
	}

	SplineComp->UpdateSpline();
	SplineComp->bSplineHasBeenEdited = true;
	FComponentVisualizer::NotifyPropertyModified(SplineComp, SplineCurvesProperty);
	if (AActor* Owner = SplineComp->GetOwner())
	{
		Owner->PostEditMove(true);
	}
	UpdateValues();

	GEditor->RedrawLevelEditingViewports(true);
}

URoadSplineComponent* FRoadSplineDetails::GetSplineComponentToVisualize() const
{
	if (SplineCompArchetype) 
	{
		check(SplineCompArchetype->IsTemplate());

		FBlueprintEditorModule& BlueprintEditorModule = FModuleManager::LoadModuleChecked<FBlueprintEditorModule>("Kismet");

		const UClass* BPClass;
		if (const AActor* OwningCDO = SplineCompArchetype->GetOwner())
		{
			// Native component template
			BPClass = OwningCDO->GetClass();
		}
		else
		{
			// Non-native component template
			BPClass = Cast<UClass>(SplineCompArchetype->GetOuter());
		}

		if (BPClass)
		{
			if (UBlueprint* Blueprint = UBlueprint::GetBlueprintFromClass(BPClass))
			{
				if (FBlueprintEditor* BlueprintEditor = StaticCast<FBlueprintEditor*>(GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->FindEditorForAsset(Blueprint, false)))
				{
					const AActor* PreviewActor = BlueprintEditor->GetPreviewActor();
					TArray<UObject*> Instances;
					SplineCompArchetype->GetArchetypeInstances(Instances);

					for (UObject* Instance : Instances)
					{
						URoadSplineComponent* SplineCompInstance = Cast<URoadSplineComponent>(Instance);
						if (SplineCompInstance->GetOwner() == PreviewActor)
						{
							return SplineCompInstance;
						}
					}
				}
			}
		}

		// If we failed to find an archetype instance, must return nullptr 
		// since component visualizer cannot visualize the archetype.
		return nullptr;
	}

	return SplineComp;
}

FReply FRoadSplineDetails::OnSelectFirstLastSplinePoint(bool bFirst)
{
	if (SplineVisualizer.IsValid())
	{
		bool bActivateComponentVis = false;

		if (!SplineComp)
		{
			SplineComp = GetSplineComponentToVisualize();
			bActivateComponentVis = true;
		}

		if (SplineComp)
		{
			if (SplineVisualizer->HandleSelectFirstLastSplinePoint(SplineComp, bFirst))
			{
				if (bActivateComponentVis)
				{
					TSharedPtr<FComponentVisualizer> Visualizer = StaticCastSharedPtr<FComponentVisualizer>(SplineVisualizer);
					GUnrealEd->ComponentVisManager.SetActiveComponentVis(GCurrentLevelEditingViewportClient, Visualizer);
				}
			}
		}
	}
	return FReply::Handled();
}

FReply FRoadSplineDetails::OnSelectPrevNextSplinePoint(bool bNext, bool bAddToSelection)
{
	if (SplineVisualizer.IsValid())
	{
		SplineVisualizer->OnSelectPrevNextSplinePoint(bNext, bAddToSelection);
	}
	return FReply::Handled();
}

FReply FRoadSplineDetails::OnSelectAllSplinePoints()
{
	if (SplineVisualizer.IsValid())
	{
		bool bActivateComponentVis = false;

		if (!SplineComp)
		{
			SplineComp = GetSplineComponentToVisualize();
			bActivateComponentVis = true;
		}

		if (SplineComp)
		{
			if (SplineVisualizer->HandleSelectAllSplinePoints(SplineComp))
			{
				if (bActivateComponentVis)
				{
					TSharedPtr<FComponentVisualizer> Visualizer = StaticCastSharedPtr<FComponentVisualizer>(SplineVisualizer);
					GUnrealEd->ComponentVisManager.SetActiveComponentVis(GCurrentLevelEditingViewportClient, Visualizer);
				}
			}
		}
	}
	return FReply::Handled();
}

TSharedRef<SWidget> FRoadSplineDetails::OnGenerateComboWidget(TSharedPtr<FString> InComboString)
{
	return SNew(STextBlock)
		.Text(FText::FromString(*InComboString))
		.Font(IDetailLayoutBuilder::GetDetailFont());
}

TSharedRef<SWidget> FRoadSplineDetails::BuildSplinePointPropertyLabel(ESplinePointProperty SplinePointProp)
{
	FText Label;
	switch (SplinePointProp)
	{
	case ESplinePointProperty::Rotation:
		Label = LOCTEXT("RotationLabel", "Rotation");
		break;
	case ESplinePointProperty::Location:
		Label = LOCTEXT("LocationLabel", "Location");
		break;
	default:
		return SNullWidget::NullWidget;
	}

	FMenuBuilder MenuBuilder(true, NULL, NULL);

	FUIAction SetRelativeLocationAction
	(
		FExecuteAction::CreateSP(this, &FRoadSplineDetails::OnSetTransformEditingAbsolute, SplinePointProp, false),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FRoadSplineDetails::IsTransformEditingRelative, SplinePointProp)
	);

	FUIAction SetWorldLocationAction
	(
		FExecuteAction::CreateSP(this, &FRoadSplineDetails::OnSetTransformEditingAbsolute, SplinePointProp, true),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FRoadSplineDetails::IsTransformEditingAbsolute, SplinePointProp)
	);

	MenuBuilder.BeginSection(TEXT("TransformType"), FText::Format(LOCTEXT("TransformType", "{0} Type"), Label));

	MenuBuilder.AddMenuEntry
	(
		FText::Format(LOCTEXT("RelativeLabel", "Relative"), Label),
		FText::Format(LOCTEXT("RelativeLabel_ToolTip", "{0} is relative to its parent"), Label),
		FSlateIcon(),
		SetRelativeLocationAction,
		NAME_None,
		EUserInterfaceActionType::RadioButton
	);

	MenuBuilder.AddMenuEntry
	(
		FText::Format(LOCTEXT("WorldLabel", "World"), Label),
		FText::Format(LOCTEXT("WorldLabel_ToolTip", "{0} is relative to the world"), Label),
		FSlateIcon(),
		SetWorldLocationAction,
		NAME_None,
		EUserInterfaceActionType::RadioButton
	);

	MenuBuilder.EndSection();


	return
		SNew(SComboButton)
		.ContentPadding(0)
		.ButtonStyle(FAppStyle::Get(), "NoBorder")
		.ForegroundColor(FSlateColor::UseForeground())
		.MenuContent()
		[
			MenuBuilder.MakeWidget()
		]
		.ButtonContent()
		[
			SNew(SBox)
			.Padding(FMargin(0.0f, 0.0f, 2.0f, 0.0f))
			[
				SNew(STextBlock)
				.Text(this, &FRoadSplineDetails::GetSplinePointPropertyText, SplinePointProp)
				.Font(IDetailLayoutBuilder::GetDetailFont())
			]
		];
}

void FRoadSplineDetails::OnSetTransformEditingAbsolute(ESplinePointProperty SplinePointProp, bool bIsAbsolute)
{
	if (SplinePointProp == ESplinePointProperty::Location)
	{
		bEditingLocationAbsolute = bIsAbsolute;
	}
	else if (SplinePointProp == ESplinePointProperty::Rotation)
	{
		bEditingRotationAbsolute = bIsAbsolute;
	}
	else
	{
		return;
	}

	UpdateValues();
}

bool FRoadSplineDetails::IsTransformEditingAbsolute(ESplinePointProperty SplinePointProp) const
{
	if (SplinePointProp == ESplinePointProperty::Location)
	{
		return bEditingLocationAbsolute;
	}
	else if (SplinePointProp == ESplinePointProperty::Rotation)
	{
		return bEditingRotationAbsolute;
	}

	return false;
}

bool FRoadSplineDetails::IsTransformEditingRelative(ESplinePointProperty SplinePointProp) const
{
	if (SplinePointProp == ESplinePointProperty::Location)
	{
		return !bEditingLocationAbsolute;
	}
	else if (SplinePointProp == ESplinePointProperty::Rotation)
	{
		return !bEditingRotationAbsolute;
	}

	return false;
}


FText FRoadSplineDetails::GetSplinePointPropertyText(ESplinePointProperty SplinePointProp) const
{
	if (SplinePointProp == ESplinePointProperty::Location)
	{
		return bEditingLocationAbsolute ? LOCTEXT("AbsoluteLocation", "Absolute Location") : LOCTEXT("Location", "Location");
	}
	else if (SplinePointProp == ESplinePointProperty::Rotation)
	{
		return bEditingRotationAbsolute ? LOCTEXT("AbsoluteRotation", "Absolute Rotation") : LOCTEXT("Rotation", "Rotation");
	}

	return FText::GetEmpty();
}

void FRoadSplineDetails::SetSplinePointProperty(ESplinePointProperty SplinePointProp, FVector NewValue, EAxisList::Type Axis, bool bCommitted)
{
	switch (SplinePointProp)
	{
	case ESplinePointProperty::Location:
		OnSetPosition(NewValue.X, ETextCommit::Default, EAxis::X);
		OnSetPosition(NewValue.Y, ETextCommit::Default, EAxis::Y);
		OnSetPosition(NewValue.Z, ETextCommit::OnEnter, EAxis::Z);
		break;
	case ESplinePointProperty::Rotation:
		OnSetRotation(NewValue.X, ETextCommit::Default, EAxis::X);
		OnSetRotation(NewValue.Y, ETextCommit::Default, EAxis::Y);
		OnSetRotation(NewValue.Z, ETextCommit::OnEnter, EAxis::Z);
		break;
	case ESplinePointProperty::Scale:
		OnSetScale(NewValue.X, ETextCommit::Default, EAxis::X);
		OnSetScale(NewValue.Y, ETextCommit::Default, EAxis::Y);
		OnSetScale(NewValue.Z, ETextCommit::OnEnter, EAxis::Z);
		break;
	case ESplinePointProperty::ArriveTangent:
		OnSetArriveTangent(NewValue.X, ETextCommit::Default, EAxis::X);
		OnSetArriveTangent(NewValue.Y, ETextCommit::Default, EAxis::Y);
		OnSetArriveTangent(NewValue.Z, ETextCommit::OnEnter, EAxis::Z);
		break;
	case ESplinePointProperty::LeaveTangent:
		OnSetLeaveTangent(NewValue.X, ETextCommit::Default, EAxis::X);
		OnSetLeaveTangent(NewValue.Y, ETextCommit::Default, EAxis::Y);
		OnSetLeaveTangent(NewValue.Z, ETextCommit::OnEnter, EAxis::Z);
		break;
	default:
		break;
	}
}

FUIAction FRoadSplineDetails::CreateCopyAction(ESplinePointProperty SplinePointProp)
{
	return
		FUIAction
		(
			FExecuteAction::CreateSP(this, &FRoadSplineDetails::OnCopy, SplinePointProp),
			FCanExecuteAction::CreateSP(this, &FRoadSplineDetails::OnCanCopy, SplinePointProp)
		);
}

FUIAction FRoadSplineDetails::CreatePasteAction(ESplinePointProperty SplinePointProp)
{
	return
		FUIAction(FExecuteAction::CreateSP(this, &FRoadSplineDetails::OnPaste, SplinePointProp));
}

void FRoadSplineDetails::OnCopy(ESplinePointProperty SplinePointProp)
{
	FString CopyStr;
	switch (SplinePointProp)
	{
	case ESplinePointProperty::Location:
		CopyStr = FString::Printf(TEXT("(X=%f,Y=%f,Z=%f)"), Position.X.GetValue(), Position.Y.GetValue(), Position.Z.GetValue());
		break;
	case ESplinePointProperty::Rotation:
		CopyStr = FString::Printf(TEXT("(Pitch=%f,Yaw=%f,Roll=%f)"), Rotation.Pitch.GetValue(), Rotation.Yaw.GetValue(), Rotation.Roll.GetValue());
		break;
	case ESplinePointProperty::Scale:
		CopyStr = FString::Printf(TEXT("(X=%f,Y=%f,Z=%f)"), Scale.X.GetValue(), Scale.Y.GetValue(), Scale.Z.GetValue());
		break;
	case ESplinePointProperty::ArriveTangent:
		CopyStr = FString::Printf(TEXT("(X=%f,Y=%f,Z=%f)"), ArriveTangent.X.GetValue(), ArriveTangent.Y.GetValue(), ArriveTangent.Z.GetValue());
		break;
	case ESplinePointProperty::LeaveTangent:
		CopyStr = FString::Printf(TEXT("(X=%f,Y=%f,Z=%f)"), LeaveTangent.X.GetValue(), LeaveTangent.Y.GetValue(), LeaveTangent.Z.GetValue());
		break;
	default:
		break;
	}

	if (!CopyStr.IsEmpty())
	{
		FPlatformApplicationMisc::ClipboardCopy(*CopyStr);
	}
}

void FRoadSplineDetails::OnPaste(ESplinePointProperty SplinePointProp)
{
	FString PastedText;
	FPlatformApplicationMisc::ClipboardPaste(PastedText);

	PasteFromText(TEXT(""), PastedText, SplinePointProp);
}

void FRoadSplineDetails::OnPasteFromText(
	const FString& InTag,
	const FString& InText,
	const TOptional<FGuid>& InOperationId,
	ESplinePointProperty SplinePointProp)
{
	PasteFromText(InTag, InText, SplinePointProp);
}

void FRoadSplineDetails::PasteFromText(
	const FString& InTag,
	const FString& InText,
	ESplinePointProperty SplinePointProp)
{
	FString PastedText = InText;
	switch (SplinePointProp)
	{
	case ESplinePointProperty::Location:
		{
			FVector NewLocation;
			if (NewLocation.InitFromString(PastedText))
			{
				FScopedTransaction Transaction(LOCTEXT("PasteLocation", "Paste Location"));
				SetSplinePointProperty(ESplinePointProperty::Location, NewLocation, EAxisList::All, true);
			}
			break;
		}
	case ESplinePointProperty::Rotation:
		{
			FVector NewRotation;
			PastedText.ReplaceInline(TEXT("Pitch="), TEXT("X="));
			PastedText.ReplaceInline(TEXT("Yaw="), TEXT("Y="));
			PastedText.ReplaceInline(TEXT("Roll="), TEXT("Z="));
			if (NewRotation.InitFromString(PastedText))
			{
				FScopedTransaction Transaction(LOCTEXT("PasteRotation", "Paste Rotation"));
				SetSplinePointProperty(ESplinePointProperty::Rotation, NewRotation, EAxisList::All, true);
			}
			break;
		}
	case ESplinePointProperty::Scale:
		{
			FVector NewScale;
			if (NewScale.InitFromString(PastedText))
			{
				FScopedTransaction Transaction(LOCTEXT("PasteScale", "Paste Scale"));
				SetSplinePointProperty(ESplinePointProperty::Scale, NewScale, EAxisList::All, true);
			}
			break;
		}
	case ESplinePointProperty::ArriveTangent:
		{
			FVector NewArrive;
			if (NewArrive.InitFromString(PastedText))
			{
				FScopedTransaction Transaction(LOCTEXT("PasteArriveTangent", "Paste Arrive Tangent"));
				SetSplinePointProperty(ESplinePointProperty::ArriveTangent, NewArrive, EAxisList::All, true);
			}
			break;
		}
	case ESplinePointProperty::LeaveTangent:
		{
			FVector NewLeave;
			if (NewLeave.InitFromString(PastedText))
			{
				FScopedTransaction Transaction(LOCTEXT("PasteLeaveTangent", "Paste Leave Tangent"));
				SetSplinePointProperty(ESplinePointProperty::LeaveTangent, NewLeave, EAxisList::All, true);
			}
			break;
		}
	default:
		break;
	}
}

void FRoadSplineDetails::OnBeginPositionSlider()
{
	bInSliderTransaction = true;
	SplineComp->Modify();
	GEditor->BeginTransaction(LOCTEXT("SetSplinePointPosition", "Set spline point position"));
}

void FRoadSplineDetails::OnBeginScaleSlider()
{
	bInSliderTransaction = true;
	SplineComp->Modify();
	GEditor->BeginTransaction(LOCTEXT("SetSplinePointScale", "Set spline point scale"));
}

void FRoadSplineDetails::OnEndSlider(float)
{
	bInSliderTransaction = false;
	GEditor->EndTransaction();
}


#undef LOCTEXT_NAMESPACE
