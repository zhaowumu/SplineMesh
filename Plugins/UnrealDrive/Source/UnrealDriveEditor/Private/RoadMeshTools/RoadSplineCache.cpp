/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "RoadMeshTools/RoadSplineCache.h"
#include "UnrealDrive.h"

using namespace UnrealDrive;

FRoadSplineCache::FRoadSplineCache(const URoadSplineComponent* RoadSpline)
{
	OriginSpline = RoadSpline;
	SplineCurves = OriginSpline->SplineCurves;
	DefaultUpVector = OriginSpline->DefaultUpVector;
	bIsClosedLoop = OriginSpline->IsClosedLoop();
	ReparamStepsPerSegment = OriginSpline->ReparamStepsPerSegment;
	bStationaryEndpoints = OriginSpline->bStationaryEndpoints;
	ComponentToWorld = OriginSpline->GetComponentTransform();
	RoadLayout = RoadSpline->RoadLayout;
	bSkipProcrdureGeneration = RoadSpline->bSkipProcrdureGeneration;
	MaterialPriority = RoadSpline->MaterialPriority;

	RoadLayout.UpdateLayout(nullptr);
}

void FRoadSplineCache::UpdateSplinesCurves2d()
{
	SplinesCurves2d = SplineCurves;
	for (auto& Pt : SplinesCurves2d.Position.Points)
	{
		Pt.OutVal.Z = 0;
		Pt.ArriveTangent.Z = 0;
		Pt.LeaveTangent.Z = 0;
	}

	SplinesCurves2d.UpdateSpline(bIsClosedLoop, bStationaryEndpoints, ReparamStepsPerSegment, false, 0.0, ComponentToWorld.GetScale3D());
}

FRoadPosition FRoadSplineCache::GetRoadPosition(int SectionIndex, int LaneIndex, double Alpha, double SOffset, ESplineCoordinateSpace::Type CoordinateSpace) const
{
	const double ROffset = RoadLayout.Sections[SectionIndex].EvalLaneROffset(LaneIndex, SOffset, Alpha) + RoadLayout.EvalROffset(SOffset);
	return GetRoadPosition(SOffset, ROffset, CoordinateSpace);
}

FRoadPosition FRoadSplineCache::GetRoadPosition(double SOffset, double ROffset, ESplineCoordinateSpace::Type CoordinateSpace) const
{
	const float Param = SplineCurves.ReparamTable.Eval(SOffset, 0.0f);
	const FVector RightVector = GetRightVectorAtSplineInputKey(Param, CoordinateSpace);

	FRoadPosition Pos;
	Pos.Location = GetLocationAtSplineInputKey(Param, CoordinateSpace) + RightVector * ROffset;
	Pos.Quat = GetQuaternionAtSplineInputKey(Param, CoordinateSpace);
	Pos.SOffset = SOffset;
	Pos.ROffset = ROffset;

	return Pos;
}

static bool IsEqual(const FRoadPosition& A, const FRoadPosition& B)
{
	return  (A.Location - B.Location).IsNearlyZero(UE_SMALL_NUMBER) && FMath::IsNearlyEqual(A.SOffset, B.SOffset, UE_SMALL_NUMBER);
}

