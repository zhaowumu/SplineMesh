/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "RoadSplineComponent.h"
#include "UnrealDriveVersion.h"
#include "Engine/Engine.h"
#include "SceneManagement.h"
#include "DynamicMeshBuilder.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "UnrealDriveSettings.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UnrealDrive.h"


#if WITH_EDITOR
#include "UnrealDriveSubsystem.h"
#include "RoadSceneProxy.h"
#endif

#define LOCTEXT_NAMESPACE "URoadSplineComponent"

/*
 * Fancy math to recalculate tangents to turn into circle
 */
static double CalcTangentMultiplier(const float InRadius, const float InRotInc)
{
	static constexpr double A = .5f;
	static constexpr double A2 = A * A;
	static constexpr double A3 = A2 * A;

	// Use first and second keys added as a sample calculation
	const FVector T0 = FVector::ForwardVector;
	const FVector T1 = T0.RotateAngleAxis(InRotInc, FVector::UpVector);
	const FVector P0 = FVector::RightVector * InRadius;
	const FVector P1 = P0.RotateAngleAxis(InRotInc, FVector::UpVector);

	// Calculate the difference between the actual interpolated midpoint and expected interpolated midpoint
	const FVector ActualVal = FMath::CubicInterp(P0, T0, P1, T1, A);
	const FVector ExpectedVal = P0.RotateAngleAxis(InRotInc * A, FVector::UpVector);
	const double Diff = (ActualVal.X - ExpectedVal.X);

	// Do a partial calculation of the cubic interpolation equation
	static constexpr double C1 = (A3 - (2 * A2) + A), C2 = (A3 - A2);
	const double PartialInterp = -1.f * ((C1 * T0.X) + (C2 * T1.X));

	// Calculate the final multiplier to multiply to all normalized tangents
	return FMath::IsNearlyZero(PartialInterp) ? 1.f : ((Diff / PartialInterp) + 1.f);
}



struct FPolynome
{
	double A, B, C, D;
};

bool PolynomialToSpline(const FPolynome & Polynome, FInterpCurveVector& Out)
{
	/*
	UE_NODISCARD static constexpr FORCEINLINE_DEBUGGABLE T CubicInterp( const T& P0, const T& T0, const T& P1, const T& T1, const U& A )
		{
			const U A2 = A * A;
			const U A3 = A2 * A;

			return T((((2*A3)-(3*A2)+1) * P0) + ((A3-(2*A2)+A) * T0) + ((A3-A2) * T1) + (((-2*A3)+(3*A2)) * P1));
		}

	*/

	return false;
}


URoadSplineMetadata::URoadSplineMetadata(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void URoadSplineMetadata::InsertPoint(int32 Index, float t, bool bClosedLoop) 
{
	check(Spline.IsValid());
	check(Spline->PointTypes.Num() == Spline->GetNumberOfSplinePoints() - 1);
	check(Index >= 0);

	const int PrevIndex = Spline->GetPrevPoint(Index);

	if (PrevIndex >= 0)
	{
		Spline->PointTypes.Insert(ERoadSplinePointTypeOverride::Inherited, Index);

		if (Spline->PointTypes[PrevIndex] == ERoadSplinePointTypeOverride::Arc)
		{
			const int32 EndPointIndex = PrevIndex;
			const int32 StartPointIndex = Spline->GetPrevPoint(EndPointIndex);

			if (EndPointIndex >= 0 && StartPointIndex >= 0)
			{
				Spline->PointTypes[StartPointIndex] = ERoadSplinePointTypeOverride::Inherited;
				Spline->PointTypes[EndPointIndex] = ERoadSplinePointTypeOverride::Inherited;
			}
		}
	}
	

}

void URoadSplineMetadata::UpdatePoint(int32 Index, float t, bool bClosedLoop) {}
void URoadSplineMetadata::AddPoint(float InputKey) {}
void URoadSplineMetadata::RemovePoint(int32 Index) {}
void URoadSplineMetadata::DuplicatePoint(int32 Index) {}
void URoadSplineMetadata::CopyPoint(const USplineMetadata* FromSplineMetadata, int32 FromIndex, int32 ToIndex) {}
void URoadSplineMetadata::Reset(int32 NumPoints) {}
void URoadSplineMetadata::Fixup(int32 NumPoints, USplineComponent* SplineComp) {}


//===========================================================================================================
URoadSplineComponent::URoadSplineComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	//bUseEditorCompositing = true;
	//DepthPriorityGroup = SDPG_Foreground;

	bHiddenInGame = true;

	SplineMetadata = CreateDefaultSubobject<URoadSplineMetadata>(TEXT("DriveSplineMetadataMetadata"));
	SplineMetadata->Spline = this;

	PredecessorConnection = CreateDefaultSubobject<URoadConnection>(TEXT("PredecessorConnection"));
	SuccessorConnection = CreateDefaultSubobject<URoadConnection>(TEXT("SuccessorConnection"));


	SplineCurves.Position.Points[1].OutVal = FVector(1000, 0, 0);
	UpdateSpline();

	//UE_LOG(LogUnrealDrive, Warning, TEXT("URoadSplineComponent::URoadSplineComponent(%s)"), *GetName());
}

URoadSplineComponent::~URoadSplineComponent()
{
	//UE_LOG(LogUnrealDrive, Warning, TEXT("URoadSplineComponent::~URoadSplineComponent(%s)"), *GetName());
}

void URoadSplineComponent::Serialize(FArchive& Ar)
{
	if (Ar.IsSaving())
	{
		FixUpSegments();
		VapidateConnections();
	}

	Super::Serialize(Ar);

	if (Ar.IsLoading())
	{
		FixUpSegments();
		VapidateConnections();
	}

	uint8 MajorVer = UNREALDRIVE_MAJOR_VERSION;
	uint8 MinorVer = UNREALDRIVE_MINOR_VERSION;
	uint8 PatchVer = UNREALDRIVE_PATCH_VERSION;
	uint8 Reserved = 0;

	Ar << MajorVer;
	Ar << MinorVer;
	Ar << PatchVer;
	Ar << Reserved;
}

void URoadSplineComponent::VapidateConnections()
{
	TSet<ULaneConnection*> ConnectionSet;
	auto ValidateUnique = [&ConnectionSet](TObjectPtr< ULaneConnection>& Connection)
	{
		if (ConnectionSet.Contains(Connection))
		{
			UE_LOG(LogUnrealDrive, Error, TEXT("URoadSplineComponent::VapidateConnections(); Found not unque connection for %s"), *Connection->GetPathName());
			Connection = nullptr;
		}
		else
		{
			ConnectionSet.Add(Connection);
		}
	};
	for (auto& Section : GetRoadLayout().Sections)
	{
		for (auto& It : Section.Left)
		{
			ValidateUnique(It.PredecessorConnection);
			ValidateUnique(It.SuccessorConnection);
		}
		for (auto& It : Section.Right)
		{
			ValidateUnique(It.PredecessorConnection);
			ValidateUnique(It.SuccessorConnection);
		}
	}

	UpdateRoadLayout();
}

void URoadSplineComponent::UpdateSpline()
{	
	//SetClosedLoop(false, false);
	FixUpSegments();
	UpdateAutoTangents();
	Super::UpdateSpline();
	UpdateLaneSectionBounds();
	/*
	for (auto& Section : Sections)
	{
		for (auto& Lane : Section.Left)
		{
			Lane.Width.AutoSetTangents();
		}
		for (auto& Lane : Section.Right)
		{
			Lane.Width.AutoSetTangents();
		}
	}
	*/
}

