/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "CoreMinimal.h"
#include "StructUtils/InstancedStruct.h"
#include "RoadLaneAttribute.h"
#include "DefaultRoadLaneAttributes.generated.h"


namespace UnrealDrive
{
	namespace LaneAttributes
	{
		static FName Mark = "Mark";
		static FName Speed = "Speed";
	}
}


/**
 * ERoadLaneMark
 * Can be used in gameplay (for example traffic generation). Has no effect on procedural generation.
 */
 UENUM(BlueprintType)
 enum class ERoadLaneMark : uint8
 {
	 None,
	 Solid,
	 Broked,
	 DoubleSolid,
	 DoubleBroked,
	 SolidBroked,
	 BrokedSolid,
	 Custom
 };
 

USTRUCT(BlueprintType, Blueprintable)
struct UNREALDRIVE_API FRoadLaneMarkProfile
{
	GENERATED_USTRUCT_BODY()

	/** Can be used in gameplay (for example traffic generation). Has no effect on procedural generation.  */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Profile)
	ERoadLaneMark Type = ERoadLaneMark::None;


	//virtual bool MakeCustomMesh(const TArray<struct FRoadPosition>& Line, double S0, double S1, double ZOffset, class FDynamicMesh3& OutDynamicMesh) { return false; }
	//virtual void DrawCustomLine(const TArray<struct FRoadPosition>& Line, double S0, double S1) { }

	FRoadLaneMarkProfile() = default;
	FRoadLaneMarkProfile(ERoadLaneMark InType)
		: Type(InType)
	{
	}

	FRoadLaneMarkProfile& SetType(ERoadLaneMark InType) { Type = InType; return *this; }
};

USTRUCT(BlueprintType, Blueprintable)
struct UNREALDRIVE_API FRoadLaneMarkProfileSolid: public FRoadLaneMarkProfile
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Profile)
	double Width = 15;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Profile)
	FColor VertexColor = FColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Profile, meta = (GetOptions = "UnrealDrive.UnrealDrivePresetBase.GetLaneMarkMaterialProfiles"))
	FName MaterialProfile = "Default";

	FRoadLaneMarkProfileSolid()
		:FRoadLaneMarkProfile(ERoadLaneMark::Solid)
	{
	}
	
	FRoadLaneMarkProfileSolid(double InWidth, const FColor& InColor = FColor::White)
		: Width(InWidth)
		, VertexColor(InColor)
	{
	}

	FRoadLaneMarkProfile& SetWidth(double InWidth) { Width = InWidth; return *this; }
	FRoadLaneMarkProfile& SetColor(const FColor& InVertexColor) { VertexColor = InVertexColor; return *this; }

};

USTRUCT(BlueprintType, Blueprintable)
struct UNREALDRIVE_API FRoadLaneMarkProfileBroked : public FRoadLaneMarkProfile
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Profile)
	double Width = 15;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Profile)
	double Long = 300;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Profile)
	double Gap = 450;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Profile)
	FColor VertexColor = FColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Profile, meta = (GetOptions = "UnrealDrive.UnrealDrivePresetBase.GetLaneMarkMaterialProfiles"))
	FName MaterialProfile = "Default";

	FRoadLaneMarkProfileBroked()
		:FRoadLaneMarkProfile(ERoadLaneMark::Broked)
	{
	}

	FRoadLaneMarkProfileBroked(double InWidth, double InLong, double InGap, const FColor& InColor = FColor::White)
		: Width(InWidth)
		, Long(InLong)
		, Gap(InGap)
		, VertexColor(InColor)
	{
	}

	//UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Profile)
	//TSoftObjectPtr<UMaterialInterface> Material;
	FRoadLaneMarkProfile& SetWidth(double InWidth) { Width = InWidth; return *this; }
	FRoadLaneMarkProfile& SetColor(const FColor& InVertexColor) { VertexColor = InVertexColor; return *this; }
	FRoadLaneMarkProfile& SetLong(double InLong) { Long = InLong; return *this; }
	FRoadLaneMarkProfile& SetGap(double InGap) { Gap = InGap; return *this; }
};


USTRUCT(BlueprintType, Blueprintable)
struct UNREALDRIVE_API FRoadLaneMarkProfileDouble : public FRoadLaneMarkProfile
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Profile, NoClear, Meta = (ExcludeBaseStruct))
	TInstancedStruct<FRoadLaneMarkProfile> Left;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Profile, NoClear, Meta = (ExcludeBaseStruct))
	TInstancedStruct<FRoadLaneMarkProfile> Right;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Profile)
	double Gap = 400;

	FRoadLaneMarkProfileDouble() = default;
	FRoadLaneMarkProfileDouble(ERoadLaneMark InType, TInstancedStruct<FRoadLaneMarkProfile>&& InLeft, TInstancedStruct<FRoadLaneMarkProfile>&& InRight, double InGap)
		: FRoadLaneMarkProfile(InType)
		, Left(MoveTemp(InLeft))
		, Right(MoveTemp(InRight))
		, Gap(InGap)
	{
	}
	FRoadLaneMarkProfileDouble(ERoadLaneMark InType, const TInstancedStruct<FRoadLaneMarkProfile>& InLeft, const TInstancedStruct<FRoadLaneMarkProfile>& InRight, double InGap)
		: FRoadLaneMarkProfile(InType)
		, Left(InLeft)
		, Right(InRight)
		, Gap(InGap)
	{
	}

	FRoadLaneMarkProfile& SetLeft(TInstancedStruct<FRoadLaneMarkProfile>&& InLeft) { Left = MoveTemp(InLeft); return *this; }
	FRoadLaneMarkProfile& SetRight(TInstancedStruct<FRoadLaneMarkProfile>&& InRight) { Right = MoveTemp(InRight); return *this; }
	FRoadLaneMarkProfile& SetLeft(const TInstancedStruct<FRoadLaneMarkProfile>& InLeft) { Left = InLeft; return *this; }
	FRoadLaneMarkProfile& SetRight(const TInstancedStruct<FRoadLaneMarkProfile>& InRight) { Right = InRight; return *this; }
	FRoadLaneMarkProfile& SetGap(double InGap) { Gap = InGap; return *this; }
};

