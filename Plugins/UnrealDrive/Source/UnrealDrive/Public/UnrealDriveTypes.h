/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "CoreMinimal.h"
#include "Curves/RichCurve.h"
#include "Curves/CurveFloat.h"
#include "Components/SplineComponent.h"
#include "StructUtils/InstancedStruct.h"
#include "Animation/AttributeCurve.h"
#include "RoadLaneAttribute.h"
#include "UnrealDriveTypes.generated.h"

enum { LANE_INDEX_NONE = 0 };

namespace UnrealDrive
{
	const static double DefaultRoadLaneWidth = 375.0;
	
	UNREALDRIVE_API void TrimCurveInRang(FRichCurve& Curve, double Time0, double Time1, bool bFitBorders);
};

class URoadSplineComponent;
class URoadConnection;
class ULaneConnection;
struct FRoadLayout;

/**
 * EDriveableRoadLaneType
 */
UENUM(BlueprintType)
enum class EDriveableRoadLaneType : uint8
{
	None, /** Describes a no-drivable the space on the road and does not have actual content. */
	Driving, /**  Describes a "normal" drivable road that is not one of the other types. */
	//Bidirectional, /** */
	Shoulder, /** Describes a soft border at the edge of the road.*/
	Border, /** Describes a hard border at the edge of the road. It has the same height as the drivable lane. */
	Stop, /** Hard shoulder on motorways for emergency stops.*/
	Biking, /**  Describes a lane that is reserved for cyclists.*/
	Restricted, /** Describes a lane on which cars should not drive. The lane has the same height as drivable lanes. Typically, the lane is separated with lines and often contains dotted lines as well.*/
	Parking, /** Describes a lane with parking spaces.*/
	Median, /** Describes a lane that sits between driving lanes that lead in opposite directions. It is typically used to separate traffic in towns on large roads. */
	RoadWorks, 
	Tram, 
	//Entry, /** Describes a lane type that is used for sections that are parallel to the main road. It is mainly used for acceleration lanes. */
	//Exit, /** Describes a lane that is used for sections that are parallel to the main road. It is mainly used for deceleration lanes. */
	//OffRamp, /** A ramp leading away from a motorway and onto rural urban roads */
	//OnRamp, /** A ramp leading to a motorway from rural or urban roads */
	//Rail, /** */
	//Sidewalk, /** Describes a lane on which pedestrians can walk. */
};


/**
 * ERoadLaneDirection
 */
UENUM(BlueprintType)
enum class ERoadLaneDirection : uint8
{
	Default,
	Invert,
};

/**
 * ERoadLaneSectionSide
 */
UENUM(BlueprintType)
enum class ERoadLaneSectionSide : uint8
{
	Both,
	Left,
	Right
};

/**
 * ERoadDirection
 */
UENUM(BlueprintType)
enum class ERoadDirection : uint8
{
	LeftHand,
	RightHand
};


/**
 * FRoadLaneInstance is the basic structure for storing FRoadLane data. 
 * The type the descendants of this structure is essentially also a type of FRoadLane (FRoadLaneDriving, FRoadLaneSidewalk)
 */
USTRUCT(BlueprintType, Blueprintable)
struct UNREALDRIVE_API FRoadLaneInstance
{
	GENERATED_USTRUCT_BODY()

	virtual ~FRoadLaneInstance() {}

	//virtual int GetMaterialPriority() const { return 0; }
	//virtual FName GetMaterialProfile() const { return NAME_None; }
};


/**
 * FRoadLaneDriving determines the type of FRoadLane - Driving.
 * This is any part of the road on which a vehicle could potentially move. For example: simple road, shoulder, biking, tram etc. lanes.
 */
USTRUCT(BlueprintType, Blueprintable)
struct UNREALDRIVE_API FRoadLaneDriving : public FRoadLaneInstance
{
	GENERATED_USTRUCT_BODY()

public:
	/** Type of road lane. Not used for procedural generation. Can be used in game mechanics in any way, mainly for traffic generation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RoadLane)
	EDriveableRoadLaneType DriveableLaneType = EDriveableRoadLaneType::Driving;

	/** Road lane material profile from UUnrealDrivePresetBase::DriveableMaterialProfiles */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RoadLane, meta = (GetOptions = "UnrealDrive.UnrealDrivePresetBase.GetDriveableMaterialProfiles"))
	FName MaterialProfile = "Default";

	// Invert U coordinate for UV0 for procedure generation
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RoadLane)
	bool bInvertUV0 = false;
};

/**
 * FRoadLaneSidewalk determines the type of FRoadLane - Sidewalk. This lane can't be used for vehicles only pedestrians.
 */