bool FRoadSplineCache::ConvertSplineToPolyline_InDistanceRange2(int SectionIndex, int LaneIndex, TAlpaFunction AlphaFunc, ESplineCoordinateSpace::Type CoordinateSpace, double InMaxSquareDistanceFromSpline, double InMinSegmentLength, double RangeStart, double RangeEnd, TArray<FRoadPosition>& OutPoints, bool bAllowWrappingIfClosed) const
{
	OutPoints.SetNum(0, EAllowShrinking::No);

	if (SectionIndex < 0 || SectionIndex >= RoadLayout.Sections.Num())
	{
		return false;
	}

	auto& Section = RoadLayout.Sections[SectionIndex];

	if (!Section.CheckLaneIndex(LaneIndex) && LaneIndex != LANE_INDEX_NONE)
	{
		return false;
	}

	const int32 NumPoints = SplineCurves.Position.Points.Num();
	if (NumPoints == 0)
	{
		return false;
	}
	const int32 NumSegments = GetNumberOfSplineSegments();

	float SplineLength = SplineCurves.GetSplineLength();
	if (SplineLength <= 0)
	{
		OutPoints.Add(GetRoadPosition(SectionIndex, LaneIndex, AlphaFunc(0.0), 0.0, CoordinateSpace));
		return false;
	}

	// Sanitize the sampling tolerance
	const float MaxSquareDistanceFromSpline = FMath::Max(UE_SMALL_NUMBER, InMaxSquareDistanceFromSpline);
	const float MinSegmentLength = FMath::Max(UE_SMALL_NUMBER, InMinSegmentLength);

	// Sanitize range and mark whether the range wraps through 0
	bool bNeedsWrap = false;
	if (!bIsClosedLoop || !bAllowWrappingIfClosed)
	{
		RangeStart = FMath::Clamp(RangeStart, 0, SplineLength);
		RangeEnd = FMath::Clamp(RangeEnd, 0, SplineLength);
	}
	else if (RangeStart < 0 || RangeEnd > SplineLength)
	{
		bNeedsWrap = true;
	}
	if (RangeStart > RangeEnd)
	{
		return false;
	}

	// expect at least 2 points per segment covered
	int32 EstimatedPoints = 2 * NumSegments * static_cast<int32>((RangeEnd - RangeStart) / SplineLength);
	OutPoints.Empty();
	OutPoints.Reserve(EstimatedPoints);

	if (RangeStart == RangeEnd)
	{
		OutPoints.Add(GetRoadPosition(SectionIndex, LaneIndex, AlphaFunc(RangeStart), RangeStart, CoordinateSpace));
		return true;
	}

	// If we need to wrap around, break the wrapped segments into non-wrapped parts and add each part separately
	if (bNeedsWrap)
	{
		float TotalRange = RangeEnd - RangeStart;
		auto WrapDistance = [SplineLength](float Distance, int32& LoopIdx) -> float
			{
				LoopIdx = FMath::FloorToInt32(Distance / SplineLength);
				float WrappedDistance = FMath::Fmod(Distance, SplineLength);
				if (WrappedDistance < 0)
				{
					WrappedDistance += SplineLength;
				}
				return WrappedDistance;
			};
		int32 StartLoopIdx, EndLoopIdx;
		float WrappedStart = WrapDistance(RangeStart, StartLoopIdx);
		float WrappedEnd = WrapDistance(RangeEnd, EndLoopIdx);
		float WrappedLoc = WrappedStart;
		bool bHasAdded = false;
		for (int32 LoopIdx = StartLoopIdx; LoopIdx <= EndLoopIdx; ++LoopIdx)
		{
			if (bHasAdded && ensure(OutPoints.Num()))
			{
				OutPoints.RemoveAt(OutPoints.Num() - 1, EAllowShrinking::No);
			}
			float EndLoc = LoopIdx == EndLoopIdx ? WrappedEnd : SplineLength;

			TArray<FRoadPosition> Points;
			ConvertSplineToPolyline_InDistanceRange2(SectionIndex, LaneIndex, AlphaFunc, CoordinateSpace, MaxSquareDistanceFromSpline, MinSegmentLength, WrappedLoc, EndLoc, Points, false);
			OutPoints.Append(Points);

			bHasAdded = true;
			WrappedLoc = 0;
		}
		return bHasAdded;
	} // end of the wrap-around case, after this values will be in the normal range


	TArray<double> Segments;
	FindAllSegmantsForLane(SectionIndex, LaneIndex, RangeStart, RangeEnd, Segments);

	TArray<FRoadPosition> NewPoints;
	for (int PointIndex = 1; PointIndex < Segments.Num(); ++PointIndex)
	{
		// Get the segment range as distances, clipped with the input range
		double StartDist = Segments[PointIndex - 1];
		double StopDist = Segments[PointIndex];
		bool bIsLast = (PointIndex == (Segments.Num() - 1));

		const int32 NumLines = 2; // Dichotomic subdivision of the spline segment
		double Dist = StopDist - StartDist;
		double SubstepSize = Dist / NumLines;
		if (SubstepSize == 0.0)
		{
			// There is no distance to cover, so handle the segment with a single point (or nothing, if this isn't the very last point)
			if (bIsLast)
			{
				OutPoints.Add(GetRoadPosition(SectionIndex, LaneIndex, AlphaFunc(StopDist), StopDist, CoordinateSpace));
			}
			continue;
		}

		double SubstepStartDist = StartDist;
		for (int32 i = 0; i < NumLines; ++i)
		{
			double SubstepEndDist = SubstepStartDist + SubstepSize;
			NewPoints.Reset();
			// Recursively sub-divide each segment until the requested precision is reached :
			if (DivideSplineIntoPolylineRecursiveWithDistancesHelper2(SectionIndex, LaneIndex, AlphaFunc, SubstepStartDist, SubstepEndDist, CoordinateSpace, MaxSquareDistanceFromSpline, MinSegmentLength, NewPoints))
			{
				if (OutPoints.Num() > 0)
				{
					check(IsEqual(OutPoints.Last(), NewPoints[0])); // our last point must be the same as the new segment's first
					OutPoints.RemoveAt(OutPoints.Num() - 1);
				}
				OutPoints.Append(NewPoints);
			}

			SubstepStartDist = SubstepEndDist;
		}
	}

	return !OutPoints.IsEmpty();
}

