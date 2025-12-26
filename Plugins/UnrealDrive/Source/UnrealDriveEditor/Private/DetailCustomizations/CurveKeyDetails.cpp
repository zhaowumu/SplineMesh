/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "CurveKeyDetails.h"
#include "RoadEditorCommands.h"
#include "UnrealDriveEditorModule.h"
#include "IDetailGroup.h"
#include "IDetailChildrenBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "CurveEditorCommands.h"
#include "Misc/Optional.h"
#include "Utils/PropertyEditorUtils.h"

#define LOCTEXT_NAMESPACE "FCurveKeyDetails"

using namespace UnrealDrive;

FCurveKeyDetails::FCurveKeyDetails(TSharedRef<IPropertyHandle> RichCurvePropertyHandle, FKeyHandle KeyHandle, UObject* InOwner)
	: RichCurvePropertyHandle(RichCurvePropertyHandle)
	, KeyHandle(KeyHandle)
{
	check(InOwner);
	check(KeyHandle != FKeyHandle::Invalid());

	FRichCurve* RichCurve = PropertyEditorUtils::GetFirstData<FRichCurve>(RichCurvePropertyHandle);
	check(RichCurve);

	Curve = MakeShared<FRichCurveEditorModelRaw>(RichCurve, InOwner);
}

void FCurveKeyDetails::Tick(float DeltaTime) 
{
	if (!RichCurvePropertyHandle->IsValidHandle() && KeyHandle != FKeyHandle::Invalid())
	{
		return;
	}

	bSelectionSupportsWeightedTangents = false;

	TOptional<FKeyAttributes> AccumulatedKeyAttributes;
	TArray<FKeyAttributes> AllKeyAttributes;
	AllKeyAttributes.SetNum(1);

	Curve->GetKeyAttributes({ KeyHandle }, AllKeyAttributes);
	for (const FKeyAttributes& Attributes : AllKeyAttributes)
	{
		if (Attributes.HasTangentWeightMode())
		{
			bSelectionSupportsWeightedTangents = true;
		}

		if (!AccumulatedKeyAttributes.IsSet())
		{
			AccumulatedKeyAttributes = Attributes;
		}
		else
		{
			AccumulatedKeyAttributes = FKeyAttributes::MaskCommon(AccumulatedKeyAttributes.GetValue(), Attributes);
		}
	}

	// Reset the common curve and key info
	CachedCommonKeyAttributes = AccumulatedKeyAttributes.Get(FKeyAttributes());
}


FName FCurveKeyDetails::GetName() const
{
	static const FName Name("FCurveKeyDetails");
	return Name;
}

void FCurveKeyDetails::GenerateHeaderRowContent(FDetailWidgetRow& NodeRow)
{
}

void FCurveKeyDetails::GenerateChildContent(IDetailChildrenBuilder& ChildrenBuilder)
{
	CommandList = MakeShared<FUICommandList>();
	BindCommands();

	FUniformToolBarBuilder ToolBarBuilder(CommandList, FMultiBoxCustomization::None);
	//ToolBarBuilder.AddToolBarButton(FCurveEditorCommands::Get().InterpolationCubicSmartAuto);
	ToolBarBuilder.AddToolBarButton(FCurveEditorCommands::Get().InterpolationCubicAuto);
	ToolBarBuilder.AddToolBarButton(FCurveEditorCommands::Get().InterpolationCubicUser);
	ToolBarBuilder.AddToolBarButton(FCurveEditorCommands::Get().InterpolationCubicBreak);
	ToolBarBuilder.AddToolBarButton(FCurveEditorCommands::Get().InterpolationLinear);
	//ToolBarBuilder.AddToolBarButton(FCurveEditorCommands::Get().InterpolationConstant);
	ToolBarBuilder.AddToolBarButton(FCurveEditorCommands::Get().InterpolationToggleWeighted);

	ChildrenBuilder.AddCustomRow(LOCTEXT("CurveKeyDetails_Search", "Curve Key Details"))
		.NameContent()
		[
			SNew(STextBlock)
			.Font(IDetailLayoutBuilder::GetDetailFontBold())
			.Text(LOCTEXT("CurveKeyDetails_Name", "Tangent Mode"))
		]
		.ValueContent()
		[
			ToolBarBuilder.MakeWidget()
		];
}