USTRUCT(BlueprintType, Blueprintable)
struct UNREALDRIVE_API FRoadLaneSidewalk : public FRoadLaneInstance
{
	GENERATED_USTRUCT_BODY()

public:
	FRoadLaneSidewalk();

	//UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RoadLane)
	//double DefaultHeight = 30;

	//TODO. Not supported yet
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RoadLane)
	bool bIsSoftBorder = false;

	//UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RoadLane)
	//double CurbWidth = 15;

	/** Whether to build the inside curb (in the direction of the spline) when using procedural generation for this road lane  */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RoadLane)
	bool bInsideCurb = true;

	/** Whether to build the outside curb (in the direction of the spline) when using procedural generation for this road lane  */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RoadLane)
	bool bOutsideCurb = true;

	/** Whether to build the begining  cap curb (in the direction of the spline) when using procedural generation for this road lane  */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RoadLane)
	bool bBeginCurb = false;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RoadLane, meta = (EditCondition = "bBeginCurb", EditConditionHides))
	FRuntimeFloatCurve BeginCapCurve;

	/** Whether to build the ending cap curb (in the direction of the spline) when using procedural generation for this road lane  */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RoadLane)
	bool bEndCurb = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RoadLane, meta = (EditCondition = "bEndCurb", EditConditionHides))
	FRuntimeFloatCurve EndCapCurve;

	/** Road lane material profile from UUnrealDrivePresetBase::SidewalkMaterialProfiles */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RoadLane, meta = (GetOptions = "UnrealDrive.UnrealDrivePresetBase.GetSidewalkMaterialProfiles"))
	FName MaterialProfile = "Default";

	/** Curb profile profile from UUnrealDrivePresetBase::CurbProfiles */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RoadLane, meta = (GetOptions = "UnrealDrive.UnrealDrivePresetBase.GetCurbProfiles"))
	FName CurbProfile = "Default";
};


/**
 * FRoadLane is essential part of all roads. Lanes are attached to the road reference line (URoadSplineComponent) and are defined from inside to outside.
 * See https://unrealdrive.readthedocs.io/en/latest/RoadModel.html#road-lanes 
 */
USTRUCT(BlueprintType, Blueprintable)
struct UNREALDRIVE_API FRoadLane
{
	GENERATED_USTRUCT_BODY()

	friend FRoadLayout;

	FRoadLane();

	/** LaneInstance defines a base type of the FRoadLane. By default, this is FRoadLaneDriving or FRoadLaneSidewalk. */
	UPROPERTY(EditAnywhere, NoClear, BlueprintReadWrite, Category = RoadLane, Export)
	TInstancedStruct<FRoadLaneInstance> LaneInstance;

	/** 
	 * Attributes are arbitrary metadata that can be assigned along the road lane.
	 * See https://unrealdrive.readthedocs.io/en/latest/RoadModel.html#lane-attributes */
	UPROPERTY(EditAnywhere, NoClear, BlueprintReadWrite, Category = RoadLane)
	TMap<FName, FRoadLaneAttribute> Attributes;

	/** Lane width curve */
	UPROPERTY(EditAnywhere, Category = RoadLane)
	FRichCurve Width;

	/** Direction of the road lane. See https://unrealdrive.readthedocs.io/en/latest/RoadModel.html#lane-direction */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RoadLane)
	ERoadLaneDirection Direction = ERoadLaneDirection::Default;

	/** Skip procrdure generation for this lane */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RoadLane)
	bool bSkipProcrdureGeneration = false;

	/** Used for any user purpose */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LaneSection)
	TMap<FString, FString> StringTags;

	/** Used for any user purpose */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LaneSection)
	TMap<FString, double> FloatTags;

	/**  Used for any user purpose (mostly debugging) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LaneSection)
	FString Label{};

	/** 
	 * Used to organize link between the FRoadLane and others URoadSplineComponent.
	 * See https://unrealdrive.readthedocs.io/en/latest/RoadModel.html#intersections-and-junctions */
	UPROPERTY(VisibleAnywhere, AdvancedDisplay, BlueprintReadOnly, Category = RoadLane)
	TObjectPtr< ULaneConnection> PredecessorConnection;

	/** 
	 * Used to organize link between the FRoadLane and others URoadSplineComponent.
	 * See https://unrealdrive.readthedocs.io/en/latest/RoadModel.html#intersections-and-junctions */
	UPROPERTY(VisibleAnywhere, AdvancedDisplay, BlueprintReadOnly, Category = RoadLane)
	TObjectPtr< ULaneConnection> SuccessorConnection;

	/** End SOffset position (from the beginning of the spline) of this lane. This value set from URoadSplineComponent::UpdateLaneSectionBounds() */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = RoadLane)
	double SOffsetEnd_Cashed = 0;

