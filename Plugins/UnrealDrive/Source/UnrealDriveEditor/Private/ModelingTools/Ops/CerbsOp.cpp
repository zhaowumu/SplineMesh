/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "TriangulateRoadOp.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "RoadMeshTools/RoadLanePolylineArrangement.h"
#include "Utils/OpUtils.h"
#include "Utils/CurveUtils.h"
#include "DynamicMesh/MeshNormals.h"
#include "UnrealDrivePreset.h"

#define LOCTEXT_NAMESPACE "FCurbsOp"

using namespace UnrealDrive;

struct FCerbRoadPosition : public FRoadPosition
{
	int SectionIndex = 0; // For dynamic mesh GroupID
};


struct FRoadLanePolylineCurb : public TRoadLanePolyline<FCerbRoadPosition, FRoadLanePolylineCurb>
{
	FName ProfileName;
	double CurbsHeight;

	virtual bool CanAppend(const FRoadLanePolylineCurb& Other, EAppandMode AppandMode, double Tolerance) const override
	{

		if (ProfileName != Other.ProfileName)
		{
			return false;
		}

		if (!FMath::IsNearlyEqual(CurbsHeight, Other.CurbsHeight, Tolerance))
		{
			return false;
		}

		return TRoadLanePolyline<FCerbRoadPosition, FRoadLanePolylineCurb>::CanAppend(Other, AppandMode, Tolerance);
	}
};

using FRoadCurbArrangemen = TRoadLanePolylineArrangement<FRoadLanePolylineCurb>;


static bool MakeCurb(const FRoadLanePolylineCurb& Polyline, const FCurblProfile& Profile, FDynamicMesh3& DynamicMesh, int MaterialID, double UV0Scale)
{
	if (Profile.CurbCurve.GetRichCurveConst()->GetNumKeys() < 2)
	{
		return false;
	}

	OpUtils::EnableDefaultAttributes(DynamicMesh, true, false, true, true, 1);

	const int StartVertexIndex = DynamicMesh.MaxVertexID();
	auto* MaterialIDOverlay = DynamicMesh.Attributes()->GetMaterialID();
	auto* UV0Overlay = DynamicMesh.Attributes()->GetUVLayer(0);

	const float MaxSquareDistanceFromCurve = 0.01;
	const float Tolerance = 0.01;
	const int ReparamSteps = 200;

	TArray<float> Values;
	TArray<float> Times;
	if (!CurveUtils::CurveToPolyline(*Profile.CurbCurve.GetRichCurveConst(), 0.0, Profile.Width, MaxSquareDistanceFromCurve, Tolerance, ReparamSteps, Values, Times))
	{
		return false;
	}
	const int StepSize = Values.Num();
	
	TArray<float> AccumulatedValue;
	AccumulatedValue.SetNum(StepSize);
	for (int i = 1; i < StepSize; ++i)
	{
		AccumulatedValue[i] = AccumulatedValue[i - 1] + FMath::Sqrt(FMath::Square(Values[i] - Values[i - 1]) + FMath::Square(Times[i] - Times[i - 1]));
	}

	float MinValue, MaxValue;
	Profile.CurbCurve.GetRichCurveConst()->GetValueRange(MinValue, MaxValue);

	float MinTime, MaxTime;
	Profile.CurbCurve.GetRichCurveConst()->GetTimeRange(MinTime, MaxTime);

	float AccumulatedLength = 0;
	for (int Step = 0; Step < Polyline.Num(); ++Step)
	{
		auto& RefPoint = Polyline[Step];

		FVector UpVector;
		FVector RightVector;
		FVector ForwardVector;
		double SinA;
		GetThreeVectors(Polyline.Vertices, Step, RightVector, UpVector, ForwardVector, SinA);

		for (int i = 0; i < StepSize; ++i)
		{
			FVector3d Vertex = RefPoint.Location - RightVector * (Times[i] - (MaxTime - MinTime) * 0.5) / SinA + UpVector * (Values[i] - MaxValue + Polyline.CurbsHeight);
			DynamicMesh.AppendVertex(Vertex);

			UV0Overlay->AppendElement(FVector2f(AccumulatedLength * UV0Scale, (AccumulatedValue.Last() - AccumulatedValue[i]) * UV0Scale));
		}

		if (Step != Polyline.Num() - 1)
		{
			AccumulatedLength += (Polyline[Step].Location - Polyline[Step + 1].Location).Length();
		}
	}

	for (int Step = 0; Step < Polyline.Num() - 1; ++Step)
	{
		for (int i = 0; i < StepSize - 1; ++i)
		{
			FIndex3i T1 = {
				(Step + 0) * StepSize + 0 + i,
				(Step + 1) * StepSize + 0 + i,
				(Step + 0) * StepSize + 1 + i,
			};
			FIndex3i T2 = {
				(Step + 1) * StepSize + 0 + i,
				(Step + 1) * StepSize + 1 + i,
				(Step + 0) * StepSize + 1 + i,
			};

			int TID1 = DynamicMesh.AppendTriangle(T1);
			int TID2 = DynamicMesh.AppendTriangle(T2);

			UV0Overlay->SetTriangle(TID1, T1);
			UV0Overlay->SetTriangle(TID2, T2);

			MaterialIDOverlay->SetValue(TID1, MaterialID);
			MaterialIDOverlay->SetValue(TID2, MaterialID);

			DynamicMesh.SetTriangleGroup(TID1, Polyline[Step].SectionIndex);
			DynamicMesh.SetTriangleGroup(TID2, Polyline[Step].SectionIndex);
		}
	}

	return true;
}

