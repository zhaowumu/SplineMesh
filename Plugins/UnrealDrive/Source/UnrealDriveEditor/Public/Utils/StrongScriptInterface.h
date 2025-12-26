/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "UObject/UObjectGlobals.h"
#include "Templates/Casts.h"
#include "Templates/UnrealTemplate.h"

#include <type_traits>

 /**
  * This utility class stores the FProperty data for an interface property used in both blueprint and native code.
  * For objects natively implementing an interface, ObjectPointer and InterfacePointer point to different locations in the same UObject.
  * For objects that only implement an interface in blueprint, only ObjectPointer will be set because there is no native representation.
  * UClass::ImplementsInterface can be used along with Execute_ event wrappers to properly handle BP-implemented interfaces.
  */
class  FStrongScriptInterface
{
private:
	/**
	 * A pointer to a UObject that implements an interface.
	 */
	TStrongObjectPtr<UObject>	ObjectPointer = nullptr;

	/**
	 * For native interfaces, pointer to the location of the interface object within the UObject referenced by ObjectPointer.
	 */
	void* InterfacePointer = nullptr;

protected:
	/**
	 * Serialize ScriptInterface
	 */
	//UNREALDRIVEEDITOR_API FArchive& Serialize(FArchive& Ar, class UClass* InterfaceType);

public:
	/**
	 * Default constructor
	 */
	FStrongScriptInterface() = default;

	/**
	 * Construction from object and interface
	 */
	FStrongScriptInterface(UObject* InObjectPointer, void* InInterfacePointer)
		: ObjectPointer(InObjectPointer), InterfacePointer(InInterfacePointer)
	{
	}

	/**
	 * Copyable
	 */
	FStrongScriptInterface(const FStrongScriptInterface&) = default;
	FStrongScriptInterface& operator=(const FStrongScriptInterface&) = default;

	/**
	 * Returns the ObjectPointer contained by this FStrongScriptInterface
	 */
	FORCEINLINE UObject* GetObject() const
	{
		return ObjectPointer.Get();
	}

	/**
	 * Returns the pointer to the native interface if it is valid
	 */
	FORCEINLINE void* GetInterface() const
	{
		// Only access the InterfacePointer if we have a valid ObjectPointer. This is necessary because garbage collection may only clear the ObjectPointer.
		// This will also return null for objects that only implement the interface in a blueprint class because there is no native representation.
		return ObjectPointer ? InterfacePointer : nullptr;
	}

	/**
	 * Sets the value of the ObjectPointer for this FStrongScriptInterface
	 */
	FORCEINLINE void SetObject(UObject* InObjectPointer)
	{
		ObjectPointer = TStrongObjectPtr<UObject>(InObjectPointer);
		if (ObjectPointer == nullptr)
		{
			SetInterface(nullptr);
		}
	}

	/**
	 * Sets the value of the InterfacePointer for this FStrongScriptInterface
	 */
	FORCEINLINE void SetInterface(void* InInterfacePointer)
	{
		InterfacePointer = InInterfacePointer;
	}

	/**
	 * Comparison operator, taking a reference to another FStrongScriptInterface
	 */
	FORCEINLINE bool operator==(const FStrongScriptInterface& Other) const
	{
		return GetInterface() == Other.GetInterface() && ObjectPointer.Get() == Other.GetObject();
	}
	FORCEINLINE bool operator!=(const FStrongScriptInterface& Other) const
	{
		return GetInterface() != Other.GetInterface() || ObjectPointer.Get() != Other.GetObject();
	}

	//void AddReferencedObjects(FReferenceCollector& Collector)
	//{
	//	Collector.AddReferencedObject(ObjectPointer.Get());
	//}

	friend inline uint32 GetTypeHash(const FStrongScriptInterface& Instance)
	{
		return GetTypeHash(Instance.InterfacePointer);
	}
};



template<> struct TIsPODType<class FStrongScriptInterface> { enum { Value = true }; };
template<> struct TIsZeroConstructType<class FStrongScriptInterface> { enum { Value = true }; };

