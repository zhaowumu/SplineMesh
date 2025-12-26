/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Engine/World.h"
#include "IndexTypes.h"
#include "InteractiveTool.h"
#include "InteractiveToolBuilder.h"
#include "InteractiveToolManager.h"
#include "InteractiveToolQueryInterfaces.h"

#include "RoadSplineComponent.h"
#include "ModelingTools/Ops/TriangulateRoadOp.h"
#include "Utils/StrongScriptInterface.h"
#include "TriangulateRoadTool.generated.h"

class AActor;

UENUM(BlueprintType)
enum class ECreateRoadObjectType : uint8
{
	StaticMesh = 0,
	DynamicMesh = 1
};

UENUM(BlueprintType)
enum class ERoadActorOutput : uint8
{
	CreateNewActor = 0,
	UseSelectedActor = 1
};


/*
UENUM()
enum class ETriangulateRoadToolDebugBoundaries: uint8
{
	Disabled,
	ShowDriveBoundaries,
	ShowSidewalkBoundaries
};
*/

namespace UnrealDrive
{
	struct FRoadBaseOperatorData;

	/**
	 * FRoadAbstractOperatorFactory
	 */
	struct UNREALDRIVEEDITOR_API FRoadAbstractOperatorFactory
	{
		TWeakObjectPtr<UTriangulateRoadTool> RoadTool;
		TWeakPtr<UnrealDrive::FRoadActorComputeScope> RoadComputeScope;

		virtual ~FRoadAbstractOperatorFactory() {}
	};

	/**
	 * FRoadBaseOperatorFactory
	 */
	class UNREALDRIVEEDITOR_API FRoadBaseOperatorFactory
		: public FRoadAbstractOperatorFactory
		, public UE::Geometry::IGenericDataOperatorFactory<UnrealDrive::FRoadBaseOperatorData>
	{
	public:
		virtual TUniquePtr<UE::Geometry::TGenericDataOperator<UnrealDrive::FRoadBaseOperatorData>> MakeNewOperator() override;
	};


	/**
	 * FRoadActorComputeScope
	 */
	struct UNREALDRIVEEDITOR_API FRoadActorComputeScope
	{
		struct FSplineData
		{
			// Track the spline 'Version' integer, which is incremented when splines and road sections are changed
			uint64 LastRoadVersions;

			// Track the spline 'Version' integer, which is incremented when road attributes are changed
			uint64 LastRoadAttributesVersion;

			// Track the spline component's transform (to world space)
			FTransform LastSplineTransforms;
		};

		TWeakObjectPtr<AActor> TargetActor;
		bool bLostInputSpline = false;
		TArray<FSplineData> SplineData;
		UE::Geometry::FGeometryResult ResultInfo = {};
		TSharedPtr<UnrealDrive::FRoadBaseOperatorData> BaseData;
		TArray<TUniquePtr<UnrealDrive::FRoadAbstractOperatorFactory>> OpFactories;
		TUniquePtr<TGenericDataBackgroundCompute<UnrealDrive::FRoadBaseOperatorData>> BaseOpCompute;
		TArray<TStrongScriptInterface<IRoadOpCompute>> OpComputes;
		bool bNeedGenerateReport = false;

		void NotifyRebuildOne(IRoadOpCompute& Preview);
		void NotifyRebuildAll();
		void AppendResultInfo(const FGeometryResult& Result);
		void ShowReport() const;
	};
}

/**
 * Parameters for controlling the spline triangulation
 */