bool FRoadSplineCache::DivideSplineIntoPolylineRecursiveWithDistancesHelper2(int SectionIndex, int LaneIndex, const TAlpaFunction& AlphaFunc, double StartDistanceAlongSpline, double EndDistanceAlongSpline, ESplineCoordinateSpace::Type CoordinateSpace, double MaxSquareDistanceFromSpline, double MinSegmentLength, TArray<FRoadPosition>& OutPoints) const
{
	auto& Section = RoadLayout.Sections[SectionIndex];

	double Dist = EndDistanceAlongSpline - StartDistanceAlongSpline;
	if (Dist <= 0.0f)
	{
		return false;
	}
	double MiddlePointDistancAlongSpline = StartDistanceAlongSpline + Dist / 2.0f;
	FRoadPosition Samples[3];
	Samples[0] = GetRoadPosition(SectionIndex, LaneIndex, AlphaFunc(StartDistanceAlongSpline), StartDistanceAlongSpline, CoordinateSpace);
	Samples[1] = GetRoadPosition(SectionIndex, LaneIndex, AlphaFunc(MiddlePointDistancAlongSpline), MiddlePointDistancAlongSpline, CoordinateSpace);
	Samples[2] = GetRoadPosition(SectionIndex, LaneIndex, AlphaFunc(EndDistanceAlongSpline), EndDistanceAlongSpline, CoordinateSpace);


	if (FMath::PointDistToSegmentSquared(Samples[1].Location, Samples[0].Location, Samples[2].Location) > MaxSquareDistanceFromSpline || FVector::Dist(Samples[0].Location, Samples[1].Location) > MinSegmentLength)
	{
		TArray<FRoadPosition> NewPoints[2];
		DivideSplineIntoPolylineRecursiveWithDistancesHelper2(SectionIndex, LaneIndex, AlphaFunc, StartDistanceAlongSpline, MiddlePointDistancAlongSpline, CoordinateSpace, MaxSquareDistanceFromSpline, MinSegmentLength, NewPoints[0]);
		DivideSplineIntoPolylineRecursiveWithDistancesHelper2(SectionIndex, LaneIndex, AlphaFunc, MiddlePointDistancAlongSpline, EndDistanceAlongSpline, CoordinateSpace, MaxSquareDistanceFromSpline, MinSegmentLength, NewPoints[1]);
		if ((NewPoints[0].Num() > 0) && (NewPoints[1].Num() > 0))
		{
			check(IsEqual(NewPoints[0].Last(), NewPoints[1][0]));
			NewPoints[0].RemoveAt(NewPoints[0].Num() - 1);
		}
		NewPoints[0].Append(NewPoints[1]);
		OutPoints.Append(NewPoints[0]);
	}
	else
	{
		// The middle point is close enough to the other 2 points, let's keep those and stop the recursion :
		OutPoints.Add(Samples[0]);
		// For a constant spline, the end can be the exact same as the start; in this case, just add the point once
		if (!IsEqual(Samples[0], Samples[2]))
		{
			OutPoints.Add(Samples[2]);
		}

	}

	return (OutPoints.Num() > 0);
}

