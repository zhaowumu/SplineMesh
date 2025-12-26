/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */


#include "Utils/CurveUtils.h"
#include "RichCurveEditorModel.h"


constexpr float TangetScale = 1.0;
constexpr float TangentCrossoverThresholdPx = 1.f;
constexpr float TangentLength = 300.0f;



FKeyHandle CurveUtils::GetKeyHandle(const FRichCurve& RichCurve, int KeyIndex)
{
	struct FRichCurve_Private : public FRichCurve
	{
		FKeyHandle GetKeyHandle(int32 KeyIndex) const
		{
			return FRichCurve::GetKeyHandle(KeyIndex);
		}
	};
	return static_cast<const FRichCurve_Private*>(&RichCurve)->GetKeyHandle(KeyIndex);
}

FVector2D CurveUtils::ComputeTangentOffset(float Tangent, float Weight)
{
	const float Angle = FMath::Atan(-Tangent);
	FVector2D Offset;
	FMath::SinCos(&Offset.Y, &Offset.X, Angle);
	Offset *= Weight;

	Offset.X *= TangetScale;
	Offset.Y *= -TangetScale;
	return Offset;
}

void CurveUtils::TangentAndWeightFromOffset(const FVector2D& TangentOffset, float& OutTangent, float& OutWeight)
{
	double X = TangentOffset.X / TangetScale;
	double Y = TangentOffset.Y / TangetScale;

	OutTangent = Y / X;
	OutWeight = FMath::Sqrt(X * X + Y * Y);
}

FVector2D CurveUtils::GetVectorFromSlopeAndLength(float Slope, float Length)
{
	float x = Length / FMath::Sqrt(Slope * Slope + 1.f);
	float y = -Slope * x;
	return FVector2D(x, y);
}


bool CurveUtils::GetArriveTangentOffset(const FRichCurve& RichCurve, const UObject* Owner, int KeyIndex, bool bIsLeftSide, FVector2D& OutTangentOffset)
{
	FKeyHandle KeyHandle = GetKeyHandle(RichCurve, KeyIndex);
	check(KeyHandle != FKeyHandle::Invalid());
	FRichCurveEditorModelRaw CurveModel(const_cast<FRichCurve*>(&RichCurve), (UObject*)Owner);
	TArray<FKeyAttributes> Attributes;
	Attributes.SetNum(1);
	CurveModel.GetKeyAttributes({ KeyHandle }, Attributes);

	if (Attributes[0].HasArriveTangent())
	{
		float ArriveTangent = Attributes[0].GetArriveTangent();
		if (Attributes[0].HasTangentWeightMode() && Attributes[0].HasArriveTangentWeight() &&
			(Attributes[0].GetTangentWeightMode() == RCTWM_WeightedBoth || Attributes[0].GetTangentWeightMode() == RCTWM_WeightedArrive))
		{
			OutTangentOffset = ComputeTangentOffset(ArriveTangent, -Attributes[0].GetArriveTangentWeight());
		}
		else
		{
			OutTangentOffset = GetVectorFromSlopeAndLength(-ArriveTangent, -TangentLength);
		}
		if (bIsLeftSide)
		{
			OutTangentOffset.Y = -OutTangentOffset.Y;
		}
		return true;
	}
	return false;
}

bool CurveUtils::GetLeaveTangentOffset(const FRichCurve& RichCurve, const UObject* Owner, int KeyIndex, bool bIsLeftSide, FVector2D& OutTangentOffset)
{
	FKeyHandle KeyHandle = GetKeyHandle(RichCurve, KeyIndex);
	check(KeyHandle != FKeyHandle::Invalid());
	FRichCurveEditorModelRaw CurveModel(const_cast<FRichCurve*>(&RichCurve), (UObject*)Owner);
	TArray<FKeyAttributes> Attributes;
	Attributes.SetNum(1);
	CurveModel.GetKeyAttributes({ KeyHandle }, Attributes);

	if (Attributes[0].HasLeaveTangent())
	{
		float LeaveTangent = Attributes[0].GetLeaveTangent();
		if (Attributes[0].HasTangentWeightMode() && Attributes[0].HasLeaveTangentWeight() &&
			(Attributes[0].GetTangentWeightMode() == RCTWM_WeightedBoth || Attributes[0].GetTangentWeightMode() == RCTWM_WeightedLeave))
		{
			OutTangentOffset = ComputeTangentOffset(LeaveTangent, Attributes[0].GetLeaveTangentWeight());
		}
		else
		{
			OutTangentOffset = GetVectorFromSlopeAndLength(-LeaveTangent, TangentLength);
		}
		if (bIsLeftSide)
		{
			OutTangentOffset.Y = -OutTangentOffset.Y;
		}
		return true;
	}
	return false;;
}