void FCurveKeyDetails::BindCommands()
{
	// Interpolation and tangents
	{
		FExecuteAction SetConstant = FExecuteAction::CreateSP(this, &FCurveKeyDetails::SetKeyAttributes, FKeyAttributes().SetInterpMode(RCIM_Constant).SetTangentMode(RCTM_Auto), LOCTEXT("SetInterpConstant", "Set Interp Constant"));
		FExecuteAction SetLinear = FExecuteAction::CreateSP(this, &FCurveKeyDetails::SetKeyAttributes, FKeyAttributes().SetInterpMode(RCIM_Linear).SetTangentMode(RCTM_Auto), LOCTEXT("SetInterpLinear", "Set Interp Linear"));
		FExecuteAction SetCubicAuto = FExecuteAction::CreateSP(this, &FCurveKeyDetails::SetKeyAttributes, FKeyAttributes().SetInterpMode(RCIM_Cubic).SetTangentMode(RCTM_Auto), LOCTEXT("SetInterpCubic", "Set Interp Auto"));
		FExecuteAction SetCubicSmartAuto = FExecuteAction::CreateSP(this, &FCurveKeyDetails::SetKeyAttributes, FKeyAttributes().SetInterpMode(RCIM_Cubic).SetTangentMode(RCTM_SmartAuto), LOCTEXT("SetInterpSmartAuto", "Set Interp Smart Auto"));
		FExecuteAction SetCubicUser = FExecuteAction::CreateSP(this, &FCurveKeyDetails::SetKeyAttributes, FKeyAttributes().SetInterpMode(RCIM_Cubic).SetTangentMode(RCTM_User), LOCTEXT("SetInterpUser", "Set Interp User"));
		FExecuteAction SetCubicBreak = FExecuteAction::CreateSP(this, &FCurveKeyDetails::SetKeyAttributes, FKeyAttributes().SetInterpMode(RCIM_Cubic).SetTangentMode(RCTM_Break), LOCTEXT("SetInterpBreak", "Set Interp Break"));

		FExecuteAction    ToggleWeighted = FExecuteAction::CreateSP(this, &FCurveKeyDetails::ToggleWeightedTangents);
		FCanExecuteAction CanToggleWeighted = FCanExecuteAction::CreateSP(this, &FCurveKeyDetails::CanToggleWeightedTangents);

		FIsActionChecked IsConstantCommon = FIsActionChecked::CreateSP(this, &FCurveKeyDetails::CompareCommonInterpolationMode, RCIM_Constant);
		FIsActionChecked IsLinearCommon = FIsActionChecked::CreateSP(this, &FCurveKeyDetails::CompareCommonInterpolationMode, RCIM_Linear);
		FIsActionChecked IsCubicAutoCommon = FIsActionChecked::CreateSP(this, &FCurveKeyDetails::CompareCommonTangentMode, RCIM_Cubic, RCTM_Auto);
		FIsActionChecked IsCubicSmartAutoCommon = FIsActionChecked::CreateSP(this, &FCurveKeyDetails::CompareCommonTangentMode, RCIM_Cubic, RCTM_SmartAuto);
		FIsActionChecked IsCubicUserCommon = FIsActionChecked::CreateSP(this, &FCurveKeyDetails::CompareCommonTangentMode, RCIM_Cubic, RCTM_User);
		FIsActionChecked IsCubicBreakCommon = FIsActionChecked::CreateSP(this, &FCurveKeyDetails::CompareCommonTangentMode, RCIM_Cubic, RCTM_Break);
		FIsActionChecked IsCubicWeightCommon = FIsActionChecked::CreateSP(this, &FCurveKeyDetails::CompareCommonTangentWeightMode, RCIM_Cubic, RCTWM_WeightedBoth);

		FCanExecuteAction CanSetKeyTangent = FIsActionChecked::CreateSP(this, &FCurveKeyDetails::CanSetKeyInterpolation);

		CommandList->MapAction(FCurveEditorCommands::Get().InterpolationCubicSmartAuto, SetCubicSmartAuto, CanSetKeyTangent, IsCubicSmartAutoCommon);
		CommandList->MapAction(FCurveEditorCommands::Get().InterpolationCubicAuto, SetCubicAuto, CanSetKeyTangent, IsCubicAutoCommon);
		CommandList->MapAction(FCurveEditorCommands::Get().InterpolationCubicUser, SetCubicUser, CanSetKeyTangent, IsCubicUserCommon);
		CommandList->MapAction(FCurveEditorCommands::Get().InterpolationCubicBreak, SetCubicBreak, CanSetKeyTangent, IsCubicBreakCommon);
		CommandList->MapAction(FCurveEditorCommands::Get().InterpolationLinear, SetLinear, CanSetKeyTangent, IsLinearCommon);
		CommandList->MapAction(FCurveEditorCommands::Get().InterpolationConstant, SetConstant, CanSetKeyTangent, IsConstantCommon);
		CommandList->MapAction(FCurveEditorCommands::Get().InterpolationToggleWeighted, ToggleWeighted, CanToggleWeighted, IsCubicWeightCommon);
			
	}
}

