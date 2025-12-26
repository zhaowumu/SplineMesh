/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "TriangulateRoadOp.h"
#include "RoadMeshTools/RoadLanePolylineArrangement.h"
#include "Utils/OpUtils.h"
#include "UnrealDriveEditorModule.h"
#include "RoadLaneAttributeEntries.h"

#define LOCTEXT_NAMESPACE "FSplineMeshOp"

using namespace UnrealDrive;

struct FRoadSplineMeshPosition
{
	FVector Location = FVector::ZeroVector;
	FQuat Quat = FQuat::Identity;
	bool bIsReverse = false;

	// Key params
	bool bIsKey = false;
	FVector2D Scale = FVector2D::One();
	FVector2D Offset = FVector2D::ZeroVector;
	double Roll = 0;
};

struct FRoadLanePolylineSplineMesh : public TRoadLanePolyline<FRoadSplineMeshPosition, FRoadLanePolylineSplineMesh>
{
	FName AttribyteEntryName;
	const TInstancedStruct<FRoadLaneAttributeEntry>* SplineMeshEntry = nullptr;

	virtual bool CanAppend(const FRoadLanePolylineSplineMesh& Other, EAppandMode AppandMode, double Tolerance) const override
	{
		if (AttribyteEntryName != Other.AttribyteEntryName)
		{
			return false;
		}

		if (SplineMeshEntry != Other.SplineMeshEntry)
		{
			return false;
		}

		return TRoadLanePolyline<FRoadSplineMeshPosition, FRoadLanePolylineSplineMesh>::CanAppend(Other, AppandMode, Tolerance);
	}

	virtual void Reverse() override
	{
		for (auto& It : Vertices)
		{
			It.bIsReverse = !It.bIsReverse;
		}
		TRoadLanePolyline<FRoadSplineMeshPosition, FRoadLanePolylineSplineMesh>::Reverse();
	}
};

using FRoadArrangemenSplineMesh = TRoadLanePolylineArrangement<FRoadLanePolylineSplineMesh>;


static int32 UpperBound(const TArray<FInterpCurvePoint<FVector>>& SplinePoints, float Value)
{
	int32 Count = SplinePoints.Num();
	int32 First = 0;

	while (Count > 0)
	{
		const int32 Middle = Count / 2;
		if (Value >= SplinePoints[First + Middle].InVal)
		{
			First += Middle + 1;
			Count -= Middle + 1;
		}
		else
		{
			Count = Middle;
		}
	}

	return First;
}


static void AddPoint(FSplineCurves& SplineCurves, const FSplinePoint& SplinePoint)
{
	const int32 Index = UpperBound(SplineCurves.Position.Points, SplinePoint.InputKey);

	SplineCurves.Position.Points.Insert(FInterpCurvePoint<FVector>(
		SplinePoint.InputKey,
		SplinePoint.Position,
		SplinePoint.ArriveTangent,
		SplinePoint.LeaveTangent,
		ConvertSplinePointTypeToInterpCurveMode(SplinePoint.Type)
	), Index);

	SplineCurves.Rotation.Points.Insert(FInterpCurvePoint<FQuat>(
		SplinePoint.InputKey,
		SplinePoint.Rotation.Quaternion(),
		FQuat::Identity,
		FQuat::Identity,
		CIM_CurveAuto
	), Index);

	SplineCurves.Scale.Points.Insert(FInterpCurvePoint<FVector>(
		SplinePoint.InputKey,
		SplinePoint.Scale,
		FVector::ZeroVector,
		FVector::ZeroVector,
		CIM_CurveAuto
	), Index);
}


