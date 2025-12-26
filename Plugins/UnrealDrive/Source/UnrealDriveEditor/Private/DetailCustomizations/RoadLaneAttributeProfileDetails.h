/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "InstancedStructDetails.h"
#include "IDetailCustomNodeBuilder.h"

class IAssetReferenceFilter;
class IPropertyHandle;
class IDetailGroup;
class IDetailPropertyRow;
class IPropertyHandle;
class FStructOnScope;
class SWidget;
class SRoadLaneAttributeProfilePicker;

/**
 * FRoadLaneAttributeProfileDetails
 */
class FRoadLaneAttributeProfileDetails  : public IPropertyTypeCustomization
{
public:
	virtual ~FRoadLaneAttributeProfileDetails() override;

	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	/** IPropertyTypeCustomization interface */
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;

private:

	using FReplacementObjectMap = TMap<UObject*, UObject*>;
	void OnObjectsReinstanced(const FReplacementObjectMap& ObjectMap);

	/** Handle to the struct property being edited */
	//TSharedPtr<IPropertyHandle> StructProperty;

	TSharedPtr<SRoadLaneAttributeProfilePicker> StructPicker;
	TSharedPtr<IPropertyUtilities> PropUtils;
	TSharedPtr<IPropertyHandle> AttributeValueProperty;

	FDelegateHandle OnObjectsReinstancedHandle;
};



