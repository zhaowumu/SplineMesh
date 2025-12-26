/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "RoadSplineComponent.h"
#include "IDetailChildrenBuilder.h"
#include "IDetailCustomNodeBuilder.h"
#include "ComponentVisualizers/RoadOffsetComponentVisualizer.h"
#include "Widgets/SWidget.h"

class FRoadOffsetDetails : 
	public IDetailCustomNodeBuilder, 
	public TSharedFromThis<FRoadOffsetDetails>
{
public:
	FRoadOffsetDetails(URoadSplineComponent* InOwningSplineComponent, IDetailLayoutBuilder& DetailBuilder);

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
	TSharedPtr<IPropertyHandle> LaneOffsetsHandle;

	int SelectedKeyIndex = INDEX_NONE;

	TSharedPtr<FRoadOffsetComponentVisualizer> Visualizer;
	FSimpleDelegate OnRegenerateChildren;
};