void CurveUtils::DragTangent(const FRichCurve& RichCurve, const UObject* Owner, int KeyIndex, const FVector2D& InDelta, bool bIsLeftSide, bool bIsArrivel)
{
	FKeyHandle KeyHandle = RichCurve.FindKey(RichCurve.Keys[KeyIndex].Time);
	check(KeyHandle != FKeyHandle::Invalid());
	FRichCurveEditorModelRaw CurveModel(const_cast<FRichCurve*>(&RichCurve), (UObject*)Owner);
	TArray<FKeyAttributes> Attributes;
	Attributes.SetNum(1);
	CurveModel.GetKeyAttributes({ KeyHandle }, Attributes);

	FVector2D Delta = InDelta;

	if (bIsLeftSide)
	{
		Delta.Y = -Delta.Y;
	}

	FKeyAttributes NewAttributes;

	if (Attributes[0].HasArriveTangent() && bIsArrivel)
	{
		const float ArriveTangent = Attributes[0].GetArriveTangent();
		if (Attributes[0].HasTangentWeightMode() && Attributes[0].HasArriveTangentWeight() &&
			(Attributes[0].GetTangentWeightMode() == RCTWM_WeightedBoth || Attributes[0].GetTangentWeightMode() == RCTWM_WeightedArrive))
		{
			FVector2D TangentOffset = ComputeTangentOffset(ArriveTangent, -Attributes[0].GetArriveTangentWeight());
			TangentOffset += Delta;


			// We prevent the handle from crossing over the 0 point... the code handles it but it creates an ugly pop in your curve and it lets your Arrive tangents become Leave tangents which defeats the point.
			TangentOffset.X = FMath::Min(TangentOffset.X, -TangentCrossoverThresholdPx);

			float Tangent, Weight;
			TangentAndWeightFromOffset(TangentOffset, Tangent, Weight);

			NewAttributes.SetArriveTangent(Tangent);
			NewAttributes.SetArriveTangentWeight(Weight);
		}
		else
		{
			FVector2D TangentOffset = GetVectorFromSlopeAndLength(-ArriveTangent, -TangentLength);
			TangentOffset += Delta;

			// We prevent the handle from crossing over the 0 point... the code handles it but it creates an ugly pop in your curve and it lets your Arrive tangents become Leave tangents which defeats the point.
			TangentOffset.X = FMath::Min(TangentOffset.X, -TangentCrossoverThresholdPx);

			const float Tangent = (TangentOffset.Y / TangentOffset.X);

			NewAttributes.SetArriveTangent(Tangent);
		}
	}

	if (Attributes[0].HasLeaveTangent() && !bIsArrivel)
	{
		const float LeaveTangent = Attributes[0].GetLeaveTangent();

		if (Attributes[0].HasTangentWeightMode() && Attributes[0].HasLeaveTangentWeight() &&
			(Attributes[0].GetTangentWeightMode() == RCTWM_WeightedBoth || Attributes[0].GetTangentWeightMode() == RCTWM_WeightedLeave))
		{
			FVector2D TangentOffset = ComputeTangentOffset(LeaveTangent, Attributes[0].GetLeaveTangentWeight());
			TangentOffset += Delta;

			// We prevent the handle from crossing over the 0 point... the code handles it but it creates an ugly pop in your curve and it lets your Arrive tangents become Leave tangents which defeats the point.
			TangentOffset.X = FMath::Max(TangentOffset.X, TangentCrossoverThresholdPx);

			float Tangent, Weight;
			TangentAndWeightFromOffset(TangentOffset, Tangent, Weight);

			NewAttributes.SetLeaveTangent(Tangent);
			NewAttributes.SetLeaveTangentWeight(Weight);
		}
		else
		{
			FVector2D TangentOffset = GetVectorFromSlopeAndLength(-LeaveTangent, TangentLength);
			TangentOffset += Delta;

			// We prevent the handle from crossing over the 0 point... the code handles it but it creates an ugly pop in your curve and it lets your Arrive tangents become Leave tangents which defeats the point.
			TangentOffset.X = FMath::Max(TangentOffset.X, TangentCrossoverThresholdPx);

			const float Tangent = (TangentOffset.Y / TangentOffset.X);

			NewAttributes.SetLeaveTangent(Tangent);
		}
	}

	CurveModel.SetKeyAttributes({ KeyHandle }, { NewAttributes }, EPropertyChangeType::Interactive);
}


