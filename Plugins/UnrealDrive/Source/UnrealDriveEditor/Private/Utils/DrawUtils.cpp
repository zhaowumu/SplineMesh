/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */


#include "Utils/DrawUtils.h"
#include "EngineUtils.h"
#include "UnrealDriveSettings.h"

//MPLEMENT_HIT_PROXY(HRoadConnectionProxy, HRoadSplineVisProxy);
IMPLEMENT_HIT_PROXY(HRoadLaneConnectionProxy, HRoadLaneVisProxy);

namespace DrawUtils
{

void DrawTriangle(class FPrimitiveDrawInterface* PDI, const FVector& A, const FVector& B, const FVector& C, const FMaterialRenderProxy* MaterialRenderProxy, uint8 DepthPriorityGroup)
{
	FVector2f UVs[4] =
	{
		FVector2f(0,0),
		FVector2f(0,1),
		FVector2f(1,1),
		FVector2f(1,0),
	};

	FDynamicMeshBuilder MeshBuilder(PDI->View->GetFeatureLevel());

	FVector3f Normal = FVector3f(0, 0, 1);
	FVector3f Tangent = FVector3f(1, 0, 0);

	MeshBuilder.AddVertex(FDynamicMeshVertex((FVector3f)A, Tangent, Normal, UVs[0], FColor::White));

	MeshBuilder.AddVertex(FDynamicMeshVertex((FVector3f)B, Tangent, Normal, UVs[1], FColor::White));
	MeshBuilder.AddVertex(FDynamicMeshVertex((FVector3f)C, Tangent, Normal, UVs[2], FColor::White));

	MeshBuilder.AddTriangle(0, 1, 2);
	MeshBuilder.Draw(PDI, FMatrix::Identity, MaterialRenderProxy, DepthPriorityGroup, false, false);

	//PDI->DrawLine(A, B, FColor::Yellow, DepthPriorityGroup, 1.f);
	//PDI->DrawLine(A, C, FColor::Yellow, DepthPriorityGroup, 1.f);
	//PDI->DrawLine(B, C, FColor::Yellow, DepthPriorityGroup, 1.f);
}

void DrawRoadLaneConnection(bool bIsSuccessorConnection, const FTransform & Transform, const FMaterialRenderProxy* MaterialRenderProxy, class FPrimitiveDrawInterface* PDI, const FSceneView* View, uint8 DepthPriorityGroup)
{
	static const float Width = 10;
	static const float Height = 10;

	const float Shift = bIsSuccessorConnection ? -Width : 0.0;

	FVector A{ Shift , -Height / 2.0, 0.0 };
	FVector B{ Shift , +Height / 2.0, 0.0 };
	FVector C{ Width + Shift, 0.0, 0.0 };


	const float ViewScale = static_cast<float>(View->WorldToScreen(Transform.GetLocation()).W * (4.0f / View->UnscaledViewRect.Width() / View->ViewMatrices.GetProjectionMatrix().M[0][0]));

	DrawUtils::DrawTriangle(
		PDI,
		Transform.TransformPosition(A * ViewScale),
		Transform.TransformPosition(B * ViewScale),
		Transform.TransformPosition(C * ViewScale),
		MaterialRenderProxy,
		DepthPriorityGroup);
}

void TrimPoints(double SplineParam0, double SplineParam1, TArray<FSplinePositionLinearApproximation>& Points)
{
	int32 StartKey = INDEX_NONE;
	int32 EndKey = INDEX_NONE;

	for (int32 KeyIndex = 0; KeyIndex < Points.Num(); ++KeyIndex)
	{
		const double CurrentS = Points[KeyIndex].SplineParam;
		if (CurrentS <= SplineParam0)
		{
			StartKey = KeyIndex;
		}
		if (CurrentS >= SplineParam1)
		{
			EndKey = KeyIndex;
			break;
		}
	}

	if (EndKey != INDEX_NONE && (Points.Num() - 1 != EndKey))
	{
		Points.SetNumUninitialized(EndKey + 1);
	}

	if (StartKey != INDEX_NONE && StartKey != 0)
	{
		Points.RemoveAt(0, StartKey);
	}

	if (Points.Num())
	{
		if (Points[0].SplineParam < SplineParam0)
		{
			if (Points.Num() > 1)
			{
				double Alpha = (SplineParam0 - Points[0].SplineParam) / (Points[1].SplineParam - Points[0].SplineParam);
				Points[0].Position = FMath::Lerp(Points[0].Position, Points[1].Position, Alpha);
			}
			Points[0].SplineParam = SplineParam0;
		}

		if (Points[Points.Num() - 1].SplineParam > SplineParam1)
		{
			if (Points.Num() > 1)
			{
				double Alpha = (SplineParam1 - Points[Points.Num() - 2].SplineParam) / (Points[Points.Num() - 1].SplineParam - Points[Points.Num() - 2].SplineParam);
				Points[Points.Num() - 1].Position = FMath::Lerp(Points[Points.Num() - 2].Position, Points[Points.Num() - 1].Position, Alpha);
			}
			Points[Points.Num() - 1].SplineParam = SplineParam1;
		}
	}
};

void DrawLaneBorder(FPrimitiveDrawInterface* PDI, const URoadSplineComponent* SplineComp, int SectionIndex, int LaneIndex, double S0, double S1, const FColor& Color1, const FColor& Color2, uint8 DepthPriorityGroup, float Thickness, float DepthBias, bool bScreenSpace)
{
	const auto& Section = SplineComp->GetLaneSection(SectionIndex);

	double StartS, EndS;
	if (LaneIndex == LANE_INDEX_NONE)
	{
		StartS = Section.SOffset;
		EndS = Section.SOffsetEnd_Cashed;
	}
	else
	{
		auto& Lane = Section.GetLaneByIndex(LaneIndex);
		StartS = Lane.GetStartOffset();
		EndS = Lane.GetEndOffset();
	}

	const int NumPointPerSegmaent = GetDefault<UUnrealDriveSettings>()->NumPointPerSegmaent;
	const int NumPointPerSection = GetDefault<UUnrealDriveSettings>()->NumPointPerSection;

	TArray<FSplinePositionLinearApproximation> Points;
	SplineComp->BuildLinearApproximation(
		Points,
		[&](double S)
		{
			return (LaneIndex == LANE_INDEX_NONE ? 0 : Section.EvalLaneROffset(LaneIndex, S, 1.0)) + SplineComp->EvalROffset(S);
		},
		StartS, EndS, NumPointPerSegmaent, NumPointPerSection, ESplineCoordinateSpace::World);

	if (!FMath::IsNearlyEqual(S0, StartS) || !FMath::IsNearlyEqual(S1, EndS))
	{
		TrimPoints(SplineComp->SplineCurves.ReparamTable.Eval(S0, 0.0f), SplineComp->SplineCurves.ReparamTable.Eval(S1, 0.0f), Points);
	}

	for (int32 StepIdx = 1; StepIdx < Points.Num(); StepIdx++)
	{
		if (StepIdx % 2)
		{
			PDI->DrawTranslucentLine(Points[StepIdx - 1].Position, Points[StepIdx].Position, Color1, DepthPriorityGroup, Thickness, DepthBias, bScreenSpace);
		}
		else
		{
			PDI->DrawTranslucentLine(Points[StepIdx - 1].Position, Points[StepIdx].Position, Color2, DepthPriorityGroup, Thickness, DepthBias, bScreenSpace);
		}
	}
}

void DrawLaneBorder(FPrimitiveDrawInterface* PDI, const URoadSplineComponent* SplineComp, int SectionIndex, int LaneIndex, const FColor& Color1, const FColor& Color2, uint8 DepthPriorityGroup, float Thickness, float DepthBias, bool bScreenSpace)
{
	const FRoadLaneSection& Section = SplineComp->GetLaneSection(SectionIndex);
	DrawLaneBorder(PDI, SplineComp, SectionIndex, LaneIndex, Section.SOffset, Section.SOffsetEnd_Cashed, Color1, Color2, DepthPriorityGroup, Thickness, DepthBias, bScreenSpace);
}

void DrawSpline(FPrimitiveDrawInterface* PDI, const URoadSplineComponent* SplineComp, double S0, double S1, const FColor& Color, uint8 DepthPriorityGroup, float Thickness, float DepthBias, bool bScreenSpace)
{
	const int NumPointPerSegmaent = GetDefault<UUnrealDriveSettings>()->NumPointPerSegmaent;
	const int NumPointPerSection = GetDefault<UUnrealDriveSettings>()->NumPointPerSection;

	TArray<FSplinePositionLinearApproximation> Points;
	SplineComp->BuildLinearApproximation(
		Points,
		[&](double S)
		{
			return 0;
		},
		S0, S1, NumPointPerSegmaent, NumPointPerSection, ESplineCoordinateSpace::World);

	for (int32 StepIdx = 1; StepIdx < Points.Num(); StepIdx++)
	{
		PDI->DrawLine(Points[StepIdx - 1].Position, Points[StepIdx].Position, Color, DepthPriorityGroup, Thickness, DepthBias, bScreenSpace);
	}
}

void DrawCrossSpline(FPrimitiveDrawInterface* PDI, const URoadSplineComponent* Spline, float SplineKey, const FColor& Color, uint8 DepthPriorityGroup, float Thickness, float DepthBias, bool bScreenSpace)
{
	int SectionIndex = Spline->FindRoadSectionOnSplineKey(SplineKey);

	if (SectionIndex == INDEX_NONE)
	{
		return;
	}

	int LeftSectionIndex = Spline->GetRoadLayout().FindSideSection(SectionIndex, ERoadLaneSectionSide::Left);
	int RightSectionIndex = Spline->GetRoadLayout().FindSideSection(SectionIndex, ERoadLaneSectionSide::Right);

	if (LeftSectionIndex == INDEX_NONE)
	{
		LeftSectionIndex = 0;
	}

	if (RightSectionIndex == INDEX_NONE)
	{
		RightSectionIndex = 0;
	}

	const float SOffset = Spline->GetDistanceAlongSplineAtSplineInputKey(SplineKey);
	const auto LeftLoc = Spline->EvalLanePoistion(LeftSectionIndex, -Spline->GetLaneSection(LeftSectionIndex).Left.Num(), SOffset, 1.0, ESplineCoordinateSpace::World);
	const auto RightLoc = Spline->EvalLanePoistion(RightSectionIndex, +Spline->GetLaneSection(RightSectionIndex).Right.Num(), SOffset, 1.0, ESplineCoordinateSpace::World);

	PDI->DrawLine(LeftLoc, RightLoc, Color, DepthPriorityGroup, Thickness, DepthBias, bScreenSpace);
}

FLinearColor HSVMul(const FLinearColor& Color, float Saturation, float Brightness)
{
	auto HSV = Color.LinearRGBToHSV();
	HSV.G *= Saturation;
	HSV.B *= Brightness;
	return HSV.HSVToLinearRGB();
}

} // DrawUtils