static TArray<FRoadSplineMeshPosition> MakePolylineSpline(const FRoadLanePolygone& Poly, double S0, double S1, const FRoadLaneGeneration* KeyStart, const FRoadLaneGeneration* KeyEnd, double MaxSquareDistanceFromSpline, double MinSegmentLength, bool bIsReverse)
{
	auto AlphaFunc = [KeyStart, KeyEnd, S0, S1](double S)
	{
		if(!KeyEnd)
		{
			return KeyStart->Alpha;
		}
		return FMath::CubicInterp(KeyStart->Alpha, 0.0, KeyEnd->Alpha, 0.0, (S - S0) / (S1 - S0));
	};

	TArray<FRoadPosition> Points;
	if (!Poly.GetRoadSplineCache().ConvertSplineToPolyline_InDistanceRange2(Poly.SectionIndex, Poly.LaneIndex, AlphaFunc, ESplineCoordinateSpace::World, MaxSquareDistanceFromSpline, MinSegmentLength, S0, S1, Points, true))
	{
		return {};
	}

	TArray<FVector2D> Points2D;
	Points2D.Reserve(Points.Num());
	for (auto& It : Points)
	{
		Points2D.Add(FVector2D{ It.Location });
	}
	OpUtils::RemovedPolylineSelfIntersection(Points2D);

	if (Points2D.Num() < 2)
	{
		return {};
	}

	TArray<FRoadSplineMeshPosition> OutPoints;
	TArray<FVector> Normals;
	OutPoints.Reserve(Points2D.Num());
	Normals.Reserve(Points2D.Num());
	for (auto& Point2D : Points2D)
	{
		FHitResult Hit;
		if (!Poly.Owner.FindRayIntersection(Point2D, Hit))
		{
			return {};
		}

		FRoadSplineMeshPosition Pos{};
		Pos.Location = Hit.ImpactPoint;
		OutPoints.Add(Pos);
		Normals.Add(Hit.Normal);
	}

	for (int Index = 0; Index < OutPoints.Num(); ++Index)
	{
		//auto UpVector = Normals[i];

		FVector ForwardVector;
		if (Index == 0)
		{
			auto& PtB = OutPoints[Index].Location;
			auto& PtC = OutPoints[Index + 1].Location;
			ForwardVector = (PtC - PtB).GetSafeNormal();

		}
		else if (Index == OutPoints.Num() - 1)
		{
			auto& PtA = OutPoints[Index - 1].Location;
			auto& PtB = OutPoints[Index].Location;
			ForwardVector = (PtB - PtA).GetSafeNormal();
		}
		else
		{
			auto& PtA = OutPoints[Index - 1].Location;
			auto& PtB = OutPoints[Index].Location;
			auto& PtC = OutPoints[Index + 1].Location;
			FVector ForwardVector0 = (PtB - PtA).GetSafeNormal();
			FVector ForwardVector1 = (PtC - PtB).GetSafeNormal();
			ForwardVector = (ForwardVector0 + ForwardVector1).GetSafeNormal();
		}

		OutPoints[Index].Quat = (FRotationMatrix::MakeFromXZ(ForwardVector, Normals[Index])).ToQuat();
		OutPoints[Index].bIsReverse = ((Poly.LaneIndex != LANE_INDEX_NONE) ? !Poly.GetLane().IsForwardLane() : false) ^ bIsReverse;
	}

	OutPoints[0].bIsKey = true;
	OutPoints[0].Scale = KeyStart->Scale;
	OutPoints[0].Offset = KeyStart->Offset;
	OutPoints[0].Roll = KeyStart->Roll;

	if (KeyEnd)
	{
		OutPoints.Last().bIsKey = true;
		OutPoints.Last().Scale = KeyStart->Scale;
		OutPoints.Last().Offset = KeyStart->Offset;
		OutPoints.Last().Roll = KeyStart->Roll;
	}


	return OutPoints;
}

static FQuat GetQuaternionAtSplineInputKey(const FSplineCurves& SplineCurves, float InKey)
{
	FQuat Quat = SplineCurves.Rotation.Eval(InKey, FQuat::Identity);
	Quat.Normalize();
	const FVector Direction = SplineCurves.Position.EvalDerivative(InKey, FVector::ZeroVector).GetSafeNormal();
	const FVector UpVector = Quat.RotateVector(FVector::UpVector);
	return (FRotationMatrix::MakeFromXZ(Direction, UpVector)).ToQuat();
}