void FRoadSplineCache::FindAllSegmantsForLane(int SectionIndex, int LaneIndex, double S0, double S1, TArray<double>& Segments) const
{

	const int32 NumSegments = GetNumberOfSplineSegments();
	const int32 SegmentStart = SplineCurves.ReparamTable.GetPointIndexForInputValue(S0) / ReparamStepsPerSegment;
	const int32 SegmentEnd = FMath::Min(NumSegments, 1 + SplineCurves.ReparamTable.GetPointIndexForInputValue(S1) / ReparamStepsPerSegment);

	for (int32 SegmentIndex = SegmentStart; SegmentIndex < SegmentEnd; ++SegmentIndex)
	{
		Segments.Add(FMath::Max(S0, GetDistanceAlongSplineAtSplinePoint(SegmentIndex)));
	}
	Segments.Add(FMath::Min(S1, GetDistanceAlongSplineAtSplinePoint(SegmentEnd)));

	const auto& Section = RoadLayout.Sections[SectionIndex];

	auto InRang = [S0, S1](double S) { return S >= S0 && S <= S1; };

	if (InRang(Section.SOffset))
	{
		Segments.Add(Section.SOffset);
	}
	if (InRang(Section.SOffsetEnd_Cashed))
	{
		Segments.Add(Section.SOffsetEnd_Cashed);
	}

	for (auto& Key : RoadLayout.ROffset.Keys)
	{
		if (InRang(Key.Time))
		{
			Segments.Add(Key.Time);
		}
	}

	/*
	if (LaneIndex > 0)
	{
		for (int i = 0; i < LaneIndex - 1; ++i)
		{
			auto& Lane = Section.Right[i];
			for (int j = 1; j < Lane.Width.GetNumKeys(); ++j)
			{
				auto& Width = Lane.Width.Keys[j];
				double S = Width.Time + Section.SOffset;
				if (InRang(S))
				{
					Segments.Add(S);
				}
			}
		}
	}
	else if (LaneIndex < 0)
	{
		for (int i = 0; i < -LaneIndex - 1; ++i)
		{
			auto& Lane = Section.Left[i];
			for (int j = 1; j < Lane.Width.GetNumKeys(); ++j)
			{
				auto& Width = Lane.Width.Keys[j];
				double S = Width.Time + Section.SOffset;
				if (InRang(S))
				{
					Segments.Add(S);
				}
			}
		}
	}
	*/

	Segments.Sort();

	if (Segments.Num() >= 2)
	{
		for (auto It = Segments.CreateIterator() + 1; It; ++It)
		{
			if (*It - *(It - 1) < SMALL_NUMBER)
			{
				It.RemoveCurrent();
			}
		}
	}
}