/**
 * ERoadLaneMarkProfile
 */
UENUM(BlueprintType)
enum class ERoadLaneMarkProfile: uint8
{
	UsePreset,
	UseCustom
};

/**
 * FRoadLaneMark is a road lane attribute that stores data about road markings and used in procedural generation of road markings in the Build Mesh Modeling tool.
 */
USTRUCT(BlueprintType, Blueprintable)
struct UNREALDRIVE_API FRoadLaneMark : public FRoadLaneAttributeValue
{
	GENERATED_USTRUCT_BODY()

	virtual ~FRoadLaneMark() {}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttributeKey)
	ERoadLaneMarkProfile ProfileSource = ERoadLaneMarkProfile::UsePreset;

	/** Mark profile from UUnrealDrivePresetBase::LaneMarkProfiles */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttributeKey,  meta = (GetOptions = "UnrealDrive.UnrealDrivePresetBase.GetLaneMarkProfileNames", EditCondition = "ProfileSource == ERoadLaneMarkProfile::UsePreset", EditConditionHides))
	FName ProfileName = NAME_None;

	/**
	 * Use a custom profile if ProfileSource = ERoadLaneMarkProfile::UseCustom
	 * ATTENTION BUG: If this option is disabled in UI, just reselect the attribute key. This is a UE bug. Hope it will be fixed in future versions of UE 
	 */
	UPROPERTY(EditAnywhere, Category = AttributeKey, Meta = (ExcludeBaseStruct, EditCondition = "ProfileSource == ERoadLaneMarkProfile::UseCustom", EditConditionHides))
	TInstancedStruct<FRoadLaneMarkProfile> CustomProfile;

#if WITH_EDITOR
	virtual const FDrawStyle& GetDrawStyle() const override;
#endif

private:
#if WITH_EDITOR
	mutable FDrawStyle CachedDrawStyle;
	mutable FName CachedProfileName;
	mutable ERoadLaneMark CachedRoadLaneType;
#endif
};

/**
 * FRaodLaneSpeed is a road lane attribute that stores data about speed limit on the lane.
 */
USTRUCT(BlueprintType, Blueprintable)
struct UNREALDRIVE_API FRaodLaneSpeed : public FRoadLaneAttributeValue
{
	GENERATED_USTRUCT_BODY()

	virtual ~FRaodLaneSpeed() {}

	// Maximum allowed speed [meters/second]
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttributeKey)
	double MaxSpeed = 15;
};

/**
 * FRoadLaneSplineMesh is a road lane attribute for store spline-like data on the lane. Used in the Build Mesh Modeling tool for ganerate URoadSplineMesh(s) and other USceneComponent along the road lane.
 * See FRoadLaneAttributeEntrySplineMesh, FRoadLaneAttributeEntryComponentTemplate, FRoadLaneAttributeEntryCustomBuilder.
 */
USTRUCT()
struct UNREALDRIVE_API FRoadLaneGeneration : public FRoadLaneAttributeValue
{
	GENERATED_USTRUCT_BODY()

	virtual ~FRoadLaneGeneration() {}

	/** Value, usually from 0 to 1, where 0 is the inner side of the lane, 1 is the outer side lane, 0.5 - center of lane */
	UPROPERTY(EditAnywhere, Category = AttributeKey, meta = (UIMin = -5.0, ClampMin = -5.0, UIMax = 5.0, ClampMax = 5.0))
	double Alpha = 0.5;

	UPROPERTY(EditAnywhere, Category = AttributeKey);
	FVector2D Scale = {1.0, 1.0};

	/* Offset (Y, Z) */
	UPROPERTY(EditAnywhere, Category = AttributeKey);
	FVector2D Offset = {};

	/** [Degrees] */
	UPROPERTY(EditAnywhere, Category = AttributeKey);
	double Roll = 0.0;

	// Has an effect only for the first point of attribute line. Also, all adjacent attributes lines must also be reversed.
	UPROPERTY(EditAnywhere, Category = AttributeKey);
	bool bIsReverse = false;

};



/*

USTRUCT(BlueprintType, Blueprintable)
struct FRoadLaneAttributeInt : public FRoadLaneAttributeValue
{
	GENERATED_USTRUCT_BODY()

	virtual ~FRoadLaneAttributeInt() {}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LaneAttribute)
	int Value{};
};

USTRUCT(BlueprintType, Blueprintable)
struct FRoadLaneAttributeFloat : public FRoadLaneAttributeValue
{
	GENERATED_USTRUCT_BODY()

	virtual ~FRoadLaneAttributeFloat() {}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LaneAttribute)
	double Value{};
};


USTRUCT(BlueprintType, Blueprintable)
struct FRoadLaneAttributeString : public FRoadLaneAttributeValue
{
	GENERATED_USTRUCT_BODY()

	virtual ~FRoadLaneAttributeString() {}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LaneAttribute)
	FString Value{};
};

USTRUCT(BlueprintType, Blueprintable)
struct FRoadLaneAttributeName : public FRoadLaneAttributeValue
{
	GENERATED_USTRUCT_BODY()

	virtual ~FRoadLaneAttributeName() {}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LaneAttribute)
	FString Value{};
};
*/

