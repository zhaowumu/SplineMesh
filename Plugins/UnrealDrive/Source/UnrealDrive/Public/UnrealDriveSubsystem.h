/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "Subsystems/WorldSubsystem.h"
#include "UnrealDriveSubsystem.generated.h"

class URoadConnection;
class URoadSplineComponent;
class ULaneConnection;

UCLASS()
class UNREALDRIVE_API UUnrealDriveSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableInEditor() const override { return true; }
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override { return WorldType == EWorldType::Editor || WorldType == EWorldType::PIE; }

	void BeginCopyRoadsTransaction();
	void EndCopyRoadTransaction();

	//virtual void OnWorldComponentsUpdated(UWorld& World) override;
	//static UUnrealDriveSubsystem* GetSubsystem(const UWorld* InWorld);

	struct FConnectionInfo
	{
		FTransform Transform;
		bool bIsSelected;
	};

	struct FViewCameraState
	{
		const FMatrix& ViewToProj;
		const FIntRect& ViewRect;
		const FVector& ViewPosition;
		bool bIsOrthographic;
		float OrthoWorldCoordinateWidth;
	};

	void UpdateObservedConnections(const URoadSplineComponent* RoadSpline);
	void CleanObservedConnections();
	void AddObservedConnections(const URoadSplineComponent* RoadSpline, const TArray<const ULaneConnection*>& Connections);
	auto GetObservedConnections() const { return ObservedConnections; }
	/**
	 * @param MaxViewDistance - valid only if bIsOrthographic is false
	 * @params OrthoWorldCoordinateWidth - Current width of viewport in world space coordinates. Only valid if bIsOrthographic is true
	 */
	void CaptureConnections(const URoadConnection* SrcConnection, const FViewCameraState& CameraState,  double MaxViewDistance, double MaxOrthoWidth, TFunction<bool(const ULaneConnection*)> IsConnectionAllowed = [](const ULaneConnection*) { return true; });
	void ForEachObservedConnection(TFunctionRef<void(const ULaneConnection*, FConnectionInfo&)> VisitorFunc);
	TPair<TWeakObjectPtr<const ULaneConnection>, FConnectionInfo>* FindObservedConnectionByPredicate(TFunctionRef<bool (const ULaneConnection*, const FConnectionInfo&) > VisitorFunc);

	bool GetRoadSplineWasSelected() const { return bRoadSplineWasSelected; }

private:
	void OnActorSpawned(AActor* InActor);
	void OnActorDeleted(AActor* InActor, bool bClearStencilIdValues);

	void OnDuplicateActorsBegin();
	void OnDuplicateActorsEnd();

	bool bDuplicationStarted = false;
	TArray<AActor*> DuplicatedActors;

	TMap<TWeakObjectPtr<const URoadSplineComponent>, TMap<TWeakObjectPtr<const ULaneConnection>, FConnectionInfo>> ObservedConnections;

	bool bRoadSplineWasSelected = false;
#endif
};