private:
	/** Set from  URoadSplineComponent::UpdateRoadLayout() */
	TWeakPtr<FRoadLayout*> OwnedRoadLayout {};

	/** Set from  URoadSplineComponent::UpdateRoadLayout() */
	UPROPERTY(VisibleAnywhere, Category = RoadLane, Transient)
	int StartSectionIndex = INDEX_NONE;

	/** Set from  URoadSplineComponent::UpdateRoadLayout() */
	UPROPERTY(VisibleAnywhere, Category = RoadLane, Transient)
	int EndSectionIndex = INDEX_NONE; 

	/** Set from  URoadSplineComponent::UpdateRoadLayout() */
	UPROPERTY(VisibleAnywhere, Category = RoadLane, Transient)
	int LaneIndex = LANE_INDEX_NONE;


public:
	bool IsLaneValid() const;
	bool IsForwardLane() const;

	int GetStartSectionIndex() const { return StartSectionIndex; }
	int GetEndSectionIndex() const { return EndSectionIndex; }
	int GetLaneIndex() const { return LaneIndex; }

	const FRoadLaneSection& GetStartSection() const;
	FRoadLaneSection& GetStartSection();

	const FRoadLaneSection& GetEndSection() const;
	FRoadLaneSection& GetEndSection();

	double GetStartOffset() const;
	double GetEndOffset() const { return SOffsetEnd_Cashed; }

	/** Fit Width and Attributes to lane section bounds */
	void Trim(bool bFitWidth);
};


/**
 * FRoadLaneSection
 * See https://unrealdrive.readthedocs.io/en/latest/RoadModel.html#lane-sections 
 */
USTRUCT(BlueprintType, Blueprintable)
struct UNREALDRIVE_API FRoadLaneSection
{
	GENERATED_USTRUCT_BODY();

	friend FRoadLayout;

	FRoadLaneSection();
	~FRoadLaneSection();

	FRoadLaneSection(FRoadLaneSection&& Other) noexcept;
	FRoadLaneSection& operator = (FRoadLaneSection&& Other) noexcept;

	FRoadLaneSection(const FRoadLaneSection& Other) noexcept;
	FRoadLaneSection& operator = (const FRoadLaneSection& Other) noexcept;

	/** Determines which side this section is on.
	 *  See https://unrealdrive.readthedocs.io/en/latest/RoadModel.html#lane-sections */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LaneSection)
	ERoadLaneSectionSide Side = ERoadLaneSectionSide::Both;

	/** 
	 * Road lanes on the left side of the URoadSplineComponent. 
	 * See https://unrealdrive.readthedocs.io/en/latest/RoadModel.html#lane-groups */
	UPROPERTY(EditAnywhere, NoClear, BlueprintReadWrite, Export, Category = LaneSection)
	TArray<FRoadLane> Left;

	/** 
	 * Road lanes on the right side of the URoadSplineComponent.
	 * See https://unrealdrive.readthedocs.io/en/latest/RoadModel.html#lane-groups */
	UPROPERTY(EditAnywhere, NoClear, BlueprintReadWrite, Export, Category = LaneSection)
	TArray<FRoadLane> Right;

	/** 
	 * Center lane attributes.
	 * See https://unrealdrive.readthedocs.io/en/latest/RoadModel.html#lane-attributes */
	UPROPERTY(EditAnywhere, NoClear, BlueprintReadWrite, Category = LaneSection)
	TMap<FName, FRoadLaneAttribute> Attributes;

	/** SOffset position (from the beginning of the spline) of this section. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LaneSection)
	double SOffset = 0;

	/** End SOffset position (from the beginning of the spline) of this section. This value automatically calucated in the UpdateLaneSectionBounds() */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = LaneSection)
	double SOffsetEnd_Cashed = 0;

	/** Used for any user purpose (mostly debugging) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LaneSection)
	FString Label{};

	bool IsSectionValid() const;
	const FRoadLane& GetLaneByIndex(int LaneIndex) const;
	FRoadLane& GetLaneByIndex(int LaneIndex);
	bool CheckLaneIndex(int LaneIndex) const;

	/** Fit Width and Attributes to far all lane of the this section bounds */
	void Trim(bool bFitWidth);

	/**
	 * @param LineIndex (> 0) - Right lane, (< 0) - Left lane
	 * @param Alpha [0..1], 0 - start lane ROffset, 1 - end ROffset
	 * @return - Right ROffset from spline
	 */
	double EvalLaneROffset(int LaneIndex, double SOffset, double Alpha = 1.0) const;

	inline bool operator < (const FRoadLaneSection& Other) const { return SOffset < Other.SOffset; }

