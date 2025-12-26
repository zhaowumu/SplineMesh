/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "RoadLaneAttribute.h"

#include "Animation/AttributeTypes.h"
#include "Animation/IAttributeBlendOperator.h"
#include "Templates/Casts.h"

//#include UE_INLINE_GENERATED_CPP_BY_NAME(AttributeCurve)


#if WITH_EDITOR
const FRoadLaneAttributeValue::FDrawStyle& FRoadLaneAttributeValue::GetDrawStyle() const
{
	static FDrawStyle DefaultStyle{ FDrawStyle::DefaultColor, FDrawStyle::DefaultColor };
	return DefaultStyle;
}
#endif


FRoadLaneAttribute::FRoadLaneAttribute(const FRoadLaneAttribute& OtherCurve)
{
	Keys = OtherCurve.Keys;
	ScriptStruct = OtherCurve.ScriptStruct;
	//Operator = OtherCurve.Operator;
	bShouldInterpolate = OtherCurve.bShouldInterpolate;
}

void FRoadLaneAttribute::SetScriptStruct(const UScriptStruct* InScriptStruct)
{
	if (InScriptStruct && ScriptStruct != InScriptStruct && (Keys.Num() == 0))
	{
		ScriptStruct = InScriptStruct;

		//Operator = UE::Anim::AttributeTypes::GetTypeOperator(ScriptStruct);
		//ensure(Operator);
		//bShouldInterpolate = UE::Anim::AttributeTypes::CanInterpolateType(ScriptStruct);
		bShouldInterpolate = false;
	}
}

bool FRoadLaneAttribute::CanEvaluate() const
{
	return ScriptStruct != nullptr && Keys.Num() > 0;
}

void FRoadLaneAttribute::EvaluateToPtr(const UScriptStruct* InScriptStruct, double SOffset, uint8* InOutDataPtr) const
{
	if (CanEvaluate() && InScriptStruct == ScriptStruct)
	{
		const void* DataPtr = Keys[0].Value.GetPtr<FRoadLaneAttributeValue>();

		/*
		if (bShouldInterpolate)
		{	
			const int32 NumKeys = Keys.Num();
			if (NumKeys == 0)
			{
				ensure(false);
				// If no keys in curve, return the Default value.
			}
			else if (NumKeys < 2 || (SOffset <= Keys[0].SOffset))
			{
				DataPtr = Keys[0].Value.GetPtr<void>();
			}
			else if (SOffset < Keys[NumKeys - 1].SOffset)
			{
				// perform a lower bound to get the second of the interpolation nodes
				int32 first = 1;
				int32 last = NumKeys - 1;
				int32 count = last - first;

				while (count > 0)
				{
					int32 step = count / 2;
					int32 middle = first + step;

					if (SOffset >= Keys[middle].SOffset)
					{
						first = middle + 1;
						count -= step + 1;
					}
					else
					{
						count = step;
					}
				}

				const FRoadLaneAttributeKey& Key = Keys[first - 1];
				const FRoadLaneAttributeKey& Key1 = Keys[first];

				const double Diff = Key1.SOffset - Key.SOffset;
				if (Diff > 0.f)
				{
					const double Alpha = (SOffset - Key.SOffset) / Diff;
					Operator->Interpolate(Key.Value.GetPtr<void>(), Key1.Value.GetPtr<void>(), Alpha, InOutDataPtr);
					return;
				}
				else
				{
					DataPtr = Key.Value.GetPtr<void>();
				}
			}
			else
			{
				// Key is beyon the last point in the curve.  Return it's value
				DataPtr = Keys[Keys.Num() - 1].Value.GetPtr<void>();
			}
		}
		else
		*/
		{
			if (Keys.Num() == 0 || (SOffset < Keys[0].SOffset))
			{
				// If no keys in curve, or bUseDefaultValueBeforeFirstKey is set and the SOffset is before the first key, return the Default value.
			}
			else if (Keys.Num() < 2 || SOffset < Keys[0].SOffset)
			{
				// There is only one key or the SOffset is before the first value. Return the first value
				DataPtr = Keys[0].Value.GetPtr<FRoadLaneAttributeValue>();
			}
			else if (SOffset < Keys[Keys.Num() - 1].SOffset)
			{
				// The key is in the range of Key[0] to Keys[Keys.Num()-1].  Find it by searching
				for (int32 i = 0; i < Keys.Num(); ++i)
				{
					if (SOffset < Keys[i].SOffset)
					{
						DataPtr = Keys[FMath::Max(0, i - 1)].Value.GetPtr<FRoadLaneAttributeValue>();
						break;
					}
				}
			}
			else
			{
				// Key is beyon the last point in the curve.  Return it's value
				DataPtr = Keys[Keys.Num() - 1].Value.GetPtr<FRoadLaneAttributeValue>();
			}
		}

		ScriptStruct->CopyScriptStruct(InOutDataPtr, DataPtr, 1);
	}
}