static TArray<FSplineMeshSegments::FSegment> MakeSegments(FRoadLanePolylineSplineMesh& Polyline)
{
	auto& Entry = Polyline.SplineMeshEntry->Get<FRoadLaneAttributeEntryRefSpline>();


	if (Polyline[0].bIsReverse ^ Entry.bReversSplineDirection)
	{
		Algo::Reverse(Polyline.Vertices);
	}

	// Create SplineCurves
	FSplineCurves SplineCurves;
	SplineCurves.Position.Points.Reserve(Polyline.Vertices.Num());
	SplineCurves.Rotation.Points.Reserve(Polyline.Vertices.Num());
	SplineCurves.Scale.Points.Reserve(Polyline.Vertices.Num());
	float InputKey = 0.0f;
	for (int i = 0; i < Polyline.Vertices.Num(); ++i)
	{
		auto& Point = Polyline[i];
		FVector RightVector, UpVector, ForwardVector;
		double SinA;
		GetThreeVectors(Polyline.Vertices, i, RightVector, UpVector, ForwardVector, SinA);

		SplineCurves.Position.Points.Emplace(InputKey, Point.Location, FVector::ZeroVector, FVector::ZeroVector, CIM_CurveAuto);
		SplineCurves.Rotation.Points.Emplace(InputKey, FRotationMatrix::MakeFromXZ(ForwardVector, UpVector).ToQuat(), FQuat::Identity, FQuat::Identity, CIM_CurveAuto);
		SplineCurves.Scale.Points.Emplace(InputKey, FVector::OneVector, FVector::ZeroVector, FVector::ZeroVector, CIM_CurveAuto);
		InputKey += 1.0f;
	}
	SplineCurves.UpdateSpline();



	// Estimate first key
	if (!Polyline.Vertices[0].bIsKey)
	{
		for (int i = 1; i <= Polyline.Vertices.Num(); ++i)
		{
			if (Polyline.Vertices[i].bIsKey)
			{
				auto& Cur = Polyline.Vertices[i];
				auto& First = Polyline.Vertices[0];
				First.Scale = Cur.Scale;
				First.Offset = Cur.Offset;
				First.Roll = Cur.Roll;
				First.bIsKey = true;
				break;
			}
		}
	}

	// Estimate last key
	if (!Polyline.Vertices.Last().bIsKey)
	{
		for (int i = Polyline.Vertices.Num() - 2; i >= 0; --i)
		{
			if (Polyline.Vertices[i].bIsKey)
			{
				auto& Cur = Polyline.Vertices[i];
				auto& Last = Polyline.Vertices.Last();
				Last.Scale = Cur.Scale;
				Last.Offset = Cur.Offset;
				Last.Roll = Cur.Roll;
				Last.bIsKey = true;
				break;
			}
		}
	}

	// Interpolate polyline values between keys 
	int StartKey = 0;
	for (int i = 1; i < Polyline.Vertices.Num(); ++i)
	{
		if (Polyline.Vertices[i].bIsKey)
		{
			const int EndKey = i;
			const auto& StartPos = Polyline.Vertices[StartKey];
			const auto& EndPos = Polyline.Vertices[EndKey];
			const float FullSegmentLength = SplineCurves.ReparamTable.Points[EndKey].InVal - SplineCurves.ReparamTable.Points[StartKey].InVal;
			for (int j = StartKey + 1; j < EndKey; ++j)
			{
				const float SegmentLength = SplineCurves.ReparamTable.Points[j].InVal - SplineCurves.ReparamTable.Points[StartKey].InVal;
				const float Alpha = SegmentLength / FullSegmentLength;
				Polyline.Vertices[j].Scale = FMath::CubicInterp(StartPos.Scale, FVector2D::ZeroVector, EndPos.Scale, FVector2D::ZeroVector, Alpha);
				Polyline.Vertices[j].Offset = FMath::CubicInterp(StartPos.Offset, FVector2D::ZeroVector, EndPos.Offset, FVector2D::ZeroVector, Alpha);
				Polyline.Vertices[j].Roll = FMath::CubicInterp(StartPos.Roll, 0.0, EndPos.Roll, 0.0, Alpha);
			}
			StartKey = EndKey;
		}
	}

	// Creat ScaleCurve, OffsetCurve, RollCurve
	FInterpCurveVector2D ScaleCurve;
	FInterpCurveVector2D OffsetCurve;
	FInterpCurveFloat RollCurve;
	ScaleCurve.Points.Reserve(Polyline.Vertices.Num());
	OffsetCurve.Points.Reserve(Polyline.Vertices.Num());
	RollCurve.Points.Reserve(Polyline.Vertices.Num());
	InputKey = 0.0;
	for (int i = 0; i < Polyline.Vertices.Num(); ++i)
	{
		auto& Point = Polyline[i];
		ScaleCurve.Points.Emplace(InputKey, Point.Scale, FVector2D::ZeroVector, FVector2D::ZeroVector, CIM_CurveAuto);
		OffsetCurve.Points.Emplace(InputKey, Point.Offset, FVector2D::ZeroVector, FVector2D::ZeroVector, CIM_CurveAuto);
		RollCurve.Points.Emplace(InputKey, Point.Roll, 0.0, 0.0, CIM_CurveAuto);
		InputKey += 1.0f;
	}
	ScaleCurve.AutoSetTangents(0.0f, false);
	OffsetCurve.AutoSetTangents(0.0f, false);
	RollCurve.AutoSetTangents(0.0f, false);


	// Fill FSplineMeshSegments
	const int NumberOfMeshes = FMath::Max(1, FMath::RoundToInt(SplineCurves.GetSplineLength() / (Entry.LengthOfSegment)));
	const double LengthOfSegment = SplineCurves.GetSplineLength() / NumberOfMeshes;
	TArray<FSplineMeshSegments::FSegment> OutSegments;
	for (int SplineCount = 0; SplineCount < NumberOfMeshes; SplineCount++)
	{
		const double SStart = SplineCount * LengthOfSegment;
		const double SEnd = (SplineCount + 1) * LengthOfSegment;

		const float ParamStart = SplineCurves.ReparamTable.Eval(SStart, 0.0f);
		const float ParamEnd = SplineCurves.ReparamTable.Eval(SEnd, 0.0f);

		auto& NewSegment = OutSegments.Add_GetRef({});
		NewSegment.bAlignWorldUpVector = Entry.bAlignWorldUpVector;

		NewSegment.SplineMeshParams.StartPos = SplineCurves.Position.Eval(ParamStart, FVector::ZeroVector);
		NewSegment.SplineMeshParams.StartTangent = SplineCurves.Position.EvalDerivative(ParamStart, FVector::ZeroVector);

		NewSegment.SplineMeshParams.EndPos = SplineCurves.Position.Eval(ParamEnd, FVector::ZeroVector);
		NewSegment.SplineMeshParams.EndTangent = SplineCurves.Position.EvalDerivative(ParamEnd, FVector::ZeroVector);

		NewSegment.SplineMeshParams.StartScale = ScaleCurve.Eval(ParamStart, FVector2D::One());
		NewSegment.SplineMeshParams.EndScale = ScaleCurve.Eval(ParamEnd, FVector2D::One());

		NewSegment.SplineMeshParams.StartOffset = OffsetCurve.Eval(ParamStart, FVector2D::One());
		NewSegment.SplineMeshParams.EndOffset = OffsetCurve.Eval(ParamEnd, FVector2D::One());

		if (!Entry.bAlignWorldUpVector)
		{
			NewSegment.SplineMeshParams.StartRoll = FMath::DegreesToRadians(GetQuaternionAtSplineInputKey(SplineCurves, ParamStart).Rotator().Roll);
			NewSegment.SplineMeshParams.EndRoll = FMath::DegreesToRadians(GetQuaternionAtSplineInputKey(SplineCurves, ParamEnd).Rotator().Roll);
		}

		NewSegment.SplineMeshParams.StartRoll += FMath::DegreesToRadians(RollCurve.Eval(ParamStart, 0.0));
		NewSegment.SplineMeshParams.EndRoll += FMath::DegreesToRadians(RollCurve.Eval(ParamEnd, 0.0));

		NewSegment.AttribyteEntry = Polyline.SplineMeshEntry;
		NewSegment.AttribyteEntryName = Polyline.AttribyteEntryName;
	}

	return OutSegments;
}

