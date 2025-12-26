/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "Components/SplineComponent.h"
#include "UnrealDriveTypes.h"
#include "Templates/Tuple.h"
#include "RoadSplineComponent.generated.h"

struct FDriveSplineInstanceData;


enum class EComputeArcMode
{
	AdjStartTangent,
	AdjEndTangent,
	AdjStartPos,
	AdjEndPos
};


UENUM(BlueprintType)
enum class ERoadSplinePointType: uint8
{
	Linear = ESplinePointType::Linear,
	Curve = ESplinePointType::Curve,
	Constant = ESplinePointType::Constant,
	CurveClamped = ESplinePointType::CurveClamped,
	CurveCustomTangent = ESplinePointType::CurveCustomTangent,
	Arc
};

UENUM(BlueprintType)
enum class ERoadSplinePointTypeOverride : uint8
{
	Inherited,
	Arc
};

UENUM(BlueprintType)
enum class ERoadSplineMagicTransformFilter: uint8
{
	InnrerOnly, // Update all connected splines in the owned actor only
	OuterOnly,  // Update all connected splines in all actores except owned actor
	All // Update all connected splines
};


/**
 * URoadSplineMetadata
 */
UCLASS()
class UNREALDRIVE_API URoadSplineMetadata : public USplineMetadata
{
	GENERATED_UCLASS_BODY()

public:
	/** Insert point before index, lerping metadata between previous and next key values */
	virtual void InsertPoint(int32 Index, float t, bool bClosedLoop) override;
	/** Update point at index by lerping metadata between previous and next key values */
	virtual void UpdatePoint(int32 Index, float t, bool bClosedLoop) override;
	virtual void AddPoint(float InputKey) override;
	virtual void RemovePoint(int32 Index) override;
	virtual void DuplicatePoint(int32 Index) override;
	virtual void CopyPoint(const USplineMetadata* FromSplineMetadata, int32 FromIndex, int32 ToIndex) override;
	virtual void Reset(int32 NumPoints) override;
	virtual void Fixup(int32 NumPoints, USplineComponent* SplineComp) override;

	TWeakObjectPtr<URoadSplineComponent> Spline;

};

struct FRoadPosition
{
	FVector Location; // Local or World
	FQuat Quat;  // Local or World
	double SOffset;
	double ROffset;

	/*
	bool operator== (const FRoadPosition& Other) const
	{
		return Location == Other.Location &&
			SOffset == Other.SOffset;
	}

	bool operator!= (const FRoadPosition& Other) const
	{
		return !(*this == Other);
	}
	*/
};

/** 
 * URoadSplineComponent
 */
UCLASS(BlueprintType, Blueprintable, ShowCategories = ("Rendering"), meta = (BlueprintSpawnableComponent))
class UNREALDRIVE_API URoadSplineComponent 
	: public USplineComponent
{
	GENERATED_UCLASS_BODY()
	
	friend URoadSplineMetadata;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Road)
	FRoadLayout RoadLayout;

	UPROPERTY(VisibleAnywhere, AdvancedDisplay, BlueprintReadOnly, Category = Road)
	TObjectPtr<URoadConnection> PredecessorConnection;

	UPROPERTY(VisibleAnywhere, AdvancedDisplay, BlueprintReadOnly, Category = Road)
	TObjectPtr<URoadConnection> SuccessorConnection;

	UPROPERTY(VisibleAnywhere, Category = Spline)
	TArray<ERoadSplinePointTypeOverride> PointTypes;

