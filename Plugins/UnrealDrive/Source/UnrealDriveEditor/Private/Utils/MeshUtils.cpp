/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "Utils/MeshUtils.h"
#include "Algo/MaxElement.h"

TArray<int32> MeshUtils::GetGroupTries(const FDynamicMesh3& Mesh, int GroupID)
{
	TArray<int32> TIDs;
	TIDs.SetNum(0, EAllowShrinking::No);
	for (int32 TID = 0; TID < Mesh.MaxTriangleID(); ++TID)
	{
		if (Mesh.IsTriangle(TID) && Mesh.GetTriangleGroup(TID) == GroupID)
		{
			TIDs.Add(TID);
		}
	}
	return TIDs;
}

int MeshUtils::FindMeshSections(const FDynamicMesh3& Mesh, const TArray<int32>& TargetTIDs, TArray<int32>& OutSections)
{
	int32 MaxTID = 0;
	for (int32 TID : TargetTIDs)
	{
		MaxTID = FMath::Max(MaxTID, TID);
	}

	TArray<int32> TID2Section;
	TID2Section.SetNum(MaxTID + 1);
	for (int32& It : TID2Section) It = -1;
	for (int32 TID : TargetTIDs) TID2Section[TID] = 0;

	int32 SectionID = 0;
	for (int32 ProbeTID : TargetTIDs)
	{
		if (TID2Section[ProbeTID] == 0)
		{
			++SectionID;
			TID2Section[ProbeTID] = SectionID;

			bool bWasAdded = true;
			while (bWasAdded)
			{
				bWasAdded = false;
				for (int32 TID : TargetTIDs)
				{
					if (Mesh.IsTriangle(TID) && TID2Section[TID] == SectionID)
					{
						for (int32 NeighbourTri : Mesh.GetTriNeighbourTris(TID).ABC)
						{
							if (NeighbourTri != -1 && NeighbourTri <= MaxTID && TID2Section[NeighbourTri] == 0)
							{
								TID2Section[NeighbourTri] = SectionID;
								bWasAdded = true;
							}
						}
					}
				}
			}
		}
	}


	OutSections.SetNum(TargetTIDs.Num());
	for (int i = 0; i < TargetTIDs.Num(); ++i)
	{
		OutSections[i] = TID2Section[TargetTIDs[i]] - 1;
	}

	return SectionID;
}

TSet<int> MeshUtils::GetAllGroups(const FDynamicMesh3& Mesh)
{
	TSet<int> Groups;
	for (int32 TID = 0; TID < Mesh.MaxTriangleID(); ++TID)
	{
		if (Mesh.IsTriangle(TID))
		{
			int GroupID = Mesh.GetTriangleGroup(TID);
			if (GroupID >= 0)
			{
				Groups.Add(GroupID);
			}
		}
	}
	return Groups;
}

double MeshUtils::GetGroupArea(const FDynamicMesh3& Mesh, int GroupID)
{
	double Area = 0;
	for (int32 TID = 0; TID < Mesh.MaxTriangleID(); ++TID)
	{
		if (Mesh.IsTriangle(TID) && Mesh.GetTriangleGroup(TID) == GroupID)
		{
			Area += Mesh.GetTriArea(TID);
		}
	}
	return Area;
}

TSet<int> MeshUtils::GetGroupNeighbours(const FDynamicMesh3& Mesh, int GroupID, const TArray<int32>& TIDs)
{
	TSet<int> NeighbourGroups;
	for (int32 TID : TIDs)
	{
		if (Mesh.IsTriangle(TID))
		{
			for (int NeighbourTID : Mesh.GetTriNeighbourTris(TID).ABC)
			{
				if (NeighbourTID != FDynamicMesh3::InvalidID)
				{
					int NeighbourGroupID = Mesh.GetTriangleGroup(NeighbourTID);
					if (NeighbourGroupID != GroupID)
					{
						NeighbourGroups.Add(NeighbourGroupID);
					}
				}
			}
		}
	}
	return NeighbourGroups;
}

int MeshUtils::FindMinAreaGroupNeighbour(const FDynamicMesh3& Mesh, int GroupID, const TArray<int32>& TIDs)
{
	int MinNeighbourGroupID = -1;
	double MinNeighbourGroupArea = 0;
	for (int NeighbourGroupID : GetGroupNeighbours(Mesh, GroupID, TIDs))
	{
		double Area = GetGroupArea(Mesh, NeighbourGroupID);
		if (MinNeighbourGroupID == -1 || Area < MinNeighbourGroupArea)
		{
			MinNeighbourGroupID = NeighbourGroupID;
			MinNeighbourGroupArea = Area;
		}
	}
	return MinNeighbourGroupID;
}

void MeshUtils::ChangeGroup(FDynamicMesh3& Mesh, int OldGroupId, int NewGroupID)
{
	for (int32 TID = 0; TID < Mesh.MaxTriangleID(); ++TID)
	{
		if (Mesh.IsTriangle(TID))
		{
			int GroupID = Mesh.GetTriangleGroup(TID);
			if (GroupID == OldGroupId)
			{
				Mesh.SetTriangleGroup(TID, NewGroupID);
			}
		}
	}
}

void MeshUtils::MergeGroupByArea(FDynamicMesh3& Mesh, double MergeSectionsAreaThreshold)
{
	bool bWasChanged = true;
	while (bWasChanged)
	{
		bWasChanged = false;
		TSet<int> Groups = MeshUtils::GetAllGroups(Mesh);
		for (int GroupID : Groups)
		{
			const TArray<int32> TIDs = MeshUtils::GetGroupTries(Mesh, GroupID);
			if (MeshUtils::GetGroupArea(Mesh, GroupID) < MergeSectionsAreaThreshold)
			{
				const int NeighbourGroupID = MeshUtils::FindMinAreaGroupNeighbour(Mesh, GroupID, TIDs);
				if (NeighbourGroupID >= 0)
				{
					MeshUtils::ChangeGroup(Mesh, GroupID, NeighbourGroupID);
					bWasChanged = true;
					break;
				}
			}
		}
	}
}

int MeshUtils::SplitMeshGroupsBySections(FDynamicMesh3& Mesh)
{
	TSet<int> Groups = MeshUtils::GetAllGroups(Mesh);
	if (Groups.Num())
	{
		int MaxGroupID = *Algo::MaxElement(Groups);
		for (int GroupID : Groups)
		{
			TArray<int32> TIDs = MeshUtils::GetGroupTries(Mesh, GroupID);
			TArray<int32> Sections;
			int NumSections = MeshUtils::FindMeshSections(Mesh, TIDs, Sections);
			if (NumSections > 1)
			{
				for (int i = 0; i < TIDs.Num(); ++i)
				{
					int TID = TIDs[i];
					int NewGroupID = MaxGroupID + Sections[i] + 1;
					Mesh.SetTriangleGroup(TID, NewGroupID);
				}
			}
			MaxGroupID += NumSections + 1;
		}
		return MaxGroupID;
	}
	return -1;
}
