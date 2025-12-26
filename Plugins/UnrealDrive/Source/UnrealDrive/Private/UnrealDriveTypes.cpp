/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "UnrealDriveTypes.h"
#include "UnrealDrive.h"
#include "RoadSplineComponent.h"
#include "DefaultRoadLaneAttributes.h"
#include "Serialization/ObjectWriter.h"
#include "Serialization/ObjectReader.h"

static FQuat InvertRotation(const FQuat& Quat)
{
	const FVector Direction = Quat.GetForwardVector();
	const FVector UpVector = Quat.GetUpVector();
	return (FRotationMatrix::MakeFromXZ(-Direction, UpVector)).ToQuat();
}

void UnrealDrive::TrimCurveInRang(FRichCurve & Curve, double Time0, double Time1, bool bFitBorders)
{
	float Value0 = Curve.Eval(Time0);
	float Value1 = Curve.Eval(Time1);

	int32 StartKey = INDEX_NONE;
	int32 EndKey = INDEX_NONE;
	for (int32 KeyIndex = 0; KeyIndex < Curve.Keys.Num(); ++KeyIndex)
	{
		const float CurrentKeyTime = Curve.Keys[KeyIndex].Time;
		if (CurrentKeyTime <= Time0)
		{
			StartKey = KeyIndex;
		}
		if (CurrentKeyTime >= Time1)
		{
			EndKey = KeyIndex;
			break;
		}
	}

	if (EndKey != INDEX_NONE && (Curve.GetNumKeys() - 1 != EndKey))
	{
		Curve.Keys.SetNum(EndKey + 1);
	}

	if (StartKey != INDEX_NONE && StartKey != 0)
	{
		Curve.Keys.RemoveAt(0, StartKey);
	}

	if (Curve.GetNumKeys())
	{
		if (Curve.Keys[Curve.GetNumKeys() - 1].Time > Time1 ) // || bFitBorders
		{
			Curve.Keys[Curve.GetNumKeys() - 1].Time = Time1;
		}

		if (Curve.Keys[0].Time < Time0 ) // || bFitBorders
		{
			Curve.Keys[0].Time = Time0;
		}
	}
		
	if (bFitBorders)
	{
		if (Curve.GetNumKeys() == 0 || !FMath::IsNearlyEqual(Curve.Eval(Time0), Value0))
		{
			FKeyHandle KeyHandle = Curve.UpdateOrAddKey(Time0, Value0);
			Curve.GetKey(KeyHandle).InterpMode = ERichCurveInterpMode::RCIM_Cubic;
			Curve.GetKey(KeyHandle).TangentMode = ERichCurveTangentMode::RCTM_Auto;
		}

		if (!FMath::IsNearlyEqual(Curve.Eval(Time1), Value1))
		{
			FKeyHandle KeyHandle = Curve.UpdateOrAddKey(Time1, Value1);
			Curve.GetKey(KeyHandle).InterpMode = ERichCurveInterpMode::RCIM_Cubic;
			Curve.GetKey(KeyHandle).TangentMode = ERichCurveTangentMode::RCTM_Auto;
		}
	}

	Curve.AutoSetTangents();
};

static bool IsCurveZero(const FRichCurve & Curve, float ErrorTolerance = 2.0)
{
	if (Curve.GetNumKeys() == 0)
	{
		return true;
	}

	const float FirstKeyTime = Curve.Keys[0].Time;
	const float LastKeyTime = Curve.Keys[Curve.Keys.Num() - 1].Time;
	const float TestPoints = 10;
	const float SampleRate = (LastKeyTime - FirstKeyTime) / TestPoints;

	for (int i = 0 ; i <= 10; ++i)
	{
		const float Value = Curve.Eval(FirstKeyTime + i * SampleRate);
		if (!FMath::IsNearlyZero(Value, ErrorTolerance))
		{
			return false;
		}

	}

	return true;
}

//----------------------------------------------------------------------------------------

FRoadLaneSection::FRoadLaneSection()
{
}

FRoadLaneSection::~FRoadLaneSection()
{
}

FRoadLaneSection::FRoadLaneSection(FRoadLaneSection&& Other) noexcept
{
	Side = Other.Side;
	Left = MoveTemp(Other.Left);
	Right = MoveTemp(Other.Right);
	SOffset = Other.SOffset;
	SOffsetEnd_Cashed = Other.SOffsetEnd_Cashed;
	Label = MoveTemp(Other.Label);
	Attributes = MoveTemp(Other.Attributes);
}

FRoadLaneSection& FRoadLaneSection::operator = (FRoadLaneSection&& Other) noexcept
{
	Side = Other.Side;
	Left = MoveTemp(Other.Left);
	Right = MoveTemp(Other.Right);
	SOffset = Other.SOffset;
	SOffsetEnd_Cashed = Other.SOffsetEnd_Cashed;
	Label = MoveTemp(Other.Label);
	Attributes = MoveTemp(Other.Attributes);
	OwnedRoadLayout = nullptr;
	SectionIndex = INDEX_NONE;
	return *this;
}

FRoadLaneSection::FRoadLaneSection(const FRoadLaneSection& Other) noexcept 
{
	Side = Other.Side;
	Left = Other.Left;
	Right = Other.Right;
	SOffset = Other.SOffset;
	SOffsetEnd_Cashed = Other.SOffsetEnd_Cashed;
	Attributes = Other.Attributes;

	for (auto& Lane : Left)
	{
		Lane.PredecessorConnection = nullptr;
		Lane.SuccessorConnection = nullptr;
	}

	for (auto& Lane : Right)
	{
		Lane.PredecessorConnection = nullptr;
		Lane.SuccessorConnection = nullptr;
	}
}