bool FRoadLaneAttribute::HasAnyData() const
{
	return Keys.Num() != 0;
}

TArray<FRoadLaneAttributeKey>::TConstIterator FRoadLaneAttribute::GetKeyIterator() const
{
	return Keys.CreateConstIterator();
}

int FRoadLaneAttribute::AddKey(double InSOffset, const void* InStructMemory)
{
	int32 Index = 0;
	for (; Index < Keys.Num() && Keys[Index].SOffset < InSOffset; ++Index);

	FRoadLaneAttributeKey& NewKey = Keys.Insert_GetRef(FRoadLaneAttributeKey(InSOffset), Index);
	NewKey.Value.InitializeAsScriptStruct(ScriptStruct, (uint8*)InStructMemory);
	
	return Index;
}


int FRoadLaneAttribute::UpdateOrAddKey(double InSOffset, const void* InStructMemory, double KeySOffsetTolerance)
{
	for (int32 KeyIndex = 0; KeyIndex < Keys.Num(); ++KeyIndex)
	{
		double KeySOffset = Keys[KeyIndex].SOffset;

		if (FMath::IsNearlyEqual(KeySOffset, InSOffset, KeySOffsetTolerance))
		{
			Keys[KeyIndex].Value.InitializeAsScriptStruct(ScriptStruct, (uint8*)InStructMemory);
			return KeyIndex;
		}

		if (KeySOffset > InSOffset)
		{
			// All the rest of the keys exist after the key we want to add
			// so there is no point in searching
			break;
		}
	}

	// A key wasnt found, add it now
	return AddKey(InSOffset, InStructMemory);
}

int FRoadLaneAttribute::FindKey(double KeySOffset, double KeySOffsetTolerance) const
{
	int32 Start = 0;
	int32 End = Keys.Num() - 1;

	// Binary search since the keys are in sorted order
	while (Start <= End)
	{
		int32 TestPos = Start + (End - Start) / 2;
		double TestKeySOffset = Keys[TestPos].SOffset;

		if (FMath::IsNearlyEqual(TestKeySOffset, KeySOffset, KeySOffsetTolerance))
		{
			return TestPos;
		}
		else if (TestKeySOffset < KeySOffset)
		{
			Start = TestPos + 1;
		}
		else
		{
			End = TestPos - 1;
		}
	}

	return INDEX_NONE;
}

int FRoadLaneAttribute::FindKeyBeforeOrAt(double KeySOffset) const
{
	// If there are no keys or the SOffset is before the first key return an invalid handle.
	if (Keys.Num() == 0 || KeySOffset < Keys[0].SOffset)
	{
		return INDEX_NONE;
	}

	// If the SOffset is after or at the last key return the last key.
	if (KeySOffset >= Keys[Keys.Num() - 1].SOffset)
	{
		return Keys.Num() - 1;
	}

	// Otherwise binary search to find the handle of the nearest key at or before the SOffset.
	int32 Start = 0;
	int32 End = Keys.Num() - 1;
	int32 FoundIndex = -1;
	while (FoundIndex < 0)
	{
		int32 TestPos = (Start + End) / 2;
		double TestKeySOffset = Keys[TestPos].SOffset;
		double NextTestKeySOffset = Keys[TestPos + 1].SOffset;
		if (TestKeySOffset <= KeySOffset)
		{
			if (NextTestKeySOffset > KeySOffset)
			{
				FoundIndex = TestPos;
			}
			else
			{
				Start = TestPos + 1;
			}
		}
		else
		{
			End = TestPos;
		}
	}
	return FoundIndex;
}