/**
 * Templated version of FStrongScriptInterface, which provides accessors and operators for referencing the interface portion of an object implementing an interface.
 * This type is only useful with native interfaces, UClass::ImplementsInterface should be used to check for blueprint interfaces.
 */
template <typename InInterfaceType>
class TStrongScriptInterface : public FStrongScriptInterface
{
public:
	using InterfaceType = InInterfaceType;
	using UObjectType = typename TCopyQualifiersFromTo<InterfaceType, UObject>::Type;

	/**
	 * Default constructor
	 */
	TStrongScriptInterface() = default;

	/**
	 * Construction from nullptr
	 */
	TStrongScriptInterface(TYPE_OF_NULLPTR) {}

	/**
	 * Construction from an object type that may natively implement InterfaceType
	 */
	template <
		typename U
		UE_REQUIRES(std::is_convertible_v<U, UObjectType*>)
	>
	FORCEINLINE TStrongScriptInterface(U&& Source)
	{
		// Always set the object
		UObjectType* SourceObject = ImplicitConv<UObjectType*>(Source);
		SetObject(SourceObject);

		if constexpr (std::is_base_of<InInterfaceType, std::remove_pointer_t<std::remove_reference_t<U>>>::value)
		{
			// If we know at compile time that we got passed some subclass of InInterfaceType, set it
			// without a cast (avoiding the cast also allows us to not require linking to its module)
			SetInterface(Source);
		}
		else
		{
			// Tries to set the native interface instance, this will set it to null for BP-implemented interfaces
			InInterfaceType* SourceInterface = Cast<InInterfaceType>(SourceObject);
			SetInterface(SourceInterface);
		}
	}

	/**
	 * Construction from another script interface of a compatible interface type
	 */
	template <
		typename OtherInterfaceType
		UE_REQUIRES(std::is_convertible_v<OtherInterfaceType*, InInterfaceType*>)
	>
	FORCEINLINE TStrongScriptInterface(const TStrongScriptInterface<OtherInterfaceType>& Other)
	{
		SetObject(Other.GetObject());

		InInterfaceType* SourceInterface = Other.GetInterface();
		SetInterface(SourceInterface);
	}

	/**
	 * Assignment from an object type that may natively implement InterfaceType
	 */
	template <typename ObjectType>
	TStrongScriptInterface(TObjectPtr<ObjectType> SourceObject)
	{
		// Always set the object
		SetObject(SourceObject);

		if constexpr (std::is_base_of<InInterfaceType, ObjectType>::value)
		{
			// If we know at compile time that we got passed some subclass of InInterfaceType, set it
			// without a cast (avoiding the cast also allows us to not require linking to its module)
			SetInterface(SourceObject.Get());
		}
		else
		{
			// Tries to set the native interface instance, this will set it to null for BP-implemented interfaces
			InInterfaceType* SourceInterface = Cast<InInterfaceType>(ToRawPtr(SourceObject));
			SetInterface(SourceInterface);
		}
	}

	/**
	 * Copyable
	 */
	TStrongScriptInterface(const TStrongScriptInterface&) = default;
	TStrongScriptInterface& operator=(const TStrongScriptInterface&) = default;

	/**
	 * Assignment from nullptr
	 */
	TStrongScriptInterface& operator=(TYPE_OF_NULLPTR)
	{
		*this = TStrongScriptInterface();
		return *this;
	}

	/**
	 * Assignment from an object type that may natively implement InterfaceType
	 */
	template <
		typename U
		UE_REQUIRES(std::is_convertible_v<U, UObjectType*>)
	>
	TStrongScriptInterface& operator=(U&& Source)
	{
		*this = TStrongScriptInterface(Source);
		return *this;
	}

	/**
	 * Assignment from another script interface of a compatible interface type
	 */
	template <
		typename OtherInterfaceType
		UE_REQUIRES(std::is_convertible_v<OtherInterfaceType*, InInterfaceType*>)
	>
	TStrongScriptInterface& operator=(const TStrongScriptInterface<OtherInterfaceType>& Other)
	{
		*this = TStrongScriptInterface(Other);
		return *this;
	}