FRoadLaneSection& FRoadLaneSection::operator= (const FRoadLaneSection& Other) noexcept
{
	Side = Other.Side;
	Left = Other.Left;
	Right = Other.Right;
	SOffset = Other.SOffset;
	SOffsetEnd_Cashed = Other.SOffsetEnd_Cashed;
	Attributes = Other.Attributes;
	OwnedRoadLayout = nullptr;
	SectionIndex = INDEX_NONE;

	for (auto& Lane : Left)
	{
		Lane.PredecessorConnection = nullptr;
		Lane.SuccessorConnection = nullptr;
	}

	for (auto& Lane : Right)
	{
		Lane.PredecessorConnection = nullptr;
		Lane.SuccessorConnection = nullptr;
	}

	return *this;
}

bool FRoadLaneSection::CheckLaneIndex(int LaneIndex) const
{
	if (LaneIndex == 0)
	{
		return false;
	}
	else if (LaneIndex > 0 && (Side == ERoadLaneSectionSide::Both || Side == ERoadLaneSectionSide::Right) )
	{
		return (LaneIndex - 1) < Right.Num();
	}
	else if (LaneIndex < 0 && (Side == ERoadLaneSectionSide::Both || Side == ERoadLaneSectionSide::Left))
	{
		return (-LaneIndex - 1) < Left.Num();
	}
	else
	{
		return false;
	}
}

const FRoadLane& FRoadLaneSection::GetLaneByIndex(int LaneIndex) const
{
	check(LaneIndex != LANE_INDEX_NONE);
	if (LaneIndex > 0)
	{
		return Right[LaneIndex - 1];
	}
	else
	{
		return Left[-LaneIndex - 1];
	}
}

FRoadLane& FRoadLaneSection::GetLaneByIndex(int LaneIndex)
{
	check(LaneIndex != LANE_INDEX_NONE);
	if (LaneIndex > 0)
	{
		return Right[LaneIndex - 1];
	}
	else
	{
		return Left[-LaneIndex - 1];
	}
}

bool FRoadLaneSection::IsSectionValid() const
{
	if (OwnedRoadLayout.IsValid())
	{
		auto& Sections = (*OwnedRoadLayout.Pin())->Sections;
		if (Sections.IsValidIndex(SectionIndex))
		{
			return &Sections[SectionIndex] == this;
		}
	}
	return false;
}

void FRoadLaneSection::Trim(bool bFitWidth)
{
	for (auto & Lane : Left)
	{
		Lane.Trim(bFitWidth);
	}
	for (auto& Lane : Right)
	{
		Lane.Trim(bFitWidth);
	}
	for (auto& Attr : Attributes)
	{
		Attr.Value.Trim(0.0, SOffsetEnd_Cashed - SOffset);
	}
}

double FRoadLaneSection::EvalLaneROffset(int LaneIndex, double InSOffset, double Alpha) const
{
	const TArray<FRoadLane>* LanesInfo = nullptr;
	if (LaneIndex == LANE_INDEX_NONE)
	{
		return 0;
	}
	else if (LaneIndex > 0)
	{
		LanesInfo = &Right;
	}
	else
	{
		LanesInfo = &Left;
	}

	check(LaneIndex <= LanesInfo->Num());

	double BorderOffset = 0.;
	double PreBorderOffset = 0.;
	for (int i = 0; i < FMath::Abs(LaneIndex); ++i)
	{
		PreBorderOffset = BorderOffset;
		BorderOffset += (*LanesInfo)[i].Width.Eval(InSOffset - SOffset);
	}

	return FMath::Lerp(PreBorderOffset, BorderOffset, Alpha) * (LaneIndex >= 0 ? 1 : -1);
}

//----------------------------------------------------------------------------------------
FRoadLayout::FRoadLayout()
{
	ThisShared = MakeShared<FRoadLayout*>(this);
}

FRoadLayout::FRoadLayout(FRoadLayout&& Other) noexcept
{
	ThisShared = MakeShared<FRoadLayout*>(this);
	Sections = MoveTemp(Other.Sections);
	ROffset = MoveTemp(Other.ROffset);
	Direction = Other.Direction;
	FilledInstance = MoveTemp(Other.FilledInstance);
	FilledInstanceTexAngle = Other.FilledInstanceTexAngle;
	FilledInstanceTexScale = Other.FilledInstanceTexScale;
	LayoutVersion = 0;
	AttributesVersion = 0;
	UpdateLayout(nullptr);
}

FRoadLayout& FRoadLayout::operator = (FRoadLayout&& Other) noexcept
{
	Sections = MoveTemp(Other.Sections);
	ROffset = MoveTemp(Other.ROffset);
	Direction = Other.Direction;
	FilledInstance = Other.FilledInstance;
	FilledInstanceTexAngle = Other.FilledInstanceTexAngle;
	FilledInstanceTexScale = Other.FilledInstanceTexScale;
	LayoutVersion = 0;
	AttributesVersion = 0;
	UpdateLayout(nullptr);
	return *this;
}