FRoadPosition FRoadSplineCache::UpRayIntersection(const FVector2D& WorldOrigin) const
{
	float Dummy;
	double Key = SplinesCurves2d.Position.FindNearest(ComponentToWorld.InverseTransformPosition(FVector(WorldOrigin.X, WorldOrigin.Y, 0.0)), Dummy);
	const FTransform WorldKeyTransform = GetTransformAtSplineInputKey(Key, ESplineCoordinateSpace::World);

	//const FVector TargetLocalLocation = KeyTransform.InverseTransformPositionNoScale(TargetWorldLocation);

	//UE_LOG(LogUnrealDrive, Log, TEXT("*** Key: %f; Roll: %f"), Key, KeyTransform.Rotator().Roll);

	FVector WorldPos = FMath::RayPlaneIntersection(
		FVector{ WorldOrigin.X, WorldOrigin.Y, WorldKeyTransform.GetLocation().Z - 10000.0 },
		FVector{ 0.0, 0.0, -1.0 }, 
		FPlane(WorldKeyTransform.GetLocation(), WorldKeyTransform.GetRotation().GetUpVector()));

	const FVector LocalPos = WorldKeyTransform.InverseTransformPositionNoScale(WorldPos);

	FRoadPosition Ret;
	Ret.SOffset = GetDistanceAlongSplineAtSplineInputKey(Key);
	Ret.ROffset = LocalPos.Y;
	Ret.Quat = WorldKeyTransform.GetRotation();
	Ret.Location = WorldPos;
	return Ret;
}

FVector FRoadSplineCache::GetRightVectorAtSplineInputKey(float InKey, ESplineCoordinateSpace::Type CoordinateSpace) const
{
	const FQuat Quat = GetQuaternionAtSplineInputKey(InKey, ESplineCoordinateSpace::Local);
	FVector RightVector = Quat.RotateVector(FVector::RightVector);

	if (CoordinateSpace == ESplineCoordinateSpace::World)
	{
		RightVector = ComponentToWorld.TransformVectorNoScale(RightVector);
	}

	return RightVector;
}

FVector FRoadSplineCache::GetLocationAtSplineInputKey(float InKey, ESplineCoordinateSpace::Type CoordinateSpace) const
{
	FVector Location = SplineCurves.Position.Eval(InKey, FVector::ZeroVector);

	if (CoordinateSpace == ESplineCoordinateSpace::World)
	{
		Location = ComponentToWorld.TransformPosition(Location);
	}

	return Location;
}

FQuat FRoadSplineCache::GetQuaternionAtSplineInputKey(float InKey, ESplineCoordinateSpace::Type CoordinateSpace) const
{
	FQuat Quat = SplineCurves.Rotation.Eval(InKey, FQuat::Identity);
	Quat.Normalize();

	const FVector Direction = SplineCurves.Position.EvalDerivative(InKey, FVector::ZeroVector).GetSafeNormal();
	const FVector UpVector = Quat.RotateVector(DefaultUpVector);

	FQuat Rot = (FRotationMatrix::MakeFromXZ(Direction, UpVector)).ToQuat();

	if (CoordinateSpace == ESplineCoordinateSpace::World)
	{
		Rot = ComponentToWorld.GetRotation() * Rot;
	}

	return Rot;
}

int32 FRoadSplineCache::GetNumberOfSplineSegments() const
{
	const int32 NumPoints = SplineCurves.Position.Points.Num();
	return (bIsClosedLoop ? NumPoints : FMath::Max(0, NumPoints - 1));
}

float FRoadSplineCache::GetDistanceAlongSplineAtSplinePoint(int32 PointIndex) const
{
	const int32 NumPoints = SplineCurves.Position.Points.Num();
	const int32 NumSegments = bIsClosedLoop ? NumPoints : NumPoints - 1;

	// Ensure that if the reparam table is not prepared yet we don't attempt to access it. This can happen
	// early in the construction of the spline component object.
	if ((PointIndex >= 0) && (PointIndex < NumSegments + 1) && ((PointIndex * ReparamStepsPerSegment) < SplineCurves.ReparamTable.Points.Num()))
	{
		return SplineCurves.ReparamTable.Points[PointIndex * ReparamStepsPerSegment].InVal;
	}

	return 0.0f;
}