private:
	/** Set from FRoadLayout::UpdateLayout() */
	TWeakPtr<FRoadLayout*> OwnedRoadLayout{}; 

	/** Set from FRoadLayout::UpdateLayout() */
	UPROPERTY(VisibleAnywhere, Category = LaneSection, Transient)
	int SectionIndex = INDEX_NONE; 
};

/**
 * FRoadLayout - determine the scope of all road lanes for URoadSplineComponent.
 */
USTRUCT(BlueprintType, Blueprintable)
struct UNREALDRIVE_API FRoadLayout
{
	GENERATED_USTRUCT_BODY();

	FRoadLayout();

	FRoadLayout(FRoadLayout&& Other) noexcept;
	FRoadLayout& operator = (FRoadLayout&& Other) noexcept;

	FRoadLayout(const FRoadLayout& Other) noexcept;
	FRoadLayout& operator = (const FRoadLayout& Other) noexcept;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = RoadLayout)
	TArray<FRoadLaneSection> Sections;

	/** Right offset of the road layout relative to URoadSplineComponent.
	  * See https://unrealdrive.readthedocs.io/en/latest/RoadModel.html#lane-offset */
	UPROPERTY(EditAnywhere, Category = RoadLayout)
	FRichCurve ROffset;

	/** Common direction of the road. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = RoadLayout)
	ERoadDirection Direction = ERoadDirection::LeftHand;

	/** Supported only for closed splines (URoadSplineComponent::bClosedLoop == true). 
	  * Fill the outline formed by the closed URoadSplineComponent with this Instance.
	  * Mainly used for procedural generation to draw the refuge islands or pedestrian crossings. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = FilledInstance, Meta = (DisplayName = "Instance"))
	TInstancedStruct<FRoadLaneInstance> FilledInstance;

	/** UV textures rotation for the FilledInstance. Used for procedure generation only. [deg]*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = FilledInstance)
	double FilledInstanceTexAngle = 0;

	/** UV textures scale for the FilledInstance. Used for procedure generation only. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = FilledInstance)
	double FilledInstanceTexScale = 1.0;

	void UpdateLayout(URoadSplineComponent* OwnedRoadSpline);
	void UpdateBounds(double SplineLength);
	void TrimSections(double SplineLength, double Tolerance, URoadSplineComponent* OwnedRoadSpline);

	double EvalROffset(double S) const;

	int FindSideSection(int SectionIndex, ERoadLaneSectionSide Side) const;

	uint64 GetLayoutVersion() const { return LayoutVersion; }
	uint64 GetAttributesVersion() const { return AttributesVersion; }

	void UpdateAttributesVersion() { ++AttributesVersion; }
	void UpdateLayoutVersion() { ++LayoutVersion; }

private:
	TSharedPtr<FRoadLayout*> ThisShared;

	uint64 LayoutVersion = 0;
	uint64 AttributesVersion = 0;
};

/**
 * URoadConnections  are located at the beginning and end of each URoadSplineComponent. 
 * Used to organize link between the FRoadLane and others URoadSplineComponent.
 * The forward vector of this connection's transform is co-directed with owned URoadSplineComponent.
 * See https://unrealdrive.readthedocs.io/en/latest/RoadModel.html#intersections-and-junctions
 */
UCLASS()
class UNREALDRIVE_API URoadConnection: public UObject
{
	GENERATED_UCLASS_BODY()

	friend URoadSplineComponent;

public:
	UPROPERTY(VisibleAnywhere, Category = RoadConnection)
	TSoftObjectPtr<ULaneConnection> OuterLaneConnection;

	/** Used for any user purpose (mostly debugging) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RoadConnection)
	FString Label{};

	/** Used only for copy/past/duplicate operataions */
	UPROPERTY()
	FGuid LaneConnectionGuid;

public:
	URoadSplineComponent* GetOwnedRoadSpline() const;
	URoadSplineComponent* GetOwnedRoadSplineChecked() const;
	virtual bool IsConnectionValid() const;

	virtual bool ConnectTo(ULaneConnection* OuterLaneConnection);
	virtual bool CanConnectTo(const ULaneConnection* OuterLaneConnection, bool bLog=false) const;
	virtual void Disconnect();
	virtual bool IsConnected() const;
	virtual bool IsConnectionFree() const;
	virtual ULaneConnection* GetOuterConnection() const { return OuterLaneConnection.Get(); }

	virtual void InitConnection();