FRoadLayout::FRoadLayout(const FRoadLayout& Other) noexcept
{
	ThisShared = MakeShared<FRoadLayout*>(this);
	Sections = Other.Sections;
	ROffset = Other.ROffset;
	Direction = Other.Direction;
	FilledInstance = Other.FilledInstance;
	FilledInstanceTexAngle = Other.FilledInstanceTexAngle;
	FilledInstanceTexScale = Other.FilledInstanceTexScale;
	LayoutVersion = 0;
	AttributesVersion = 0;
	UpdateLayout(nullptr);
}

FRoadLayout& FRoadLayout::operator = (const FRoadLayout& Other) noexcept
{
	Sections = Other.Sections;
	ROffset = Other.ROffset;
	Direction = Other.Direction;
	FilledInstance = Other.FilledInstance;
	FilledInstanceTexAngle = Other.FilledInstanceTexAngle;
	FilledInstanceTexScale = Other.FilledInstanceTexScale;
	LayoutVersion = 0;
	AttributesVersion = 0;
	UpdateLayout(nullptr);
	return *this;
}

void FRoadLayout::UpdateLayout(URoadSplineComponent* OwnedRoadSpline)
{
	// Remove lanes with width == 0
	for (auto& Section: Sections)
	{
		for (auto It = Section.Left.CreateIterator(); It; ++It)
		{
			if (IsCurveZero(It->Width))
			{
				It.RemoveCurrent();
			}
		}
		for (auto It = Section.Right.CreateIterator(); It; ++It)
		{
			if (IsCurveZero(It->Width))
			{
				It.RemoveCurrent();
			}
		}
	}

	// Update temp properties (OwnedRoadSpline, OwnedRoadLayout, SectionIndex, LaneIndex...)
	auto UpdateConnection = [OwnedRoadSpline](TObjectPtr< ULaneConnection>& Connection, const FRoadLane& RoadLane)
	{
		if(OwnedRoadSpline)
		{
			if (!IsValid(Connection))
			{
				Connection = NewObject<ULaneConnection>(OwnedRoadSpline, NAME_None, RF_Transactional);
			}
			Connection->OwnedRoadSpline = OwnedRoadSpline;
			Connection->SectionIndex = RoadLane.StartSectionIndex;
			Connection->LaneIndex = RoadLane.LaneIndex;
		}
		else
		{
			Connection = nullptr;
		}
	};

	int FinishedLeftSectionIndex = -1;
	int FinishedRighSectionIndex = -1;

	for (int SectionIndex = 0; SectionIndex < Sections.Num(); ++SectionIndex)
	{
		auto& Section = Sections[SectionIndex];
		Section.OwnedRoadLayout = ThisShared;
		Section.SectionIndex = SectionIndex;

		if (Section.Side == ERoadLaneSectionSide::Left)
		{
			Section.Right.Empty();
		}
		else if (Section.Side == ERoadLaneSectionSide::Right)
		{
			Section.Left.Empty();
		}

		if (Section.Side == ERoadLaneSectionSide::Both || Section.Side == ERoadLaneSectionSide::Left)
		{
			for (int i = 0; i < Section.Left.Num(); ++i)
			{
				auto& Lane = Section.Left[i];
				Lane.LaneIndex = -i - 1;
				Lane.StartSectionIndex = SectionIndex;
				Lane.EndSectionIndex = SectionIndex;
				Lane.OwnedRoadLayout = ThisShared;
				UpdateConnection(Lane.PredecessorConnection, Lane);
				UpdateConnection(Lane.SuccessorConnection, Lane);
			}
			FinishedLeftSectionIndex = SectionIndex;
		}
		else if (FinishedLeftSectionIndex >= 0)
		{
			for (int i = 0; i < Sections[FinishedLeftSectionIndex].Left.Num(); ++i)
			{
				auto& Lane = Sections[FinishedLeftSectionIndex].Left[i];
				Lane.EndSectionIndex = SectionIndex;
			}
		}

		if (Section.Side == ERoadLaneSectionSide::Both || Section.Side == ERoadLaneSectionSide::Right)
		{
			for (int i = 0; i < Section.Right.Num(); ++i)
			{
				auto& Lane = Section.Right[i];
				Lane.LaneIndex = i + 1;
				Lane.StartSectionIndex = SectionIndex;
				Lane.EndSectionIndex = SectionIndex;
				Lane.OwnedRoadLayout = ThisShared;
				UpdateConnection(Lane.PredecessorConnection, Lane);
				UpdateConnection(Lane.SuccessorConnection, Lane);
			}
			FinishedRighSectionIndex = SectionIndex;
		}
		else if (FinishedRighSectionIndex >= 0)
		{
			for (int i = 0; i < Sections[FinishedRighSectionIndex].Right.Num(); ++i)
			{
				auto& Lane = Sections[FinishedRighSectionIndex].Right[i];
				Lane.EndSectionIndex = SectionIndex;
			}
		}
	}

	// Check closed loop spline
	if (OwnedRoadSpline && OwnedRoadSpline->IsClosedLoop())
	{
		if (IsValid(OwnedRoadSpline->GetPredecessorConnection()))
		{
			OwnedRoadSpline->GetPredecessorConnection()->Disconnect();
		}
		if (IsValid(OwnedRoadSpline->GetSuccessorConnection()))
		{
			OwnedRoadSpline->GetSuccessorConnection()->Disconnect();
		}
	}

	++LayoutVersion;
}

