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
	// 默认道路车道宽度
	const static double DefaultRoadLaneWidth = 375.0;
	
	UNREALDRIVE_API void TrimCurveInRang(FRichCurve& Curve, double Time0, double Time1, bool bFitBorders);
};

class URoadSplineComponent;
class URoadConnection;
class ULaneConnection;
struct FRoadLayout;

/**
 * 可行驶道路车道类型
 */
UENUM(BlueprintType)
enum class EDriveableRoadLaneType : uint8
{
	None, /** 描述道路上的非行驶区域，但无实际内容。 */
	Driving, /**  描述一条"普通"的可行驶道路，不属于其他类型。 */
	//Bidirectional, /** */
	Shoulder, /** 路肩-描述道路边缘的软质边界。*/
	Border, /** 描述道路边缘的硬质边界。其高度与可行驶车道相同。 */
	Stop, /** 高速公路上的硬路肩，供紧急停车使用。*/
	Biking, /** 骑行-描述一条专为骑行者保留的车道。*/
	Restricted, /** "受限制的" 或 "限制通行区域"-描述一条汽车不应驶入的车道。该车道与可行驶车道高度相同。通常，该车道通过标线进行分隔，且常包含虚线。*/
	Parking, /** 停车-描述带有停车位的车道。*/
	Median, /** 中央分隔带-描述位于相反方向行驶车道之间的隔离区域。通常用于城镇主干道上分隔对向车流。 */
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
 * 道路车道方向枚举
 */
UENUM(BlueprintType)
enum class ERoadLaneDirection : uint8
{
	Default,
	Invert,
};

/**
 * 道路车道断面侧边枚举
 */
UENUM(BlueprintType)
enum class ERoadLaneSectionSide : uint8
{
	Both,
	Left,
	Right
};

/**
 * 道路方向
 */
UENUM(BlueprintType)
enum class ERoadDirection : uint8
{
	LeftHand,
	RightHand
};


/**
 * FRoadLaneInstance 是用于存储 FRoadLane 数据的基本结构。
 * 该结构的衍生类型本质上也是 FRoadLane 的一种类型（例如 FRoadLaneDriving、FRoadLaneSidewalk）
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
 * FRoadLaneDriving 定义了 FRoadLane 的类型 - 行驶车道。
 * 这是指车辆可以通行的任何道路部分，例如：普通车道、路肩、自行车道、有轨电车道等。
 */
USTRUCT(BlueprintType, Blueprintable)
struct UNREALDRIVE_API FRoadLaneDriving : public FRoadLaneInstance
{
	GENERATED_USTRUCT_BODY()

public:
	/** 道路车道类型。不用于程序化生成，可在游戏机制中任意使用，主要用于交通生成。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RoadLane)
	EDriveableRoadLaneType DriveableLaneType = EDriveableRoadLaneType::Driving;

	/** 来自 UUnrealDrivePresetBase::DriveableMaterialProfiles 的道路车道材质配置 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RoadLane, meta = (GetOptions = "UnrealDrive.UnrealDrivePresetBase.GetDriveableMaterialProfiles"))
	FName MaterialProfile = "Default";

	/** 在程序化生成中反转 UV0 的 U 坐标 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RoadLane)
	bool bInvertUV0 = false;
};

/**
 * FRoadLaneSidewalk 定义了 FRoadLane 的类型 - 人行道。此车道仅供行人使用，车辆不可通行。
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

	/** 在使用程序化生成为此道路车道时，是否构建内侧路缘（沿样条方向）  */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RoadLane)
	bool bInsideCurb = true;

	/** 在使用程序化生成为此道路车道时，是否构建外侧路缘（沿样条方向）  */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RoadLane)
	bool bOutsideCurb = true;

	/** 在使用程序化生成为此道路车道时，是否构建起始端盖路缘（沿样条方向）  */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RoadLane)
	bool bBeginCurb = false;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RoadLane, meta = (EditCondition = "bBeginCurb", EditConditionHides))
	FRuntimeFloatCurve BeginCapCurve;

	/** 在使用程序化生成为此道路车道时，是否构建结束端盖路缘（沿样条方向）  */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RoadLane)
	bool bEndCurb = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RoadLane, meta = (EditCondition = "bEndCurb", EditConditionHides))
	FRuntimeFloatCurve EndCapCurve;

	/** 道路车道材质配置，取自 UUnrealDrivePresetBase::SidewalkMaterialProfiles */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RoadLane, meta = (GetOptions = "UnrealDrive.UnrealDrivePresetBase.GetSidewalkMaterialProfiles"))
	FName MaterialProfile = "Default";

	/** 路缘石配置，取自 UUnrealDrivePresetBase::CurbProfiles */
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
 * 定义 URoadSplineComponent 中所有道路车道的范围。
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

	/** 道路布局相对于 URoadSplineComponent 的右侧偏移量。
	  * See https://unrealdrive.readthedocs.io/en/latest/RoadModel.html#lane-offset */
	UPROPERTY(EditAnywhere, Category = RoadLayout)
	FRichCurve ROffset;

	/** 道路的通用方向。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = RoadLayout)
	ERoadDirection Direction = ERoadDirection::LeftHand;

	/** 仅支持闭合样条线（URoadSplineComponent::bClosedLoop == true
	  * 用此实例填充由闭合 URoadSplineComponent 形成的外轮廓。
	  * 主要用于程序化生成以绘制安全岛或人行横道区域。  */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = FilledInstance, Meta = (DisplayName = "Instance"))
	TInstancedStruct<FRoadLaneInstance> FilledInstance;

	/** 用于 FilledInstance 的 UV 纹理旋转。仅用于程序化生成。[单位：度]*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = FilledInstance)
	double FilledInstanceTexAngle = 0;

	/** 用于 FilledInstance 的 UV 纹理缩放。仅用于程序化生成。 */
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
 * URoadConnections 位于每个 URoadSplineComponent 的起点和终点。
 * 用于组织 FRoadLane 与其他 URoadSplineComponent 之间的连接。
 * 该连接变换的前向向量与所属的 URoadSplineComponent 同向。
 * 参见 https://unrealdrive.readthedocs.io/en/latest/RoadModel.html#intersections-and-junctions
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
 * ULaneConnections 位于每个 FRoadLane 的起点和终点。
 * 用于组织 FRoadLane 与其他 URoadSplineComponent 之间的连接。
 * 该连接变换的前向向量与 FRoadLane 同向。
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