void URoadSplineComponent::UpdateSpline(int EditingPointIndex)
{
	//SetClosedLoop(false, false);
	FixUpSegments();
	UpdateAutoTangents(EditingPointIndex);
	Super::UpdateSpline();
	UpdateLaneSectionBounds();
	/*
	for (auto& Section : Sections)
	{
		for (auto& Lane : Section.Left)
		{
			Lane.Width.AutoSetTangents();
		}
		for (auto& Lane : Section.Right)
		{
			Lane.Width.AutoSetTangents();
		}
	}
	*/
}

ERoadSplinePointType URoadSplineComponent::GetRoadSplinePointType(int32 PointIndex) const
{
	if ((PointIndex >= 0) && (PointIndex < SplineCurves.Position.Points.Num()))
	{
		if (PointTypes.IsValidIndex(PointIndex) && PointTypes[PointIndex] == ERoadSplinePointTypeOverride::Arc)
		{
			return ERoadSplinePointType::Arc;
		}
		return static_cast<ERoadSplinePointType>(GetSplinePointType(PointIndex));
	}

	return ERoadSplinePointType::Constant;
}

void URoadSplineComponent::SetRoadSplinePointType(int32 PointIndex, ERoadSplinePointType Mode, bool bUpdateSpline)
{
	FixUpSegments();

	if ((PointIndex >= 0) && (PointIndex < SplineCurves.Position.Points.Num()))
	{
		if (Mode == ERoadSplinePointType::Arc)
		{
			PointTypes[PointIndex] = ERoadSplinePointTypeOverride::Arc;
			SplineCurves.Position.Points[PointIndex].InterpMode = EInterpCurveMode::CIM_CurveUser;
			const int NextPoint = GetNextPoint(PointIndex);
			if (NextPoint >= 0)
			{
				SplineCurves.Position.Points[NextPoint].InterpMode = EInterpCurveMode::CIM_CurveUser;
			}
		}
		else
		{
			PointTypes[PointIndex] = ERoadSplinePointTypeOverride::Inherited;
			SplineCurves.Position.Points[PointIndex].InterpMode = ConvertSplinePointTypeToInterpCurveMode(static_cast<ESplinePointType::Type>(Mode));
		}
		if (bUpdateSpline)
		{
			UpdateSpline();
		}
	}
}

void URoadSplineComponent::FixUpSegments()
{
	
	if (GetNumberOfSplinePoints() != PointTypes.Num())
	{
		PointTypes.Reset();
		PointTypes.SetNum(GetNumberOfSplinePoints());
		//UE_LOG(LogUnrealDrive, Warning, TEXT("URoadSplineComponent::FixUpSegments() %i %i"), GetNumberOfSplinePoints(), PointTypes.Num());
	}
}

int URoadSplineComponent::GetNextPoint(int PointIndex) const
{
	if (IsClosedLoop())
	{
		return (PointIndex + 1) % GetNumberOfSplinePoints();
	}
	else if (PointIndex < (GetNumberOfSplinePoints() - 2))
	{
		return PointIndex + 1;
	}
	else
	{
		return -1;
	}
};

int URoadSplineComponent::GetPrevPoint(int PointIndex) const
{
	if (IsClosedLoop())
	{
		return (PointIndex == 0) ? GetNumberOfSplinePoints() - 1 : PointIndex - 1;
	}
	else if (PointIndex > 0)
	{
		return PointIndex - 1;
	}
	else
	{
		return -1;
	}
};

void URoadSplineComponent::UpdateAutoTangents(int EditingPointIndex)
{
	if (GetNumberOfSplineSegments() <= 0)
	{
		return; 
	}

	FixUpSegments();

	// Set CIM_CurveUser for all arc segments
	for (int PointIndex = 0; PointIndex < GetNumberOfSplineSegments(); ++PointIndex)
	{
		if (PointTypes[PointIndex] == ERoadSplinePointTypeOverride::Arc)
		{
			SplineCurves.Position.Points[PointIndex].InterpMode = CIM_CurveUser;
			const int NextPointIndex = GetNextPoint(PointIndex);
			if (NextPointIndex >= 0)
			{
				SplineCurves.Position.Points[NextPointIndex].InterpMode = CIM_CurveUser;
			}
		}
	}

	// Normalize all Linear segments
	/*
	for (int PointIndex = 0; PointIndex < GetNumberOfSplineSegments(); ++PointIndex)
	{
		if (GetRoadSplinePointType(PointIndex) == ERoadSplinePointType::Linear)
		{
			AdjustLineSegment(PointIndex);
		}
	}
	*/
	
	// Normalize all Arc segments
	if (EditingPointIndex == INDEX_NONE)
	{
		for (int PointIndex = 0; PointIndex < GetNumberOfSplineSegments(); ++PointIndex)
		{
			if (GetRoadSplinePointType(PointIndex) == ERoadSplinePointType::Arc)
			{
				const int NextPointIndex = GetNextPoint(PointIndex);
				const int PrePointIndex = GetPrevPoint(PointIndex);

				const bool bPinStartTangent = PrePointIndex >= 0 && GetRoadSplinePointType(PrePointIndex) == ERoadSplinePointType::Linear;
				const bool bPinEndTangent = NextPointIndex >= 0 && GetRoadSplinePointType(NextPointIndex) == ERoadSplinePointType::Linear;

				if (bPinStartTangent && bPinEndTangent)
				{
					AdjustArcSegment(PointIndex, EComputeArcMode::AdjEndTangent);
				}
				else if (bPinStartTangent)
				{
					AdjustArcSegment(PointIndex, EComputeArcMode::AdjEndTangent);
				}
				else if (bPinEndTangent)
				{
					AdjustArcSegment(PointIndex, EComputeArcMode::AdjStartTangent);
				}
				else // !bPinEndTangent && !bPinEndTangent
				{
					AdjustArcSegment(PointIndex, EComputeArcMode::AdjEndTangent);
				}
			}
		}
	}
	else // EditingPointIndex != INDEX_NONE
	{
		// Update the second part of spline
		int NumSegments = IsClosedLoop() ? GetNumberOfSplineSegments() - 1 : GetNumberOfSplineSegments() - EditingPointIndex;
		for (int i = 0; i < NumSegments; ++i)
		{
			const int iSegment = (i + EditingPointIndex) % GetNumberOfSplineSegments();
			if (GetRoadSplinePointType(iSegment) == ERoadSplinePointType::Arc)
			{
				const int iNextSegment = GetNextPoint(iSegment);
				const int iPreSegment = GetPrevPoint(iSegment);

				const bool bPinStartTangent = iPreSegment >= 0 && GetRoadSplinePointType(iPreSegment) == ERoadSplinePointType::Linear;
				const bool bPinEndTangent = iNextSegment >= 0 && GetRoadSplinePointType(iNextSegment) == ERoadSplinePointType::Linear;

				if (bPinStartTangent && bPinEndTangent)
				{
					AdjustArcSegment(iSegment, EComputeArcMode::AdjEndPos);
					break;
				}
				else if (bPinStartTangent)
				{
					AdjustArcSegment(iSegment, EComputeArcMode::AdjEndTangent);
				}
				else if (bPinEndTangent)
				{
					AdjustArcSegment(iSegment, EComputeArcMode::AdjEndPos);
					break;
				}
				else // !bPinEndTangent && !bPinEndTangent
				{
					AdjustArcSegment(iSegment, EComputeArcMode::AdjEndTangent);
				}
			}
			else
			{
				break;
			}
		}

		// Update the first part of the spline
		NumSegments = IsClosedLoop() ? GetNumberOfSplineSegments() - 1 : EditingPointIndex;
		for (int i = 0; i < NumSegments; ++i)
		{
			int iSegment = (EditingPointIndex - 1 - i);
			if (iSegment < 0) iSegment += GetNumberOfSplineSegments();
			
			if (GetRoadSplinePointType(iSegment) == ERoadSplinePointType::Arc)
			{
				const int iNextSegment = GetNextPoint(iSegment);
				const int iPreSegment = GetPrevPoint(iSegment);

				const bool bPinStartTangent = iPreSegment >= 0 && GetRoadSplinePointType(iPreSegment) == ERoadSplinePointType::Linear;
				const bool bPinEndTangent = iNextSegment >= 0 && GetRoadSplinePointType(iNextSegment) == ERoadSplinePointType::Linear;

				if (bPinStartTangent && bPinEndTangent)
				{
					AdjustArcSegment(iSegment, EComputeArcMode::AdjStartPos);
					break;
				}
				else if (bPinStartTangent)
				{
					AdjustArcSegment(iSegment, EComputeArcMode::AdjStartPos);
					break;
				}
				else if (bPinEndTangent)
				{
					AdjustArcSegment(iSegment, EComputeArcMode::AdjStartTangent);
				}
				else // !bPinEndTangent && !bPinEndTangent
				{
					AdjustArcSegment(iSegment, EComputeArcMode::AdjStartTangent);
				}
			}
			else
			{
				break;
			}
		}
	}

	// Check segments
	/*
	for (int iSegment = 0; iSegment < GetNumberOfSplineSegments(); ++iSegment)
	{
		if (GetRoadSplinePointType(iSegment) == ERoadSplinePointType::Arc)
		{
			Segments[iSegment].bIsBreaked = !CheckArc(iSegment);
		}
		else if (GetRoadSplinePointType(iSegment) == ERoadSplinePointType::Linear)
		{
			Segments[iSegment].bIsBreaked = false; // TODO: check line segmenta
		}
		else
		{
			Segments[iSegment].bIsBreaked = false;
		}
	}	
	*/
}