// OutValues and OutTimes include TimeEnd and exclude TimeStart
static void CurveToPolyline_RecursiveHelper(const FRichCurve& Curve, const FInterpCurveFloat& DistanceToTimeTable, const FInterpCurveFloat& TimeToDistanceTable, float TimeStart, float TimeEnd, float MaxSquareDistanceFromSpline, TArray<float>& OutValues, TArray<float>& OutTimes)
{
	if (TimeEnd - TimeStart <= 0.0f)
	{
		return;
	}

	float DistancesStart = TimeToDistanceTable.Eval(TimeStart);
	float DistancesEnd = TimeToDistanceTable.Eval(TimeEnd);
	float TimeMiddle = DistanceToTimeTable.Eval(DistancesStart + (DistancesEnd - DistancesStart) * 0.5);

	float Values[3];
	Values[0] = Curve.Eval(TimeStart);
	Values[1] = Curve.Eval(TimeMiddle);
	Values[2] = Curve.Eval(TimeEnd);

	FVector Points[3];
	Points[0] = FVector(TimeStart, Values[0], 0.0);
	Points[1] = FVector(TimeMiddle, Values[1], 0.0);
	Points[2] = FVector(TimeEnd, Values[2], 0.0);

	if (FMath::PointDistToSegmentSquared(Points[1], Points[0], Points[2]) > MaxSquareDistanceFromSpline)
	{
		TArray<float> NewValues[2];
		TArray<float> NewTimes[2];
		CurveToPolyline_RecursiveHelper(Curve, DistanceToTimeTable, TimeToDistanceTable, TimeStart, TimeMiddle, MaxSquareDistanceFromSpline, NewValues[0], NewTimes[0]);
		CurveToPolyline_RecursiveHelper(Curve, DistanceToTimeTable, TimeToDistanceTable, TimeMiddle, TimeEnd, MaxSquareDistanceFromSpline, NewValues[1], NewTimes[1]);

		OutValues.Append(NewValues[0]);
		OutTimes.Append(NewTimes[0]);

		OutValues.Append(NewValues[1]);
		OutTimes.Append(NewTimes[1]);
	}
	else
	{
		OutValues.Add(Values[2]);
		OutTimes.Add(TimeEnd);
	}
}

// This is similar CurveToPolyline_RecursiveHelper() but split segment in center to check S-spline form
static void CurveToPolyline_Helper(const FRichCurve& Curve, const FInterpCurveFloat& DistanceToTimeTable, const FInterpCurveFloat& TimeToDistanceTable, float TimeStart, float TimeEnd, float MaxSquareDistanceFromSpline, TArray<float>& OutValues, TArray<float>& OutTimes)
{
	/*/
	if (TimeEnd - TimeStart <= 0.0f)
	{
		return;
	}
	const double TimeMiddle = TimeStart + (TimeEnd - TimeStart) * 0.5;

	float Samples[3];
	Samples[0] = Curve.Eval(TimeStart);
	Samples[1] = Curve.Eval(TimeMiddle);
	Samples[2] = Curve.Eval(TimeEnd);

	FVector Points[3];
	Points[0] = FVector(TimeStart, Samples[0], 0.0);
	Points[1] = FVector(TimeMiddle, Samples[1], 0.0);
	Points[2] = FVector(TimeEnd, Samples[2], 0.0);

	TArray<float> NewValues[2];
	TArray<float> NewTimes[2];
	CurveToPolyline_RecursiveHelper(Curve, DistanceToTimeTable, TimeStart, TimeMiddle, MaxSquareDistanceFromSpline, NewValues[0], NewTimes[0]);
	CurveToPolyline_RecursiveHelper(Curve, DistanceToTimeTable, TimeMiddle, TimeEnd, MaxSquareDistanceFromSpline, NewValues[1], NewTimes[1]);

	if (NewValues[0].Num() == 1 && NewValues[1].Num() == 1 && FMath::PointDistToSegmentSquared(Points[1], Points[0], Points[2]) < MaxSquareDistanceFromSpline)
	{
		OutValues.Add(NewValues[1][0]);
		OutTimes.Add(NewTimes[1][0]);
	}
	else
	{
		OutValues.Append(NewValues[0]);
		OutTimes.Append(NewTimes[0]);

		OutValues.Append(NewValues[1]);
		OutTimes.Append(NewTimes[1]);
	}
	*/


	CurveToPolyline_RecursiveHelper(Curve, DistanceToTimeTable, TimeToDistanceTable, TimeStart, TimeEnd, MaxSquareDistanceFromSpline, OutValues, OutTimes);
}



