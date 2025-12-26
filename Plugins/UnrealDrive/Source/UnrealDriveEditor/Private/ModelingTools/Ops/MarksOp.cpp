/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "TriangulateRoadOp.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "UnrealDrivePreset.h"
#include "RoadMeshTools/RoadLanePolylineArrangement.h"

#define LOCTEXT_NAMESPACE "MarksOp"

using namespace UnrealDrive;

struct FMarkRoadPosition: public FRoadPosition
{
	int SectionIndex = 0; // For dynamic mesh GroupID
};


struct FRoadLanePolylineMark : public TRoadLanePolyline<FMarkRoadPosition, FRoadLanePolylineMark>
{
	FRoadLanePolylineMark() = default;
	FRoadLanePolylineMark(const TArray<FMarkRoadPosition>& Vertices)
		: TRoadLanePolyline<FMarkRoadPosition, FRoadLanePolylineMark>(Vertices)
	{
	}

	FRoadLanePolylineMark(TArray<FMarkRoadPosition>&& Vertices)
		: TRoadLanePolyline<FMarkRoadPosition, FRoadLanePolylineMark>(MoveTemp(Vertices))
	{
	}

	FName ProfileName;

	virtual bool CanAppend(const FRoadLanePolylineMark& Other, EAppandMode AppandMode, double Tolerance) const override
	{
		if (ProfileName != Other.ProfileName)
		{
			return false;
		}

		return TRoadLanePolyline<FMarkRoadPosition, FRoadLanePolylineMark>::CanAppend(Other, AppandMode, Tolerance);
	}
};

using FRoadMarkArrangemen = TRoadLanePolylineArrangement<FRoadLanePolylineMark>;



TArray<FMarkRoadPosition> GetSubPolyline(const TArray<FMarkRoadPosition>& Vertices, double S0, double S1, double SnapToDistance = 0.1)
{
	int32 StartKey = 0;
	int32 EndKey = Vertices.Num() - 1;

	for (int32 KeyIndex = 0; KeyIndex < Vertices.Num(); ++KeyIndex)
	{
		const double CurrentS = Vertices[KeyIndex].SOffset;
		if (CurrentS < S0)
		{
			StartKey = KeyIndex;
		}
		if (CurrentS > S1)
		{
			EndKey = KeyIndex;
			break;
		}
	}

	TArray<FMarkRoadPosition> SubLane(&Vertices[StartKey], EndKey - StartKey + 1);

	if (SubLane.Num())
	{
		if (SubLane[0].SOffset < S0)
		{
			if (SubLane.Num() > 1)
			{
				double Alpha = (S0 - SubLane[0].SOffset) / (SubLane[1].SOffset - SubLane[0].SOffset);
				SubLane[0].Location = FMath::Lerp(SubLane[0].Location, SubLane[1].Location, Alpha);
				if ((SubLane[0].Location - SubLane[1].Location).Size() < SnapToDistance)
				{
					SubLane.RemoveAt(0, EAllowShrinking::No);
				}
			}
			SubLane[0].SOffset = S0;
		}

		if (SubLane[SubLane.Num() - 1].SOffset > S1)
		{
			if (SubLane.Num() > 1)
			{
				double Alpha = (S1 - SubLane[SubLane.Num() - 2].SOffset) / (SubLane[SubLane.Num() - 1].SOffset - SubLane[SubLane.Num() - 2].SOffset);
				SubLane[SubLane.Num() - 1].Location = FMath::Lerp(SubLane[SubLane.Num() - 2].Location, SubLane[SubLane.Num() - 1].Location, Alpha);
				if ((SubLane[SubLane.Num() - 1].Location - SubLane[SubLane.Num() - 2].Location).Size() < SnapToDistance)
				{
					SubLane.RemoveAt(SubLane.Num() - 1, EAllowShrinking::No);
				}
			}
			SubLane[SubLane.Num() - 1].SOffset = S1;
		}
	}

	return MoveTemp(SubLane);
}