bool URoadSplineComponent::AdjustArcSegment(int PointIndex, EComputeArcMode Mode)
{
	check(PointIndex >= 0 && PointIndex < GetNumberOfSplineSegments());

	bool Result = true;

	const int StartPointIndex = PointIndex;
	const int EndPointIndex = (PointIndex + 1) % GetNumberOfSplinePoints();

	auto& StartPoint = SplineCurves.Position.Points[StartPointIndex];
	auto& EndPoint = SplineCurves.Position.Points[EndPointIndex];

	FVector StartToEnd;
	FVector StartToEndDir;

	FVector StartDir;
	FVector EndDir;

	if (Mode == EComputeArcMode::AdjStartTangent || Mode == EComputeArcMode::AdjEndTangent)
	{
		StartToEnd = EndPoint.OutVal - StartPoint.OutVal;
		StartToEndDir = StartToEnd.GetSafeNormal();
		if (Mode == EComputeArcMode::AdjStartTangent)
		{
			EndDir = -GetDirectionAtSplinePoint(EndPointIndex, ESplineCoordinateSpace::Local);
			StartDir = FMath::GetReflectionVector(EndDir, StartToEndDir);
		}
		else // EComputeArcMode::AdjEndTangent
		{
			StartDir = GetDirectionAtSplinePoint(StartPointIndex, ESplineCoordinateSpace::Local);
			EndDir = FMath::GetReflectionVector(StartDir, -StartToEndDir);
		}
	}
	else // ComputeArcMode::AdjStartPos || EComputeArcMode::AdjEndPos
	{
		// TODO: Make sure that the length of the previes and next linear spline is at least 0.1 meters

		StartDir = GetDirectionAtSplinePoint(StartPointIndex, ESplineCoordinateSpace::Local);
		EndDir = -GetDirectionAtSplinePoint(EndPointIndex, ESplineCoordinateSpace::Local);
		FVector Intersection;
		if (FMath::SegmentIntersection2D(StartPoint.OutVal, StartPoint.OutVal + StartDir * 1.0e+7f, EndPoint.OutVal, EndPoint.OutVal + EndDir * 1.0e+7f, Intersection))
		{
			const double A = (Intersection - StartPoint.OutVal).Size();
			const double B = (Intersection - EndPoint.OutVal).Size();
			if (Mode == EComputeArcMode::AdjStartPos)
			{
				StartPoint.OutVal += StartDir * (A - B);
			}
			else // EComputeArcMode::AdjEndPos
			{
				EndPoint.OutVal += EndDir * (B - A);
			}
			StartToEnd = EndPoint.OutVal - StartPoint.OutVal;
			StartToEndDir = StartToEnd.GetSafeNormal();
		}
		else
		{
			// TODO: What need to do? - just don't adjust the position of the points
			Result = false;
		}
	}

	const FVector StartRightVector(FVector2D(StartDir.X, StartDir.Y).GetRotated(90), StartDir.Z);
	const double CosA = StartRightVector.CosineAngle2D(StartToEndDir);

	if (StartDir.CosineAngle2D(StartToEndDir) < 0) 
	{
		// Force to fit angel to 180deg
		/*
		double Sign = CosA >= 0 ? 1 : -1;
		CosA = (1 - UE_KINDA_SMALL_NUMBER) * Sign;
		FVector2D Dir = FVector2D(StartToEndDir).GetRotated(-90 * Sign);
		StartDir = FVector(Dir, StartDir.Z);
		EndDir = FVector(Dir, EndDir.Z);
		*/

		Result = false;
	}

	const double Radius = (StartToEnd.Size2D() / 2) / CosA;
	const double Ang = (180 - FMath::Acos(CosA) * 2 / PI * 180);
	const double TangentMult = CalcTangentMultiplier(Radius, -Ang);

	StartPoint.LeaveTangent = StartDir * TangentMult;
	StartPoint.ArriveTangent = StartDir * StartPoint.ArriveTangent.Size() * (TangentMult >= 0 ? 1: -1);

	EndPoint.LeaveTangent = -EndDir * EndPoint.LeaveTangent.Size() * (TangentMult >= 0 ? 1 : -1);
	EndPoint.ArriveTangent = -EndDir * TangentMult;

	//SplineCurves.Position.Points[PointIndex].InterpMode = CIM_CurveUser;

	return Result;
}

void URoadSplineComponent::AdjustLineSegment(int PointIndex)
{
	if (PointIndex < 0 || PointIndex >= GetNumberOfSplineSegments())
	{
		return;
	}

	const int StartPointIndex = PointIndex;
	const int EndPointIndex = (PointIndex + 1) % GetNumberOfSplinePoints();

	auto& StartPoint = SplineCurves.Position.Points[StartPointIndex];
	auto& EndPoint = SplineCurves.Position.Points[EndPointIndex];

	const FVector Tangent = (EndPoint.OutVal - StartPoint.OutVal).GetSafeNormal();
	const double TangentLen = (EndPoint.OutVal - StartPoint.OutVal).Size() * 0.5; //0.5 = 0.25 size of segment

	StartPoint.ArriveTangent = Tangent * StartPoint.ArriveTangent.Size();
	StartPoint.LeaveTangent = Tangent * TangentLen;

	EndPoint.ArriveTangent = Tangent * TangentLen;
	EndPoint.LeaveTangent = Tangent * EndPoint.LeaveTangent.Size();
}

