/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "CoreMinimal.h"
#include "DynamicMesh/DynamicMesh3.h"

namespace MeshUtils 
{
	using namespace UE::Geometry;

	UNREALDRIVEEDITOR_API TArray<int32> GetGroupTries(const FDynamicMesh3& Mesh, int GroupID);
	UNREALDRIVEEDITOR_API int FindMeshSections(const FDynamicMesh3& Mesh, const TArray<int32>& TargetTIDs, TArray<int32>& OutSections);
	UNREALDRIVEEDITOR_API TSet<int> GetAllGroups(const FDynamicMesh3& Mesh);
	UNREALDRIVEEDITOR_API double GetGroupArea(const FDynamicMesh3& Mesh, int GroupID);
	UNREALDRIVEEDITOR_API TSet<int> GetGroupNeighbours(const FDynamicMesh3& Mesh, int GroupID, const TArray<int32>& TIDs);
	UNREALDRIVEEDITOR_API int FindMinAreaGroupNeighbour(const FDynamicMesh3& Mesh, int GroupID, const TArray<int32>& TIDs);
	UNREALDRIVEEDITOR_API void ChangeGroup(FDynamicMesh3& Mesh, int OldGroupId, int NewGroupID);
	UNREALDRIVEEDITOR_API void MergeGroupByArea(FDynamicMesh3& Mesh, double MergeSectionsAreaThreshold);
	UNREALDRIVEEDITOR_API int SplitMeshGroupsBySections(UE::Geometry::FDynamicMesh3& Mesh);


} // MeshUtils
