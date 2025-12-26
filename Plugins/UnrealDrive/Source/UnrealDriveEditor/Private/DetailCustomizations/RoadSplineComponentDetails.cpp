/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "RoadSplineComponentDetails.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "RoadSelectionDetails.h"
#include "RoadSplineDetails.h"
#include "RoadOffsetDetails.h"
#include "RoadSplineComponent.h"
#include "UnrealDriveEditorModule.h"

//#include "ComponentVisualizers/RoadSplineComponentVisualizer.h"
#include "ComponentVisualizers/RoadSectionComponentVisualizer.h"
//#include "ComponentVisualizers/RoadOffsetComponentVisualizer.h"
#include "ComponentVisualizers/RoadWidthComponentVisualizer.h"
#include "ComponentVisualizers/RoadAttributeComponentVisualizer.h"

#define LOCTEXT_NAMESPACE "RoadSplineComponentDetails"

static URoadSectionComponentVisualizerSelectionState* GetSelectionState(const FString& ReferencerName)
{
	auto Visualizer = StaticCastSharedPtr<FRoadSectionComponentVisualizer>(FUnrealDriveEditorModule::Get().GetComponentVisualizer());
	check(Visualizer.IsValid());
	check(Visualizer->GetReferencerName() == ReferencerName);
	auto * SelectionState = Visualizer->GetSelectionState();
	check(SelectionState);
	return SelectionState;
}


TSharedRef<IDetailCustomization> FRoadSplineComponentDetails::MakeInstance()
{
	return MakeShareable(new FRoadSplineComponentDetails);
}

void FRoadSplineComponentDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	// Hide the SplineCurves property
	//TSharedPtr<IPropertyHandle> SplineCurvesProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(USplineComponent, SplineCurves));
	//SplineCurvesProperty->MarkHiddenByCustomization();


	TArray<TWeakObjectPtr<UObject>> ObjectsBeingCustomized;
	DetailBuilder.GetObjectsBeingCustomized(ObjectsBeingCustomized);

	if (ObjectsBeingCustomized.Num() == 1)
	{
		if (URoadSplineComponent* Comp = Cast<URoadSplineComponent>(ObjectsBeingCustomized[0]))
		{
			if (!Comp->IsTemplate())
			{
				IDetailCategoryBuilder& Category = DetailBuilder.EditCategory("Selection", FText::GetEmpty(), ECategoryPriority::Important);

				if (FUnrealDriveEditorModule::Get().GetRoadSelectionMode() == ERoadSelectionMode::Spline)
				{
					Category.AddCustomBuilder(MakeShareable(new FRoadSplineDetails(Comp)));
				}
				else if (FUnrealDriveEditorModule::Get().GetRoadSelectionMode() == ERoadSelectionMode::Section)
				{
					Category.AddCustomBuilder(MakeShareable(new FRoadSelectionDetails(Comp, GetSelectionState(FRoadSectionComponentVisualizer::GetReferencerNameStatic()), DetailBuilder)));
				}
				else if (FUnrealDriveEditorModule::Get().GetRoadSelectionMode() == ERoadSelectionMode::Offset)
				{
					Category.AddCustomBuilder(MakeShareable(new FRoadOffsetDetails(Comp, DetailBuilder)));
				}
				else if (FUnrealDriveEditorModule::Get().GetRoadSelectionMode() == ERoadSelectionMode::Width)
				{
					Category.AddCustomBuilder(MakeShareable(new FRoadSelectionDetails(Comp, GetSelectionState(FRoadWidthComponentVisualizer::GetReferencerNameStatic()), DetailBuilder)));
				}
				else if (FUnrealDriveEditorModule::Get().GetRoadSelectionMode() == ERoadSelectionMode::Attribute)
				{
					Category.AddCustomBuilder(MakeShareable(new FRoadSelectionDetails(Comp, GetSelectionState(FRoadAttributeComponentVisualizer::GetReferencerNameStatic()), DetailBuilder)));
				}
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