bool URoadSplineComponent::CheckArc(int PointIndex) const
{
	if (PointIndex < 0 || PointIndex >= GetNumberOfSplineSegments())
	{
		return false;
	}

	const int StartPointIndex = PointIndex;
	const int EndPointIndex = (PointIndex + 1) % GetNumberOfSplinePoints();

	const auto& StartPoint = SplineCurves.Position.Points[StartPointIndex];
	const auto& EndPoint = SplineCurves.Position.Points[EndPointIndex];

	const FVector StartToEnd = EndPoint.OutVal - StartPoint.OutVal;
	const FVector StartToEndDir = StartToEnd.GetSafeNormal();
	const FVector StartDir = StartPoint.LeaveTangent;
	const FVector EndDir = -FMath::GetReflectionVector(StartDir, -StartToEndDir);

	if ((EndDir - EndPoint.ArriveTangent).Size() > UE_KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const FVector StartRightVector(FVector2D(StartDir.X, StartDir.Y).GetRotated(90), StartDir.Z);
	const double CosA = StartRightVector.CosineAngle2D(StartToEndDir);
	if (StartDir.CosineAngle2D(StartToEndDir) < 0)
	{
		return false;
	}

	const double Radius = (StartToEnd.Size2D() / 2) / CosA;
	const double Ang = (180 - FMath::Acos(CosA) * 2 / PI * 180);
	const double TangentMult = FMath::Abs(CalcTangentMultiplier(Radius, -Ang));

	if (FMath::Abs(StartPoint.LeaveTangent.Size() - TangentMult) / TangentMult > UE_KINDA_SMALL_NUMBER)
	{
		return false;
	}

	if (FMath::Abs(EndPoint.ArriveTangent.Size() - TangentMult) / TangentMult > UE_KINDA_SMALL_NUMBER)
	{
		return false;
	}

	return true;
}

void URoadSplineComponent::ApplyComponentInstanceData(struct FDriveSplineInstanceData* SplineInstanceData, const bool bPostUCS)
{
	check(SplineInstanceData);

	if (bPostUCS)
	{
		if (bInputSplinePointsToConstructionScript)
		{
			// Don't reapply the saved state after the UCS has run if we are inputting the points to it.
			// This allows the UCS to work on the edited points and make its own changes.
			return;
		}
		else
		{
			bModifiedByConstructionScript = (SplineInstanceData->SplineCurvesPreUCS != SplineCurves);

			// If we are restoring the saved state, unmark the SplineCurves property as 'modified'.
			// We don't want to consider that these changes have been made through the UCS.
			TArray<FProperty*> Properties;
			Properties.Emplace(FindFProperty<FProperty>(USplineComponent::StaticClass(), GET_MEMBER_NAME_CHECKED(USplineComponent, SplineCurves)));
			RemoveUCSModifiedProperties(Properties);
		}
	}
	else
	{
		SplineInstanceData->SplineCurvesPreUCS = SplineCurves;
	}

	if (SplineInstanceData->bSplineHasBeenEdited)
	{
		SplineCurves = SplineInstanceData->SplineCurves;
		PointTypes = SplineInstanceData->PointTypes;
		bModifiedByConstructionScript = false;
	}

	bSplineHasBeenEdited = SplineInstanceData->bSplineHasBeenEdited;

	UpdateSpline();
}

TStructOnScope<FActorComponentInstanceData> URoadSplineComponent::GetComponentInstanceData() const
{
	TStructOnScope<FActorComponentInstanceData> InstanceData = MakeStructOnScope<FActorComponentInstanceData, FDriveSplineInstanceData>(this);
	FDriveSplineInstanceData* SplineInstanceData = InstanceData.Cast<FDriveSplineInstanceData>();

	if (bSplineHasBeenEdited)
	{
		SplineInstanceData->SplineCurves = SplineCurves;
		SplineInstanceData->PointTypes = PointTypes;
	}
	SplineInstanceData->bSplineHasBeenEdited = bSplineHasBeenEdited;

	return InstanceData;
}

void URoadSplineComponent::UpdateRoadLayout()
{
	RoadLayout.UpdateLayout(this);
	UpdateLaneSectionBounds();
}

void URoadSplineComponent::UpdateLaneSectionBounds()
{
	RoadLayout.UpdateBounds(GetSplineLength());
}


double URoadSplineComponent::EvalROffset(double S) const
{
	return RoadLayout.EvalROffset(S);
}

URoadSplineComponent::FRang URoadSplineComponent::GetLaneRang(int SectionIndex, int LaneIndex) const
{
	auto& Section = GetLaneSection(SectionIndex);

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
	return { StartS, EndS };
}

void URoadSplineComponent::TrimLaneSections(double Tolerance)
{
	RoadLayout.TrimSections(GetSplineLength(), Tolerance, this);
	RoadLayout.UpdateLayout(this);
}

FPrimitiveSceneProxy* URoadSplineComponent::CreateSceneProxy()
{
#if WITH_EDITOR
	return new FRoadSplineSceneProxy(this);
#else
	return nullptr;
#endif
}

void URoadSplineComponent::GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials) const
{
	TSet<TObjectPtr<UMaterialInterface>> UsedMaterials;

	for (auto& It : GetDefault<UUnrealDriveSettings>()->DriveableLaneMatrtials)
	{
		OutMaterials.AddUnique(It.Value);
	}

	//OutMaterials.AddUnique(GetDefault<UUnrealDriveSettings>()->CurbLaneMatrtial);
	OutMaterials.AddUnique(GetDefault<UUnrealDriveSettings>()->SplineArrowMatrtial);
	OutMaterials.AddUnique(GetDefault<UUnrealDriveSettings>()->SidewalkMatrtial);

	OutMaterials.AddUnique(GetDefault<UUnrealDriveSettings>()->SelectedLaneMatrtial);
	OutMaterials.AddUnique(GetDefault<UUnrealDriveSettings>()->HiddenLaneMatrtial);
	OutMaterials.AddUnique(GetDefault<UUnrealDriveSettings>()->EmptyLaneMatrtial);
}

void URoadSplineComponent::CreateRenderState_Concurrent(FRegisterComponentContext* Context)
{
	Super::CreateRenderState_Concurrent(Context);

#if WITH_EDITOR
	if (auto* Subsystem = GetWorld()->GetSubsystem<UUnrealDriveSubsystem>())
	{
		Subsystem->UpdateObservedConnections(this);
	}
#endif
}

void URoadSplineComponent::BuildOffsetCurves(double RightOffset, FSplineCurves& OutCurves) const
{
	OutCurves = SplineCurves;

	for (int i = 0; i < GetNumberOfSplinePoints(); ++i)
	{
		const FVector RightVector = GetRightVectorAtSplinePoint(i, ESplineCoordinateSpace::Local);
		OutCurves.Position.Points[i].OutVal = SplineCurves.Position.Points[i].OutVal + RightVector * RightOffset;
	}

	/*
	for (int SegmentIndex = 0; SegmentIndex < GetNumberOfSplineSegments(); ++SegmentIndex)
	{
		const int StartPointIndex = SegmentIndex;
		const int EndPointIndex = (SegmentIndex + 1) % GetNumberOfSplinePoints();

		auto& StartPoint1 = SplineCurves.Position.Points[StartPointIndex];
		auto& EndPoint1 = SplineCurves.Position.Points[EndPointIndex];

		auto& StartPoint2 = OutCurves.Position.Points[StartPointIndex];
		auto& EndPoint2 = OutCurves.Position.Points[EndPointIndex];

		const double Scale = (EndPoint2.OutVal - StartPoint2.OutVal).Size() / (EndPoint1.OutVal - StartPoint1.OutVal).Size();

		StartPoint2.LeaveTangent = StartPoint2.LeaveTangent * Scale;
		EndPoint2.ArriveTangent = EndPoint2.ArriveTangent * Scale;
	}
	*/
	

	OutCurves.UpdateSpline(IsClosedLoop(), bStationaryEndpoints, ReparamStepsPerSegment, false/*bLoopPositionOverride*/, 0.0f/*LoopPosition*/, GetComponentTransform().GetScale3D());

}