void FRoadLaneAttribute::RemoveRedundantKeys()
{
	TSet<int32> KeyIndicesToRemove;
	for (int32 KeyIndex = 0; KeyIndex < Keys.Num(); ++KeyIndex)
	{
		if (KeyIndex + 2 < Keys.Num())
		{
			const FRoadLaneAttributeKey& CurrentKey = Keys[KeyIndex];
			const FRoadLaneAttributeKey& NextKeyOne = Keys[KeyIndex + 1];
			const FRoadLaneAttributeKey& NextKeyTwo = Keys[KeyIndex + 2];

			if (ScriptStruct->CompareScriptStruct(CurrentKey.Value.GetPtr<FRoadLaneAttributeValue>(), NextKeyOne.Value.GetPtr<FRoadLaneAttributeValue>(), 0)
				&& ScriptStruct->CompareScriptStruct(NextKeyOne.Value.GetPtr<FRoadLaneAttributeValue>(), NextKeyTwo.Value.GetPtr<FRoadLaneAttributeValue>(), 0))
			{
				KeyIndicesToRemove.Add(KeyIndex + 1);
			}
		}
	}

	if (KeyIndicesToRemove.Num())
	{
	    TArray<FRoadLaneAttributeKey> NewKeys;
		NewKeys.Reserve(Keys.Num() - KeyIndicesToRemove.Num());
	    for (int32 KeyIndex = 0; KeyIndex < Keys.Num(); ++KeyIndex)
	    {
		    if (!KeyIndicesToRemove.Contains(KeyIndex))
		    {
			    NewKeys.Add(Keys[KeyIndex]);
		    }
	    }
    
	    Swap(Keys, NewKeys);
	}

	// If only two keys left and they are identical as well, remove the 2nd one.
	if (Keys.Num() == 2 && ScriptStruct->CompareScriptStruct(Keys[0].Value.GetPtr<FRoadLaneAttributeValue>(), Keys[1].Value.GetPtr<FRoadLaneAttributeValue>(), 0))
	{
		Keys.RemoveAt(1);
	}
}

void FRoadLaneAttribute::Reset()
{
	Keys.Empty();
}

void FRoadLaneAttribute::Trim(double S0, double S1)
{
	int32 StartKey = INDEX_NONE;
	int32 EndKey = INDEX_NONE;
	for (int32 KeyIndex = 0; KeyIndex < Keys.Num(); ++KeyIndex)
	{
		const double KeyS = Keys[KeyIndex].SOffset;
		if (KeyS <= S0)
		{
			StartKey = KeyIndex;
		}
		if (KeyS >= S1)
		{
			EndKey = KeyIndex;
			break;
		}
	}

	if (EndKey != INDEX_NONE && (Keys.Num() - 1 != EndKey))
	{
		Keys.SetNum(EndKey + 1);
	}

	if (StartKey != INDEX_NONE && StartKey != 0)
	{
		Keys.RemoveAt(0, StartKey);
	}

	if (Keys.Num())
	{
		auto& FirstKey = Keys[0];
		if (FirstKey.SOffset < S0)
		{
			FirstKey.SOffset = S0;
		}

		auto& LastKey = Keys[Keys.Num() - 1];
		if (LastKey.SOffset > S1)
		{
			LastKey.SOffset = S1;
		}
	}
};

/*
int FRoadLaneAttribute::FindIndex(double SOffset) const
{
	if (Keys.Num() == 0)
	{
		return INDEX_NONE;
	}

	int RetIndex = 0;
	for (int32 KeyIndex = 0; KeyIndex < Keys.Num(); ++KeyIndex)
	{
		const double KeyS = Keys[KeyIndex].SOffset;
		if (KeyS < SOffset)
		{
			RetIndex = KeyIndex;
		}
		else
		{
			break;
		}
	}
	return RetIndex;
}
*/

