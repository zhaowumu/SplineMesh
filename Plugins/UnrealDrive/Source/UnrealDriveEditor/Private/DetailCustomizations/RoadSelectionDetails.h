/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "RoadSplineComponent.h"
#include "IDetailChildrenBuilder.h"
#include "IDetailCustomNodeBuilder.h"
#include "ComponentVisualizers/RoadSectionComponentVisualizer.h"
#include "Widgets/SWidget.h"

namespace UnrealDrive
{
	class FInstancedStructProvider;
}

class FRoadSelectionDetails : public IDetailCustomNodeBuilder, public TSharedFromThis<FRoadSelectionDetails>
{
public:
	FRoadSelectionDetails(URoadSplineComponent* InOwningSplineComponent, URoadSectionComponentVisualizerSelectionState* SelectionState, IDetailLayoutBuilder& DetailBuilder);

	//~ Begin IDetailCustomNodeBuilder interface
	virtual void SetOnRebuildChildren(FSimpleDelegate InOnRegenerateChildren) override;
	virtual void GenerateHeaderRowContent(FDetailWidgetRow& NodeRow) override;
	virtual void GenerateChildContent(IDetailChildrenBuilder& ChildrenBuilder) override;
	virtual void Tick(float DeltaTime) override;
	virtual bool RequiresTick() const override { return true; }
	virtual bool InitiallyCollapsed() const override { return false; }
	virtual FName GetName() const override;
	//~ End IDetailCustomNodeBuilder interface

private:
	URoadSplineComponent* RoadSplineComp;
	URoadSplineComponent* RoadSplineCompArchetype;
	TSharedPtr<IPropertyHandle> SectionsProperty;

	int SelectedSectiontIndex = INDEX_NONE;
	int SelectedLaneIndex = 0;
	FName SelectedAttributeName = NAME_None;
	int SelectedKeyIndex = INDEX_NONE;

	TObjectPtr<URoadSectionComponentVisualizerSelectionState> SelectionState;
	FSimpleDelegate OnRegenerateChildren;

	TSharedPtr<class SRoadLaneLanePicker> RoadLaneLanePicker;

	TSharedPtr<UnrealDrive::FInstancedStructProvider> RoadLaneAttributeStruct;


};