void URoadSplineComponent::BuildLinearApproximation(TArray<FSplinePositionLinearApproximation>& OutPoints, const TFunction<double(double)>& RightOffsetFunc, double S0, double S1, int InReparamStepsPerSegment, int MinNumSteps, ESplineCoordinateSpace::Type CoordinateSpace) const
{
	OutPoints.Reset();
	const double SO_Param = GetInputKeyValueAtDistanceAlongSpline(S0);
	const double S1_Param = GetInputKeyValueAtDistanceAlongSpline(S1);

	int NumStep = int((S1_Param - SO_Param) * InReparamStepsPerSegment + 0.5);
	if (NumStep < MinNumSteps) NumStep = MinNumSteps;
	const double Step = (S1_Param - SO_Param) / NumStep;

	for (int i = 0; i <= NumStep; ++i)
	{
		const float Param = SO_Param + i * Step;
		const FVector RightVector = GetRightVectorAtSplineInputKey(Param, ESplineCoordinateSpace::Local);
		const double RightOffset = RightOffsetFunc(GetDistanceAlongSplineAtSplineInputKey(Param));
		FVector Point = SplineCurves.Position.Eval(Param, FVector::ZeroVector) + RightVector * RightOffset;

		if (CoordinateSpace == ESplineCoordinateSpace::World)
		{
			Point = GetComponentTransform().TransformPosition(Point);
		}

		OutPoints.Emplace(Point, Param); 
	}
}

/*
bool URoadSplineComponent::ConvertSplineSegmentToPolyLine(const double StartDist, const double EndDist, ESplineCoordinateSpace::Type CoordinateSpace, const float MaxSquareDistanceFromSpline, TArray<FVector>& OutPoints) const
{
	OutPoints.Empty();


	const int32 NumLines = 2; // Dichotomic subdivision of the spline segment
	double Dist = EndDist - StartDist;
	double SubstepSize = Dist / NumLines;
	if (SubstepSize == 0.0)
	{
		// There is no distance to cover, so handle the segment with a single point
		OutPoints.Add(GetLocationAtDistanceAlongSpline(EndDist, CoordinateSpace));
		return true;
	}

	double SubstepStartDist = StartDist;
	for (int32 i = 0; i < NumLines; ++i)
	{
		double SubstepEndDist = SubstepStartDist + SubstepSize;
		TArray<FVector> NewPoints;
		// Recursively sub-divide each segment until the requested precision is reached :
		if (DivideSplineIntoPolylineRecursiveHelper(SubstepStartDist, SubstepEndDist, CoordinateSpace, MaxSquareDistanceFromSpline, NewPoints))
		{
			if (OutPoints.Num() > 0)
			{
				check(OutPoints.Last() == NewPoints[0]); // our last point must be the same as the new segment's first
				OutPoints.RemoveAt(OutPoints.Num() - 1);
			}
			OutPoints.Append(NewPoints);
		}

		SubstepStartDist = SubstepEndDist;
	}

	return (OutPoints.Num() > 0);
}
*/
FVector URoadSplineComponent::EvalLanePoistion(int SectionIndex, int LaneIndex, double S, double Alpha, ESplineCoordinateSpace::Type CoordinateSpace) const
{
	const double SParam = GetInputKeyValueAtDistanceAlongSpline(S);
	const FVector Loc = GetLocationAtSplineInputKey(SParam, CoordinateSpace);

	double ROffset = EvalROffset(S);
	if (LaneIndex != LANE_INDEX_NONE)
	{
		ROffset += RoadLayout.Sections[SectionIndex].EvalLaneROffset(LaneIndex, S, Alpha);
	}

	const FVector RVector = GetRightVectorAtSplineInputKey(SParam, CoordinateSpace);
	return Loc + RVector * ROffset;
}

void URoadSplineComponent::PushSelectionToProxy()
{
	Super::PushSelectionToProxy();

	if (!IsComponentIndividuallySelected())
	{
		SetSelectedLane(INDEX_NONE, LANE_INDEX_NONE);
	}

	//UE_LOG(LogUnrealDrive, Warning, TEXT("URoadSplineComponent::PushSelectionToProxy() **** %s %i"), *GetName(), IsComponentIndividuallySelected());

}

bool URoadSplineComponent::ShouldRenderSelected() const
{
	/*
	if (OwnerIsJunction())
	{
		return false;
	}
	*/

	return Super::ShouldRenderSelected();
}

bool URoadSplineComponent::MoveComponentImpl(const FVector& Delta, const FQuat& NewRotation, bool bSweep, FHitResult* OutHit, EMoveComponentFlags MoveFlags, ETeleportType Teleport)
{
	if (Super::MoveComponentImpl(Delta, NewRotation, bSweep, OutHit, MoveFlags, Teleport))
	{
		UpdateMagicTransform();
		return true;
	}
	return false;
}

void URoadSplineComponent::OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport)
{
	Super::OnUpdateTransform(UpdateTransformFlags, Teleport);

	if (int32(UpdateTransformFlags & EUpdateTransformFlags::PropagateFromParent))
	{
		UpdateMagicTransform(ERoadSplineMagicTransformFilter::OuterOnly);
	}
	
}

int URoadSplineComponent::FindRoadSectionOnSplineKey(float SplineKey) const
{
	int SectionIndex = INDEX_NONE;

	for (int i = 0; i < GetLaneSections().Num(); ++i)
	{
		auto& Section = GetLaneSection(i);
		const float StartKey = GetInputKeyValueAtDistanceAlongSpline(Section.SOffset);
		const float EndKey = GetInputKeyValueAtDistanceAlongSpline(Section.SOffsetEnd_Cashed);
		if (SplineKey >= StartKey && SplineKey <= EndKey)
		{
			SectionIndex = i;
			break;
		}
	}

	return SectionIndex;
}