UCLASS()
class UNREALDRIVEEDITOR_API UTriangulateRoadToolProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:

	UTriangulateRoadToolProperties() {}

	// Split the road(s) into several components, placing each road section in a separate component. 
	UPROPERTY(EditAnywhere, Category = Mesh, meta = (RebuildAll))
	bool bSplitBySections = false;

	// If the SplitBySections is set, then road sections smaller than the MergeSectionsAreaThreshold will be merged with adjacent ones [m^2].
	UPROPERTY(EditAnywhere, Category = Mesh, meta = (RebuildAll, EditCondition="bSplitBySections"))
	double MergeSectionsAreaThreshold = 100;

	// How far to allow the triangulation boundary can deviate from the spline curve before we add more vertices [cm]
	UPROPERTY(EditAnywhere, Category = Mesh, meta = (ClampMin = 0.1, ClampMax = 100, RebuildAll), AdvancedDisplay)
	double ErrorTolerance = 5.0;

	// How far to allow the triangulation boundary can deviate from the aidewalk cap curve before we add more vertices [cm]
	UPROPERTY(EditAnywhere, Category = Mesh, meta = (ClampMin = 0.1, ClampMax = 100, RebuildAll), AdvancedDisplay)
	double SidewalkCapErrorTolerance = 2.0;

	// Minimum length of the spline segment into which it will be divided
	UPROPERTY(EditAnywhere, Category = Mesh, meta = (ClampMin = 0.1, RebuildAll))
	double MinSegmentLength = 375;

	// Points within this tolerance are merged
	UPROPERTY(EditAnywhere, Category = Mesh, meta = (ClampMin = 0.001, ClampMax = 100, RebuildAll), AdvancedDisplay)
	double VertexSnapTol = 0.01;

	UPROPERTY(EditAnywhere, Category = Mesh, meta = (ClampMin = 0.0001, ClampMax = 10, RebuildAll))
	double UV0VScale = 0.0025;

	UPROPERTY(EditAnywhere, Category = Mesh, meta = (ClampMin = 0.0001, ClampMax = 10, RebuildAll))
	double UV1VScale = 0.001;

	UPROPERTY(EditAnywhere, Category = Mesh, meta = (ClampMin = 0.0001, ClampMax = 10, RebuildAll))
	double UV2VScale = 0.001;

	// How to determine the height of the road surface if several spline pass over the same surface
	UPROPERTY(EditAnywhere, Category = Mesh, meta = (RebuildAll))
	ERoadOverlapStrategy OverlapStrategy = ERoadOverlapStrategy::UseMaxZ;

	// Radius of computing of road surface height in case of intersection of several spline. See OverlapStrategy
	UPROPERTY(EditAnywhere, Category = Mesh, meta = (ClampMin = 0, ClampMax = 5000, RebuildAll))
	double OverlapRadius = 500;

	UPROPERTY(EditAnywhere, Category = Mesh, meta = (RebuildAll))
	bool bSmooth = true;

	/** Smoothing speed */
	//UPROPERTY(EditAnywhere, Category = Mesh, meta = (UIMin = "0.0", UIMax = "1.0", ClampMin = "0.0", ClampMax = "1.0"))
	UPROPERTY(meta=(EditCondition = "bSmooth", RebuildAll))
	float SmoothSpeed = 0.1f;

	/** Desired Smoothness. This is not a linear quantity, but larger numbers produce smoother results */
	UPROPERTY(EditAnywhere, Category = Mesh, meta = (UIMin = "0.0", UIMax = "1.0", ClampMin = "0.0", ClampMax = "100.0", EditCondition = "bSmooth", RebuildAll))
	float Smoothness = 0.5f;

	UPROPERTY(EditAnywhere, Category = Mesh)
	ECreateRoadObjectType ObjectType = ECreateRoadObjectType::StaticMesh;

	UPROPERTY(EditAnywhere, Category = Mesh)
	ERoadActorOutput OutputActor = ERoadActorOutput::CreateNewActor;

	UPROPERTY(EditAnywhere, Category = Mesh, meta = (RebuildAll), AdvancedDisplay)
	bool bDrawBoundaries = false;

	UPROPERTY(EditAnywhere, Category = Mesh, AdvancedDisplay)
	bool bShowWireframe = false;
};


/**
 * Tool to create a mesh from a set of selected Spline Components
 */
UCLASS()
class UNREALDRIVEEDITOR_API UTriangulateRoadTool : 
	public UInteractiveTool, 
	public IInteractiveToolEditorGizmoAPI
{
	GENERATED_BODY()


public:

	UTriangulateRoadTool();

	// IInteractiveToolEditorGizmoAPI -- allow editor gizmo so users can live-edit the splines
	virtual bool GetAllowStandardEditorGizmos() override{ return true; }

	// InteractiveTool API
	virtual void Setup() override;
	virtual void Shutdown(EToolShutdownType ShutdownType) override;
	virtual void OnTick(float DeltaTime) override;
	virtual void Render(IToolsContextRenderAPI* RenderAPI) override;
	virtual bool HasCancel() const override { return true; }
	virtual bool HasAccept() const override { return true; }
	virtual bool CanAccept() const override;
	virtual void OnPropertyModified(UObject* PropertySet, FProperty* Property) override;
	virtual void AddToolPropertySource(UObject* PropertyObject) override;
	virtual void AddToolPropertySource(UInteractiveToolPropertySet* PropertySet) override;

public:
	UInteractiveToolPropertySet* SetupPropertySet(const TSubclassOf<UInteractiveToolPropertySet>& PropertySet);

	void SetSplineActors(TArray<TWeakObjectPtr<AActor>> InSplineActors);
	void SetWorld(UWorld* World);
	UWorld* GetTargetWorld();
	void PollRoadsUpdates(bool bForce);
	void NotifyOpWasUpdated() { bOpWasJustUpdated = true; }

public:
	UPROPERTY()
	TWeakObjectPtr<UWorld> TargetWorld = nullptr;

	UPROPERTY()
	TObjectPtr<UTriangulateRoadToolProperties> TriangulateProperties;

protected:
	TArray<TSharedPtr<UnrealDrive::FRoadActorComputeScope>> RoadsComputeScope;
	bool bOpWasJustUpdated = false;
};



/**
 * Base Tool Builder for tools that operate on a selection of Spline Components
 */
UCLASS(Transient)
class UNREALDRIVEEDITOR_API UTriangulateRoadToolBuilder : public UInteractiveToolBuilder
{
	GENERATED_BODY()

public:
	/** @return true if spline component sources can be found in the active selection */
	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override;

	/** Called by BuildTool to configure the Tool with the input spline source(s) based on the SceneState */
	virtual void InitializeNewTool(UTriangulateRoadTool* Tool, const FToolBuilderState& SceneState) const;

	// @return the min and max (inclusive) number of splines allowed in the selection for the tool to be built. A value of -1 can be used to indicate there is no maximum.
	virtual UE::Geometry::FIndex2i GetSupportedSplineCountRange() const
	{
		return UE::Geometry::FIndex2i(1, -1);
	}

	/** @return new Tool instance initialized with selected spline source(s) */
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;
};



