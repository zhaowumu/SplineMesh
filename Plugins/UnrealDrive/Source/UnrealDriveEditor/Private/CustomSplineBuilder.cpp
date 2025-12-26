/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */
#include "CustomSplineBuilder.h"
#include "Components/SplineComponent.h"

static float SmoothStep(float A, float B, float X)
{
	if (X < A)
	{
		return 0.0f;
	}
	else if (X >= B)
	{
		return 1.0f;
	}
	const float InterpFraction = (X - A) / (B - A);
	return InterpFraction * InterpFraction * (3.0f - 2.0f * InterpFraction);
}

static FVector SplineEvalPos(const FVector& StartPos, const FVector& StartTangent, const FVector& EndPos, const FVector& EndTangent, float A)
{
	const float A2 = A * A;
	const float A3 = A2 * A;

	return (((2 * A3) - (3 * A2) + 1) * StartPos) + ((A3 - (2 * A2) + A) * StartTangent) + ((A3 - A2) * EndTangent) + (((-2 * A3) + (3 * A2)) * EndPos);
}

static FVector SplineEvalPos(const FReferenceSplineMeshParams& Params, float A)
{
	// TODO: these don't need to be doubles!
	const FVector StartPos = FVector(Params.StartPos);
	const FVector StartTangent = FVector(Params.StartTangent);
	const FVector EndPos = FVector(Params.EndPos);
	const FVector EndTangent = FVector(Params.EndTangent);

	return SplineEvalPos(StartPos, StartTangent, EndPos, EndTangent, A);
}

static FVector SplineEvalTangent(const FVector& StartPos, const FVector& StartTangent, const FVector& EndPos, const FVector& EndTangent, const float A)
{
	const FVector C = (6 * StartPos) + (3 * StartTangent) + (3 * EndTangent) - (6 * EndPos);
	const FVector D = (-6 * StartPos) - (4 * StartTangent) - (2 * EndTangent) + (6 * EndPos);
	const FVector E = StartTangent;

	const float A2 = A * A;

	return (C * A2) + (D * A) + E;
}

static FVector SplineEvalTangent(const FReferenceSplineMeshParams& Params, const float A)
{
	// TODO: these don't need to be doubles!
	const FVector StartPos = FVector(Params.StartPos);
	const FVector StartTangent = FVector(Params.StartTangent);
	const FVector EndPos = FVector(Params.EndPos);
	const FVector EndTangent = FVector(Params.EndTangent);

	return SplineEvalTangent(StartPos, StartTangent, EndPos, EndTangent, A);
}

static FVector SplineEvalDir(const FReferenceSplineMeshParams& Params, const float A)
{
	return SplineEvalTangent(Params, A).GetSafeNormal();
}