int URoadSplineComponent::SplitSection(float SplineKey, ERoadLaneSectionSide Side)
{
	const int SectionIndex = FindRoadSectionOnSplineKey(SplineKey);

	if(SectionIndex == INDEX_NONE)
	{
		UE_LOG(LogUnrealDrive, Error, TEXT("URoadSplineComponent::SplitSection() can't find section in key %f"), SplineKey);
		return INDEX_NONE;
	}

#if WITH_EDITOR
	const FScopedTransaction Transaction(LOCTEXT("SplitSection", "Split Section"), !GIsTransacting);
	Modify();
#endif

	const double SOffset = GetDistanceAlongSplineAtSplineInputKey(SplineKey);

	FRoadLaneSection SectionToAdd{};
	SectionToAdd.SOffset = SOffset;
	SectionToAdd.Side = Side;
	SectionToAdd.Attributes = RoadLayout.Sections[SectionIndex].Attributes;

	int LeftSectionIndex = INDEX_NONE;
	if (Side == ERoadLaneSectionSide::Both || Side == ERoadLaneSectionSide::Left)
	{
		LeftSectionIndex = RoadLayout.FindSideSection(SectionIndex, ERoadLaneSectionSide::Left);
		if (LeftSectionIndex != INDEX_NONE)
		{
			SectionToAdd.Left = RoadLayout.Sections[LeftSectionIndex].Left;
		}
	}

	int RightSectionIndex = INDEX_NONE;
	if (Side == ERoadLaneSectionSide::Both || Side == ERoadLaneSectionSide::Right)
	{
		RightSectionIndex = RoadLayout.FindSideSection(SectionIndex, ERoadLaneSectionSide::Right);
		if (RightSectionIndex != INDEX_NONE)
		{
			SectionToAdd.Right = RoadLayout.Sections[RightSectionIndex].Right;
		}
	}
	
	RoadLayout.Sections.Insert(MoveTemp(SectionToAdd), SectionIndex + 1);
	UpdateLaneSectionBounds();
	UpdateRoadLayout();

	static auto ShiftWidth = [](FRichCurve& Width, double SDelta)
	{
		for(auto & Key: Width.Keys)
		{
			Key.Time += SDelta;
		}
	};

	static auto ShiftAttribute = [](FRoadLaneAttribute & Attribute, double SDelta)
	{
		for(auto & Key: Attribute.Keys)
		{
			Key.SOffset += SDelta;
		}
	};

	static auto ShifLane = [](FRoadLane& OldLane, FRoadLane& NewLane, double SDelta)
	{
		ShiftWidth(NewLane.Width, SDelta);
		for (auto& [Name, Attribute] : NewLane.Attributes)
		{
			ShiftAttribute(Attribute, SDelta);
		}

		if (OldLane.IsForwardLane())
		{
			NewLane.SuccessorConnection = OldLane.SuccessorConnection;
			OldLane.SuccessorConnection = nullptr;
			NewLane.PredecessorConnection = nullptr;
		}
		else
		{
			NewLane.PredecessorConnection = OldLane.PredecessorConnection;
			OldLane.PredecessorConnection = nullptr;
			NewLane.SuccessorConnection = nullptr;
		}

	};

	auto& NewSection = RoadLayout.Sections[SectionIndex + 1];

	for (int LaneIndex = 0; LaneIndex < NewSection.Left.Num(); ++LaneIndex)
	{
		auto& OldSection = RoadLayout.Sections[LeftSectionIndex];
		auto& OldLane = OldSection.Left[LaneIndex];
		auto& NewLane = NewSection.Left[LaneIndex];
		ShifLane(OldLane, NewLane, OldSection.SOffset - SOffset);
	}
	
	for (int LaneIndex = 0; LaneIndex < NewSection.Right.Num(); ++LaneIndex)
	{
		auto& OldSection = RoadLayout.Sections[RightSectionIndex];
		auto& OldLane = OldSection.Right[LaneIndex];
		auto& NewLane = NewSection.Right[LaneIndex];
		ShifLane(OldLane, NewLane, OldSection.SOffset - SOffset);
	}

	const int CenterSectionIndex = FMath::Max(LeftSectionIndex, RightSectionIndex);
	if (CenterSectionIndex != INDEX_NONE)
	{
		auto& OldSection = RoadLayout.Sections[CenterSectionIndex];
		for (auto& [Name, Attribute] : NewSection.Attributes)
		{
			ShiftAttribute(Attribute, OldSection.SOffset - SOffset);
		}
	}

	if (LeftSectionIndex == RightSectionIndex)
	{
		if (LeftSectionIndex != INDEX_NONE)
		{
			RoadLayout.Sections[LeftSectionIndex].Trim(true);
		}
	}
	else
	{
		if (LeftSectionIndex != INDEX_NONE)
		{
			RoadLayout.Sections[LeftSectionIndex].Trim(true);
		}
		if (RightSectionIndex != INDEX_NONE)
		{
			RoadLayout.Sections[RightSectionIndex].Trim(true);
		}
	}

	NewSection.Trim(true);

	UpdateRoadLayout();
	MarkRenderStateDirty();

	return SectionIndex + 1;
}

void URoadSplineComponent::DisconnectAll()
{
#if WITH_EDITOR
	const FScopedTransaction Transaction(LOCTEXT("DisconnectAll", "Disconnect All"), !GIsTransacting);
#endif

	if (IsValid(PredecessorConnection)) PredecessorConnection->Disconnect();
	if (IsValid(SuccessorConnection)) SuccessorConnection->Disconnect();

	for (auto& Section : RoadLayout.Sections)
	{
		for (auto& Lane : Section.Left)
		{
			if (IsValid(Lane.PredecessorConnection)) Lane.PredecessorConnection->DisconnectAll();
			if (IsValid(Lane.SuccessorConnection)) Lane.SuccessorConnection->DisconnectAll();
		}
		for (auto& Lane : Section.Right)
		{
			if (IsValid(Lane.PredecessorConnection)) Lane.PredecessorConnection->DisconnectAll();
			if (IsValid(Lane.SuccessorConnection)) Lane.SuccessorConnection->DisconnectAll();
		}
	}
}

/*
bool URoadSplineComponent::Modify(bool bAlwaysMarkDirty)
{
	bool bWasModified = Super::Modify(bAlwaysMarkDirty);

	bWasModified |= PredecessorConnection->Modify();
	bWasModified |= SuccessorConnection->Modify();

	return bWasModified;
}\*/

#if WITH_EDITOR
void URoadSplineComponent::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	Super::PostEditChangeChainProperty(PropertyChangedEvent);

	FProperty* MemberProperty = PropertyChangedEvent.PropertyChain.GetHead()->GetValue();

	if (MemberProperty)
	{
		static const FName RoadLayoutName = GET_MEMBER_NAME_CHECKED(URoadSplineComponent, RoadLayout);
		static const FName ClosedLoopName = "bClosedLoop"; //GET_MEMBER_NAME_CHECKED(USplineComponent, bClosedLoop);

		const FName PropertyName(MemberProperty->GetFName());

		if (PropertyName == RoadLayoutName || PropertyName == ClosedLoopName)
		{
			UpdateRoadLayout();
			UpdateMagicTransform();
			UpdateBounds();
		}
	}

	GetRoadLayout().UpdateLayoutVersion();
}
#endif

void URoadSplineComponent::UpdateMagicTransform(ERoadSplineMagicTransformFilter Filter)
{
	URoadConnection::GlobalTransformMagic = FMath::Rand();

	TFunction<bool(const URoadSplineComponent*)> FilterFunc;

	if (Filter == ERoadSplineMagicTransformFilter::InnrerOnly)
	{
		FilterFunc = [this](const URoadSplineComponent* TargetSpline) 
		{ 
			return TargetSpline && TargetSpline->GetOwner() == GetOwner();
		};
	}
	else if (Filter == ERoadSplineMagicTransformFilter::OuterOnly)
	{
		FilterFunc = [this](const URoadSplineComponent* TargetSpline)
		{
			return TargetSpline && TargetSpline->GetOwner() != GetOwner();
		};
	}
	else
	{
		FilterFunc = [](const URoadSplineComponent*) { return true; };
	}

	MagicUpdateTransformInner(FilterFunc);

	URoadConnection::GlobalTransformMagic = 0;
}

void URoadSplineComponent::MagicUpdateTransformInner(TFunction<bool(const URoadSplineComponent*)> Filter)
{
	Modify();

	TArray<URoadSplineComponent*> SplinesToUpdate;

	auto TransformRoadConnection = [&SplinesToUpdate, &Filter](URoadConnection * RoadConnection)
	{
		if (RoadConnection && RoadConnection->IsConnectionValid())
		{
			if (RoadConnection->IsConnected() && Filter(RoadConnection->GetOuterConnection()->GetOwnedRoadSpline()))
			{
				if (RoadConnection->CanTransform())
				{
					RoadConnection->OuterLaneConnection->GetOwnedRoadSpline()->Modify();
					bool bIsTransformed = RoadConnection->SetTransformToOuter();
					RoadConnection->TransformMagic = URoadConnection::GlobalTransformMagic;
					if (bIsTransformed)
					{
						SplinesToUpdate.Add(RoadConnection->OuterLaneConnection->GetOwnedRoadSplineChecked());
					}
				}
				else
				{
					RoadConnection->SetTransformFormOuter();
				}
			}
			RoadConnection->TransformMagic = URoadConnection::GlobalTransformMagic;
		}
	};

	auto TransformLaneConnection = [&SplinesToUpdate, &Filter](ULaneConnection * LaneConnection)
	{
		if (LaneConnection && LaneConnection->IsConnectionValid())
		{
			const FTransform Transform = LaneConnection->EvalTransform(0.0, ESplineCoordinateSpace::World);
			for (auto& RoadConnection : LaneConnection->OuterRoadConnections)
			{
				if (RoadConnection.IsValid() && Filter(RoadConnection->GetOwnedRoadSpline()))
				{
					RoadConnection->GetOwnedRoadSpline()->Modify();
					if (RoadConnection->SetTransform(Transform, true, ESplineCoordinateSpace::World))
					{
						SplinesToUpdate.Add(RoadConnection->GetOwnedRoadSplineChecked());
					}
				}
			}
		}
	};

	TransformRoadConnection(GetPredecessorConnection());
	TransformRoadConnection(GetSuccessorConnection());
	
	for (auto& Section : RoadLayout.Sections)
	{
		for (auto& Lane : Section.Left)
		{
			TransformLaneConnection(Lane.PredecessorConnection);
			TransformLaneConnection(Lane.SuccessorConnection);
		}
		for (auto& Lane : Section.Right)
		{
			TransformLaneConnection(Lane.PredecessorConnection);
			TransformLaneConnection(Lane.SuccessorConnection);
		}
	}

	for (auto& It : SplinesToUpdate)
	{
		It->MagicUpdateTransformInner(Filter);
	}
}