/*
void FRoadLaneAttribute::PostSerialize(const FArchive& Ar)
{
	
	if (!ScriptStructPath.IsNull())
	{
		ScriptStruct = Cast<UScriptStruct>(ScriptStructPath.ResolveObject());
		//bShouldInterpolate = UE::Anim::AttributeTypes::CanInterpolateType(ScriptStruct);
	}
	
}
*/

/*

bool FRoadLaneAttribute::Serialize(FArchive& Ar)
{
	Ar << Keys;
	Ar << ScriptStructPath;

	if (!ScriptStructPath.IsNull())
	{
		ScriptStruct = Cast<UScriptStruct>(ScriptStructPath.ResolveObject());

		if (ensure(ScriptStruct))
		{
			bShouldInterpolate = UE::Anim::AttributeTypes::CanInterpolateType(ScriptStruct);

			if (Ar.IsSaving())
			{

				for (auto& Key : Keys)
				{
					ConstCast(ScriptStruct)->SerializeItem(Ar, Key.Value.GetPtr<void>(), nullptr);
				}

			}
			else if (Ar.IsLoading())
			{

				for (auto& Key : Keys)
				{
					Key.Value.Allocate(ScriptStruct);
					ConstCast(ScriptStruct)->SerializeItem(Ar, Key.Value.GetPtr<void>(), nullptr);
				}
			}
		}
	}

	return true;
}

bool FRoadLaneAttribute::ExportTextItem(FString& ValueStr, FRoadLaneAttribute const& DefaultValue, class UObject* Parent, int32 PortFlags, class UObject* ExportRootScope) const
{
	if (const UScriptStruct* StructTypePtr = GetScriptStruct())
	{
		ValueStr += TEXT("(");

		ValueStr += StructTypePtr->GetPathName();

		bool bFirst = true;
		for (auto& Key : Keys)
		{
			if (!bFirst)
			{
				ValueStr += TEXT(",");
			}
			bFirst = false;
			StructTypePtr->ExportText(ValueStr, Key.GetValuePtr<uint8>(), StructTypePtr == DefaultValue.GetScriptStruct() ? DefaultValue.GetMemory() : nullptr, Parent, PortFlags, ExportRootScope);
		}

		ValueStr += TEXT(")");
	}
	else
	{
		ValueStr += TEXT("()");
	}
	return true;
}

bool FRoadLaneAttribute::ImportTextItem(const TCHAR*& Buffer, int32 PortFlags, UObject* Parent, FOutputDevice* ErrorText, FArchive* InSerializingArchive)
{
	if (*Buffer != '(')
	{
		ErrorText->Logf(ELogVerbosity::Warning, TEXT("FRoadLaneAttribute: Missing opening \'(\' while importing property values."));

		// Parse error
		return false;
	}
	Buffer++;

	FNameBuilder StructPathName;
	if (const TCHAR* Result = FPropertyHelpers::ReadToken(Buffer, StructPathName, true))
	{
		Buffer = Result;
	}
	else
	{
		return false;
	}

	if (StructPathName.Len() == 0 || FCString::Stricmp(StructPathName.ToString(), TEXT("None")) == 0)
	{
		return true;
	}

	// Make sure the struct is actually loaded before trying to import the text (this boils down to FindObject if the struct is already loaded).
	// This is needed for user defined structs, BP pin values, config, copy/paste, where there's no guarantee that the referenced struct has actually been loaded yet.
	UScriptStruct* StructTypePtr = LoadObject<UScriptStruct>(nullptr, StructPathName.ToString());
	if (!StructTypePtr)
	{
		return false;
	}

	InitializeAs(StructTypePtr);

	while (*Buffer != ')')
	{

		if (const TCHAR* Result = StructTypePtr->ImportText(Buffer, GetMutableMemory(), Parent, PortFlags, ErrorText, [StructTypePtr]() { return StructTypePtr->GetName(); }))
		{
			Buffer = Result;
		}
		else
		{
			return false;
		}

		if (*Buffer == ',')
		{
			Buffer++;
		}
		else if (*Buffer != ')')
		{
			ErrorText->Logf(ELogVerbosity::Warning, TEXT("FRoadLaneAttribute: Missing \')\' while importing property values."));
			return false;
		}
	}


	return true;
}

*/