public:
	/** Skip procrdure generation for this spline. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Road)
	bool bSkipProcrdureGeneration = false;

	/** MaterialPriority for procrdure generation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Road)
	uint8 MaterialPriority = 0;

public:
	virtual ~URoadSplineComponent();

	virtual USplineMetadata* GetSplinePointsMetadata() { return SplineMetadata; }
	virtual const USplineMetadata* GetSplinePointsMetadata() const { return SplineMetadata; }

	UFUNCTION(BlueprintCallable, Category = Road)
	void UpdateRoadLayout();

	UFUNCTION(BlueprintCallable, Category = Road)
	void UpdateLaneSectionBounds();

	// Fit Width and Attributes to far all lane of all sections and remove unbound sections
	UFUNCTION(BlueprintCallable, Category = Road)
	void TrimLaneSections(double Tolerance = 0.1);

	UFUNCTION(BlueprintCallable, Category = Spline)
	void UpdateMagicTransform(ERoadSplineMagicTransformFilter Filter = ERoadSplineMagicTransformFilter::All);

	UFUNCTION(BlueprintCallable, Category = Spline)
	virtual void UpdateAutoTangents(int EditingPointIndex = -1);

	//UFUNCTION(BlueprintCallable, Category = Spline)
	virtual void UpdateSpline(int EditingPointIndex);

	UFUNCTION(BlueprintCallable, Category = Road)
	const FRoadLayout& GetRoadLayout() const { return RoadLayout; }
	FRoadLayout& GetRoadLayout() { return RoadLayout; }

	UFUNCTION(BlueprintCallable, Category = Road)
	const TArray<FRoadLaneSection> & GetLaneSections() const { return RoadLayout.Sections; }
	TArray<FRoadLaneSection>& GetLaneSections() { return RoadLayout.Sections; }

	UFUNCTION(BlueprintCallable, Category = Road)
	const FRoadLaneSection & GetLaneSection(int i) const { return RoadLayout.Sections[i]; }
	FRoadLaneSection& GetLaneSection(int i) { return RoadLayout.Sections[i]; }

	UFUNCTION(BlueprintCallable, Category = Road)
	int GetLaneSectionsNum() const { return RoadLayout.Sections.Num(); }

	const FRoadLane* GetRoadLane(int SectionIndex, int LaneIndex) const;
	FRoadLane* GetRoadLane(int SectionIndex, int LaneIndex);

	//const TArray<ERoadSplinePointTypeOverride>& GetRoadPointTypes() const { return PointTypes; }

	UFUNCTION(BlueprintCallable, Category = Spline)
	ERoadSplinePointType GetRoadSplinePointType(int32 PointIndex) const;

	UFUNCTION(BlueprintCallable, Category = Spline)
	void SetRoadSplinePointType(int32 PointIndex, ERoadSplinePointType Mode, bool bUpdateSpline = true);

	void ApplyComponentInstanceData(struct FDriveSplineInstanceData* ComponentInstanceData, const bool bPostUCS);

	void BuildOffsetCurves(double RightOffset, FSplineCurves& OutCurves) const;

	void BuildLinearApproximation(TArray<FSplinePositionLinearApproximation>& OutPoints, const TFunction<double(double)>& RightOffsetFunc, double S0, double S1, int ReparamStepsPerSegment, int MinNumSteps, ESplineCoordinateSpace::Type CoordinateSpace) const;

	FVector EvalLanePoistion(int SectionIndex, int LaneIndex, double S, double Alpha, ESplineCoordinateSpace::Type CoordinateSpace) const;

	double EvalROffset(double S) const;

	struct FRang { double StartS; double EndS; };
	FRang GetLaneRang(int SectionIndex, int LaneIndex) const;

	//  Find closest points between spline segments (Key1 and Key2) and linear segment (A1 and B2A2 return spline kay
	float ClosetsKeyToSegmant(float Key1, float Key2, const FVector& A1, const FVector& A2) const;

	//  Find closest points between spline segments (S1 and S2) and linear segment (A1 and A2) return spline kay
	float ClosetsKeyToSegmant2(float S1, float S2, const FVector& A1, const FVector& A2) const;

	URoadConnection* GetPredecessorConnection() const { return PredecessorConnection; }
	URoadConnection* GetSuccessorConnection() const { return SuccessorConnection; }

	TArray<ULaneConnection*> FindAllSuccessors(int SectionIndex, int LaneIndex, bool bIncludesThisRoad = false) const;
	TArray<ULaneConnection*> FindAllPredecessors(int SectionIndex, int LaneIndex, bool bIncludesThisRoad = false) const;

	int FindRoadSectionOnSplineKey(float SplineKey) const;

	//void InsertSection(int SectionIndex, const FRoadLaneSection & Section);
	//bool DeleteSection(int SectionIndex);
	virtual int SplitSection(float SplineKey, ERoadLaneSectionSide Side);

	/** LaneIndex rang [... -2, -1, 0, 1, 2, 3 ...] */
	//void InsertLane(int SectionIndex, int LaneIndex, const FRoadLane& Lane);

	/** LaneIndex rang [... -2, -1, 0, 1, 2, 3 ...] */
	//void DeleteLane(int SectionInddex, int LaneIndex);

	virtual void DisconnectAll();


	void SetSelectedLane(int InSectionIndex, int InLaneSectionIndex) { SelectedSectionIndex = InSectionIndex; SelectedLaneSectionIndex = InLaneSectionIndex; };
	TTuple<int, int> GetSelectedLane() const { return { SelectedSectionIndex , SelectedLaneSectionIndex }; }


	FQuat GetBackwardQuaternionAtSplineInputKey(float InKey, ESplineCoordinateSpace::Type CoordinateSpace) const;
	FTransform GetBackwardTransformAtSplineInputKey(float InKey, ESplineCoordinateSpace::Type CoordinateSpace, bool bUseScale=false) const;
	
	/** Fixed version of origin USplineComponent::SetRotationAtSplinePoint(). Origin function has a bug in case of ESplineCoordinateSpace::World */
	void SetRotationAtSplinePoint_Fixed(int32 PointIndex, const FRotator& InRotation, ESplineCoordinateSpace::Type CoordinateSpace, bool bUpdateSpline=true);

	static void SetRotationAtSplinePoint_Fixed(USplineComponent* Spline, int32 PointIndex, const FRotator& InRotation, ESplineCoordinateSpace::Type CoordinateSpace, bool bUpdateSpline /*= true*/);

	virtual uint64 GetSplineCurvesVersion() const { return SplineCurves.Version; }

	void VapidateConnections();

