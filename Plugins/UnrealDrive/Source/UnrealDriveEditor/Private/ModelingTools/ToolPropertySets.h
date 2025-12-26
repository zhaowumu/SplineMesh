/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "ModelingTools/TriangulateRoadTool.h"
//#include "RoadMeshTools/SplineMeshOpHelpers.h"
//#include "RoadMeshTools/RoadMeshOpHelper.h"
#include "ToolPropertySets.generated.h"

/**
 * URoadSurfaceToolProperties 
 */
UCLASS()
class UNREALDRIVEEDITOR_API URoadSurfaceToolProperties 
	: public UInteractiveToolPropertySet
	, public IInteractiveToolPropertyMaterialInterface
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = DriveSurface, meta = (RebuilDriveSurface))
	bool bBuild = true;

	UPROPERTY(EditAnywhere, Category = DriveSurface, NonTransactional, meta = (GetOptions = "UnrealDrive.UnrealDrivePresetBase.GetDriveableMaterialProfiles", EditCondition = "bBuild", RebuilDriveSurface))
	TMap<FName, TObjectPtr<UMaterialInterface>> Materials;

	UPROPERTY(EditAnywhere, Category = DriveSurface, NonTransactional, meta = (GetOptions = "UnrealDrive.UnrealDrivePresetBase.GetDriveableMaterialProfiles", EditCondition = "bBuild", RebuilDriveSurface))
	FName DriveSurfaceIslandMaterial = "Default";

	UPROPERTY(EditAnywhere, Category = DriveSurface, NonTransactional, meta = (EditCondition = "bBuild", RebuilDriveSurface))
	FColor DefaultVertexColor = FColor::White;

	UPROPERTY(EditAnywhere, Category = DriveSurface, NonTransactional, meta = (EditCondition = "bBuild", RebuilDriveSurface))
	FColor EdgeVertexColor = FColor::Black;

	UPROPERTY(EditAnywhere, Category = DriveSurface, NonTransactional, meta = (EditCondition = "bBuild", RebuilDriveSurface))
	bool bComputVertexColor = true;

	UPROPERTY(EditAnywhere, Category = DriveSurface, NonTransactional, meta = (EditCondition = "bBuild && bComputVertexColor", RebuilDriveSurface))
	double VertexColorSmoothRadius = 200;

	virtual TMap<FName, TObjectPtr<UMaterialInterface>> GetMaterialsMap() const override;
};

/**
 * URoadDecalToolProperties
 */
UCLASS()
class UNREALDRIVEEDITOR_API URoadDecalToolProperties 
	: public UInteractiveToolPropertySet
	, public IInteractiveToolPropertyMaterialInterface
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = Decales, meta = (RebuildDecales))
	bool bBuild = true;

	UPROPERTY(EditAnywhere, Category = Decales, NonTransactional, meta = (GetOptions = "UnrealDrive.UnrealDrivePresetBase.GetDriveableMaterialProfiles", EditCondition = "bBuild", RebuildDecales))
	TMap<FName, TObjectPtr<UMaterialInterface>> Materials;

	// [cm]
	UPROPERTY(EditAnywhere, Category = Decales, meta = (ClampMin = 0.0, ClampMax = 100, EditCondition = "bBuild", RebuildDecales))
	double DecalOffset = 3;

	virtual TMap<FName, TObjectPtr<UMaterialInterface>> GetMaterialsMap() const override;
};

/**
 * URoadSidewalkToolProperties
 */
UCLASS()
class UNREALDRIVEEDITOR_API URoadSidewalkToolProperties
	: public UInteractiveToolPropertySet
	, public IInteractiveToolPropertyMaterialInterface
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = Sidewalks, meta = (RebuildSidewalks))
	bool bBuild = true;

	UPROPERTY(EditAnywhere, Category = Sidewalks, NonTransactional, meta = (GetOptions = "UnrealDrive.UnrealDrivePresetBase.GetSidewalkMaterialProfiles", EditCondition = "bBuild", RebuildSidewalks))
	TMap<FName, TObjectPtr<UMaterialInterface>> Materials;

	UPROPERTY(EditAnywhere, Category = Sidewalks, meta = (ClampMin = 0.0, ClampMax = 100, EditCondition = "bBuild", RebuildSidewalks))
	double SidewalkHeight = 15;

	virtual TMap<FName, TObjectPtr<UMaterialInterface>> GetMaterialsMap() const override;
};

/**
 * URoadCertbToolProperties
 */
UCLASS()
class UNREALDRIVEEDITOR_API URoadCertbToolProperties
	: public UInteractiveToolPropertySet
	, public IInteractiveToolPropertyMaterialInterface
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = Curbs, meta = (RebuildCurbs), meta = (RebuildCurbs))
	bool bBuild = true;

	UPROPERTY(EditAnywhere, Category = Curbs, NonTransactional, meta = (GetOptions = "UnrealDrive.UnrealDrivePresetBase.GetCurbProfiles", EditCondition = "bBuild", RebuildCurbs))
	TMap<FName, TObjectPtr<UMaterialInterface>> Materials;

	UPROPERTY(EditAnywhere, Category = Curbs, meta = (ClampMin = 0.0, ClampMax = 100, EditCondition = "bBuild", RebuildCurbs))
	double CurbsHeight = 15.5;

	UPROPERTY(EditAnywhere, Category = Curbs, meta = (ClampMin = 0.0, ClampMax = 100, EditCondition = "bBuild", RebuildCurbs))
	double CurbsUV0Scale = 0.002;

	virtual TMap<FName, TObjectPtr<UMaterialInterface>> GetMaterialsMap() const override;
};

/**
 * URoadMarkToolProperties
 */
UCLASS()
class UNREALDRIVEEDITOR_API URoadMarkToolProperties
	: public UInteractiveToolPropertySet
	, public IInteractiveToolPropertyMaterialInterface
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = Marks, meta = (RebuildMarks))
	bool bBuild = true;

	UPROPERTY(EditAnywhere, Category = Marks, NonTransactional, meta = (GetOptions = "UnrealDrive.UnrealDrivePresetBase.GetLaneMarkMaterialProfiles", EditCondition = "bBuild", RebuildMarks))
	TMap<FName, TObjectPtr<UMaterialInterface>> Materials;

	// [cm]
	UPROPERTY(EditAnywhere, Category = Marks, meta = (ClampMin = 0.0, ClampMax = 100, EditCondition = "bBuild", RebuildMarks))
	double MarkOffset = 3;

	virtual TMap<FName, TObjectPtr<UMaterialInterface>> GetMaterialsMap() const override;
};

/**
 * URoadAttributesToolProperties
 */
UCLASS()
class UNREALDRIVEEDITOR_API URoadAttributesToolProperties
	: public UInteractiveToolPropertySet
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = Attributes, meta = (RebuilAttributes))
	bool bBuild = true;

	/** Draw debug referance splines for  the spline meshes*/
	UPROPERTY(EditAnywhere, Category = Attributes, meta = (EditCondition = "bBuild", RebuilAttributes))
	bool bDrawRefSplines = false;
};