static bool MakeMarkMesh(const TArray<FMarkRoadPosition>& InVertices, double S0, double S1, double ZOffset, double ROffset, double Width, double VScaleFactor, const FColor & VertectColor, uint8 MaterialID, FDynamicMesh3& DynamicMesh)
{
	FRoadLanePolylineMark SubLine;
	const TArray<FMarkRoadPosition>* Vertices;
	if (S0 < 0 || S1 < 0)
	{
		Vertices = &InVertices;

	}
	else
	{
		SubLine.Vertices = GetSubPolyline(InVertices, S0, S1);
		Vertices = &SubLine.Vertices;
	}

	if (Vertices->Num() < 2)
	{
		return false;
	}

	OpUtils::EnableDefaultAttributes(DynamicMesh, true, true, true, true, 1);

	const int StartVertexIndex = DynamicMesh.MaxVertexID();
	const FLinearColor LinearColor(VertectColor);
	const FVector4f FloatColor(LinearColor.R, LinearColor.G, LinearColor.B, LinearColor.A);
	auto* ColorOverlay = DynamicMesh.Attributes()->PrimaryColors();
	auto* MaterialIDOverlay = DynamicMesh.Attributes()->GetMaterialID();
	auto* UV0Overlay = DynamicMesh.Attributes()->GetUVLayer(0);

	for (int i = 0; i < Vertices->Num(); ++i)
	{
		const auto& It = (*Vertices)[i];

		FVector UpVector;
		FVector RightVector;
		FVector ForwardVector;
		double SinA;
		GetThreeVectors(*Vertices, i, RightVector, UpVector, ForwardVector, SinA);

		FVertexInfo VertexA;
		VertexA.bHaveN = true;
		//VertexA.bHaveUV = true;
		VertexA.bHaveC = true;
		VertexA.Position = It.Location + RightVector * ((ROffset - Width * 0.5) / SinA) + UpVector * ZOffset; 
		VertexA.Normal = FVector3f(UpVector);
		//VertexA.UV = FVector2f(0.0, S0 * VScaleFactor);
		//VertexA.Color = 

		FVertexInfo VertexB;
		VertexB.bHaveN = true;
		//VertexB.bHaveUV = true;
		VertexB.Position = It.Location + RightVector * ((ROffset + Width * 0.5) / SinA) + UpVector * ZOffset;
		VertexB.Normal = FVector3f(UpVector);
		//VertexB.UV = FVector2f(1.0, S0 * VScaleFactor);
		//VertexB.Color = 

		DynamicMesh.AppendVertex(VertexA);
		DynamicMesh.AppendVertex(VertexB);

		UV0Overlay->AppendElement(FVector2f(0.0, It.SOffset * VScaleFactor));
		UV0Overlay->AppendElement(FVector2f(1.0, It.SOffset * VScaleFactor));

		ColorOverlay->AppendElement(FloatColor);
		ColorOverlay->AppendElement(FloatColor);
	}

	for (int i = 0; i < Vertices->Num() - 1; ++i)
	{
		FIndex3i T1 = FIndex3i{ StartVertexIndex + i * 2 + 0, StartVertexIndex + i * 2 + 1, StartVertexIndex + i * 2 + 2 };
		FIndex3i T2 = FIndex3i{ StartVertexIndex + i * 2 + 1, StartVertexIndex + i * 2 + 3, StartVertexIndex + i * 2 + 2 };

		int TID1 = DynamicMesh.AppendTriangle(T1);
		int TID2 = DynamicMesh.AppendTriangle(T2);

		UV0Overlay->SetTriangle(TID1, T1);
		UV0Overlay->SetTriangle(TID2, T2);

		ColorOverlay->SetTriangle(TID1, T1);
		ColorOverlay->SetTriangle(TID2, T2);

		MaterialIDOverlay->SetValue(TID1, (int32)MaterialID);
		MaterialIDOverlay->SetValue(TID2, (int32)MaterialID);

		DynamicMesh.SetTriangleGroup(TID1, (*Vertices)[i].SectionIndex);
		DynamicMesh.SetTriangleGroup(TID2, (*Vertices)[i].SectionIndex);
	}

	return true;
};