static TArray<FCerbRoadPosition> MakePolylineCerb(const TArray<FArrangementVertex3d>& Vertexes, const TArray<int>& VerticesIDs, const FRoadPolygoneBase* PolyFilter)
{
	TArray<FRoadPosition> Tmp = RoadPolygoneUtils::MakePolyline(Vertexes, VerticesIDs, PolyFilter);
	TArray<FCerbRoadPosition> Ret;
	Ret.SetNumZeroed(Tmp.Num());
	for (int i = 0; i < Tmp.Num(); ++i)
	{
		*static_cast<FRoadPosition*>(&Ret[i]) = Tmp[i];
	}

	if (PolyFilter && PolyFilter->GetType() == ERoadPolygoneType::RoadLane)
	{
		auto* LanePoly = static_cast<const FRoadLanePolygone*>(PolyFilter);
		for (auto& It : Ret)
		{
			It.SectionIndex = LanePoly->SectionIndex;
		}
	}
	return Ret;
}


void FCurbsOp::CalculateResult(FProgressCancel* Progress)
{
	ResultInfo.Result = EGeometryResultType::InProgress;

	if (!BaseData || BaseData->ResultInfo.HasFailed())
	{
		ResultInfo.SetFailed();
		return;
	}

	FRoadCurbArrangemen Arrangemen;

	// Add road lanes mark attributes to Arrangemen
	for (const auto& Poly : BaseData->Polygons)
	{
		if (auto* RoadLaneSidewalk = Poly->GetLaneInstance().GetPtr<FRoadLaneSidewalk>())
		{
			auto AddToArrangemen = [this, &Poly, RoadLaneSidewalk, &Arrangemen](const TArray<int>& VIDs, bool bReverse)
			{
				FRoadLanePolylineCurb Polyline;
				Polyline.Vertices = MakePolylineCerb(BaseData->Vertices3d, VIDs, Poly.Get());
				if (bReverse)
				{
					Algo::Reverse(Polyline.Vertices);
				}
				Polyline.ProfileName = RoadLaneSidewalk->CurbProfile;
				Polyline.CurbsHeight = CurbsHeight;
				if (Polyline.Vertices.Num() > 1)
				{
					const double ArrangemenTolerance = 1.0;
					Arrangemen.Insert(MoveTemp(Polyline), ArrangemenTolerance);
				}
			};

			if (Poly->GetType() == ERoadPolygoneType::RoadLane)
			{
				auto LanePoly = StaticCastSharedPtr<FRoadLanePolygone>(Poly);
				const bool bIsRight = LanePoly->LaneIndex >= 0;
				if (RoadLaneSidewalk->bBeginCurb && !LanePoly->IsLoop())
				{
					AddToArrangemen(LanePoly->BeginCapVertices, bIsRight);
				}
				if (RoadLaneSidewalk->bEndCurb && !LanePoly->IsLoop())
				{
					AddToArrangemen(LanePoly->EndCapVertices, !bIsRight);
				}
				if (RoadLaneSidewalk->bInsideCurb)
				{
					AddToArrangemen(LanePoly->InsideLineVertices, !bIsRight);
				}
				if (RoadLaneSidewalk->bOutsideCurb)
				{
					AddToArrangemen(LanePoly->OutsideLineVertices, bIsRight);
				}
			}
			else if (Poly->GetType() == ERoadPolygoneType::Simple)
			{
				auto SimplePoly = StaticCastSharedPtr<FRoadSimplePolygone>(Poly);
				if (RoadLaneSidewalk->bInsideCurb || RoadLaneSidewalk->bOutsideCurb)
				{
					AddToArrangemen(SimplePoly->LineVertices, true);
				}
			}
		}
	}

	OpUtils::EnableDefaultAttributes(*ResultMesh, true, true, true, true, 1);

	auto Profiles = UUnrealDrivePresetBase::GetAllProfiles(&UUnrealDrivePresetBase::CurbProfiles);
	int MaxMaterialId = 0;
	TMap<FName, int> MaterialIDMap;

	for (const auto& It : Arrangemen.Polylines)
	{
		if (!It.ProfileName.IsNone())
		{
			if (auto* Profile = Profiles.Find(It.ProfileName))
			{
				int MaterialID;
				if (int* FoundMaterialId = MaterialIDMap.Find(It.ProfileName))
				{
					MaterialID = *FoundMaterialId;
				}
				else
				{
					MaterialIDMap.Add(It.ProfileName, MaxMaterialId);
					MaterialID = MaxMaterialId;
					++MaxMaterialId;
				}

				const double VScaleFactor = 0.001;
				FDynamicMesh3 DynamicMesh;
				if (!MakeCurb(It, *Profile,  DynamicMesh, MaterialID, UV0Scale))
				{
					ResultInfo.AddWarning({ 0, LOCTEXT("CalculateResultWarning_MarkStruct", "Mark: Can't build curb mesh") });
				}

				if (DynamicMesh.VertexCount() > 0 && DynamicMesh.TriangleCount() > 0)
				{
					OpUtils::AppendMesh(*ResultMesh, DynamicMesh);
				}
			}
			else
			{
				ResultInfo.AddWarning({ 0, FText::Format(LOCTEXT("CalculateResultWarning_MarkNAme", "Mark: Can't find curb profile: {0}"), FText::FromName(It.ProfileName)) });
			}
		}
	}

	// Resolve materials slot name
	ResultMaterialSlots.SetNum(MaxMaterialId);
	for (int i = 0; i < MaxMaterialId; ++i)
	{
		ResultMaterialSlots[i] = *MaterialIDMap.FindKey(i);
	}

	// ========================== Compute Normals ==========================
	FMeshNormals::QuickComputeVertexNormals(*ResultMesh);
	FMeshNormals::InitializeOverlayToPerVertexNormals(ResultMesh->Attributes()->PrimaryNormals(), true);
	FMeshNormals::QuickRecomputeOverlayNormals(*ResultMesh);

	ResultInfo.SetSuccess();
}



#undef LOCTEXT_NAMESPACE
