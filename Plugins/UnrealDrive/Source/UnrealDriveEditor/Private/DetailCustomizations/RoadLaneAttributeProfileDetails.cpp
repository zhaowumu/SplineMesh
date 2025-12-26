/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */
#include "RoadLaneAttributeProfileDetails.h"
#include "DetailWidgetRow.h"
#include "DetailLayoutBuilder.h"
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "IDetailChildrenBuilder.h"
#include "IDetailGroup.h"
#include "IPropertyUtilities.h"
#include "UObject/Package.h"
#include "Widgets/Images/SImage.h"
#include "Modules/ModuleManager.h"
#include "StructUtils/UserDefinedStruct.h"
#include "StructUtils/InstancedStruct.h"
#include "IStructureDataProvider.h"
#include "Misc/ConfigCacheIni.h"
#include "StructUtilsDelegates.h"
#include "SRoadLaneAttributeProfilePicker.h"
#include "UnrealDrivePreset.h"

#define LOCTEXT_NAMESPACE "RoadLaneAttributeProfileDetails"

TSharedRef<IPropertyTypeCustomization> FRoadLaneAttributeProfileDetails::MakeInstance()
{
	return MakeShared<FRoadLaneAttributeProfileDetails>();
}

FRoadLaneAttributeProfileDetails::~FRoadLaneAttributeProfileDetails()
{
	if (OnObjectsReinstancedHandle.IsValid())
	{
		FCoreUObjectDelegates::OnObjectsReinstanced.Remove(OnObjectsReinstancedHandle);
	}
}

void FRoadLaneAttributeProfileDetails::CustomizeHeader(TSharedRef<class IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	//StructProperty = StructPropertyHandle;
	AttributeValueProperty = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FRoadLaneAttributeProfile, AttributeValueTemplate));
	PropUtils = StructCustomizationUtils.GetPropertyUtilities();
	OnObjectsReinstancedHandle = FCoreUObjectDelegates::OnObjectsReinstanced.AddSP(this, &FRoadLaneAttributeProfileDetails::OnObjectsReinstanced);

	check(AttributeValueProperty);

	HeaderRow
		.ShouldAutoExpand(true)
		.NameContent()
		[
			StructPropertyHandle->CreatePropertyNameWidget(LOCTEXT("Template_Caption", "Template"), LOCTEXT("Template_ToolTip", "Road lane attribute value template."))
		]
		.ValueContent()
		.MinDesiredWidth(250.f)
		.VAlign(VAlign_Center)
		[
			SAssignNew(StructPicker, SRoadLaneAttributeProfilePicker, StructPropertyHandle, PropUtils)
		]
		.IsEnabled(StructPropertyHandle->IsEditable());
}

void FRoadLaneAttributeProfileDetails::OnObjectsReinstanced(const FReplacementObjectMap& ObjectMap)
{
	// Force update the details when BP is compiled, since we may cached hold references to the old object or class.
	if (!ObjectMap.IsEmpty() && PropUtils.IsValid())
	{
		PropUtils->RequestRefresh();
	}
}

void FRoadLaneAttributeProfileDetails::CustomizeChildren(TSharedRef<class IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	TSharedRef<FInstancedStructDataDetails> DataDetails = MakeShared<FInstancedStructDataDetails>(AttributeValueProperty);
	StructBuilder.AddCustomBuilder(DataDetails);
}