static bool MakeMarkMeshBroken(const TArray<FMarkRoadPosition>& Vertices, double ZOffset, double ROffset, double Width, double Long, double Gap, double VScaleFactor, const FColor& VertectColor, uint8 MaterialID, FDynamicMesh3& DynamicMesh)
{
	if (Vertices.Num() < 2)
	{
		return false;
	}

	double Length = 0; 

	for (int i = 0; i < Vertices.Num() - 1; ++i)
	{
		Length += (Vertices[i].Location - Vertices[i + 1].Location).Length();
	}
	
	if (Length < Long)
	{
		const double S0 = Vertices[0].SOffset;
		const double S1 = Vertices.Last().SOffset;
		return MakeMarkMesh(Vertices, S0, S1, ZOffset, ROffset, Width, VScaleFactor, VertectColor, MaterialID, DynamicMesh);
	}

	const double SectionRation = Long / (Long + Gap);
	const int NumSections = FMath::RoundToInt(Length / (Long + Gap));
	const double SectionLength = Length / NumSections;
	const double AlignedLong = SectionLength * SectionRation;
	//const double AlignedGap = SectionLength * (1.0 - SectionRation);

	for (int i = 0; i < NumSections; ++i)
	{

		const double L0 = i * SectionLength;
		const double L1 = i * SectionLength + AlignedLong;
		MakeMarkMesh(Vertices, L0 , L1, ZOffset, ROffset, Width, VScaleFactor, VertectColor, MaterialID, DynamicMesh);
	}

	return true;
}


static bool MakeMarkMeshFromProfile(const TArray<FMarkRoadPosition>& Vertices, double ZOffset, double ROffset, const TInstancedStruct<FRoadLaneMarkProfile>& Profile, double VScaleFactor, FDynamicMesh3& DynamicMesh, TMap<FName, int>& MaterialIDMap, int& MaxMaterialId)
{
	if (auto* SolidProfile = Profile.GetPtr<FRoadLaneMarkProfileSolid>())
	{
		int MaterialID;
		if (int* FoundMaterialId = MaterialIDMap.Find(SolidProfile->MaterialProfile))
		{
			MaterialID = *FoundMaterialId;
		}
		else
		{
			MaterialIDMap.Add(SolidProfile->MaterialProfile, MaxMaterialId);
			MaterialID = MaxMaterialId;
			++MaxMaterialId;
		}
		return MakeMarkMesh(Vertices, -1, -1, ZOffset, ROffset, SolidProfile->Width, VScaleFactor, SolidProfile->VertexColor, MaterialID, DynamicMesh);
	}
	else if (auto* BrokedProfile = Profile.GetPtr<FRoadLaneMarkProfileBroked>())
	{
		int MaterialID;
		if (int* FoundMaterialId = MaterialIDMap.Find(BrokedProfile->MaterialProfile))
		{
			MaterialID = *FoundMaterialId;
		}
		else
		{
			MaterialIDMap.Add(BrokedProfile->MaterialProfile, MaxMaterialId);
			MaterialID = MaxMaterialId;
			++MaxMaterialId;
		}
		return MakeMarkMeshBroken(Vertices, ZOffset, ROffset, BrokedProfile->Width, BrokedProfile->Long, BrokedProfile->Gap, VScaleFactor, BrokedProfile->VertexColor, MaterialID, DynamicMesh);
	}
	else if (auto* DoubleProfile = Profile.GetPtr<FRoadLaneMarkProfileDouble>())
	{
		MakeMarkMeshFromProfile(Vertices, ZOffset, ROffset - DoubleProfile->Gap * 0.5, DoubleProfile->Left, VScaleFactor, DynamicMesh, MaterialIDMap, MaxMaterialId);
		MakeMarkMeshFromProfile(Vertices, ZOffset, ROffset + DoubleProfile->Gap * 0.5, DoubleProfile->Right, VScaleFactor, DynamicMesh, MaterialIDMap, MaxMaterialId);
		return true;
	}
	return false;
}

