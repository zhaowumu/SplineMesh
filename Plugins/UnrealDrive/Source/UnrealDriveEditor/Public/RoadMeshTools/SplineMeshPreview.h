/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "CoreMinimal.h"
#include "PreviewMesh.h"
#include "SplineMeshSegments.h"
#include "SplineMeshPreview.generated.h"

UCLASS(Transient)
class UNREALDRIVEEDITOR_API USplineMeshPreview : public UObject
{
	GENERATED_BODY()

public:
	USplineMeshPreview();
	virtual ~USplineMeshPreview();

	/**
	 * Create preview mesh in the World with the given transform
	 */
	void CreateInWorld(UWorld* World, const FTransform& WithTransform);

	/**
	 * Remove and destroy preview mesh
	 */
	void Disconnect();

	AActor* GetActor() const { return TemporaryParentActor; }

	FTransform GetTransform() const;
	void SetTransform(const FTransform& UseTransform);

	//bool IsVisible() const;
	void SetVisible(bool bVisible);

	void ClearPreview();
	void UpdatePreview(TUniquePtr<UnrealDrive::FSplineMeshSegments>&& MeshSegments);

	const UnrealDrive::FSplineMeshSegments* GetMeshSegments() const { return MeshSegments.Get(); }

	TUniquePtr<UnrealDrive::FSplineMeshSegments> ExtractMeshSegments() { return MoveTemp(MeshSegments); }

protected:
	TUniquePtr<UnrealDrive::FSplineMeshSegments> MeshSegments;

	/** The temporary actor we create internally to own the preview mesh component */
	APreviewMeshActor* TemporaryParentActor = nullptr;

	// This function is called internally on some changes, to let the path tracer know that this mesha/actor
	// has been modified in a way that will require invalidating the current path tracing result
	void NotifyWorldPathTracedOutputInvalidated();

	void UpdatePreview();
};