void FRoadLayout::UpdateBounds(double SplineLength)
{
	if (Sections.Num())
	{
		Sections[0].SOffset = 0;
		Sections.Last().SOffsetEnd_Cashed = SplineLength;
	}

	for (int i = 0; i < Sections.Num() - 1; ++i)
	{
		Sections[i].SOffsetEnd_Cashed = Sections[i + 1].SOffset;
	}

	int FinishedLeftSectionIndex = -1;
	int FinishedRighSectionIndex = -1;

	for (int SectionIndex = 0; SectionIndex < Sections.Num(); ++SectionIndex)
	{
		auto& Section = Sections[SectionIndex];

		if (Section.Side == ERoadLaneSectionSide::Both || Section.Side == ERoadLaneSectionSide::Left)
		{
			for (int i = 0; i < Section.Left.Num(); ++i)
			{
				auto& Lane = Section.Left[i];
				Lane.SOffsetEnd_Cashed = Section.SOffsetEnd_Cashed;
			}
			FinishedLeftSectionIndex = SectionIndex;
		}
		else if (FinishedLeftSectionIndex >= 0)
		{
			for (int i = 0; i < Sections[FinishedLeftSectionIndex].Left.Num(); ++i)
			{
				auto& Lane = Sections[FinishedLeftSectionIndex].Left[i];
				Lane.SOffsetEnd_Cashed = Section.SOffsetEnd_Cashed;
			}
		}

		if (Section.Side == ERoadLaneSectionSide::Both || Section.Side == ERoadLaneSectionSide::Right)
		{
			for (int i = 0; i < Section.Right.Num(); ++i)
			{
				auto& Lane = Section.Right[i];
				Lane.SOffsetEnd_Cashed = Section.SOffsetEnd_Cashed;
			}
			FinishedRighSectionIndex = SectionIndex;
		}
		else if (FinishedRighSectionIndex >= 0)
		{
			for (int i = 0; i < Sections[FinishedRighSectionIndex].Right.Num(); ++i)
			{
				auto& Lane = Sections[FinishedRighSectionIndex].Right[i];
				Lane.SOffsetEnd_Cashed = Section.SOffsetEnd_Cashed;
			}
		}
	}
}

void FRoadLayout::TrimSections(double SplineLength, double Tolerance, URoadSplineComponent* OwnedRoadSpline)
{
	UpdateBounds(SplineLength);

	UnrealDrive::TrimCurveInRang(ROffset, 0.0, SplineLength, false);

	bool bWasRemoved = false;
	do
	{
		bWasRemoved = false;
		for (auto It = Sections.CreateIterator(); It; ++It)
		{
			if ((It->SOffset >= SplineLength) || ((It->SOffsetEnd_Cashed - It->SOffset) <= Tolerance))
			{
				auto NextIt = (It + 1);

				if (NextIt && (NextIt->SOffsetEnd_Cashed - It->SOffset) <= Tolerance)
				{
					++It;
				}
				It.RemoveCurrent();
				bWasRemoved = true;
				UpdateBounds(SplineLength);
			}
		}
	} while (bWasRemoved);

	UpdateLayout(OwnedRoadSpline);

	for (auto& It: Sections)
	{
		It.Trim(false);
	}
}

double FRoadLayout::EvalROffset(double S) const
{
	if (ROffset.GetNumKeys() == 0)
	{
		return 0.0;
	}
	else
	{
		return ROffset.Eval(S);
	}
}

int FRoadLayout::FindSideSection(int SectionIndex, ERoadLaneSectionSide Side) const
{
	for (; SectionIndex >= 0; --SectionIndex)
	{
		if (Sections[SectionIndex].Side == Side || Sections[SectionIndex].Side == ERoadLaneSectionSide::Both)
		{
			break;
		}
	}
	return SectionIndex;
}

//----------------------------------------------------------------------------------------

FRoadLane::FRoadLane()
{
	Width.SetDefaultValue(UnrealDrive::DefaultRoadLaneWidth);
}

/*
URoadSplineComponent* FRoadLane::GetOwnedRoadSplineChecked() const
{ 
	check(OwnedRoadSpline.IsValid());
	return OwnedRoadSpline.Get();
}
*/

bool FRoadLane::IsLaneValid() const
{
	if (OwnedRoadLayout.IsValid())
	{
		auto& Sections = (*OwnedRoadLayout.Pin())->Sections;
		if (Sections.IsValidIndex(StartSectionIndex))
		{
			const auto& Section = Sections[StartSectionIndex];
			const FRoadLane* Lane = nullptr;

			if (LaneIndex > 0 && LaneIndex <= Section.Right.Num())
			{
				Lane = &Section.Right[LaneIndex - 1];
			}
			else if (LaneIndex < 0 && -LaneIndex <= Section.Left.Num())
			{
				Lane = &Section.Left[ - LaneIndex - 1];
			}

			if (Lane == this)
			{
				return true;
			}
		}
	}
	return false;
}

bool FRoadLane::IsForwardLane() const
{
	return (LaneIndex > 0) ^ (Direction == ERoadLaneDirection::Invert) ^ ((*OwnedRoadLayout.Pin())->Direction == ERoadDirection::RightHand);
}

const FRoadLaneSection& FRoadLane::GetStartSection() const
{
	return (*OwnedRoadLayout.Pin())->Sections[StartSectionIndex];
}

FRoadLaneSection& FRoadLane::GetStartSection()
{
	return (*OwnedRoadLayout.Pin())->Sections[StartSectionIndex];
}

const FRoadLaneSection& FRoadLane::GetEndSection() const
{
	return (*OwnedRoadLayout.Pin())->Sections[EndSectionIndex];
}	