static int FindMinIndex(const TArray<FMarkRoadPosition>& Polyline) 
{
	if (Polyline.IsEmpty()) 
	{
		return INDEX_NONE;
	}

	const FMarkRoadPosition* MinVal = &Polyline[0];
	int MinIndex = 0;

	for (size_t i = 1; i < Polyline.Num(); ++i) 
	{
		if (Polyline[i].SOffset < MinVal->SOffset) 
		{
			MinVal = &Polyline[i];
			MinIndex = i;
		}
	}
	return MinIndex;
}

static void NormalizPolylineBySOffset(TArray<FMarkRoadPosition>& Polyline)
{
	if (Polyline.Num() == 0)
	{
		return;
	}

	int MinInd = FindMinIndex(Polyline);
	if (MinInd > 0)
	{
		Algo::Rotate(Polyline, MinInd);
	}

	if (Polyline.Num() > 2 && Polyline[1].SOffset > Polyline[2].SOffset)
	{
		Algo::Reverse(Polyline);
		MinInd = FindMinIndex(Polyline);
		if (MinInd > 0)
		{
			Algo::Rotate(Polyline, MinInd);
		}
	}
}

static TArray<FMarkRoadPosition> MakePolylineMark(const TArray<FArrangementVertex3d>& Vertexes, const TArray<int>& VerticesIDs, const FRoadPolygoneBase* PolyFilter)
{
	TArray<FRoadPosition> Tmp = RoadPolygoneUtils::MakePolyline(Vertexes, VerticesIDs, PolyFilter);
	TArray<FMarkRoadPosition> Ret;
	Ret.SetNumZeroed(Tmp.Num());
	for (int i = 0; i < Tmp.Num(); ++i)
	{
		*static_cast<FRoadPosition*>(&Ret[i]) = Tmp[i];
	}

	if (PolyFilter && PolyFilter->GetType() == ERoadPolygoneType::RoadLane)
	{
		auto* LanePoly = static_cast<const FRoadLanePolygone*>(PolyFilter);
		for (auto& It: Ret)
		{
			It.SectionIndex = LanePoly->SectionIndex;
		}
	}

	NormalizPolylineBySOffset(Ret);
	return Ret;
}


// ---------------------------------------------------------------------------------------------------------------------------------