public:
	FRoadPosition GetRoadPosition(int SectionIndex, int LaneIndex, double Alpha, double SOffset, ESplineCoordinateSpace::Type CoordinateSpace) const;
	FRoadPosition GetRoadPosition(double SOffset, double ROffset, ESplineCoordinateSpace::Type CoordinateSpace) const;

public:
	virtual void UpdateSpline() override;
	virtual void Serialize(FArchive& Ar) override;
	virtual bool ShouldRenderSelected() const override;
	virtual void DestroyComponent(bool bPromoteChildren = false) override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;

protected:
	virtual bool MoveComponentImpl(const FVector& Delta, const FQuat& NewRotation, bool bSweep, FHitResult* OutHit = NULL, EMoveComponentFlags MoveFlags = MOVECOMP_NoFlags, ETeleportType Teleport = ETeleportType::None) override;
	virtual void OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport = ETeleportType::None) override;
#if WITH_EDITOR
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
#endif
	virtual void PostLoad() override;
	virtual void PostInitProperties() override;
	virtual void PostEditImport() override;
	virtual void PreDuplicate(FObjectDuplicationParameters& DupParams) override;
	virtual void PostDuplicate(bool bDuplicateForPIE) override;
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;
	virtual TStructOnScope<FActorComponentInstanceData> GetComponentInstanceData() const override;
	virtual void PushSelectionToProxy() override;
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual void GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials = false) const override;
	virtual void CreateRenderState_Concurrent(FRegisterComponentContext* Context) override;

protected:
	bool AdjustArcSegment(int PointIndex, EComputeArcMode Mode);
	void AdjustLineSegment(int SegmentIndex);
	bool CheckArc(int PointIndex) const;
	void FixUpSegments();

	int GetNextPoint(int iSegment) const;
	int GetPrevPoint(int iSegment) const;

	virtual void MagicUpdateTransformInner(TFunction<bool(const URoadSplineComponent*)> Filter);

protected:
	UPROPERTY();
	URoadSplineMetadata* SplineMetadata = nullptr;

	int SelectedSectionIndex = INDEX_NONE;
	int SelectedLaneSectionIndex = 0;
};


/** 
 * Used to store spline data during RerunConstructionScripts 
 */
USTRUCT()
struct UNREALDRIVE_API FDriveSplineInstanceData : public FSplineInstanceData
{
	GENERATED_BODY()
public:
	FDriveSplineInstanceData()
	{}
	explicit FDriveSplineInstanceData(const URoadSplineComponent* SourceComponent)
		: FSplineInstanceData(SourceComponent)
	{}
	virtual ~FDriveSplineInstanceData() = default;

	virtual void ApplyToComponent(UActorComponent* Component, const ECacheApplyPhase CacheApplyPhase) override
	{
		FSceneComponentInstanceData::ApplyToComponent(Component, CacheApplyPhase);
		CastChecked<URoadSplineComponent>(Component)->ApplyComponentInstanceData(this, (CacheApplyPhase == ECacheApplyPhase::PostUserConstructionScript));
	}

	UPROPERTY()
	TArray<ERoadSplinePointTypeOverride> PointTypes;
};


