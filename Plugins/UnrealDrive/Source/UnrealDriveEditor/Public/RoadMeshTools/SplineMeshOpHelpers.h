/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "MeshOpPreviewHelpers.h"
#include "ModelingOperators.h"
#include "SplineMeshPreview.h"
#include "IRoadOpCompute.h"
#include "SplineMeshOpHelpers.generated.h"

class UTriangulateRoadTool;

namespace UnrealDrive
{
	struct FRoadActorComputeScope;
}


namespace UnrealDrive
{
	using namespace UE::Geometry;

	/**
	 *  FSplineMeshOperator
	 */
	class UNREALDRIVEEDITOR_API FSplineMeshOperator
	{
	protected:
		TUniquePtr<FSplineMeshSegments> ResultSegments;
		FTransformSRT3d ResultTransform;
		FGeometryResult ResultInfo;

	public:
		FSplineMeshOperator()
		{
			ResultSegments = MakeUnique<FSplineMeshSegments>();
			ResultTransform = FTransformSRT3d::Identity();
		}
		virtual ~FSplineMeshOperator()
		{
		}

		virtual void SetResultTransform(const FTransformSRT3d& Transform)
		{
			ResultTransform = Transform;
		}

		virtual void SetResultInfo(const FGeometryResult& Info)
		{
			ResultInfo = Info;
		}

		TUniquePtr<FSplineMeshSegments> ExtractResult()
		{
			return MoveTemp(ResultSegments);
		}

		const FTransformSRT3d& GetResultTransform() const
		{
			return ResultTransform;
		}


		const FGeometryResult& GetResultInfo() const
		{
			return ResultInfo;
		}

		virtual void CalculateResult(FProgressCancel* Progress) = 0;
	};

	/**
	 * ISplineMeshOperatorFactory 
	 * A ISplineMeshOperatorFactory is a base interface to a factory that creates FSplineMeshOperator
	 */
	class UNREALDRIVEEDITOR_API ISplineMeshOperatorFactory
	{
	public:
		virtual ~ISplineMeshOperatorFactory() {}

		virtual TUniquePtr<FSplineMeshOperator> MakeNewOperator() = 0;
	};

	/**
	 * FBackgroundSplineMeshComputeSource is an instantiation of the TBackgroundModelingComputeSource template for FSplineMeshOperator / ISplineMeshOperatorFactory
	 */
	using FBackgroundSplineMeshComputeSource = UE::Geometry::TBackgroundModelingComputeSource<FSplineMeshOperator, ISplineMeshOperatorFactory>;


	/**
	 * FSplineMeshOpResult
	 */
	struct FSplineMeshOpResult
	{
		TUniquePtr<FSplineMeshSegments> MeshSegments;
		UE::Geometry::FTransformSRT3d Transform;
	};

} // namespace UnrealDrive


/**
 * USplineMeshOpPreviewWithBackgroundCompute 
 */