	virtual bool CanTransform() const;
	virtual const FTransform GetTransform(ESplineCoordinateSpace::Type Space) const;
	virtual bool SetTransform(const FTransform& Transform, bool bUpdateSpline, ESplineCoordinateSpace::Type Space);

	//Set transform to this connection from connected ULaneConnection */
	virtual bool SetTransformFormOuter();

	//Set transform from this connection to connected LaneConnection */
	virtual bool SetTransformToOuter() ;

	virtual bool IsSuccessorConnection() const;
	virtual bool IsPredecessorConnection() const;

	virtual void BeginDestroy() override;
	virtual void Serialize(FArchive& Ar) override;
	//virtual void PostInitProperties() override;
	virtual void PostLoad() override;

	virtual void ExportCustomProperties(FOutputDevice& Out, uint32 Indent) override;
	virtual void ImportCustomProperties(const TCHAR* SourceText, FFeedbackContext* Warn) override;

	static int32 GlobalTransformMagic;

private:
	/** Used to prevent recursive SetTransform() */
	mutable int32 TransformMagic = 0;
};


/**
 * ULaneConnections are located at the beginning and end of each FRoadLane. 
 * Used to organize link between the FRoadLane and others URoadSplineComponent.
 * The forward vector of this connection's transform is co-directed with FRoadLane.
 * See https://unrealdrive.readthedocs.io/en/latest/RoadModel.html#intersections-and-junctions
 */
UCLASS(BlueprintType, Blueprintable)
class UNREALDRIVE_API ULaneConnection: public UObject
{
	GENERATED_UCLASS_BODY()

	friend FRoadLayout;

public:
	TSet<TWeakObjectPtr< URoadConnection>>  OuterRoadConnections;

	/** Used for any user purpose (mostly debugging) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LaneConnection)
	FString Label{};

	// Used only for copy/past/duplicate operataions
	UPROPERTY()
	FGuid Guid;

public:
	URoadSplineComponent* GetOwnedRoadSpline() const { return OwnedRoadSpline.Get(); }
	URoadSplineComponent* GetOwnedRoadSplineChecked() const { check(OwnedRoadSpline.IsValid()); return OwnedRoadSpline.Get(); }
	int GetSectionIndex() const { return SectionIndex; }
	int GetLaneIndex() const { return LaneIndex; }

	const FRoadLane & GetOwnedRoadLane() const;
	FRoadLane& GetOwnedRoadLane();

	virtual bool IsConnectionValid() const;

	virtual bool IsConnected() const;
	//virtual bool ConnectTo(URoadConnection* RoadConnection);
	virtual void DisconnectAll();

	/**
	 * @param Alpha see FRoadLaneSection::EvalLaneROffset(). By default Alpha=0.0
	 */
	virtual const FTransform EvalTransform(double Alpha, ESplineCoordinateSpace::Type Space) const;

	/** Set transform from this connection to all connected URoadConnection */
	virtual void SetTransformToOuter();

	/**
	* A transform can only be set if this connection is the start and end of the owning spline.
	* The resulting transform may differ from the given one.
	* Returns the resulting transform for the connection.
	* TODO: Support SetTransform() to any connection. 
	*       Probably, for this it will be necessary to solve the optimization problem, to calculate new tangents for the spline section.
	*/
	virtual bool SetTransform(const FTransform& Transform, bool bUpdateSpline, ESplineCoordinateSpace::Type Space);

	virtual bool CanTransform() const;

	bool IsSuccessorConnection() const;
	inline bool IsRoadSuccessorConnection() const;
	inline bool IsRoadPredecessorConnection() const;

public:
	//virtual bool Modify(bool bAlwaysMarkDirty = true) override;
	virtual void BeginDestroy() override;
	virtual void Serialize(FArchive& Ar) override;
	virtual void PostEditImport() override;

	virtual void ExportCustomProperties(FOutputDevice& Out, uint32 Indent) override;
	virtual void ImportCustomProperties(const TCHAR* SourceText, FFeedbackContext* Warn) override;

private:
	mutable int32 TransformMagic = 0;

	/** Set from  FRoadLayout::UpdateLayout() */
	UPROPERTY(VisibleAnywhere, Category = LaneConnection, Transient)
	TWeakObjectPtr<URoadSplineComponent> OwnedRoadSpline;
	
	/* Set from  FRoadLayout::UpdateLayout() */
	UPROPERTY(VisibleAnywhere, Category = LaneConnection, Transient)
	int SectionIndex = INDEX_NONE; 

	/** Set from  FRoadLayout::UpdateLayout() */
	UPROPERTY(VisibleAnywhere, Category = LaneConnection, Transient)
	int LaneIndex = LANE_INDEX_NONE;
};