void FMarksOp::CalculateResult(FProgressCancel* Progress)
{
#define CHECK_CANCLE() if (Progress && Progress->Cancelled()) { ResultInfo.Result = EGeometryResultType::Cancelled; return; }

	ResultInfo.Result = EGeometryResultType::InProgress;

	if (!BaseData || BaseData->ResultInfo.HasFailed())
	{
		ResultInfo.SetFailed();
		return;
	}

	if (Progress && Progress->Cancelled())
	{
		ResultInfo.Result = EGeometryResultType::Cancelled;
		return;
	}

	FRoadMarkArrangemen Arrangemen;
	const double ArrangemenTolerance = 1.0;

	// ========================== Add road lanes mark attributes to Arrangemen ==========================
	for (const auto& Poly : BaseData->Polygons)
	{
		if (Poly->GetType() == ERoadPolygoneType::RoadLane)
		{
			auto LanePoly = StaticCastSharedPtr<FRoadLanePolygone>(Poly);
			const auto& Section = LanePoly->GetSection();
			if (const auto* FoundAttribute = LanePoly->GetLaneAttributes().Find(UnrealDrive::LaneAttributes::Mark))
			{
				for (int AttributeIndex = 0; AttributeIndex < FoundAttribute->Keys.Num(); ++AttributeIndex)
				{
					const auto& MarkKey = FoundAttribute->Keys[AttributeIndex];
					const auto& MarkValue = MarkKey.GetValue<FRoadLaneMark>();
					if (!MarkValue.ProfileName.IsNone())
					{
						const double SOffsetStart = MarkKey.SOffset + Section.SOffset;
						const double SOffsetEnd = (AttributeIndex < FoundAttribute->Keys.Num() - 1) ? FoundAttribute->Keys[AttributeIndex + 1].SOffset + Section.SOffset : LanePoly->GetEndOffset();

						auto& LineVertices = LanePoly->LaneIndex == 0 ? LanePoly->InsideLineVertices : LanePoly->OutsideLineVertices;
						if (ensure(LineVertices.Num()))
						{
							FRoadLanePolylineMark LineMark;
							LineMark.Vertices = GetSubPolyline(::MakePolylineMark(BaseData->Vertices3d, LineVertices, LanePoly.Get()), SOffsetStart, SOffsetEnd);
							LineMark.ProfileName = MarkValue.ProfileName;
							if (LineMark.Vertices.Num() > 1)
							{
								Arrangemen.Insert(MoveTemp(LineMark), ArrangemenTolerance);
							}
						}
					}
				}
			}
		}


		CHECK_CANCLE();
	}

	// ========================== Convert polylines S-offset to L-offset ==========================
	for (auto& It : Arrangemen.Polylines)
	{
		if (It.Num() > 0)
		{
			double Length = 0;
			It[0].SOffset = Length;
			for (int i = 1; i < It.Num(); ++i)
			{
				const auto& PtA = It[i];
				const auto& PtB = It[i - 1];
				Length += (PtB.Location - PtA.Location).Length();
				It[i].SOffset = Length;
			}
		}
	}

	// ========================== Create mesh ==========================
	OpUtils::EnableDefaultAttributes(*ResultMesh, true, true, true, true, 1);

	auto Profiles = UUnrealDrivePresetBase::GetAllProfiles(&UUnrealDrivePresetBase::LaneMarkProfiles);
	int MaxMaterialId = 0;
	TMap<FName, int> MaterialIDMap;

	for (const auto& It : Arrangemen.Polylines)
	{
		if (!It.ProfileName.IsNone())
		{
			if (auto* Profile = Profiles.Find(It.ProfileName))
			{
				const double VScaleFactor = 0.001;
				FDynamicMesh3 DynamicMesh;
				if (!MakeMarkMeshFromProfile(It.Vertices, MarkOffset, 0.0, *Profile, VScaleFactor, DynamicMesh, MaterialIDMap, MaxMaterialId))
				{
					ResultInfo.AddWarning({ 0, FText::Format(LOCTEXT("CalculateResultWarning_MarkStruct", "Mark: Can't build mark for unqnown FRoadLaneMarkProfile struct: {0}"), FText::FromString(Profile->GetScriptStruct()->GetName())) });
				}

				if (DynamicMesh.VertexCount() > 0 && DynamicMesh.TriangleCount() > 0)
				{
					OpUtils::AppendMesh(*ResultMesh, DynamicMesh);
				}
			}
			else
			{
				ResultInfo.AddWarning({ 0, FText::Format(LOCTEXT("CalculateResultWarning_MarkNAme", "Mark: Can't find mark profile: {0}"), FText::FromName(It.ProfileName)) });
			}
		}

		CHECK_CANCLE();
	}

	// ========================== Resolve materials slot name ==========================
	ResultMaterialSlots.SetNum(MaxMaterialId);
	for (int i = 0; i < MaxMaterialId; ++i)
	{
		ResultMaterialSlots[i] = *MaterialIDMap.FindKey(i);
	}
	
	ResultInfo.SetSuccess();

#undef CHECK_CANCLE
}

#undef LOCTEXT_NAMESPACE
