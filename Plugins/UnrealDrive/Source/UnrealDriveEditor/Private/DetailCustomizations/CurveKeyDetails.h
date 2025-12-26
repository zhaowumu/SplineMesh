/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "IDetailCustomNodeBuilder.h"
#include "CurveDataAbstraction.h"
#include "RichCurveEditorModel.h"

namespace UnrealDrive
{

class FCurveKeyDetails 
	: public IDetailCustomNodeBuilder
	, public TSharedFromThis<FCurveKeyDetails>
{
public:
	FCurveKeyDetails(TSharedRef<IPropertyHandle> RichCurvePropertyHandle, FKeyHandle KeyHandle, UObject* InOwner);

	//virtual void SetOnRebuildChildren(FSimpleDelegate InOnRegenerateChildren) override;
	virtual void Tick(float DeltaTime) override;

	virtual bool RequiresTick() const override { return true; }

	virtual FName GetName() const override;
	virtual void GenerateHeaderRowContent(FDetailWidgetRow& NodeRow) override;
	virtual void GenerateChildContent(IDetailChildrenBuilder& ChildrenBuilder) override;

	void BindCommands();
	void SetKeyAttributes(FKeyAttributes KeyAttributes, FText Description);

	void ToggleWeightedTangents();

	/**
	 * Check whether we can toggle weighted tangents on the current selection
	 */
	bool CanToggleWeightedTangents() const;

	/**
	 * Check whether or not we can set a key interpolation on the current selection. If no keys are selected, you can't set an interpolation!
	 */
	bool CanSetKeyInterpolation() const;

	/** Compare all the currently selected keys' interp modes against the specified interp mode */
	bool CompareCommonInterpolationMode(ERichCurveInterpMode InterpMode) const;

	/** Compare all the currently selected keys' tangent modes against the specified tangent mode */
	bool CompareCommonTangentMode(ERichCurveInterpMode InterpMode, ERichCurveTangentMode TangentMode) const;

	/** Compare all the currently selected keys' tangent modes against the specified tangent mode */
	bool CompareCommonTangentWeightMode(ERichCurveInterpMode InterpMode, ERichCurveTangentWeightMode TangentWeightMode) const;

	FSimpleDelegate OnTangenModeChanged;

private:
	TSharedPtr<IPropertyHandle> RichCurvePropertyHandle;
	FKeyHandle KeyHandle;

	TSharedPtr<FRichCurveEditorModelRaw> Curve;

	TSharedPtr<FUICommandList> CommandList;

	/** Cached key attributes that are common to all selected keys */
	FKeyAttributes CachedCommonKeyAttributes;

	/** True if the current selection supports weighted tangents, false otherwise */
	bool bSelectionSupportsWeightedTangents;

};

} // namespace UnrealDrive