FRoadLaneSection& FRoadLane::GetEndSection()
{
	return (*OwnedRoadLayout.Pin())->Sections[EndSectionIndex];
}	

double FRoadLane::GetStartOffset() const 
{ 
	return GetStartSection().SOffset; 
}

void FRoadLane::Trim(bool bFitWidth)
{
	double SOffsetEnd = GetEndOffset() - GetStartOffset();
	UnrealDrive::TrimCurveInRang(Width, 0.0, SOffsetEnd, bFitWidth);
	for (auto& Attr : Attributes)
	{
		Attr.Value.Trim(0.0, SOffsetEnd);
	}
}

//---------------------------------------------------------------------------------------
FRoadLaneSidewalk::FRoadLaneSidewalk()
{
	TSoftObjectPtr<UCurveFloat> DefaultCapProfile(FSoftObjectPath(TEXT("/UnrealDrive/Curves/SidewalkCap_Round.SidewalkCap_Round")));

	BeginCapCurve.ExternalCurve = DefaultCapProfile.Get();
	EndCapCurve.ExternalCurve = DefaultCapProfile.Get();
}

//----------------------------------------------------------------------------------------
int32 URoadConnection::GlobalTransformMagic = 0;

URoadConnection::URoadConnection(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		checkf(Cast<URoadSplineComponent>(GetOuter()), TEXT("Outer object must be only URoadSplineComponent for URoadConnection"));
	}
}

URoadSplineComponent* URoadConnection::GetOwnedRoadSpline() const
{
	return Cast<URoadSplineComponent>(GetOuter());
}

URoadSplineComponent* URoadConnection::GetOwnedRoadSplineChecked() const
{
	return CastChecked<URoadSplineComponent>(GetOuter());
}

bool URoadConnection::IsConnectionValid() const
{
	return IsValid(GetOwnedRoadSpline());
}

const FTransform URoadConnection::GetTransform(ESplineCoordinateSpace::Type Space) const
{
	URoadSplineComponent* OwnedRoadSpline = GetOwnedRoadSplineChecked();
	check(IsValid(OwnedRoadSpline));

	FTransform Transform;

	if (IsSuccessorConnection())
	{
		Transform = OwnedRoadSpline->GetTransformAtSplinePoint(OwnedRoadSpline->GetNumberOfSplinePoints() - 1, ESplineCoordinateSpace::Local);
	}
	else  // IsPredecessorConnection
	{
		Transform = OwnedRoadSpline->GetTransformAtSplinePoint(0, ESplineCoordinateSpace::Local);
	}
	
	if (Space == ESplineCoordinateSpace::World)
	{
		return Transform * OwnedRoadSpline->GetComponentTransform();
	}
	return Transform;
}


bool URoadConnection::CanTransform() const
{
	URoadConnection* RootRoadConnection = nullptr;
	ULaneConnection* LaneConnection = OuterLaneConnection.Get();


	while (LaneConnection && LaneConnection->IsConnectionValid())
	{
		URoadConnection* NewRoadConnection = nullptr;
		if (LaneConnection->IsRoadSuccessorConnection())
		{
			RootRoadConnection = LaneConnection->GetOwnedRoadSplineChecked()->GetSuccessorConnection();
		}
		else if (LaneConnection->IsRoadPredecessorConnection())
		{
			RootRoadConnection = LaneConnection->GetOwnedRoadSplineChecked()->GetPredecessorConnection();
		}
		else
		{
			return false;
		}

		if (RootRoadConnection && RootRoadConnection->IsConnected())
		{
			LaneConnection = RootRoadConnection->OuterLaneConnection.Get();
		}
		else
		{
			return true;
		}

		if (LaneConnection == OuterLaneConnection.Get())
		{
			checkf(false, TEXT("Loop connection detected"));
			return false;
		}
	}

	return true;
}

bool URoadConnection::SetTransform(const FTransform& Transform, bool bUpdateSpline, ESplineCoordinateSpace::Type Space)
{
	URoadSplineComponent* OwnedRoadSpline = GetOwnedRoadSplineChecked();

	if (GlobalTransformMagic != 0 && TransformMagic == GlobalTransformMagic)
	{
		return false;
	}

	TransformMagic = GlobalTransformMagic;

	if (GetTransform(Space).EqualsNoScale(Transform))
	{
		return false;
	}

	const int PointIndex = IsPredecessorConnection() ? 0 : OwnedRoadSpline->GetNumberOfSplinePoints() - 1;
	auto& Point = OwnedRoadSpline->SplineCurves.Position.Points[PointIndex];

	const float LeaveTangentSize = Point.LeaveTangent.Size();
	const float ArriveTangentSize = Point.ArriveTangent.Size();

	OwnedRoadSpline->SetLocationAtSplinePoint(PointIndex, Transform.GetTranslation(), Space, false);
	OwnedRoadSpline->SetRotationAtSplinePoint_Fixed(PointIndex, Transform.GetRotation().Rotator(), Space, false);

	Point.LeaveTangent *= LeaveTangentSize;
	Point.ArriveTangent *= ArriveTangentSize;

	if (OwnedRoadSpline->GetNumberOfSplinePoints() >= 2)
	{
		if (IsPredecessorConnection())
		{
			if (Point.InterpMode == EInterpCurveMode::CIM_Linear)
			{
				Point.InterpMode = EInterpCurveMode::CIM_CurveUser;
			}
		}
		else
		{
			auto& PrevPoint = OwnedRoadSpline->SplineCurves.Position.Points[PointIndex - 1];
			if (PrevPoint.InterpMode == EInterpCurveMode::CIM_Linear)
			{
				PrevPoint.InterpMode = EInterpCurveMode::CIM_CurveUser;
			}
		}
	}

	if (bUpdateSpline)
	{
		OwnedRoadSpline->UpdateSpline(PointIndex);
		OwnedRoadSpline->MarkRenderStateDirty();
	}

	/*
	if (GlobalTransformMagic != 0)
	{
		OwnedRoadSpline->UpdateMagicTransform(false);
	}
	*/

	//UE_LOG(LogUnrealDrive, Warning, TEXT("URoadConnection::SetTransform(%s); for %s "), *Transform.GetLocation().ToString(), *Label);

	return true;
}

