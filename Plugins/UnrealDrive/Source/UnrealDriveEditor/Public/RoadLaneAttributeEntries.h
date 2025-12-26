/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "CoreMinimal.h"
#include "DefaultRoadLaneAttributes.h"
#include "RoadLaneAttributeEntries.generated.h"

struct FReferenceSplineMeshParams;

USTRUCT(BlueprintType)
struct FRoadLaneAttributeEntry
{
	GENERATED_BODY()

	FRoadLaneAttributeEntry() = default;
	virtual ~FRoadLaneAttributeEntry() = default;

	FRoadLaneAttributeEntry(const TInstancedStruct<FRoadLaneAttributeValue>& AttributeValueTemplate, FText LabelOverride, FText ToolTip, const FName& IconStyleName)
		: AttributeValueTemplate(AttributeValueTemplate)
		, LabelOverride(LabelOverride)
		, ToolTip(ToolTip)
		, IconStyleName(IconStyleName)
		
	{}

	// Struct child of RoadLaneAttributeValue
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttributeEntry)
	TInstancedStruct<FRoadLaneAttributeValue> AttributeValueTemplate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttributeEntry)
	FText LabelOverride;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttributeEntry)
	FText ToolTip;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttributeEntry)
	FName IconStyleName = "RoadEditor.RoadLaneBuildMode";


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttributeEntry)
	FName StyleName = "UnrealDriveEditor"; //FUnrealDriveEditorStyle::Get().GetStyleSetName();

	inline FSlateIcon GetIcon() const
	{
		return  FSlateIcon(StyleName, IconStyleName);
	}


	virtual void GenerateAsset(const FReferenceSplineMeshParams& SplineMeshParams, const  TInstancedStruct<FRoadLaneAttributeEntry>& AttributeEntry, FName AttributeEntryName, AActor* TargetActor, bool bIsPreview) const {}
};

USTRUCT(BlueprintType)
struct FRoadLaneAttributeEntryRefSpline: public FRoadLaneAttributeEntry
{
	GENERATED_BODY()

	FRoadLaneAttributeEntryRefSpline()
	{
		AttributeValueTemplate.InitializeAs<FRoadLaneGeneration>();
	}

	// Desired length of each mesh segment placed on spline [cm] 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttributeEntry, meta = (UIMin = 1.0, ClampMin = 1.0))
	double LengthOfSegment = 1500;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttributeEntry)
	bool bAlignWorldUpVector = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttributeEntry)
	bool bReversSplineDirection = false;

};

USTRUCT(BlueprintType)
struct FRoadLaneAttributeEntrySplineMesh : public FRoadLaneAttributeEntryRefSpline
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LaneAttribute)
	TObjectPtr<class UStaticMesh> StaticMesh;

	virtual void GenerateAsset(const FReferenceSplineMeshParams& SplineMeshParams, const  TInstancedStruct<FRoadLaneAttributeEntry>& AttributeEntry, FName AttributeEntryName, AActor* TargetActor, bool bIsPreview) const override;
};

USTRUCT(BlueprintType)
struct FRoadLaneAttributeEntryComponentTemplate : public FRoadLaneAttributeEntryRefSpline
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LaneAttribute)
	TSubclassOf<class USceneComponent> ComponentTemplate;

	// Determines in which part (by SOffset) of the segment the components should be placed: 0.0 - start, 1.0 - end, 0.5 - middle.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttributeEntry, meta = (UIMin = 0.0, ClampMin = 0.0, UIMax = 1.0, ClampMax = 1.0))
	double ComponentToSegmentAlign = 0.0;

	virtual void GenerateAsset(const FReferenceSplineMeshParams& SplineMeshParams, const  TInstancedStruct<FRoadLaneAttributeEntry>& AttributeEntry, FName AttributeEntryName, AActor* TargetActor, bool bIsPreview) const override;
};

USTRUCT(BlueprintType)
struct FRoadLaneAttributeEntryCustomBuilder : public FRoadLaneAttributeEntryRefSpline
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LaneAttribute)
	TSubclassOf<class UCustomSplineBuilder> CustomBuilder;

	virtual void GenerateAsset(const FReferenceSplineMeshParams& SplineMeshParams, const  TInstancedStruct<FRoadLaneAttributeEntry>& AttributeEntry, FName AttributeEntryName, AActor* TargetActor, bool bIsPreview) const override;
};