FSplineMeshOp::FSplineMeshOp()
{
	FUnrealDriveEditorModule::Get().ForEachRoadLaneAttributEntries([this](FName Name, const TInstancedStruct<FRoadLaneAttributeEntry>* Value)
	{
		if (Value->GetPtr<FRoadLaneAttributeEntryRefSpline>())
		{
			ResultSegments->AttribyteEntries.Add(Name, *Value);
		}
		return false;
	});
}


void FSplineMeshOp::CalculateResult(FProgressCancel* Progress)
{
#define CHECK_CANCLE() if (Progress && Progress->Cancelled()) { ResultInfo.Result = EGeometryResultType::Cancelled; return; }

	ResultInfo.Result = EGeometryResultType::InProgress;

	if (!BaseData || BaseData->ResultInfo.HasFailed())
	{
		ResultInfo.SetFailed();
		return;
	}

	// ========================== Add FRoadLaneGeneration attributes to thr arrangemen ==========================

	FRoadArrangemenSplineMesh Arrangemen;
	
	for (const auto& Poly : BaseData->Polygons)
	{
		if (Poly->GetType() != ERoadPolygoneType::RoadLane)
		{
			continue;
		}
		auto LanePoly = StaticCastSharedPtr<FRoadLanePolygone>(Poly);
		auto& Section = LanePoly->GetSection();
		for (auto& [AttribyteEntryName, AttributeEntry] : LanePoly->GetLaneAttributes())
		{
			if (auto* FoundEntry = ResultSegments->AttribyteEntries.Find(AttribyteEntryName))
			{
				bool bIsReverse = false;
				if (AttributeEntry.Keys.Num())
				{
					if (auto* Value = AttributeEntry.Keys[0].GetValuePtr<FRoadLaneGeneration>())
					{
						bIsReverse = Value->bIsReverse;
					}
				}

				for (int AttributeIndex = 0; AttributeIndex < AttributeEntry.Keys.Num(); ++AttributeIndex)
				{
					const auto* KeyStart = &AttributeEntry.Keys[AttributeIndex];
					const auto* ValueStart = KeyStart->GetValuePtr<FRoadLaneGeneration>();

					if (ValueStart)
					{
						const auto* KeyEnd = (AttributeIndex < AttributeEntry.Keys.Num() - 1) ? &AttributeEntry.Keys[AttributeIndex + 1] : nullptr;
						const auto* ValueEnd = KeyEnd ? KeyEnd->GetValuePtr<FRoadLaneGeneration>() : nullptr;

						const double SOffsetStart = KeyStart->SOffset + Section.SOffset;
						const double SOffsetEnd = KeyEnd ? KeyEnd->SOffset + Section.SOffset : LanePoly->GetEndOffset();
						//const double MaxSquareDistanceFromSpline = 2.0;

						FRoadLanePolylineSplineMesh Polyline;
						Polyline.AttribyteEntryName = AttribyteEntryName;
						Polyline.SplineMeshEntry = FoundEntry;
						Polyline.Vertices = MakePolylineSpline(*LanePoly.Get(), SOffsetStart, SOffsetEnd, ValueStart, ValueEnd, FLT_MAX, FoundEntry->Get<FRoadLaneAttributeEntryRefSpline>().LengthOfSegment * 0.5, bIsReverse);
						if (Polyline.Vertices.Num() > 1)
						{
							const double ArrangemenTolerance = 10.0;
							Arrangemen.Insert(MoveTemp(Polyline), ArrangemenTolerance);
						}
					}
				}
			}
		}

		CHECK_CANCLE();
	}

	// ========================== Create segments  ==========================
	for (auto& Polyline : Arrangemen.Polylines)
	{
		auto Segments = MakeSegments(Polyline);
		ResultSegments->Segments.Append(MoveTemp(Segments));

		CHECK_CANCLE();
	}

	// ========================== Draw debug lines  ==========================
	if (bDrawRefSplines)
	{
		FScopeLock Lock(&BaseData->RenderAPIMutex);
		for (auto& Polyline : Arrangemen.Polylines)
		{
			FRoadBaseOperatorData::FDebugLines& DebugLines = BaseData->DebugLines.Add_GetRef({});

			DebugLines.Thickness = 4.0;
			DebugLines.Color = FColor(0, 255, 0, 100);
			for (int i = 0; i < Polyline.Num() - 1; ++i)
			{
				DebugLines.Lines.Add({ Polyline[i].Location, Polyline[i + 1].Location, });
			}
		}
	}

	ResultInfo.SetSuccess();

#undef CHECK_CANCLE
}

#undef LOCTEXT_NAMESPACE