bool URoadConnection::SetTransformFormOuter()
{
	check(IsConnectionValid());

	if (IsConnected())
	{
		return SetTransform(OuterLaneConnection->EvalTransform(0.0, ESplineCoordinateSpace::World), true, ESplineCoordinateSpace::World);
	}
	return false;
}

bool URoadConnection::SetTransformToOuter()
{
	check(IsConnectionValid());

	if (IsConnected())
	{
		const FTransform Transform = GetTransform(ESplineCoordinateSpace::World);
		if (OuterLaneConnection->SetTransform(Transform, true, ESplineCoordinateSpace::World))
		{
			for (auto& It : OuterLaneConnection->OuterRoadConnections)
			{
				if (It.IsValid() && It != this)
				{
					It->SetTransform(Transform, true, ESplineCoordinateSpace::World);
				}
			}
			return true;
		}
	}

	return false;
}

bool URoadConnection::IsSuccessorConnection() const
{
	URoadSplineComponent* OwnedRoadSpline = GetOwnedRoadSplineChecked();
	check(IsValid(OwnedRoadSpline));

	if (OwnedRoadSpline->GetSuccessorConnection() == this)
	{
		return true;
	}
	
	return false;
}

bool URoadConnection::IsPredecessorConnection() const
{
	URoadSplineComponent* OwnedRoadSpline = GetOwnedRoadSplineChecked();
	check(IsValid(OwnedRoadSpline));

	if (OwnedRoadSpline->GetPredecessorConnection() == this)
	{
		return true;
	}
	
	return false;
}

bool URoadConnection::ConnectTo(ULaneConnection* InOuterLaneConnection)
{
	if (!CanConnectTo(InOuterLaneConnection, true))
	{
		return false;
	}

	Modify();

	if (IsConnected())
	{
		Disconnect();
	}

	OuterLaneConnection = InOuterLaneConnection;

	InitConnection();

	return true;
}

void URoadConnection::InitConnection()
{
	if (OuterLaneConnection.IsValid())
	{
		OuterLaneConnection.LoadSynchronous();
		if (OuterLaneConnection->IsConnectionValid())
		{
			OuterLaneConnection->Modify();
			OuterLaneConnection->OuterRoadConnections.Add(this);
		}
		else
		{
			UE_LOG(LogUnrealDrive, Error, TEXT("URoadConnection::InitConnection(); OuterLaneConnection is broken"));
			Disconnect();
		}
	}
}

void URoadConnection::Disconnect()
{
	if (OuterLaneConnection.IsValid())
	{
		Modify();
		OuterLaneConnection->Modify();
		OuterLaneConnection->OuterRoadConnections.Remove(this);
		OuterLaneConnection = nullptr;
	}
}

bool URoadConnection::IsConnected() const 
{ 
	return OuterLaneConnection.IsValid() && OuterLaneConnection->IsConnectionValid(); 
}

bool URoadConnection::IsConnectionFree() const
{
	if (IsConnected())
	{
		return false;
	}

	auto& Layout = GetOwnedRoadSplineChecked()->GetRoadLayout();

	if (Layout.Sections.Num() == 0)
	{
		return false;
	}

	if (IsPredecessorConnection())
	{
		for (auto& Lane : Layout.Sections[0].Left)
		{
			if (Lane.IsForwardLane())
			{
				if (Lane.PredecessorConnection->IsConnected())
				{
					return false;
				}
			}
			else
			{
				if (Lane.SuccessorConnection->IsConnected())
				{
					return false;
				}
			}
		}

		for (auto& Lane : Layout.Sections[0].Right)
		{
			if (Lane.IsForwardLane())
			{
				if (Lane.PredecessorConnection->IsConnected())
				{
					return false;
				}
			}
			else
			{
				if (Lane.SuccessorConnection->IsConnected())
				{
					return false;
				}
			}
		}
	}
	else if (IsSuccessorConnection())
	{
		const int LeftSectionIndex = Layout.FindSideSection(Layout.Sections.Num() - 1, ERoadLaneSectionSide::Left);
		const int RightSectionIndex = Layout.FindSideSection(Layout.Sections.Num() - 1, ERoadLaneSectionSide::Right);

		for (auto& Lane : Layout.Sections[LeftSectionIndex].Left)
		{
			if (Lane.IsForwardLane())
			{
				if (Lane.SuccessorConnection->IsConnected())
				{
					return false;
				}
			}
			else
			{
				if (Lane.PredecessorConnection->IsConnected())
				{
					return false;
				}
			}
		}

		for (auto& Lane : Layout.Sections[RightSectionIndex].Right)
		{
			if (Lane.IsForwardLane())
			{
				if (Lane.SuccessorConnection->IsConnected())
				{
					return false;
				}
			}
			else
			{
				if (Lane.PredecessorConnection->IsConnected())
				{
					return false;
				}
			}
		}
	}
	return true;
}

