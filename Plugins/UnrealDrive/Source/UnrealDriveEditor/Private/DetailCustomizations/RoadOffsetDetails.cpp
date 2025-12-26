/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "RoadOffsetDetails.h"
#include "RoadEditorCommands.h"
#include "UnrealDriveEditorModule.h"
#include "IDetailGroup.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "DefaultRoadLaneAttributes.h"
#include "CurveKeyDetails.h"
#include "Utils/PropertyEditorUtils.h"
#include "Utils/CurveUtils.h"

#define LOCTEXT_NAMESPACE "FRoadOffsetDetails"

FRoadOffsetDetails::FRoadOffsetDetails(URoadSplineComponent* InOwningComponent, IDetailLayoutBuilder& DetailBuilder)
	: RoadSplineComp(nullptr)
{
	check(InOwningComponent);
	Visualizer = StaticCastSharedPtr<FRoadOffsetComponentVisualizer>(FUnrealDriveEditorModule::Get().GetComponentVisualizer());
	check(Visualizer.IsValid());
	check(Visualizer->GetReferencerName() == TEXT("FRoadOffsetComponentVisualizer"));

	if (InOwningComponent->IsTemplate())
	{
		// For blueprints, SplineComp will be set to the preview actor in UpdateValues().
		RoadSplineComp = nullptr;
		RoadSplineCompArchetype = InOwningComponent;
	}
	else
	{
		RoadSplineComp = InOwningComponent;
		RoadSplineCompArchetype = nullptr;
	}

	LaneOffsetsHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(URoadSplineComponent, RoadLayout))->GetChildHandle(GET_MEMBER_NAME_CHECKED(FRoadLayout, ROffset));
	check(LaneOffsetsHandle);
}

void FRoadOffsetDetails::SetOnRebuildChildren(FSimpleDelegate InOnRegenerateChildren)
{
	OnRegenerateChildren = InOnRegenerateChildren;
}

void FRoadOffsetDetails::GenerateHeaderRowContent(FDetailWidgetRow& NodeRow)
{
}

void FRoadOffsetDetails::GenerateChildContent(IDetailChildrenBuilder& ChildrenBuilder)
{
	auto& Commands = FRoadEditorCommands::Get();

	SelectedKeyIndex = Visualizer->GetSelectionState()->GetSelectedKeyVerified();

	if (SelectedKeyIndex != INDEX_NONE)
	{
		auto KeysHandle = LaneOffsetsHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FRichCurve, Keys))->AsArray();

		uint32 Num = INDEX_NONE;
		if(KeysHandle->GetNumElements(Num) == FPropertyAccess::Success && uint32(SelectedKeyIndex) < Num)
		{
			PropertyEditorUtils::AddTextRow(
				ChildrenBuilder,
				LOCTEXT("SelectedKey_Search", "Selected Key"),
				LOCTEXT("SelectedKey_Name", "Selected Key"),
				FText::Format(LOCTEXT("SelectedKey_Value", "< {0} >"), SelectedKeyIndex));

			auto KeyHandle = LaneOffsetsHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FRichCurve, Keys))->AsArray()->GetElement(SelectedKeyIndex);

			FRichCurve* RichCurve = PropertyEditorUtils::GetFirstData<FRichCurve>(LaneOffsetsHandle);
			FRichCurveKey* CurveKey = PropertyEditorUtils::GetFirstData<FRichCurveKey>(KeyHandle);
			if (ensure(CurveKey && CurveKey))
			{
				FKeyHandle Key = CurveUtils::GetKeyHandle(*RichCurve, SelectedKeyIndex);
				check(Key != FKeyHandle::Invalid());
				auto CurveKeyDetails = MakeShared<UnrealDrive::FCurveKeyDetails>(LaneOffsetsHandle.ToSharedRef(), Key, RoadSplineComp);
				CurveKeyDetails->OnTangenModeChanged.BindLambda([this]()
				{
					if (IsValid(RoadSplineComp))
					{
						RoadSplineComp->GetRoadLayout().UpdateAttributesVersion();
						RoadSplineComp->UpdateMagicTransform();
						RoadSplineComp->MarkRenderStateDirty();
						GEditor->RedrawLevelEditingViewports(true);
					}
				});
				ChildrenBuilder.AddCustomBuilder(CurveKeyDetails);
			}


			auto TimeProperty = KeyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FRichCurveKey, Time));
			TimeProperty->SetPropertyDisplayName(LOCTEXT("SelectedKey_TimeLabel", "SOffset"));
			TimeProperty->SetToolTipText(LOCTEXT("SelectedKey_TimeTipText", "Spline offset [cm] starts from current road section"));

			ChildrenBuilder.AddProperty(TimeProperty.ToSharedRef());
			ChildrenBuilder.AddProperty(KeyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FRichCurveKey, Value)).ToSharedRef());
			//ChildrenBuilder.AddProperty(KeyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FRichCurveKey, InterpMode)).ToSharedRef());
			//ChildrenBuilder.AddProperty(KeyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FRichCurveKey, TangentMode)).ToSharedRef());
			//ChildrenBuilder.AddProperty(KeyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FRichCurveKey, TangentWeightMode)).ToSharedRef());
		}

	}
	else
	{
		ChildrenBuilder.AddCustomRow(LOCTEXT("NoneSelected", "None selected"))
		.RowTag(TEXT("NoneSelected"))
		[
			SNew(SBox)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("NoneSelected", "No road elements are selected."))
				.Font(IDetailLayoutBuilder::GetDetailFont())
			]
		];
	}
}

void FRoadOffsetDetails::Tick(float DeltaTime)
{
	// If this is a blueprint spline, always update the spline component based on 
	// the spline component visualizer's currently edited spline component.
	if (RoadSplineCompArchetype)
	{
		URoadSplineComponent* EditedSplineComp = Visualizer.IsValid() ? Visualizer->GetEditedSplineComponent() : nullptr;

		if (!EditedSplineComp || (EditedSplineComp->GetArchetype() != RoadSplineCompArchetype))
		{
			return;
		}

		RoadSplineComp = EditedSplineComp;
	}

	if (!RoadSplineComp || !Visualizer.IsValid())
	{
		return;
	}

	bool bNeedsRebuild = Visualizer->GetSelectionState()->GetSelectedKeyVerified() != SelectedKeyIndex;

	if (bNeedsRebuild)
	{
		OnRegenerateChildren.ExecuteIfBound();
	}
}

FName FRoadOffsetDetails::GetName() const
{
	static const FName Name("RoadOffsetDetails");
	return Name;
}


#undef LOCTEXT_NAMESPACE