UCLASS(Transient)
class  UNREALDRIVEEDITOR_API USplineMeshOpPreviewWithBackgroundCompute
	: public UObject
	, public IRoadOpCompute
{
	GENERATED_BODY()

public:

	void Setup(UTriangulateRoadTool* Owner, TWeakPtr<UnrealDrive::FRoadActorComputeScope> RoadComputeScope, UnrealDrive::ISplineMeshOperatorFactory* OpFactory);

	/**
	 * @param InWorld the Preview  actor will be created in this UWorld
	 * @param OpGenerator This factory is called to create new MeshSplineOperators on-demand
	 */
	void Setup(UWorld* InWorld, UnrealDrive::ISplineMeshOperatorFactory* OpGenerator);

	void Setup(UWorld* InWorld);

	/**
	 * Terminate any active computation and return the current Preview SplineMesh/Transform
	 */
	UnrealDrive::FSplineMeshOpResult Shutdown();

	/**
	 * Stops any running computes and swaps in a different op generator. Does not
	 * update the preview mesh or start a new compute.
	 */
	void ChangeOpFactory(UnrealDrive::ISplineMeshOperatorFactory* OpGenerator);


	void ClearOpFactory();

	/**
	 * Cancel the active computation without returning anything. Doesn't destroy the mesh.
	 */
	virtual void CancelCompute() override;

	/**
	* Terminate any active computation without returning anything. Destroys the preview
	* mesh.
	*/
	virtual void Cancel() override;

	/**
	 * Tick the background computation and Preview update.
	 * @warning this must be called regularly for the class to function properly
	 */
	virtual void Tick(float DeltaTime) override;


	/**
	 * Request that the current computation be canceled and a new one started
	 */
	virtual void InvalidateResult() override ;

	/**
	 * @return true if the current PreviewMesh result is valid, ie no update being actively computed
	 */
	bool HaveValidResult() const { return bResultValid; }

	double GetValidResultComputeTime() const
	{
		if (HaveValidResult())
		{
			return ValidResultComputeTimeSeconds;
		}
		return -1;
	}


	/**
	 * @return true if current PreviewMesh result is valid (no update actively being computed) and that mesh has at least one triangle
	 */
	virtual bool HaveValidNonEmptyResult() const override { return bResultValid && PreviewMesh && PreviewMesh->GetMeshSegments()->Segments.Num() > 0; }

	/**
	 * @return true if current PreviewMesh result is valid (no update actively being computed) but that mesh has no triangles
	 */
	bool HaveEmptyResult() const { return bResultValid && PreviewMesh && PreviewMesh->GetMeshSegments()->Segments.Num() == 0; }

	/** @return UWorld that the created PreviewMesh exist in */
	virtual UWorld* GetWorld() const override { return PreviewWorld.Get(); }

	//
	// Optional configuration
	// 

	/**
	 * Set the visibility of the Preview mesh
	 */
	virtual void SetVisibility(bool bVisible) override;


	// Configure the maximum allowed number of background tasks
	void SetMaxActiveBackgroundTasks(int32 NewMaxTasks);

	// Configure maximum allowed number of background tasks heuristically from the input triangle count
	void SetMaxActiveBackgroundTasksFromMeshSizeHeuristic(int32 InputMeshTriangleCount, int32 MaxSimultaneousTrianglesToProcess = 10000000, int32 MaxShouldNotExceed = 20)
	{
		SetMaxActiveBackgroundTasks(FMath::Clamp(MaxSimultaneousTrianglesToProcess / FMath::Max(1, InputMeshTriangleCount), 1, MaxShouldNotExceed));
	}

	virtual void EnableWireframe(bool) override {}

	virtual bool IsRoadAttribute() const { return true; }

	virtual int GetNumVertices() const { return 0; };

	virtual int GetNumTriangles() const { return 0; };


	UE::Geometry::EBackgroundComputeTaskStatus GetLastComputeStatus() const { return LastComputeStatus; }

	virtual void ShutdownAndGenerateAssets(AActor* TargetActor, const FTransform3d& ActorToWorld) override;

	//
	// Change notification
	//
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnMeshUpdated, USplineMeshOpPreviewWithBackgroundCompute*);
	/** This delegate is broadcast whenever the embedded preview mesh is updated */
	FOnMeshUpdated OnMeshUpdated;

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnOpCompleted, const UnrealDrive::FSplineMeshOperator*);
	FOnOpCompleted OnOpSplineMeshCompleted;

public:
	// preview of MeshOperator result
	UPROPERTY()
	TObjectPtr<USplineMeshPreview> PreviewMesh = nullptr;



	UPROPERTY()
	TWeakObjectPtr<UWorld> PreviewWorld = nullptr;

	/**
	 * When true, the preview mesh is allowed to be temporarily updated using results that we know
	 * are dirty (i.e., the preview was invalidated, but a result became available before the operation
	 * was restarted, so we can at least show that while we wait for the new result). The change
	 * notifications will be fired as normal for these results, but HasValidResult will return false.
	 */
	bool bAllowDirtyResultUpdates = true;

protected:
	// state flag, if true then we have valid result
	bool bResultValid = false;
	double ValidResultComputeTimeSeconds = -1;

	// Stored status of last compute, mainly so that we know when we should
	// show the "busy" material.
	UE::Geometry::EBackgroundComputeTaskStatus LastComputeStatus = UE::Geometry::EBackgroundComputeTaskStatus::NotComputing;

	bool bVisible = true;

	bool bMeshInitialized = false;

	float SecondsBeforeWorkingMaterial = 2.0;

	// this object manages the background computes
	TUniquePtr<UnrealDrive::FBackgroundSplineMeshComputeSource> BackgroundCompute;

	// update the PreviewMesh if a new result is available from BackgroundCompute
	void UpdateResults();

private:

	int32 MaxActiveBackgroundTasks = 5;
	bool bWaitingForBackgroundTasks = false;
};