bool URoadConnection::CanConnectTo(const ULaneConnection* InOuterLaneConnection, bool bLog) const
{
	URoadSplineComponent* OwnedRoadSpline = GetOwnedRoadSplineChecked();

	if (OwnedRoadSpline->IsClosedLoop())
	{
		return false;
	}

	if (!IsValid(InOuterLaneConnection) || !InOuterLaneConnection->IsConnectionValid())
	{
		return false;
	}

	if (InOuterLaneConnection->GetOwnedRoadSplineChecked() == OwnedRoadSpline)
	{
		if (bLog) UE_LOG(LogUnrealDrive, Warning, TEXT("URoadConnection::CanConnectTo(); Don't support loop connections"));
		return false;
	}

	
	if (!(InOuterLaneConnection->IsSuccessorConnection() ^ IsSuccessorConnection()))
	{
		if(bLog) UE_LOG(LogUnrealDrive, Warning, TEXT("URoadConnection::CanConnectTo(); Successors & Predecessors connection mismatch"));
		return false;
	}
	
	// Iteratively find road connection == this in heap of connections
	const ULaneConnection* LaneConnection = InOuterLaneConnection;
	while (LaneConnection)
	{
		if (!LaneConnection->IsConnectionValid())
		{
			return false;
		}

		const URoadConnection* RoadConnection = nullptr;

		if (LaneConnection->IsRoadSuccessorConnection())
		{
			RoadConnection = LaneConnection->GetOwnedRoadSplineChecked()->GetSuccessorConnection();
		}
		else if (LaneConnection->IsRoadPredecessorConnection())
		{
			RoadConnection = LaneConnection->GetOwnedRoadSplineChecked()->GetPredecessorConnection();
		}

		if (RoadConnection == this)
		{
			if (bLog) UE_LOG(LogUnrealDrive, Warning, TEXT("URoadConnection::CanConnectTo(); Detected heap connection"));
			return false;
		}

		if (RoadConnection && RoadConnection->IsConnected())
		{
			const ULaneConnection* OuterLaneConnection2 = RoadConnection->OuterLaneConnection.Get();
			if (OuterLaneConnection2->IsRoadPredecessorConnection())
			{
				RoadConnection = OuterLaneConnection2->GetOwnedRoadSplineChecked()->GetPredecessorConnection();
			}
			else if (OuterLaneConnection2->IsRoadSuccessorConnection())
			{
				RoadConnection = OuterLaneConnection2->GetOwnedRoadSplineChecked()->GetSuccessorConnection();
			}
		}

		if (RoadConnection == this)
		{
			if (bLog) UE_LOG(LogUnrealDrive, Warning, TEXT("URoadConnection::CanConnectTo(); Detected heap connection"));
			return false;
		}

		if (RoadConnection)
		{
			LaneConnection = RoadConnection->GetOuterConnection();
		}
		else
		{
			break;
		}

		if (LaneConnection == InOuterLaneConnection)
		{
			checkf(false, TEXT("Detected loop connection for %s connection"), *InOuterLaneConnection->GetName());
			return false;
		}
	}

	return true;
}

void URoadConnection::BeginDestroy()
{
	Super::BeginDestroy();
	Disconnect();
}

void URoadConnection::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if (Ar.IsLoading())
	{
		
	}
}

void URoadConnection::PostLoad()
{
	Super::PostLoad();

	//InitConnection();
}

void URoadConnection::ExportCustomProperties(FOutputDevice& Out, uint32 Indent)
{
	Super::ExportCustomProperties(Out, Indent);

	LaneConnectionGuid = 
		OuterLaneConnection.IsValid() && OuterLaneConnection->IsConnectionValid() 
		? OuterLaneConnection->Guid 
		: FGuid{};
}

void URoadConnection::ImportCustomProperties(const TCHAR* SourceText, FFeedbackContext* Warn)
{
	Super::ImportCustomProperties(SourceText, Warn);
}

/*
void URoadConnection::PostInitProperties()
{
	Super::PostInitProperties();
}
*/


