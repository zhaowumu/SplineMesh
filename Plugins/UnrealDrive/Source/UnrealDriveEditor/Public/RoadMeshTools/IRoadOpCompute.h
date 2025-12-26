
/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Copyright Epic Games, Inc. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "BackgroundModelingComputeSource.h"
#include "MeshOpPreviewHelpers.h"
#include "IRoadOpCompute.generated.h"


UINTERFACE()
class UNREALDRIVEEDITOR_API URoadOpCompute: public UInterface
{
	GENERATED_BODY()
};

class UNREALDRIVEEDITOR_API IRoadOpCompute
{
	GENERATED_BODY()

public:

	virtual void ShutdownAndGenerateAssets(AActor* TargetActor, const FTransform3d& ActorToWorld) = 0;

	/**
	 * Request that the current computation be canceled and a new one started
	 */
	virtual void InvalidateResult() = 0;

	/**
	 * Cancel the active computation without returning anything. Doesn't destroy the mesh.
	 */
	virtual void CancelCompute() = 0;

	/**
	 * Set the visibility of the Preview mesh
	 */
	virtual void SetVisibility(bool bVisible) = 0;

	/**
	 * Tick the background computation and Preview update.
	 * @warning this must be called regularly for the class to function properly
	 */
	virtual void Tick(float DeltaTime) = 0;

	/**
	 * Enable/disable wireframe overlay rendering
	 */
	virtual void EnableWireframe(bool bEnable) = 0;

	/**
	* Terminate any active computation without returning anything
	*/
	virtual void Cancel() = 0;

	/**
	 * @return true if current PreviewMesh result is valid (no update actively being computed) and that mesh has at least one triangle
	 */
	virtual bool HaveValidNonEmptyResult() const = 0;

	// Stored status of last compute, mainly so that we know when we should show the "busy" material.
	virtual UE::Geometry::EBackgroundComputeTaskStatus GetLastComputeStatus() const  = 0;

	virtual bool IsRoadAttribute() const = 0;

	virtual int GetNumVertices() const = 0;

	virtual int GetNumTriangles() const = 0;

	TSet<FName> RebuildTags;
	
	//virtual void GetVertexCount() const;
	//virtual void GetTriangleCount() const;
};
