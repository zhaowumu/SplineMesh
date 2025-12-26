/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "ModelingTools/TriangulateRoadTool.h"
#include "RoadMeshTools/SplineMeshOpHelpers.h"
#include "RoadMeshTools/RoadMeshOpHelper.h"
#include "ToolPropertySets.h"

namespace UnrealDrive 
{

/**
 * FDriveSurfaceOperatorFactory
 */
class FDriveSurfaceOperatorFactory
	: public FRoadAbstractOperatorFactory
	, public UE::Geometry::IDynamicMeshOperatorFactory
{
public:
	virtual TUniquePtr<UE::Geometry::FDynamicMeshOperator> MakeNewOperator() override;

	TWeakObjectPtr<URoadSurfaceToolProperties> Properties;
};


/**
 * FRoadDecalsOperatorFactory
 */
class FRoadDecalsOperatorFactory
	: public FRoadAbstractOperatorFactory
	, public UE::Geometry::IDynamicMeshOperatorFactory
{

public:
	virtual TUniquePtr<UE::Geometry::FDynamicMeshOperator> MakeNewOperator() override;

	TWeakObjectPtr<URoadDecalToolProperties> Properties;
};


/**
 * FRoadSidewalksOperatorFactory
 */
class FRoadSidewalksOperatorFactory
	: public FRoadAbstractOperatorFactory
	, public UE::Geometry::IDynamicMeshOperatorFactory
{
public:
	virtual TUniquePtr<UE::Geometry::FDynamicMeshOperator> MakeNewOperator() override;

	TWeakObjectPtr<URoadSidewalkToolProperties> Properties;
};

/**
 * FRoadCurbsOperatorFactory
 */

class FRoadCurbsOperatorFactory
	: public FRoadAbstractOperatorFactory
	, public UE::Geometry::IDynamicMeshOperatorFactory
{
public:
	virtual TUniquePtr<UE::Geometry::FDynamicMeshOperator> MakeNewOperator() override;

	TWeakObjectPtr<URoadCertbToolProperties> Properties;
};

/**
 * FRoadMarksOperatorFactory
 */
class FRoadMarksOperatorFactory
	: public FRoadAbstractOperatorFactory
	, public UE::Geometry::IDynamicMeshOperatorFactory
{
public:
	virtual TUniquePtr<UE::Geometry::FDynamicMeshOperator> MakeNewOperator() override;

	TWeakObjectPtr<URoadMarkToolProperties> Properties;
};

/**
 * FRoadSplineMeshOperatorFactory
 */
class FRoadSplineMeshOperatorFactory
	: public FRoadAbstractOperatorFactory
	, public UnrealDrive::ISplineMeshOperatorFactory
{
public:
	virtual TUniquePtr<UnrealDrive::FSplineMeshOperator> MakeNewOperator() override;

	TWeakObjectPtr<URoadAttributesToolProperties> Properties;
};

} // UnrealDrive