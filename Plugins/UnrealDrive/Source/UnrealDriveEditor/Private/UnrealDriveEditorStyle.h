/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "Styling/SlateStyle.h"


class FUnrealDriveEditorStyle
    : public FSlateStyleSet
{
public:
	static FUnrealDriveEditorStyle& Get();

protected:
	friend class FUnrealDriveEditorModule;

	static void Register();
	static void Unregister();

private:
	FUnrealDriveEditorStyle();
};