float FRoadSplineCache::GetDistanceAlongSplineAtSplineInputKey(float InKey) const
{
	const int32 NumPoints = SplineCurves.Position.Points.Num();
	const int32 NumSegments = bIsClosedLoop ? NumPoints : NumPoints - 1;

	if ((InKey >= 0) && (InKey < NumSegments))
	{
		const int32 PointIndex = FMath::FloorToInt(InKey);
		const float Fraction = InKey - PointIndex;
		const int32 ReparamPointIndex = PointIndex * ReparamStepsPerSegment;
		const float Distance = SplineCurves.ReparamTable.Points[ReparamPointIndex].InVal;
		return Distance + GetSegmentLength(PointIndex, Fraction);
	}
	else if (InKey >= NumSegments)
	{
		return SplineCurves.GetSplineLength();
	}

	return 0.0f;
}

float FRoadSplineCache::GetSegmentLength(const int32 Index, const float Param) const
{
	return SplineCurves.GetSegmentLength(Index, Param, bIsClosedLoop, ComponentToWorld.GetScale3D());
}

FTransform FRoadSplineCache::GetTransformAtSplineInputKey(float InKey, ESplineCoordinateSpace::Type CoordinateSpace) const
{
	const FVector Location(GetLocationAtSplineInputKey(InKey, ESplineCoordinateSpace::Local));
	const FQuat Rotation(GetQuaternionAtSplineInputKey(InKey, ESplineCoordinateSpace::Local));

	FTransform Transform(Rotation, Location, FVector(1.0f));

	if (CoordinateSpace == ESplineCoordinateSpace::World)
	{
		Transform = Transform * ComponentToWorld;
	}

	return Transform;
}

FBoxSphereBounds FRoadSplineCache::CalcBounds(const FTransform& LocalToWorld) const
{

#define SPLINE_FAST_BOUNDS_CALCULATION 0

#if SPLINE_FAST_BOUNDS_CALCULATION
	FBox BoundingBox(0);
	for (const auto& InterpPoint : SplineCurves.Position.Points)
	{
		BoundingBox += InterpPoint.OutVal;
	}

	return FBoxSphereBounds(BoundingBox.TransformBy(LocalToWorld));
#else
	const int32 NumPoints = SplineCurves.Position.Points.Num();
	const int32 NumSegments = bIsClosedLoop ? NumPoints : NumPoints - 1;

	FVector Min(WORLD_MAX);
	FVector Max(-WORLD_MAX);
	if (NumSegments > 0)
	{
		for (int32 Index = 0; Index < NumSegments; Index++)
		{
			const bool bLoopSegment = (Index == NumPoints - 1);
			const int32 NextIndex = bLoopSegment ? 0 : Index + 1;
			const FInterpCurvePoint<FVector>& ThisInterpPoint = SplineCurves.Position.Points[Index];
			FInterpCurvePoint<FVector> NextInterpPoint = SplineCurves.Position.Points[NextIndex];
			if (bLoopSegment)
			{
				NextInterpPoint.InVal = ThisInterpPoint.InVal + SplineCurves.Position.LoopKeyOffset;
			}

			CurveVectorFindIntervalBounds(ThisInterpPoint, NextInterpPoint, Min, Max);
		}
	}
	else if (NumPoints == 1)
	{
		Min = Max = SplineCurves.Position.Points[0].OutVal;
	}
	else
	{
		Min = FVector::ZeroVector;
		Max = FVector::ZeroVector;
	}

	return FBoxSphereBounds(FBox(Min, Max).TransformBy(LocalToWorld));
#endif
}