//----------------------------------------------------------------------------------------
ULaneConnection::ULaneConnection(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

const FRoadLane& ULaneConnection::GetOwnedRoadLane() const
{
	return OwnedRoadSpline->GetLaneSection(SectionIndex).GetLaneByIndex(LaneIndex);
}

FRoadLane& ULaneConnection::GetOwnedRoadLane()
{
	return OwnedRoadSpline->GetLaneSection(SectionIndex).GetLaneByIndex(LaneIndex);
}

bool ULaneConnection::IsConnectionValid() const 
{ 
	if (!OwnedRoadSpline.IsValid())
	{
		return false;
	}

	FRoadLane* OwnedlLane = OwnedRoadSpline->GetRoadLane(SectionIndex, LaneIndex);
	if (!OwnedlLane)
	{
		return false;
	}

	if (!OwnedlLane->IsLaneValid())
	{
		return false;
	}

	if (OwnedlLane->PredecessorConnection != this && OwnedlLane->SuccessorConnection != this)
	{
		return false;
	}

	return true;
}

bool ULaneConnection::IsConnected() const
{
	return !OuterRoadConnections.IsEmpty();
}
/*
bool ULaneConnection::ConnectTo(URoadConnection* InRoadConnection)
{
	check(IsConnectionValid());
	check(IsValid(InRoadConnection));

	RoadConnections.Add(InRoadConnection);
	InRoadConnection->LaneConnection = this;
	return true;
}
*/

void ULaneConnection::DisconnectAll()
{
	Modify();

	for (auto& It : OuterRoadConnections)
	{
		if (It.IsValid())
		{
			if (It->OuterLaneConnection == this)
			{
				It->Modify();
				It->OuterLaneConnection = nullptr;
			}
		}
	}
	OuterRoadConnections.Reset();
}

const FTransform ULaneConnection::EvalTransform(double Alpha, ESplineCoordinateSpace::Type Space) const
{
	check(IsConnectionValid());

	auto& Lane = GetOwnedRoadLane();
	auto& Section = OwnedRoadSpline->GetLaneSection(SectionIndex);

	float SOffest;
	double SParam;
	FTransform Transform;

	if (Lane.IsForwardLane())
	{
		SOffest = IsSuccessorConnection() ? Lane.GetEndOffset() : Lane.GetStartOffset();
		SParam = OwnedRoadSpline->GetInputKeyValueAtDistanceAlongSpline(SOffest);
		Transform = OwnedRoadSpline->GetTransformAtSplineInputKey(SParam, Space);
	}
	else 
	{
		SOffest = IsSuccessorConnection() ? Lane.GetStartOffset() : Lane.GetEndOffset();
		SParam = OwnedRoadSpline->GetInputKeyValueAtDistanceAlongSpline(SOffest);
		Transform = OwnedRoadSpline->GetBackwardTransformAtSplineInputKey(SParam, Space);
	}

	const double ROffset = Section.EvalLaneROffset(LaneIndex, SOffest, Alpha) + OwnedRoadSpline->EvalROffset(SOffest);

	const FVector RVector = OwnedRoadSpline->GetRightVectorAtSplineInputKey(SParam, Space);
	Transform.SetLocation(Transform.GetLocation() + RVector * ROffset);
	return Transform;
}

void ULaneConnection::SetTransformToOuter()
{
	check(IsConnectionValid());

	const FTransform Transform = EvalTransform(0.0, ESplineCoordinateSpace::World);
	for (auto& Connection : OuterRoadConnections)
	{
		if (Connection.IsValid())
		{
			Connection->SetTransform(Transform, true, ESplineCoordinateSpace::World);
		}
	}
}

bool ULaneConnection::CanTransform() const
{
	return IsRoadSuccessorConnection() || IsRoadPredecessorConnection();
}

bool ULaneConnection::SetTransform(const FTransform& Transform, bool bUpdateSpline, ESplineCoordinateSpace::Type Space)
{
	check(IsConnectionValid());

	auto& OwnedLane = GetOwnedRoadLane();

	URoadConnection* RoadConnection = nullptr;;

	if (IsRoadSuccessorConnection())
	{
		RoadConnection = OwnedRoadSpline->GetSuccessorConnection();
	}
	else if (IsRoadPredecessorConnection())
	{
		RoadConnection = OwnedRoadSpline->GetPredecessorConnection();
	}

	if (IsValid(RoadConnection))
	{
		FTransform NewTransform = Transform;
		if (!OwnedLane.IsForwardLane())
		{
			NewTransform.SetRotation(InvertRotation(NewTransform.GetRotation()));
		}

		const double SOffset = IsSuccessorConnection() ? OwnedRoadSpline->GetSplineLength() : 0;
		const double ROffset = OwnedLane.GetStartSection().EvalLaneROffset(OwnedLane.GetLaneIndex(), SOffset, 0.0) + OwnedRoadSpline->EvalROffset(SOffset);
		const FVector RVector = NewTransform.GetRotation().GetRightVector();
		
		NewTransform.SetLocation(NewTransform.GetLocation() - RVector * ROffset);
		return RoadConnection->SetTransform(NewTransform, bUpdateSpline, Space);
	}

	return false;
}

bool ULaneConnection::IsSuccessorConnection() const
{
	return GetOwnedRoadLane().SuccessorConnection == this;
}

inline bool ULaneConnection::IsRoadSuccessorConnection() const
{
	auto & Lane = GetOwnedRoadLane();
	if (Lane.GetEndSectionIndex() == GetOwnedRoadSplineChecked()->GetLaneSectionsNum() - 1)
	{
		return IsSuccessorConnection() ^ (!Lane.IsForwardLane());
	}
	return false;
}
inline bool ULaneConnection::IsRoadPredecessorConnection() const
{
	auto & Lane = GetOwnedRoadLane();
	if (Lane.GetStartSectionIndex() == 0)
	{
		return !IsSuccessorConnection() ^ (!Lane.IsForwardLane());
	}
	return false;
}


void ULaneConnection::BeginDestroy()
{
	Super::BeginDestroy();

	if (IsConnectionValid())
	{
		DisconnectAll();
	}
}

void ULaneConnection::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
}

void ULaneConnection::PostEditImport()
{
	Super::PostEditImport();

	OuterRoadConnections.Reset();
}

void ULaneConnection::ExportCustomProperties(FOutputDevice& Out, uint32 Indent)
{
	Super::ExportCustomProperties(Out, Indent);
}

void ULaneConnection::ImportCustomProperties(const TCHAR* SourceText, FFeedbackContext* Warn)
{
	Super::ImportCustomProperties(SourceText, Warn);
}
