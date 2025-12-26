/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Curves/IndexedCurve.h"
#include "Serialization/Archive.h"
#include "StructUtils/InstancedStruct.h"
#include "RoadLaneAttribute.generated.h"

USTRUCT()
struct UNREALDRIVE_API FRoadLaneAttributeValue
{
	GENERATED_USTRUCT_BODY()
public:

	virtual ~FRoadLaneAttributeValue() {}

#if WITH_EDITOR
	struct FDrawStyle
	{
		FDrawStyle() = default;
		FDrawStyle(const FColor& Color1, const FColor& Color2)
			: Color1(Color1)
			, Color2(Color2)
		{
		}
		FDrawStyle(const FColor& Color)
			: Color1(Color)
			, Color2(Color)
		{
		}


		static constexpr FColor DefaultColor = FColor(129, 106, 196);

		// Segment odd color
		FColor Color1 = DefaultColor;

		// Segment even color
		FColor Color2 = DefaultColor;
	};

	virtual const FDrawStyle& GetDrawStyle() const;
#endif

};


USTRUCT(BlueprintType, Blueprintable)
struct UNREALDRIVE_API FRoadLaneAttributeKey
{
	GENERATED_USTRUCT_BODY()
public:
	 
	FRoadLaneAttributeKey() = default;
	FRoadLaneAttributeKey(double InSOffset) : SOffset(InSOffset) {}
	virtual ~FRoadLaneAttributeKey() {}

	/** The keyed SOffset */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = LaneAttribute)
	double SOffset;

	template<typename AttributeType>
	const AttributeType& GetValue() const
	{
		return Value.Get<AttributeType>();
	}

	template<typename AttributeType>
	const AttributeType* GetValuePtr() const
	{
		return Value.GetPtr<AttributeType>();
	}

	template<typename AttributeType>
	AttributeType& GetValue()
	{
		return Value.GetMutable<AttributeType>();
	}

	template<typename AttributeType>
	AttributeType* GetValuePtr()
	{
		return Value.GetMutablePtr<AttributeType>();
	}


	bool operator < (const FRoadLaneAttributeKey& Other) const
	{
		return SOffset < Other.SOffset;
	}
	

//protected:
	/** Value for this key, populated by FRoadLaneAttribute during serialization */
	UPROPERTY(EditAnywhere, Category = LaneAttribute)
	TInstancedStruct<FRoadLaneAttributeValue> Value;

	//friend struct FRoadLaneAttribute;
};

/**
 * FRoadLaneAttribute.
 * Lane attributes are arbitrary metadata that can be assigned along the road lane.
 * See https://unrealdrive.readthedocs.io/en/latest/RoadModel.html#lane-attributes
 */
USTRUCT(BlueprintType)
struct UNREALDRIVE_API FRoadLaneAttribute 
{
	GENERATED_USTRUCT_BODY()

public:
	FRoadLaneAttribute() 
		: ScriptStruct(nullptr)
		, bShouldInterpolate(false) 
		//, Operator(nullptr)
	{}
	FRoadLaneAttribute(UScriptStruct* InScriptStruct) 
		: ScriptStruct(InScriptStruct)
		, bShouldInterpolate(false)
		//, Operator(nullptr)
	{}

	FRoadLaneAttribute(const FRoadLaneAttribute& OtherCurve);

	/** Virtual destructor. */
	virtual ~FRoadLaneAttribute() { }

	
	/** Sets the underlying type for the curve, only possible when not containing any keys (see ::Reset) */
	void SetScriptStruct(const UScriptStruct* InScriptStruct);
	const UScriptStruct* GetScriptStruct() const { return ScriptStruct; }

	/** Whether or not the curve can be evaluated, based upon having a valid type and any keys */
	bool CanEvaluate() const;

	/** Evaluate the curve keys into a temporary value container */
	template<typename AttributeType>
	AttributeType Evaluate(double SOffset) const
	{
		AttributeType EvaluatedValue;
		EvaluateToPtr(AttributeType::StaticStruct(), SOffset, (uint8*)&EvaluatedValue);
		return EvaluatedValue;
	}

	/** Check whether this curve has any data or not */
	bool HasAnyData() const;

	/** Removes all key data */
	void Reset();

	/** Const iterator for the keys, so the indices and handles stay valid */
	TArray<FRoadLaneAttributeKey>::TConstIterator GetKeyIterator() const;

	/** Add a new typed key to the curve with the supplied SOffset and Value. */
	template<typename AttributeType>
	int AddTypedKey(double InSOffset, const AttributeType& InValue)
	{
		check(AttributeType::StaticStruct() == ScriptStruct); 
		return AddKey(InSOffset, &InValue);
	}

	/** Finds the key at InSOffset, and updates its typed value. If it can't find the key within the KeySOffsetTolerance, it adds one at that SOffset */
	template<typename AttributeType>
	int UpdateOrAddTypedKey(double InSOffset, const AttributeType& InValue, double KeySOffsetTolerance = UE_KINDA_SMALL_NUMBER)
	{
		check(AttributeType::StaticStruct() == ScriptStruct);
		return UpdateOrAddKey(InSOffset, &InValue, KeySOffsetTolerance);
	}

	/** Finds the key at InSOffset, and updates its typed value. If it can't find the key within the KeySOffsetTolerance, it adds one at that SOffset */
	int UpdateOrAddTypedKey(double InSOffset, const void* InValue, const UScriptStruct* ValueType, double KeySOffsetTolerance = UE_KINDA_SMALL_NUMBER)
	{
		check(ValueType == ScriptStruct);
		return UpdateOrAddKey(InSOffset, InValue, KeySOffsetTolerance);
	}
			

	/** Finds the key at KeySOffset and returns its handle. If it can't find the key within the KeySOffsetTolerance, it will return an invalid handle */
	int FindKey(double KeySOffset, double KeySOffsetTolerance = UE_KINDA_SMALL_NUMBER) const;

	/** Gets the handle for the last key which is at or before the SOffset requested.  If there are no keys at or before the requested SOffset, an invalid handle is returned. */
	int FindKeyBeforeOrAt(double KeySOffset) const;

	/** Tries to reduce the number of keys required for accurate evaluation (zero error threshold) */
	void RemoveRedundantKeys();

	void Trim(double S0, double S1);

public:
	/** The keys, ordered by SOffset */
	UPROPERTY(EditAnywhere, Category = "Custom Attributes")
	TArray<FRoadLaneAttributeKey> Keys;

protected:
	/** Evaluate the curve keys into the provided memory (should be appropriatedly sized) */
	void EvaluateToPtr(const UScriptStruct* InScriptStruct, double SOffset, uint8* InOutDataPtr) const;

	/** Finds the key at InSOffset, and updates its typed value. If it can't find the key within the KeySOffsetTolerance, it adds one at that SOffset */
	int UpdateOrAddKey(double InSOffset, const void* InStructMemory, double KeySOffsetTolerance = UE_KINDA_SMALL_NUMBER);

	/** Add a new raw memory key (should be appropriately sized) to the curve with the supplied SOffset and Value. */
	int AddKey(double InSOffset, const void* InStructMemory);

protected:

	/* Transient UScriptStruct instance representing the underlying value type for the curve */
	UPROPERTY(EditAnywhere, Category = "Custom Attributes")
	TObjectPtr<const UScriptStruct> ScriptStruct;

	/** Whether or not to interpolate between keys of ScripStruct type */
	UPROPERTY(EditAnywhere, Transient, Category = "Custom Attributes")
	bool bShouldInterpolate;

	/** Operator instanced used for interpolating between keys */
	//const UE::Anim::IAttributeBlendOperator* Operator;

};