	/**
	 * Assignment from an object type that may natively implement InterfaceType
	 */
	template <typename ObjectType>
	TStrongScriptInterface& operator=(TObjectPtr<ObjectType> SourceObject)
	{
		*this = TStrongScriptInterface(SourceObject);
		return *this;
	}

	/**
	 * Comparison operator, taking a pointer to InterfaceType
	 */
	template <
		typename OtherInterface
		UE_REQUIRES(std::is_convertible_v<OtherInterface*, InInterfaceType*>)
	>
	FORCEINLINE bool operator==(const OtherInterface* Other) const
	{
		return GetInterface() == Other;
	}
	template <
		typename OtherInterface
		UE_REQUIRES(std::is_convertible_v<OtherInterface*, InInterfaceType*>)
	>
	FORCEINLINE bool operator!=(const OtherInterface* Other) const
	{
		return GetInterface() != Other;
	}

	/**
	 * Comparison operator, taking a reference to another TStrongScriptInterface
	 */
	FORCEINLINE bool operator==(const TStrongScriptInterface& Other) const
	{
		return GetInterface() == Other.GetInterface() && GetObject() == Other.GetObject();
	}
	FORCEINLINE bool operator!=(const TStrongScriptInterface& Other) const
	{
		return GetInterface() != Other.GetInterface() || GetObject() != Other.GetObject();
	}

	/**
	 * Comparison operator, taking a nullptr
	 */
	FORCEINLINE bool operator==(TYPE_OF_NULLPTR) const
	{
		return GetInterface() == nullptr;
	}
	FORCEINLINE bool operator!=(TYPE_OF_NULLPTR) const
	{
		return GetInterface() != nullptr;
	}

	/**
	 * Member access operator.  Provides transparent access to the native interface pointer contained by this TStrongScriptInterface
	 */
	FORCEINLINE InInterfaceType* operator->() const
	{
		return GetInterface();
	}

	/**
	 * Dereference operator.  Provides transparent access to the native interface pointer contained by this TStrongScriptInterface
	 *
	 * @return	a reference (of type InterfaceType) to the object pointed to by InterfacePointer
	 */
	FORCEINLINE InInterfaceType& operator*() const
	{
		return *GetInterface();
	}

	/**
	 * Returns the pointer to the interface
	 */
	FORCEINLINE InInterfaceType* GetInterface() const
	{
		return (InInterfaceType*)FStrongScriptInterface::GetInterface();
	}

	/**
	 * Sets the value of the InterfacePointer for this TStrongScriptInterface
	 */
	FORCEINLINE void SetInterface(InInterfaceType* InInterfacePointer)
	{
		FStrongScriptInterface::SetInterface((void*)InInterfacePointer);
	}

	/**
	 * Returns the ObjectPointer contained by this TStrongScriptInterface
	 */
	FORCEINLINE UObjectType* GetObject() const
	{
		return FStrongScriptInterface::GetObject();
	}

	/**
	 * Returns the ObjectPointer contained by this TStrongScriptInterface
	 */
	//FORCEINLINE TObjectPtr<UObjectType>& GetObjectRef()
	//{
	//	return *(TObjectPtr<UObjectType>*) & FStrongScriptInterface::GetObjectRef();
	//}

	/**
	 * Sets the value of the ObjectPointer for this TStrongScriptInterface
	 */
	FORCEINLINE void SetObject(UObjectType* InObjectPointer)
	{
		FStrongScriptInterface::SetObject(const_cast<UObject*>(InObjectPointer));
	}

	/**
	 * Boolean operator, returns true if this object natively implements InterfaceType.
	 * This will return false for objects that only implement the interface in blueprint classes.
	 */
	FORCEINLINE explicit operator bool() const
	{
		return GetInterface() != nullptr;
	}

	friend FArchive& operator<<(FArchive& Ar, TStrongScriptInterface& Interface)
	{
		return Interface.Serialize(Ar, InInterfaceType::UClassType::StaticClass());
	}
};
