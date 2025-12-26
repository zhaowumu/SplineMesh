/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "CoreMinimal.h"
#include "Curves/RichCurve.h"

//#include "Kismet/BlueprintFunctionLibrary.h"
//#include "CurveUtils.generated.h"


namespace CurveUtils 
{
	UNREALDRIVEEDITOR_API FKeyHandle GetKeyHandle(const FRichCurve& RichCurve, int KeyIndex);
	UNREALDRIVEEDITOR_API FVector2D ComputeTangentOffset(float Tangent, float Weight);
	UNREALDRIVEEDITOR_API void TangentAndWeightFromOffset(const FVector2D& TangentOffset, float& OutTangent, float& OutWeight);
	UNREALDRIVEEDITOR_API FVector2D  GetVectorFromSlopeAndLength(float Slope, float Length);
	UNREALDRIVEEDITOR_API bool GetArriveTangentOffset(const FRichCurve& RichCurve, const UObject* Owner, int KeyIndex, bool bIsLeftSide, FVector2D& OutTangentOffset);
	UNREALDRIVEEDITOR_API bool GetLeaveTangentOffset(const FRichCurve& RichCurve, const UObject* Owner, int KeyIndex, bool bIsLeftSide, FVector2D& OutTangentOffset);
	UNREALDRIVEEDITOR_API void DragTangent(const FRichCurve& RichCurve, const UObject* Owner, int KeyIndex, const FVector2D& Delta, bool bIsLeftSide, bool bIsArrivel);
	UNREALDRIVEEDITOR_API bool CurveToPolyline(const FRichCurve& Curve, float TimeStart, float TimeEnd, float MaxSquareDistanceFromCurve, float Tolerance, int ReparamSteps, TArray<float>& OutValues, TArray<float>& OutTimes);
	UNREALDRIVEEDITOR_API bool DoesContaintKey(const FRichCurve& Curve, float Time, float Tolerance);
} // CurveUtils

// For  debug
/*
UCLASS()
class UNREALDRIVEEDITOR_API UOpUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable)
	static bool BakeCurve(const UCurveFloat* SrcCurve, UCurveFloat* TargetCurve, float TimeStart, float TimeEnd, float MaxSquareDistanceFromCurve, float Tolerance, int ReparamSteps = 200);
};
*/

