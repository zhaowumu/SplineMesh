/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "CoreMinimal.h"
#include "IDetailChildrenBuilder.h"

namespace PropertyEditorUtils 
{
	UNREALDRIVEEDITOR_API void AddTextRow(IDetailChildrenBuilder& ChildrenBuilder, const FText& Search, const FText& Name, const FText& Value);

	UNREALDRIVEEDITOR_API void AddTextRow(IDetailChildrenBuilder& ChildrenBuilder, const FText& Search, const FText& Name, TFunction<FText()>&& Value);

	template <typename T>
	T * GetFirstData(const TSharedPtr<IPropertyHandle>& PropertyHandle)
	{
		TArray<void*> RawData;
		PropertyHandle->AccessRawData(RawData);
		if (ensure(RawData.Num()))
		{
			return reinterpret_cast<T*>(RawData[0]);
		}
		return nullptr;
	}


} // PropertyEditorUtils