TArray<ULaneConnection*> URoadSplineComponent::FindAllSuccessors(int SectionIndex, int LaneIndex, bool bIncludesThisRoad) const
{
	if (LaneIndex == LANE_INDEX_NONE || SectionIndex < 0 || SectionIndex >= RoadLayout.Sections.Num() - 1)
	{
		return {};
	}

	const auto& Section = RoadLayout.Sections[SectionIndex];
	if (!Section.CheckLaneIndex(LaneIndex))
	{
		return {};
	}

	TArray<ULaneConnection*> Ret;

	auto CheckLane = [&Ret](const FRoadLane & Lane, int DesierIndex)
	{
		if(IsValid(Lane.SuccessorConnection))
		{
			for (const auto& Outer : Lane.SuccessorConnection->OuterRoadConnections)
			{
				if (Outer.IsValid())
				{
					check(Outer->IsPredecessorConnection());
					const FRoadLaneSection& OuterSections = Outer->GetOwnedRoadSplineChecked()->RoadLayout.Sections[0];
					if (OuterSections.CheckLaneIndex(DesierIndex))
					{
						auto PredecessorConnection = OuterSections.GetLaneByIndex(DesierIndex).PredecessorConnection;
						if (IsValid(PredecessorConnection))
						{
							Ret.Add(PredecessorConnection);
						}
					}
				}
			}
		}
	};

	for (int i = 0; i < Section.Left.Num(); ++i)
	{
		int DesierIndex = LaneIndex - (-i - 1);
		if (DesierIndex >= 0) ++LaneIndex;
		CheckLane(Section.Left[i], DesierIndex);
	}

	for (int i = 0; i < Section.Right.Num(); ++i)
	{
		int DesierIndex = LaneIndex - (i + 1);
		if (DesierIndex <= 0) --LaneIndex;
		CheckLane(Section.Right[i], DesierIndex);
	}

	if (bIncludesThisRoad && RoadLayout.Sections.Num() > 1 && SectionIndex < RoadLayout.Sections.Num() - 2)
	{
		const auto& NextSection = RoadLayout.Sections[SectionIndex + 1];
		if (NextSection.CheckLaneIndex(LaneIndex))
		{
			const auto& NextLane = NextSection.GetLaneByIndex(LaneIndex);
			if (IsValid(NextLane.PredecessorConnection))
			{
				Ret.Add(NextLane.PredecessorConnection);
			}
		}
	}

	if (SectionIndex == RoadLayout.Sections.Num() - 1 && SuccessorConnection->IsConnected())
	{
		Ret.Add(SuccessorConnection->OuterLaneConnection.Get());
	}

	return Ret;
}


TArray<ULaneConnection*> URoadSplineComponent::FindAllPredecessors(int SectionIndex, int LaneIndex, bool bIncludesThisRoad) const
{
	if (LaneIndex == LANE_INDEX_NONE || SectionIndex < 0 || SectionIndex >= RoadLayout.Sections.Num() - 1)
	{
		return {};
	}

	const auto& Section = RoadLayout.Sections[SectionIndex];
	if (!Section.CheckLaneIndex(LaneIndex))
	{
		return {};
	}

	TArray<ULaneConnection*> Ret;

	auto CheckLane = [&Ret](const FRoadLane& Lane, int DesierIndex)
	{
		if(IsValid(Lane.PredecessorConnection))
		{
			for (const auto& Outer : Lane.PredecessorConnection->OuterRoadConnections)
			{
				if (Outer.IsValid())
				{
					check(Outer->IsSuccessorConnection());
					const FRoadLaneSection& OuterSections = Outer->GetOwnedRoadSplineChecked()->RoadLayout.Sections[0];
					if (OuterSections.CheckLaneIndex(DesierIndex))
					{
						auto& SuccessorConnection = OuterSections.GetLaneByIndex(DesierIndex).SuccessorConnection;
						if (IsValid(SuccessorConnection))
						{
							Ret.Add(SuccessorConnection);
						}
					}
				}
			}
		}
	};

	for (int i = 0; i < Section.Left.Num(); ++i)
	{
		int DesierIndex = LaneIndex - (-i - 1);
		if (DesierIndex >= 0) ++LaneIndex;
		CheckLane(Section.Left[i], DesierIndex);
	}

	for (int i = 0; i < Section.Right.Num(); ++i)
	{
		int DesierIndex = LaneIndex - (i + 1);
		if (DesierIndex <= 0) --LaneIndex;
		CheckLane(Section.Right[i], DesierIndex);
	}

	if (bIncludesThisRoad && RoadLayout.Sections.Num() > 1 && SectionIndex > 0)
	{
		const auto& PrevSection = RoadLayout.Sections[SectionIndex - 1];
		if (PrevSection.CheckLaneIndex(LaneIndex))
		{
			const auto& PreLane = PrevSection.GetLaneByIndex(LaneIndex);
			if (IsValid(PreLane.SuccessorConnection))
			{
				Ret.Add(PreLane.SuccessorConnection);
			}
		}
	}

	if (SectionIndex == 0 && PredecessorConnection->IsConnected())
	{
		Ret.Add(PredecessorConnection->OuterLaneConnection.Get());
	}

	return Ret;
}

FQuat URoadSplineComponent::GetBackwardQuaternionAtSplineInputKey(float InKey, ESplineCoordinateSpace::Type CoordinateSpace) const
{
	FQuat Quat = SplineCurves.Rotation.Eval(InKey, FQuat::Identity);
	Quat.Normalize();

	const FVector Direction = -SplineCurves.Position.EvalDerivative(InKey, FVector::ZeroVector).GetSafeNormal();
	const FVector UpVector = Quat.RotateVector(DefaultUpVector);

	FQuat Rot = (FRotationMatrix::MakeFromXZ(Direction, UpVector)).ToQuat();

	if (CoordinateSpace == ESplineCoordinateSpace::World)
	{
		Rot = GetComponentTransform().GetRotation() * Rot;
	}

	return Rot;
}

FTransform URoadSplineComponent::GetBackwardTransformAtSplineInputKey(float InKey, ESplineCoordinateSpace::Type CoordinateSpace, bool bUseScale) const
{
	const FVector Location(GetLocationAtSplineInputKey(InKey, ESplineCoordinateSpace::Local));
	const FQuat Rotation(GetBackwardQuaternionAtSplineInputKey(InKey, ESplineCoordinateSpace::Local));
	const FVector Scale = bUseScale ? GetScaleAtSplineInputKey(InKey) : FVector(1.0f);

	FTransform Transform(Rotation, Location, Scale);

	if (CoordinateSpace == ESplineCoordinateSpace::World)
	{
		Transform = Transform * GetComponentTransform();
	}

	return Transform;
}


void URoadSplineComponent::PostLoad()
{
	Super::PostLoad();

	UpdateRoadLayout();
}

