/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "UnrealDriveTypes.h"
#include "RoadSceneProxy.h"

struct HRoadLaneConnectionProxy : public HRoadLaneVisProxy
{
	DECLARE_HIT_PROXY();

	HRoadLaneConnectionProxy(ULaneConnection* InConnection, int InSectionIndex, int InLaneIndex, EHitProxyPriority InPriority = HPP_UI)
		: HRoadLaneVisProxy(InConnection->GetOwnedRoadSplineChecked(), InSectionIndex, InLaneIndex, InPriority)
		, Connection(InConnection)
	{
	}

	virtual EMouseCursor::Type GetMouseCursor() override
	{
		return EMouseCursor::Hand;
	}

	TWeakObjectPtr<ULaneConnection> Connection;
};


namespace DrawUtils
{
	UNREALDRIVEEDITOR_API void  DrawTriangle(class FPrimitiveDrawInterface* PDI, const FVector& A, const FVector& B, const FVector& C, const class FMaterialRenderProxy* MaterialRenderProxy, uint8 DepthPriorityGroup);
	UNREALDRIVEEDITOR_API void  DrawRoadLaneConnection(bool bIsSuccessorConnection, const FTransform& Transform, const FMaterialRenderProxy* MaterialRenderProxy, class FPrimitiveDrawInterface* PDI, const FSceneView* View, uint8 DepthPriorityGroup);


	UNREALDRIVEEDITOR_API void  DrawLaneBorder(FPrimitiveDrawInterface* PDI, const URoadSplineComponent* SplineComp, int SectionIndex, int LaneIndex, double S0, double S1, const FColor& Color1, const FColor& Color2, uint8 DepthPriorityGroup, float Thickness = 0.0f, float DepthBias = 0.0f, bool bScreenSpace = false);
	UNREALDRIVEEDITOR_API void  DrawLaneBorder(FPrimitiveDrawInterface* PDI, const URoadSplineComponent* SplineComp, int SectionIndex, int LaneIndex, const FColor& Color1, const FColor& Color2, uint8 DepthPriorityGroup, float Thickness = 0.0f, float DepthBias = 0.0f, bool bScreenSpace = false);

	UNREALDRIVEEDITOR_API void  DrawSpline(FPrimitiveDrawInterface* PDI, const URoadSplineComponent* SplineComp, double S0, double S1, const FColor& Color, uint8 DepthPriorityGroup, float Thickness = 0.0f, float DepthBias = 0.0f, bool bScreenSpace = false);

	UNREALDRIVEEDITOR_API void  DrawCrossSpline(FPrimitiveDrawInterface* PDI, const URoadSplineComponent* Spline, float SplineKey, const FColor& Color, uint8 DepthPriorityGroup, float Thickness = 0.0f, float DepthBias = 0.0f, bool bScreenSpace = false);

	UNREALDRIVEEDITOR_API FLinearColor HSVMul(const FLinearColor& Color, float Saturation, float Brightness);

	inline FLinearColor MakeLowAccent(const FLinearColor& Color) 
	{ 
		return HSVMul(Color, 0.5, 0.8); 
	}

} // UnrealDrive