bool CurveUtils::CurveToPolyline(const FRichCurve& Curve, float TimeStart, float TimeEnd, float MaxSquareDistanceFromSpline, float Tolerance, int ReparamSteps, TArray<float>& OutValues, TArray<float>& OutTimes)
{
	if (TimeEnd <= TimeStart)
	{
		return false;
	}

	if (Curve.GetNumKeys() == 0)
	{
		return false;
	}

	FInterpCurveFloat DistanceToTimeTable; // Input: distance along curve, output: time.
	FInterpCurveFloat TimeToDistanceTable; // Input: time along curve, output: distance.

	DistanceToTimeTable.Points.Reset(ReparamSteps + 1);
	DistanceToTimeTable.Points.Emplace(0.0, TimeStart, 0.0f, 0.0f, CIM_Linear);

	TimeToDistanceTable.Points.Reset(ReparamSteps + 1);
	TimeToDistanceTable.Points.Emplace(TimeStart, 0.0, 0.0f, 0.0f, CIM_Linear);

	const float TimeStep = (TimeEnd - TimeStart) / ReparamSteps;
	float AccumulatedLength = 0;
	float PreValue = Curve.Eval(TimeStart);
	for (int32 Step = 1; Step <= ReparamSteps; ++Step)
	{
		const float Time = static_cast<float>(Step) * TimeStep + TimeStart;
		const float Value = Curve.Eval(Time);
		AccumulatedLength += FMath::Sqrt(FMath::Square(PreValue - Value) + FMath::Square(TimeStep));
		DistanceToTimeTable.Points.Emplace(AccumulatedLength, Time, 0.0f, 0.0f, CIM_Linear);
		TimeToDistanceTable.Points.Emplace(Time, AccumulatedLength, 0.0f, 0.0f, CIM_Linear);
		PreValue = Value;
	}

	int32 StartKey = 0;
	int32 EndKey = Curve.GetNumKeys() - 1;

	for (int32 KeyIndex = 0; KeyIndex < Curve.GetNumKeys(); ++KeyIndex)
	{
		const double KeyTime = Curve.Keys[KeyIndex].Time;
		if (KeyTime <= TimeStart || FMath::IsNearlyEqual(KeyTime, TimeStart, Tolerance))
		{
			StartKey = KeyIndex;
		}
		if (KeyTime >= TimeEnd || FMath::IsNearlyEqual(KeyTime, TimeEnd, Tolerance))
		{
			EndKey = KeyIndex;
			break;
		}
	}

	const bool bStartIsEqual = FMath::IsNearlyEqual(Curve.Keys[StartKey].Time, TimeStart, Tolerance);
	const bool bEndIsEqual = FMath::IsNearlyEqual(Curve.Keys[EndKey].Time, TimeEnd, Tolerance);

	OutValues.Add(Curve.Eval(TimeStart));
	OutTimes.Add(TimeStart);

	if (StartKey == EndKey)
	{
		OutValues.Add(Curve.Eval(TimeEnd));
		OutTimes.Add(TimeEnd);
		return true;
	}

	if (!bStartIsEqual)
	{
		if (Curve.Keys[StartKey].Time < TimeStart)
		{
			++StartKey;
			if (StartKey == EndKey)
			{
				CurveToPolyline_Helper(Curve, DistanceToTimeTable, TimeToDistanceTable, TimeStart, TimeEnd, MaxSquareDistanceFromSpline, OutValues, OutTimes);
				return true;
			}
			else
			{
				CurveToPolyline_Helper(Curve, DistanceToTimeTable, TimeToDistanceTable, TimeStart, Curve.Keys[StartKey].Time, MaxSquareDistanceFromSpline, OutValues, OutTimes);
			}
		}
		else
		{
			CurveToPolyline_Helper(Curve, DistanceToTimeTable, TimeToDistanceTable, TimeStart, Curve.Keys[StartKey].Time, MaxSquareDistanceFromSpline, OutValues, OutTimes);
		}
	}

	if (!bEndIsEqual)
	{
		if (Curve.Keys[EndKey].Time > TimeEnd)
		{
			--EndKey;
		}
	}

	for (int32 KeyIndex = StartKey; KeyIndex < EndKey; ++KeyIndex) // Add inner segments
	{
		CurveToPolyline_Helper(Curve, DistanceToTimeTable, TimeToDistanceTable, Curve.Keys[KeyIndex].Time, Curve.Keys[KeyIndex + 1].Time, MaxSquareDistanceFromSpline, OutValues, OutTimes);
	}


	if (!bEndIsEqual) // Add last segmant
	{
		CurveToPolyline_Helper(Curve, DistanceToTimeTable, TimeToDistanceTable, Curve.Keys[EndKey].Time, TimeEnd, MaxSquareDistanceFromSpline, OutValues, OutTimes);
	}

	OutValues.Last() = Curve.Eval(TimeEnd);
	OutTimes.Last() = TimeEnd;

	return true;
}

bool CurveUtils::DoesContaintKey(const FRichCurve& Curve, float Time, float Tolerance)
{
	for (const FRichCurveKey& Key : Curve.Keys)
	{
		if (FMath::IsNearlyEqual(Key.Time, Time, Tolerance))
		{
			return true;
		}
	}

	return false;
}