void URoadSplineComponent::PostInitProperties()
{
	Super::PostInitProperties();
}

void URoadSplineComponent::PostEditImport()
{
	Super::PostEditImport();

	UpdateRoadLayout();

	PredecessorConnection->OuterLaneConnection = nullptr;
	SuccessorConnection->OuterLaneConnection = nullptr;
}

void URoadSplineComponent::PreDuplicate(FObjectDuplicationParameters& DupParams)
{
	Super::PreDuplicate(DupParams);
}

void URoadSplineComponent::PostDuplicate(bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);

	UpdateRoadLayout();

	//PredecessorConnection->OuterLaneConnection = nullptr;
	//SuccessorConnection->OuterLaneConnection = nullptr;
}

void URoadSplineComponent::DestroyComponent(bool bPromoteChildren)
{
	Super::DestroyComponent(bPromoteChildren);

}

void URoadSplineComponent::OnRegister()
{
	Super::OnRegister();

	UpdateRoadLayout();

	GetPredecessorConnection()->InitConnection();
	GetSuccessorConnection()->InitConnection();
}

void URoadSplineComponent::OnUnregister()
{
	Super::OnUnregister();
}

void URoadSplineComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
	Super::OnComponentDestroyed(bDestroyingHierarchy);
	DisconnectAll();
}

void URoadSplineComponent::SetRotationAtSplinePoint_Fixed(int32 PointIndex, const FRotator& InRotation, ESplineCoordinateSpace::Type CoordinateSpace, bool bUpdateSpline /*= true*/)
{
	SetRotationAtSplinePoint_Fixed(this, PointIndex, InRotation, CoordinateSpace, bUpdateSpline);
}

void URoadSplineComponent::SetRotationAtSplinePoint_Fixed(USplineComponent* Spline, int32 PointIndex, const FRotator& InRotation, ESplineCoordinateSpace::Type CoordinateSpace, bool bUpdateSpline)
{
	if (Spline->SplineCurves.Rotation.Points.IsValidIndex(PointIndex))
	{
		const FQuat Quat = (CoordinateSpace == ESplineCoordinateSpace::World) ?
			Spline->GetComponentTransform().InverseTransformRotation(InRotation.Quaternion()) : InRotation.Quaternion();

		FVector UpVector = Quat.GetUpVector();
		Spline->SetUpVectorAtSplinePoint(PointIndex, UpVector, ESplineCoordinateSpace::Local, false); // Origin SetRotationAtSplinePoint() use "CoordinateSpace" insted of ESplineCoordinateSpace::Local here

		FVector Direction = Quat.GetForwardVector();
		Spline->SetTangentAtSplinePoint(PointIndex, Direction, ESplineCoordinateSpace::Local, false); // Origin SetRotationAtSplinePoint() use "CoordinateSpace" insted of ESplineCoordinateSpace::Local here

		if (bUpdateSpline)
		{
			Spline->UpdateSpline();
		}
	}
}


FBoxSphereBounds URoadSplineComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	float MaxWidth = 0;

	for (auto& Section : RoadLayout.Sections)
	{
		float MaxLeftWidth = 0;
		float MaxRightWidth = 0;

		for (auto & Lane: Section.Left)
		{
			float MinValue;
			float MaxValue;
			Lane.Width.GetValueRange(MinValue, MaxValue);
			MaxLeftWidth += FMath::Max(0.0f, MaxValue);
		}
		for (auto& Lane : Section.Right)
		{
			float MinValue;
			float MaxValue;
			Lane.Width.GetValueRange(MinValue, MaxValue);
			MaxRightWidth += FMath::Max(0.0f, MaxValue);
		}

		MaxWidth = FMath::Max3(MaxWidth, MaxLeftWidth, MaxRightWidth);
	}

	float MinOffst;
	float MaxOffset;
	RoadLayout.ROffset.GetValueRange(MinOffst, MaxOffset);

	MaxWidth += FMath::Max(FMath::Abs(MinOffst), FMath::Abs(MaxOffset));

	return Super::CalcBounds(LocalToWorld).ExpandBy(MaxWidth);
}

static bool IsEqual(const FRoadPosition& A, const FRoadPosition& B)
{
	return A.Location == B.Location && A.SOffset == B.SOffset;
}

FRoadPosition URoadSplineComponent::GetRoadPosition(double SOffset, double ROffset, ESplineCoordinateSpace::Type CoordinateSpace) const
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

FRoadPosition URoadSplineComponent::GetRoadPosition(int SectionIndex, int LaneIndex, double Alpha, double SOffset, ESplineCoordinateSpace::Type CoordinateSpace) const
{
	const double ROffset = RoadLayout.Sections[SectionIndex].EvalLaneROffset(LaneIndex, SOffset, Alpha) + EvalROffset(SOffset);
	return GetRoadPosition(SOffset, ROffset, CoordinateSpace);
}

const FRoadLane* URoadSplineComponent::GetRoadLane(int SectionIndex, int LaneIndex) const
{
	if (SectionIndex >= 0 && SectionIndex < GetLaneSectionsNum())
	{
		const auto& Sections = GetLaneSection(SectionIndex);
		if (LaneIndex > 0 && LaneIndex <= Sections.Right.Num())
		{
			return &Sections.Right[LaneIndex - 1];
		}
		else if (LaneIndex < 0 && -LaneIndex <= Sections.Left.Num())
		{
			return &Sections.Left[-LaneIndex - 1];
		}
	}
	return nullptr;
}

FRoadLane* URoadSplineComponent::GetRoadLane(int SectionIndex, int LaneIndex)
{
	return const_cast<FRoadLane*>(const_cast<const URoadSplineComponent*>(this)->GetRoadLane(SectionIndex, LaneIndex));
}

float URoadSplineComponent::ClosetsKeyToSegmant(float Key1, float Key2, const FVector& A1, const FVector& A2) const
{
	check(Key2 > Key1);

	FVector SubsegmentStart = GetLocationAtSplineInputKey(Key1, ESplineCoordinateSpace::World);
	double ClosestDistance = TNumericLimits<double>::Max();
	FVector OutBestLocation = SubsegmentStart;

	// Divide segment into subsegments and test each subsegment against ray representing click position and camera direction.
	// Closest encounter with the spline determines the spline position.
	const int32 NumSubdivisionsPerSegm = 16;

	int32 NumSubdivisions = NumSubdivisionsPerSegm * (Key2 - Key1) + .5f;
	if (NumSubdivisions <= 1) NumSubdivisions = 2;
	const float StepLen = (Key2 - Key1) / NumSubdivisions;

	for (int32 Step = 1; Step <= NumSubdivisions; Step++)
	{
		const float Key = Key1 + Step * StepLen;
		const FVector SubsegmentEnd = GetLocationAtSplineInputKey(Key, ESplineCoordinateSpace::World);

		FVector SplineClosest;
		FVector RayClosest;
		FMath::SegmentDistToSegmentSafe(SubsegmentStart, SubsegmentEnd, A1, A2, SplineClosest, RayClosest);

		const double Distance = FVector::DistSquared(SplineClosest, RayClosest);
		if (Distance < ClosestDistance)
		{
			ClosestDistance = Distance;
			OutBestLocation = SplineClosest;
		}
		SubsegmentStart = SubsegmentEnd;
	}

	return FindInputKeyClosestToWorldLocation(OutBestLocation);
}

float URoadSplineComponent::ClosetsKeyToSegmant2(float S1, float S2, const FVector& A1, const FVector& A2) const
{
	float Key1 = SplineCurves.ReparamTable.Eval(S1, 0.0f);
	float Key2 = SplineCurves.ReparamTable.Eval(S2, 0.0f);
	return ClosetsKeyToSegmant(Key1, Key2, A1, A2);
}