FTransform UCustomSplineBuilder::CalcSliceTransformAtSplineOffset(const FReferenceSplineMeshParams& SplineParams, const float Alpha, const float MinT, const float MaxT)
{
	static const FVector SplineUpDir = FVector::UpVector;
	static const bool bSmoothInterpRollScale = false;

	// Apply hermite interp to Alpha if desired
	const float HermiteAlpha = bSmoothInterpRollScale ? SmoothStep(0.0, 1.0, Alpha) : Alpha;

	// Then find the point and direction of the spline at this point along
	FVector SplinePos;
	FVector SplineDir;

	// Use linear extrapolation
	if (Alpha < MinT)
	{
		const FVector StartTangent(SplineEvalTangent(SplineParams, MinT));
		SplinePos = SplineEvalPos(SplineParams, MinT) + (StartTangent * (Alpha - MinT));
		SplineDir = StartTangent.GetSafeNormal();
	}
	else if (Alpha > MaxT)
	{
		const FVector EndTangent(SplineEvalTangent(SplineParams, MaxT));
		SplinePos = SplineEvalPos(SplineParams, MaxT) + (EndTangent * (Alpha - MaxT));
		SplineDir = EndTangent.GetSafeNormal();
	}
	else
	{
		SplinePos = SplineEvalPos(SplineParams, Alpha);
		SplineDir = SplineEvalDir(SplineParams, Alpha);
	}

	// Find scale at this point along spline
	const FVector2D UseScale = FMath::Lerp(FVector2D(SplineParams.StartScale), FVector2D(SplineParams.EndScale), HermiteAlpha);

	if (SplineParams.bAlignWorldUpVector)
	{
		FVector SplineDir2D = FVector(SplineDir.X, SplineDir.Y, 0.0).GetSafeNormal();
		return FTransform(
			FRotationMatrix::MakeFromXZ(SplineDir2D, SplineUpDir).ToQuat(),
			SplinePos,
			FVector(1, UseScale.X, UseScale.Y)
		);
	}

	// Find base frenet frame
	const FVector BaseXVec = (SplineUpDir ^ SplineDir).GetSafeNormal();
	const FVector BaseYVec = (SplineDir ^ BaseXVec).GetSafeNormal();

	// Offset the spline by the desired amount
	const FVector2D SliceOffset = FMath::Lerp(SplineParams.StartOffset, SplineParams.EndOffset, HermiteAlpha);
	SplinePos += SliceOffset.X * BaseXVec;
	SplinePos += SliceOffset.Y * BaseYVec;

	// Apply roll to frame around spline
	const float UseRoll = FMath::Lerp(SplineParams.StartRoll, SplineParams.EndRoll, HermiteAlpha);
	const float CosAng = FMath::Cos(UseRoll);
	const float SinAng = FMath::Sin(UseRoll);
	const FVector XVec = (CosAng * BaseXVec) - (SinAng * BaseYVec);
	const FVector YVec = (CosAng * BaseYVec) + (SinAng * BaseXVec);



	// Build overall transform
	FTransform SliceTransform;
	//switch (ForwardAxis)
	//{
	//case ESplineMeshAxis::X:
		SliceTransform = FTransform(SplineDir, XVec, YVec, SplinePos);
		SliceTransform.SetScale3D(FVector(1, UseScale.X, UseScale.Y));
	//	break;
	//case ESplineMeshAxis::Y:
	//	SliceTransform = FTransform(FVector(YVec), FVector(SplineDir), FVector(XVec), FVector(SplinePos));
	//	SliceTransform.SetScale3D(FVector(UseScale.Y, 1, UseScale.X));
	//	break;
	//case ESplineMeshAxis::Z:
	//	SliceTransform = FTransform(FVector(XVec), FVector(YVec), FVector(SplineDir), FVector(SplinePos));
	//	SliceTransform.SetScale3D(FVector(UseScale.X, UseScale.Y, 1));
	//	break;
	//default:
	//	check(0);
	//	break;
	//}

	return SliceTransform;
}


FReferenceSplineMeshParams::FReferenceSplineMeshParams(const FSplineMeshParams& Other)
{
	StartPos = Other.StartPos;
	StartTangent = Other.StartTangent;
	StartScale = Other.StartScale;
	StartRoll = Other.StartRoll;
	EndRoll = Other.EndRoll;
	StartOffset = Other.StartOffset;
	EndPos = Other.EndPos;
	EndScale = Other.EndScale;
	EndTangent = Other.EndTangent;
	EndOffset = Other.EndOffset;
}

FReferenceSplineMeshParams::operator FSplineMeshParams() const
{
	FSplineMeshParams Other;
	Other.StartPos = StartPos;
	Other.StartTangent = StartTangent;
	Other.StartScale = StartScale;
	Other.StartRoll = StartRoll;
	Other.EndRoll = EndRoll;
	Other.StartOffset = StartOffset;
	Other.EndPos = EndPos;
	Other.EndScale = EndScale;
	Other.EndTangent = EndTangent;
	Other.EndOffset = EndOffset;
	return Other;
}