void FCurveKeyDetails::SetKeyAttributes(FKeyAttributes KeyAttributes, FText Description)
{
	if (!RichCurvePropertyHandle->IsValidHandle() && KeyHandle != FKeyHandle::Invalid())
	{
		return;
	}

	FScopedTransaction Transaction(Description);

	Curve->Modify();
	Curve->SetKeyAttributes({ KeyHandle }, { KeyAttributes });

	OnTangenModeChanged.ExecuteIfBound();
}

void FCurveKeyDetails::ToggleWeightedTangents()
{
	if (!RichCurvePropertyHandle->IsValidHandle() && KeyHandle != FKeyHandle::Invalid())
	{
		return;
	}

	FScopedTransaction Transaction(LOCTEXT("ToggleWeightedTangents_Transaction", "Toggle Weighted Tangents"));

	// Disable weights unless we find something that doesn't have weights, then add them
	FKeyAttributes KeyAttributesToAssign = FKeyAttributes().SetTangentWeightMode(RCTWM_WeightedNone);


	TArray<FKeyAttributes> KeyAttributes;
	KeyAttributes.SetNum(1);
	Curve->GetKeyAttributes({ KeyHandle }, KeyAttributes);

	// Check all the key attributes if they support tangent weights, but don't have any. If we find any such keys, we'll enable weights on all.
	for (const FKeyAttributes& Attributes : KeyAttributes)
	{
		if (Attributes.HasTangentWeightMode() && !(Attributes.HasArriveTangentWeight() || Attributes.HasLeaveTangentWeight()))
		{
			KeyAttributesToAssign.SetTangentWeightMode(RCTWM_WeightedBoth);
			break;
		}
	}
		
	Curve->Modify();
	Curve->SetKeyAttributes({ KeyHandle }, { KeyAttributesToAssign });

	OnTangenModeChanged.ExecuteIfBound();
}


bool FCurveKeyDetails::CanToggleWeightedTangents() const
{
	return /*bSelectionSupportsWeightedTangents &&*/ CanSetKeyInterpolation();
}


bool FCurveKeyDetails::FCurveKeyDetails::CanSetKeyInterpolation() const
{
	return true;
}

bool FCurveKeyDetails::CompareCommonInterpolationMode(ERichCurveInterpMode InterpMode) const
{
	return CachedCommonKeyAttributes.HasInterpMode() && CachedCommonKeyAttributes.GetInterpMode() == InterpMode;
}

bool FCurveKeyDetails::CompareCommonTangentMode(ERichCurveInterpMode InterpMode, ERichCurveTangentMode TangentMode) const
{
	return CompareCommonInterpolationMode(InterpMode) && CachedCommonKeyAttributes.HasTangentMode() && CachedCommonKeyAttributes.GetTangentMode() == TangentMode;
}

bool FCurveKeyDetails::CompareCommonTangentWeightMode(ERichCurveInterpMode InterpMode, ERichCurveTangentWeightMode TangentWeightMode) const
{
	return CompareCommonInterpolationMode(InterpMode) && CachedCommonKeyAttributes.HasTangentWeightMode() && CachedCommonKeyAttributes.GetTangentWeightMode() == TangentWeightMode;
}




#undef LOCTEXT_NAMESPACE
