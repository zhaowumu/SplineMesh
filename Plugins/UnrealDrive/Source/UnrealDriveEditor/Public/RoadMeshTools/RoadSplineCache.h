/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "RoadSplineComponent.h"
#include "BoxTypes.h"

namespace UnrealDrive 
{

	struct UNREALDRIVEEDITOR_API FRoadSplineCache
	{
		// Copies from USplineComponent
		FSplineCurves SplineCurves;
		bool bIsClosedLoop;
		FVector DefaultUpVector;
		int32 ReparamStepsPerSegment;
		bool bStationaryEndpoints;
		FTransform ComponentToWorld;

		// Copies from URoadSplineComponent
		FRoadLayout RoadLayout;
		bool bSkipProcrdureGeneration;
		uint8 MaterialPriority;

		// Do not use spline data. It may not be relevant for this cache.
		TWeakObjectPtr<const URoadSplineComponent> OriginSpline;

	public:
		FSplineCurves SplinesCurves2d;
		UE::Geometry::FAxisAlignedBox2d SplineBounds; // X - SOffset, Y - ROffset

		using TAlpaFunction = TFunction<double(double S)>;

	public:
		FRoadSplineCache(const URoadSplineComponent* Spline);
		void UpdateSplinesCurves2d();

	public:
		FRoadPosition GetRoadPosition(int SectionIndex, int LaneIndex, double Alpha, double SOffset, ESplineCoordinateSpace::Type CoordinateSpace) const;
		FRoadPosition GetRoadPosition(double SOffset, double ROffset, ESplineCoordinateSpace::Type CoordinateSpace) const;
		bool ConvertSplineToPolyline_InDistanceRange2(int SectionIndex, int LaneIndex, TAlpaFunction AlphaFunc, ESplineCoordinateSpace::Type CoordinateSpace, double MaxSquareDistanceFromSpline, double MinSegmentLength, double S0, double S1, TArray<FRoadPosition>& OutPoints, bool bAllowWrappingIfClosed) const;
		void FindAllSegmantsForLane(int SectionIndex, int LaneIndex, double S0, double S1, TArray<double>& Segments) const;
		FRoadPosition UpRayIntersection(const FVector2D& WorldOrigin) const;

	public:
		// Copies of functions from USplineComponent
		FVector GetRightVectorAtSplineInputKey(float InKey, ESplineCoordinateSpace::Type CoordinateSpace) const;
		FVector GetLocationAtSplineInputKey(float InKey, ESplineCoordinateSpace::Type CoordinateSpace) const;
		FQuat GetQuaternionAtSplineInputKey(float InKey, ESplineCoordinateSpace::Type CoordinateSpace) const;
		int32 GetNumberOfSplineSegments() const;
		float GetDistanceAlongSplineAtSplinePoint(int32 PointIndex) const;
		float GetDistanceAlongSplineAtSplineInputKey(float InKey) const;
		float GetSegmentLength(const int32 Index, const float Param) const;
		FTransform GetTransformAtSplineInputKey(float InKey, ESplineCoordinateSpace::Type CoordinateSpace) const;
		FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const;

	private:
		bool DivideSplineIntoPolylineRecursiveWithDistancesHelper2(int SectionIndex, int LaneIndex, const TAlpaFunction& AlphaFunc, double S0, double S1, ESplineCoordinateSpace::Type CoordinateSpace, double MaxSquareDistanceFromSpline, double MinSegmentLength, TArray<FRoadPosition>& OutPoints) const;
	};

} // UnrealDrive

