/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "RoadMeshTools/RoadLanePolygone.h"
#include "UnrealDrive.h"
#include "Utils/OpUtils.h"
#include "Utils/CurveUtils.h"
#include "ModelingTools/TriangulateRoadTool.h"
#include "UnrealDrivePreset.h"
#include "Algo/MaxElement.h"
#include "Algo/MinElement.h"
#include "Misc/Optional.h"
#include <queue>
#include <map>

#define LOCTEXT_NAMESPACE "RoadLanePolygone"

using namespace UnrealDrive;

static void RemovePointsFromBegin(TArray<FVector2D>& Vertices, double Distance)
{
	if (Vertices.Num() && Distance >= 0)
	{
		double Dt = 0;
		for (int i = 1; i < Vertices.Num(); ++i)
		{
			Dt += (Vertices[i] - Vertices[i - 1]).Length();
			if (Dt >= Distance)
			{
				Vertices.RemoveAt(0, i);
				return;
			}
		}
		Vertices.Empty();
	}
}

static void RemovePointsFromEnd(TArray<FVector2D>& Vertices, double Distance)
{
	if (Vertices.Num() && Distance > 0)
	{
		double Dt = 0;
		for (int i = Vertices.Num() - 2; i >= 0; --i)
		{
			Dt += (Vertices[i] - Vertices[i + 1]).Length();
			if (Dt >= Distance)
			{
				Vertices.RemoveAt(i + 1, Vertices.Num() - i - 1);
				return;
			}
		}
		Vertices.Empty();
	}
}


// @param Adj - Assuming graph representation as adjacency list {neighbor, weight}. See https://en.wikipedia.org/wiki/Adjacency_list
static TArray<int> Dijkstra(const TMap<int, TArray<TPair<int, double>>>& Adj, int Source, int Destination)
{
	TMap<int, double> dist;
	TMap<int, int> parent; // Store parent for path reconstruction

	for (auto& [Key, Value] : Adj)
	{
		dist.Add(Key, std::numeric_limits<double>::max());
		parent.Add(Key, -1);
	}

	std::priority_queue<std::pair<double, int>, std::vector<std::pair<double, int>>, std::greater<std::pair<double, int>>> pq;

	dist[Source] = 0.0;
	pq.push({ 0.0, Source });

	while (!pq.empty()) 
	{
		double d = pq.top().first;
		int u = pq.top().second;
		pq.pop();

		if (d > dist[u]) continue; // Already found a shorter path

		for (const auto& edge : Adj[u])
		{
			int v = edge.Key;
			double weight = edge.Value;

			if (dist[u] + weight < dist[v]) 
			{
				dist[v] = dist[u] + weight;
				parent[v] = u; // Update parent
				pq.push({ dist[v], v });
			}
		}
	}

	// Reconstruct the path
	TArray<int> Path;
	int currentNode = Destination;
	while (currentNode != -1 && currentNode != Source) // Stop if no path or reached source
	{ 
		Path.Insert(currentNode, 0); // Add to front for correct order
		currentNode = parent[currentNode];
	}
	if (currentNode == Source)  // If source was reached
	{
		Path.Insert(Source, 0);
	}
	else 
	{
		// No path found (destination unreachable from source)
		return {}; // Return empty path
	}

	return Path;
}

static bool FindPolyline(const UnrealDrive::FDynamicGraph2d& Graph, const FLineInfo& LineInfo, TArray<int>& VIDs)
{
	if (LineInfo.VID_B == -1 && Graph.IsVertex(LineInfo.VID_A))
	{
		VIDs.Add(LineInfo.VID_A);
		return true;
	}

	TMap<int, TArray<TPair<int, double>>> Adj;
	for (int EID : Graph.EdgeIndices())
	{
		if (Graph.HasPolylineID(EID, LineInfo.PID))
		{
			const auto& Edge = Graph.GetEdgeRef(EID);
			double Len = Graph.GetEdgeSegment(EID).Length();
			Adj.FindOrAdd(Edge.A).Add({ Edge.B, Len });
			Adj.FindOrAdd(Edge.B).Add({ Edge.A, Len });
		}
	}

	if (!Adj.Contains(LineInfo.VID_A))
	{
		return false;
	}

	if (!Adj.Contains(LineInfo.VID_B))
	{
		return false;
	}

	if (LineInfo.IsLoop())
	{
		auto& NodeA = Adj[LineInfo.VID_A];
		for (auto& Edge: NodeA)
		{
			auto AdjCpy = Adj;
			AdjCpy[LineInfo.VID_A].RemoveAll([&Edge](const TPair<int, double>& It)
			{
				return It.Key == Edge.Key;
			});
			AdjCpy[Edge.Key].RemoveAll([&LineInfo](const TPair<int, double>& It)
			{
				return It.Key == LineInfo.VID_A;
			});
			VIDs = Dijkstra(AdjCpy, LineInfo.VID_A, Edge.Key);
			if (VIDs.Num() > 0)
			{
				VIDs.Add(LineInfo.VID_A);
				return true;
			}
		}
	}
	else
	{
		VIDs = Dijkstra(Adj, LineInfo.VID_A, LineInfo.VID_B);
	}

	return VIDs.Num() > 0;
}

// If the polygon is not looped, then it simply returns Info.Pos.SOffset,
// otherwise it is necessary to determine the SOffset on the seam (0 or SplineLength)
static double GetSOffset(const FRoadPolygoneBase::FTriInfo& TriInfo, int TID, int VID)
{
	auto& VertexInfo = TriInfo.GetVertexInfo(VID);

	if (int32(VertexInfo.Flags & ERoadVertexInfoFlags::LoopSeam))
	{
		auto* Poly = static_cast<const FRoadLanePolygone*>(VertexInfo.Poly);

		double SplineLength = Poly->GetRoadSplineCache().SplineCurves.GetSplineLength();

		check(VertexInfo.Poly);
		auto& BasOpData = VertexInfo.Poly->Owner;

		const FVector ForwardVector = VertexInfo.Pos.Quat.GetForwardVector();
		auto& T = BasOpData.Triangles[TID];

		const FVector& V_A = BasOpData.Vertices3d[T.A].Vertex;
		const FVector& V_B = BasOpData.Vertices3d[T.B].Vertex;
		const FVector& V_C = BasOpData.Vertices3d[T.C].Vertex;

		bool bIsSeam_A = bool(TriInfo.A->Flags & ERoadVertexInfoFlags::LoopSeam);
		bool bIsSeam_B = bool(TriInfo.B->Flags & ERoadVertexInfoFlags::LoopSeam);
		bool bIsSeam_C = bool(TriInfo.C->Flags & ERoadVertexInfoFlags::LoopSeam);

		if (bIsSeam_A && bIsSeam_B && bIsSeam_C)
		{
			ensure(false);
			return VertexInfo.Pos.SOffset;
		}

		TOptional<FVector> V1;
		TOptional<FVector> V2;

		if (VertexInfo.VID == T.A)
		{
			if (!bIsSeam_B)
			{
				V1 = (V_B - V_A).GetSafeNormal();
			}
			if (!bIsSeam_C)
			{
				V2 = (V_C - V_A).GetSafeNormal();
			}
		}
		else if (VertexInfo.VID == T.B)
		{
			if (!bIsSeam_A)
			{
				V1 = (V_A - V_B).GetSafeNormal();
			}
			if (!bIsSeam_C)
			{
				V2 = (V_C - V_B).GetSafeNormal();
			}
		}
		else if (VertexInfo.VID == T.C)
		{
			if (!bIsSeam_A)
			{
				V1 = (V_A - V_C).GetSafeNormal();
			}
			if (!bIsSeam_B)
			{
				V2 = (V_B - V_C).GetSafeNormal();
			}
		}
		else
		{
			ensure(false);
			return VertexInfo.Pos.SOffset;
		}

		if (V1.IsSet() && V2.IsSet())
		{
			if (ForwardVector.Dot(*V1) < 0 && ForwardVector.Dot(*V2) < 0)
			{
				return SplineLength;
			}
			else
			{
				return VertexInfo.Pos.SOffset;
			}
		}
		else if (V1.IsSet())
		{
			if (ForwardVector.Dot(*V1) < 0)
			{
				return SplineLength;
			}
			else
			{
				return VertexInfo.Pos.SOffset;
			}
		}
		else if (V2.IsSet())
		{
			if (ForwardVector.Dot(*V2) < 0)
			{
				return SplineLength;
			}
			else
			{
				return VertexInfo.Pos.SOffset;
			}
		}
		else
		{
			return VertexInfo.Pos.SOffset;
		}
	}
	else
	{
		return VertexInfo.Pos.SOffset;
	}
}


// @param adj - Assuming graph representation as adjacency list {neighbor, weight}. See https://en.wikipedia.org/wiki/Adjacency_list
/*
static std::vector<int> Dijkstra(const std::map<int, std::vector<std::pair<int, double>>>& adj, int source, int destination)
{
	std::map<int, double> dist;
	std::map<int, int> parent; // Store parent for path reconstruction

	for (auto& it : adj)
	{
		dist[it.first] = std::numeric_limits<double>::max();
		parent[it.first] = -1;
	}

	std::priority_queue<std::pair<double, int>, std::vector<std::pair<double, int>>, std::greater<std::pair<double, int>>> pq;

	dist[source] = 0.0;
	pq.push({ 0.0, source });

	while (!pq.empty())
	{
		double d = pq.top().first;
		int u = pq.top().second;
		pq.pop();

		if (d > dist[u]) continue; // Already found a shorter path

		for (const auto& edge : adj.at(u))
		{
			int v = edge.first;
			double weight = edge.second;

			if (dist[u] + weight < dist[v])
			{
				dist[v] = dist[u] + weight;
				parent[v] = u; // Update parent
				pq.push({ dist[v], v });
			}
		}
	}

	// Reconstruct the path
	std::vector<int> path;
	int currentNode = destination;
	while (currentNode != -1 && currentNode != source) // Stop if no path or reached source
	{
		path.insert(path.begin(), currentNode); // Add to front for correct order
		currentNode = parent[currentNode];
	}
	if (currentNode == source)  // If source was reached
	{
		path.insert(path.begin(), source);
	}
	else
	{
		// No path found (destination unreachable from source)
		return {}; // Return empty path
	}

	return path;
}

static bool FindPolyline(const UnrealDrive::FDynamicGraph2d& Graph, const FLineInfo& LineInfo, TArray<int>& VIDs)
{
	if (LineInfo.VID_B == -1 && Graph.IsVertex(LineInfo.VID_A))
	{
		VIDs.Add(LineInfo.VID_A);
		return true;
	}

	std::map<int, std::vector<std::pair<int, double>>> Adj;
	for (int EID : Graph.EdgeIndices())
	{
		if (Graph.HasPolylineID(EID, LineInfo.PID))
		{
			const auto& Edge = Graph.GetEdgeRef(EID);
			double Len = Graph.GetEdgeSegment(EID).Length();
			Adj[Edge.A].push_back({ Edge.B, Len });
			Adj[Edge.B].push_back({ Edge.A, Len });
		}
	}

	if (!Adj.contains(LineInfo.VID_A))
	{
		return false;
	}

	if (!Adj.contains(LineInfo.VID_B))
	{
		return false;
	}

	std::vector<int> Path;
	if (LineInfo.IsLoop())
	{
		auto& NodeA = Adj[LineInfo.VID_A];
		for (auto& Edge: NodeA)
		{
			auto AdjCpy = Adj;
			std::erase_if(AdjCpy[LineInfo.VID_A], [&Edge](const std::pair<int, double>& It)
			{
				return It.first == Edge.first;
			});
			std::erase_if(AdjCpy[Edge.first],[&LineInfo](const std::pair<int, double>& It)
			{
				return It.first == LineInfo.VID_A;
			});
			Path = Dijkstra(AdjCpy, LineInfo.VID_A, Edge.first);
			if (Path.size() > 0)
			{
				break;
			}
		}
	}
	else
	{
		Path = Dijkstra(Adj, LineInfo.VID_A, LineInfo.VID_B);
	}

	if (Path.size())
	{
		VIDs = TArray<int>(&Path[0], Path.size());
		return true;
	}
	else
	{
		return false;
	}
}
*/


/*
static bool FindPolyline_2(const UnrealDrive::FDynamicGraph2d& Graph, const FLineInfo& LineInfo, TArray<int>& VIDs)
{
	if (LineInfo.VID_B == -1 && Graph.IsVertex(LineInfo.VID_A))
	{
		VIDs.Add(LineInfo.VID_A);
		return true;
	}

	if (LineInfo.IsLoop())
	{
		return false; // Not supported
	}

	TMap<int, TSet<int>> Nodes;
	for (int EID : Graph.EdgeIndices())
	{
		if (Graph.HasPolylineID(EID, LineInfo.PID))
		{
			const auto& Edge = Graph.GetEdgeRef(EID);
			Nodes.FindOrAdd(Edge.A).Add(Edge.B);
			Nodes.FindOrAdd(Edge.B).Add(Edge.A);
		}
	}
		
	if (!Nodes.Contains(LineInfo.VID_A))
	{
		return false;
	}

	if (!Nodes.Contains(LineInfo.VID_B))
	{
		return false;
	}

	std::queue<TArray<int>> Queue;
	TMap<int, bool> Visited;
	for (auto& It : Nodes)
	{
		Visited.Add(It.Key, false);
	}

	Visited[LineInfo.VID_A] = true;
	Queue.push({ LineInfo.VID_A });
	VIDs.Empty();


	while (!Queue.empty())
	{
		TArray<int>& CurrPath = Queue.front();

		if (CurrPath.Last() == LineInfo.VID_B)
		{
			VIDs = CurrPath;
			return true;
		}

		auto& Node = Nodes[CurrPath.Last()];
		bool bWasPushed = false;
		if (Node.Num() == 1)
		{
			int VID = *Node.begin();
			if (!Visited[VID])
			{
				Visited[VID] = true;
				CurrPath.Add(VID);
				bWasPushed = true;
			}
		}
		else if(Node.Num() > 1)
		{
			for (int VID : Node)
			{
				if (!Visited[VID])
				{
					TArray<int> NewPath = CurrPath;
					NewPath.Add(VID);
					Visited[VID] = true;
					Queue.push(NewPath);
					bWasPushed = true;
				}
			}
		}
		if(!bWasPushed)
		{
			Queue.pop();
		}
	}

	return false;
}
*/

/*
static FRoadPosition FindNearestAtKay(const URoadSplineComponent* Spline, double Key, const FVector& TargetWorldLocation)
{
	const FTransform KeyTransform = Spline->GetTransformAtSplineInputKey(Key, ESplineCoordinateSpace::World);
	const FVector TargetLocalLocation = KeyTransform.InverseTransformPositionNoScale(TargetWorldLocation);

	FRoadPosition Ret;
	Ret.SOffset = Spline->GetDistanceAlongSplineAtSplineInputKey(Key);
	Ret.ROffset = TargetLocalLocation.Y;
	Ret.Quat = KeyTransform.GetRotation();
	Ret.Location = KeyTransform.TransformPosition(FVector(0, TargetLocalLocation.Y, 0.0));
	return Ret;
}

static FRoadPosition FindNearestAtDistance(const URoadSplineComponent* Spline, double SOffest, const FVector& TargetWorldLocation)
{
	const FTransform KeyTransform = Spline->GetTransformAtDistanceAlongSpline(SOffest, ESplineCoordinateSpace::World);
	const FVector TargetLocalLocation = KeyTransform.InverseTransformPositionNoScale(TargetWorldLocation);

	FRoadPosition Ret;
	Ret.SOffset = SOffest;
	Ret.ROffset = TargetLocalLocation.Y;
	Ret.Quat = KeyTransform.GetRotation();
	Ret.Location = KeyTransform.TransformPosition(FVector(0, TargetLocalLocation.Y, 0.0));
	return Ret;
}
*/



// ---------------------------------------------------------------------------------------------------------------------------------
const FRoadSplineCache& FRoadPolygoneBase::GetRoadSplineCache() const
{
	return Owner.RoadSplinesCache[SplineIndex];
}

FRoadSplineCache& FRoadPolygoneBase::GetRoadSplineCache()
{
	return Owner.RoadSplinesCache[SplineIndex];
}

FRoadPolygoneBase::FTriInfo FRoadPolygoneBase::FindTri(int TID) const
{
	auto& T = Owner.Triangles[TID];
	auto* InfoA = Owner.Vertices3d[T.A].Infos.FindByPredicate([this](const FRoadVertexInfo& It) { return It.Poly == this; });
	auto* InfoB = Owner.Vertices3d[T.B].Infos.FindByPredicate([this](const FRoadVertexInfo& It) { return It.Poly == this; });
	auto* InfoC = Owner.Vertices3d[T.C].Infos.FindByPredicate([this](const FRoadVertexInfo& It) { return It.Poly == this; });
	return { InfoA , InfoB, InfoC };
}

double FRoadPolygoneBase::GetPriority() const
{
	static auto GetMaterialPriority = []<typename TRoadLaneDriving>(const TRoadLaneDriving & Lane)
	{
		auto Profiles = UUnrealDrivePresetBase::GetAllProfiles(&UUnrealDrivePresetBase::DriveableMaterialProfiles);
		if (auto* Found = Profiles.Find(Lane.MaterialProfile))
		{
			return Found->Priority;
		}
		return 0;
	};

	int ProfilePriority = 0;

	if (auto* RoadLaneDriving = GetLaneInstance().GetPtr<FRoadLaneDriving>())
	{
		ProfilePriority = GetMaterialPriority(*RoadLaneDriving);
	}
	else if (auto* RoadLaneSidewalk = GetLaneInstance().GetPtr<FRoadLaneSidewalk>())
	{
		ProfilePriority = GetMaterialPriority(*RoadLaneSidewalk);
	}

	return ProfilePriority + double(GetRoadSplineCache().MaterialPriority) / 1000.0 + double(Owner.RoadSplinesCache.Num() - SplineIndex - 1) / 1000000.0;
}

// ---------------------------------------------------------------------------------------------------------------------------------

FRoadLanePolygone::FRoadLanePolygone(FRoadBaseOperatorData& Owner, int SplineIndex, int SectionIndex, int LaneIndex, double MaxSquareDistanceFromSpline, double MaxSquareDistanceFromCap, double MinSegmentLength)
	: FRoadPolygoneBase(Owner, SplineIndex)
	, SectionIndex(SectionIndex)
	, LaneIndex(LaneIndex)
	, bIsLoop(false)
{

	ResultInfo = { EGeometryResultType::InProgress };

	const auto& RoadSplineCash = GetRoadSplineCache();
	const auto& Section = GetSection();
	const FRoadLaneSidewalk* LaneSidewalk = GetLaneInstance().GetPtr<FRoadLaneSidewalk>();

	auto ConvertSplineToPolylin = [&](double Aplha)
	{
		TArray<FRoadPosition> Points;

		if (LaneIndex != LANE_INDEX_NONE)
		{
			auto& Lane = Section.GetLaneByIndex(LaneIndex);
			for (int i = Lane.GetStartSectionIndex(); i <= Lane.GetEndSectionIndex(); ++i)
			{
				TArray<FRoadPosition> TmpPoints;
				if (RoadSplineCash.ConvertSplineToPolyline_InDistanceRange2(
					SectionIndex,
					LaneIndex,
					[Aplha](double S) { return Aplha; },
					ESplineCoordinateSpace::World,
					MaxSquareDistanceFromSpline,
					MinSegmentLength,
					RoadSplineCash.RoadLayout.Sections[i].SOffset,
					RoadSplineCash.RoadLayout.Sections[i].SOffsetEnd_Cashed,
					TmpPoints,
					true))
				{
					if (Points.Num())
					{
						Points.RemoveAt(Points.Num() - 1);
					}
					Points.Append(MoveTemp(TmpPoints));
				}
			}
		}
		else
		{
			RoadSplineCash.ConvertSplineToPolyline_InDistanceRange2(
				SectionIndex,
				LaneIndex,
				[Aplha](double S) { return Aplha; },
				ESplineCoordinateSpace::World,
				MaxSquareDistanceFromSpline,
				MinSegmentLength,
				GetSection().SOffset,
				GetSection().SOffsetEnd_Cashed,
				Points,
				true);

		}

		TArray<FVector2D> Ret;

		if (Points.Num() == 0)
		{
			return Ret;
		}

		Ret.Reserve(Points.Num());
		for (auto& It : Points)
		{
			Ret.Add(FVector2D{ It.Location });
		}
		OpUtils::RemovedPolylineSelfIntersection(Ret);
		return Ret;
	};

	TArray<FVector2D> InsideLineVertices2D = ConvertSplineToPolylin(0.0);
	TArray<FVector2D> OutsideLineVertices2D;
	TArray<FVector2D> EndCapVertices2D;
	TArray<FVector2D> BeginCapVertices2D;

	if (LaneIndex != LANE_INDEX_NONE)
	{
		OutsideLineVertices2D = ConvertSplineToPolylin(1.0);
	}

	if (LaneSidewalk && LaneIndex != LANE_INDEX_NONE)
	{
		const FRoadLane& Lane = GetLane();

		if (LaneSidewalk->bEndCurb && LaneSidewalk->EndCapCurve.GetRichCurveConst()->GetNumKeys())
		{
			double LaneWidth = Lane.Width.Eval(Lane.GetEndOffset());

			TArray<float> Values;
			TArray<float> Times;
			CurveUtils::CurveToPolyline(*LaneSidewalk->EndCapCurve.GetRichCurveConst(), 0.0, 1.0, MaxSquareDistanceFromCap / (LaneWidth * LaneWidth), 0.0001, 200, Values, Times);
			double MaxValue = *Algo::MaxElement(Values);

			for (int i = 0; i < Values.Num(); ++i)
			{
				EndCapVertices2D.Add(FVector2D{ RoadSplineCash.GetRoadPosition(SectionIndex, LaneIndex, Times[i], Lane.GetEndOffset() - (MaxValue - Values[i]) * LaneWidth, ESplineCoordinateSpace::World).Location});
			}
			if (MaxValue > 0)
			{
				RemovePointsFromEnd(InsideLineVertices2D, MaxValue * LaneWidth);
				RemovePointsFromEnd(OutsideLineVertices2D, MaxValue * LaneWidth);

				InsideLineVertices2D.Add(EndCapVertices2D[0]);
				OutsideLineVertices2D.Add(EndCapVertices2D.Last());
			}
		}
		if (LaneSidewalk->bBeginCurb && LaneSidewalk->BeginCapCurve.GetRichCurveConst()->GetNumKeys())
		{
			double LaneWidth = Lane.Width.Eval(Section.SOffset);
	
			TArray<float> Values;
			TArray<float> Times;
			CurveUtils::CurveToPolyline(*LaneSidewalk->BeginCapCurve.GetRichCurveConst(), 0.0, 1.0, MaxSquareDistanceFromCap / (LaneWidth * LaneWidth), 0.0001, 200, Values, Times);
			double MaxValue = *Algo::MaxElement(Values);

			for (int i = 0; i < Values.Num(); ++i)
			{
				BeginCapVertices2D.Add(FVector2D{ RoadSplineCash.GetRoadPosition(SectionIndex, LaneIndex, Times[i], Section.SOffset + (MaxValue - Values[i]) * LaneWidth, ESplineCoordinateSpace::World).Location });
			}
			if (MaxValue > 0)
			{
				RemovePointsFromBegin(InsideLineVertices2D, MaxValue * LaneWidth);
				RemovePointsFromBegin(OutsideLineVertices2D, MaxValue * LaneWidth);

				InsideLineVertices2D.Insert(BeginCapVertices2D[0], 0);
				OutsideLineVertices2D.Insert(BeginCapVertices2D.Last(), 0);
			}
		}
	}

	if (LaneIndex != LANE_INDEX_NONE)
	{
		if (BeginCapVertices2D.Num() < 2)
		{
			BeginCapVertices2D.Empty();
			BeginCapVertices2D.Add(InsideLineVertices2D[0]);
			BeginCapVertices2D.Add(OutsideLineVertices2D[0]);
		}

		if (EndCapVertices2D.Num() < 2)
		{
			EndCapVertices2D.Empty();
			EndCapVertices2D.Add(InsideLineVertices2D.Last());
			EndCapVertices2D.Add(OutsideLineVertices2D.Last());
		}
	}

	int GID = 0; 

	if (LaneIndex == 0)
	{
		GID = GUIFlags::CenterLine;
	}
	else if (GetLaneInstance().GetPtr<FRoadLaneDriving>() != nullptr)
	{
		GID = GUIFlags::DrivingSurface;
	}
	else if (LaneSidewalk)
	{
		if (LaneSidewalk->bIsSoftBorder)
		{
			GID = GUIFlags::SidewalksSoft;
			
		}
		else
		{
			GID = GUIFlags::SidewalksHard;
		}
	}

	auto AddToArrangement = [this](const TArray<FVector2D>& Points, int GID)
	{
		if(Points.Num() <= 1)
		{
			return FLineInfo{};
		}

		FLineInfo Info;
		Info.PID = this->Owner.Arrangement->Graph.AllocateEdgePolylines();
		for (int i = 0; i < Points.Num() - 1; ++i)
		{
			this->Owner.Arrangement->Insert( Points[i], Points[i + 1], GID, Info.PID);
		}

		Info.VID_A = this->Owner.Arrangement->FindExistingVertex( Points[0] );
		Info.VID_B = this->Owner.Arrangement->FindExistingVertex( Points.Last());

		if (Info.VID_A == Info.VID_B && Points.Num() <= 2)
		{
			Info.VID_B = -1;
		}

		return Info;
	};

	InsideLineInfo = AddToArrangement(InsideLineVertices2D, GID);
	if (!InsideLineInfo.IsValid())
	{
		ResultInfo.SetFailed(FText::Format(LOCTEXT("RoadLanePolygone_InsideLineFaild", "{0}: InsideLineInfo faild"), GetDescription()));
		return;
	}

	bIsLoop = InsideLineInfo.IsLoop();

	if (LaneIndex != LANE_INDEX_NONE)
	{
		OutsideLineInfo = AddToArrangement(OutsideLineVertices2D, GID);
		if (!OutsideLineInfo.IsValid())
		{
			ResultInfo.SetFailed(FText::Format(LOCTEXT("RoadLanePolygone_OutsideLineFaild", "{0}: OutsideLineInfo faild "), GetDescription()));
			return;
		}

		if (InsideLineInfo.IsLoop() ^ OutsideLineInfo.IsLoop())
		{
			// There should not be situations when only one of the lines is a loop.
			ResultInfo.SetFailed(FText::Format(LOCTEXT("RoadLanePolygone_LoopFaild", "{0}: Wrong loop"), GetDescription()));
			return;
		}

		EndCapInfo = AddToArrangement(EndCapVertices2D, GID);
		BeginCapInfo = AddToArrangement(BeginCapVertices2D, GID);

		if (!BeginCapInfo.IsValid())
		{
			ResultInfo.SetFailed(FText::Format(LOCTEXT("RoadLanePolygone_BeginCapInfo", "{0}: BeginCapInfo line info faild "), GetDescription()));
			return;
		}

		if (!EndCapInfo.IsValid())
		{
			ResultInfo.SetFailed(FText::Format(LOCTEXT("RoadLanePolygone_EndCapInfo", "{0}: EndCapInfo line info faild "), GetDescription()));
			return;
		}
	}
	

	ResultInfo.SetSuccess();
}


const FRoadLaneSection& FRoadLanePolygone::GetSection() const
{
	return GetRoadSplineCache().RoadLayout.Sections[SectionIndex];
}

const FRoadLane& FRoadLanePolygone::GetLane() const
{
	check(LaneIndex != LANE_INDEX_NONE);
	if (LaneIndex > 0)
	{
		return GetSection().Right[LaneIndex - 1];
	}
	else
	{
		return GetSection().Left[-LaneIndex - 1];
	}
}

const TMap<FName, FRoadLaneAttribute>& FRoadLanePolygone::GetLaneAttributes() const
{
	if (LaneIndex != LANE_INDEX_NONE)
	{
		return GetLane().Attributes;
	}
	else
	{
		return GetSection().Attributes;
	}
}

double FRoadLanePolygone::GetStartOffset() const
{
	if (LaneIndex != LANE_INDEX_NONE)
	{
		return GetLane().GetStartOffset();
	}
	else
	{
		return GetSection().SOffset;
	}
}

double FRoadLanePolygone::GetEndOffset() const
{
	if (LaneIndex != LANE_INDEX_NONE)
	{
		return GetLane().GetEndOffset();
	}
	else
	{
		return GetSection().SOffsetEnd_Cashed;
	}
}

bool FRoadLanePolygone::CompleteArrangement()
{
	if (ResultInfo.HasFailed())
	{
		return false;
	}

	if (!ProcessPolyline(InsideLineInfo, InsideLineVertices, ERoadVertexInfoFlags::Inside))
	{
		ResultInfo.SetFailed(FText::Format(LOCTEXT("RoadLanePolygone_InsideLineNotFound", "{0}: Inside line not found after arrangement"), GetDescription()));
		return false;
	}

	if (LaneIndex != LANE_INDEX_NONE)
	{
		if (!ProcessPolyline(OutsideLineInfo, OutsideLineVertices, ERoadVertexInfoFlags::Outside))
		{
			ResultInfo.SetFailed(FText::Format(LOCTEXT("RoadLanePolygone_OutsideLineNotFound", "{0}: Outside line not found after arrangement"), GetDescription()));
			return false;
		}

		if (!bIsLoop)
		{
			if (!ProcessPolyline(BeginCapInfo, BeginCapVertices, ERoadVertexInfoFlags::BeginCap))
			{
				ResultInfo.SetFailed(FText::Format(LOCTEXT("RoadLanePolygone_BeginCapeNotFound", "{0}: BeginCap line not found after arrangement"), GetDescription()));
				return false;
			}

			if (!ProcessPolyline(EndCapInfo, EndCapVertices, ERoadVertexInfoFlags::EndCap))
			{
				ResultInfo.SetFailed(FText::Format(LOCTEXT("RoadLanePolygone_EndCapLineNotFound", "{0}: EndCap line not found after arrangement"), GetDescription()));
				return false;
			}
		}
		else
		{
			if (!ProcessPolyline(BeginCapInfo, BeginCapVertices, ERoadVertexInfoFlags::LoopSeam))
			{
				ResultInfo.SetFailed(FText::Format(LOCTEXT("RoadLanePolygone_LoopSeamNotFound", "{0}: LoopSeam line not found after arrangement"), GetDescription()));
				return false;
			}
		}
	}

	if (LaneIndex == LANE_INDEX_NONE)
	{
		for (int i = 0; i < InsideLineVertices.Num() - 1; ++i)
		{
			Boundary.Add({ InsideLineVertices[i], InsideLineVertices[i + 1] });
		}
	}
	else if (bIsLoop)
	{
		FAxisAlignedBox2d InsideBound;
		for (int i = 0; i < InsideLineVertices.Num() - 1; ++i)
		{
			auto& It = Owner.Vertices3d[InsideLineVertices[i]].Infos.Last();
			InsideBound.Contain(FVector2D{ It.Pos.Location });
		}
		FAxisAlignedBox2d OutsideBound;
		for (int i = 0; i < OutsideLineVertices.Num() - 1; ++i)
		{
			auto& It = Owner.Vertices3d[OutsideLineVertices[i]].Infos.Last();
			OutsideBound.Contain(FVector2D{ It.Pos.Location });
		}

		auto AddBoundaries = [this](const TArray<int> & InBoundary, const TArray<int>& InHole)
		{
			for (int i = 0; i < InBoundary.Num() - 1; ++i)
			{
				Boundary.Add({ InBoundary[i], InBoundary[i + 1] });
			}
			Holse.Add({});
			for (int i = 0; i < InHole.Num() - 1; ++i)
			{
				Holse[0].Add({ InHole[i], InHole[i + 1]});
			}
			if (InBoundary.Num())
			{
				SplineBounds.Contain(FVector2D{
					GetRoadSplineCache().SplineCurves.GetSplineLength(),
					Owner.Vertices3d[InBoundary[0]].Infos.Last().Pos.ROffset
				});
			}
		};

		if (InsideBound.Area() > OutsideBound.Area())
		{
			AddBoundaries(InsideLineVertices, OutsideLineVertices);
		}
		else 
		{
			AddBoundaries(OutsideLineVertices, InsideLineVertices);
		}
	}
	else
	{
		for (int i = 0; i < InsideLineVertices.Num() - 1; ++i)
		{
			Boundary.Add({ InsideLineVertices[i], InsideLineVertices[i + 1] });
		}
		for (int i = 0; i < EndCapVertices.Num() - 1; ++i)
		{
			Boundary.Add({ EndCapVertices[i], EndCapVertices[i + 1] });
		}
		for (int i = OutsideLineVertices.Num() - 1; i >= 1; --i)
		{
			Boundary.Add({ OutsideLineVertices[i], OutsideLineVertices[i - 1] });
		}
		for (int i = BeginCapVertices.Num() - 1; i >= 1; --i)
		{
			Boundary.Add({ BeginCapVertices[i], BeginCapVertices[i - 1] });
		}
	}

	auto MakePoly = [this](TArray<FIndex2i>& InBoundary)
	{
		TArray<FVector2d> Vertex2d;
		{
			const auto& FirstInfo = Owner.Vertices3d[InBoundary[0].A].Infos.Last();
			Vertex2d.Add(FVector2D{ FirstInfo.Pos.Location });
			SplineBounds.Contain(FVector2D{ FirstInfo.Pos.SOffset, FirstInfo.Pos.ROffset });
		}
		for (const auto& It : InBoundary)
		{
			const auto& Info = Owner.Vertices3d[It.B].Infos.Last();
			Vertex2d.Add(FVector2D{ Info.Pos.Location });
			SplineBounds.Contain(FVector2D{ Info.Pos.SOffset, Info.Pos.ROffset });
		}
		auto Poly = UE::Geometry::FPolygon2d(Vertex2d);

		if (!Poly.IsClockwise())
		{
			Algo::Reverse(InBoundary);
			for (auto& It : InBoundary)
			{
				It = FIndex2i{ It.B, It.A };
			}
		}

		return Poly;
	};


	Poly2d = MakePoly(Boundary);

	if (Holse.Num())
	{
		MakePoly(Holse[0]);
	}

	Bounds = FAxisAlignedBox2d(Poly2d.GetVertices());
	GetRoadSplineCache().SplineBounds.Contain(SplineBounds);

	return true;
}

bool FRoadLanePolygone::ProcessPolyline(const FLineInfo& LineInfo, TArray<int>& VIDs, ERoadVertexInfoFlags Flags)
{
	if (!FindPolyline(Owner.Arrangement->Graph, LineInfo, VIDs))
	{
		return false;
	}
	
	for (int i = 0; i < VIDs.Num(); ++i)
	{
		AddVertexInfo(VIDs[i], nullptr, Flags);
	}

	return true;
};

void FRoadLanePolygone::AddVertexInfo(int VID, const FAxisAlignedBox2d* RoadSplineBounds, ERoadVertexInfoFlags Flags) const
{
	check(Owner.Arrangement->Graph.IsVertex(VID));

	if (auto* FoundVertex = Owner.Vertices3d[VID].Infos.FindByPredicate([this](const FRoadVertexInfo& Info) { return Info.Poly == this; }))
	{
		FoundVertex->Alpha1 = RoadSplineBounds ? (FoundVertex->Pos.ROffset - RoadSplineBounds->Max.Y) / (RoadSplineBounds->Max.Y - RoadSplineBounds->Min.Y) : 0.0; // Alpha1 = [0..1]
		FoundVertex->Flags = FoundVertex->Flags | Flags;

		return;
	}

	const auto& RoadSplineCash = GetRoadSplineCache();

	const auto V2d = Owner.Arrangement->Graph.GetVertex(VID);
	const auto Pos = RoadSplineCash.UpRayIntersection(V2d);
	const double ROffset1 = GetSection().EvalLaneROffset( LaneIndex, Pos.SOffset, 0.0);
	const double ROffset2 = GetSection().EvalLaneROffset( LaneIndex, Pos.SOffset, 1.0);

	auto& VertexInfo = Owner.Vertices3d[VID].Infos.Add_GetRef({});
	VertexInfo.Poly = this;
	VertexInfo.Pos = Pos;
	VertexInfo.Pos.Location.X = V2d.X;
	VertexInfo.Pos.Location.Y = V2d.Y;
	VertexInfo.Alpha0 = (Pos.ROffset - ROffset1) / (ROffset2 - ROffset1);  // Alpha0 = [0..1]
	VertexInfo.Alpha1 = RoadSplineBounds ? (VertexInfo.Pos.ROffset - RoadSplineBounds->Max.Y) / (RoadSplineBounds->Max.Y - RoadSplineBounds->Min.Y) : 0.0; // Alpha1 = [0..1]
	VertexInfo.Alpha2 = (Pos.ROffset - ROffset1); // Alpha0 = [0..width]
	VertexInfo.VID = VID;
	VertexInfo.Flags = Flags;

	if (const FRoadLaneDriving* LaneDriving = GetLaneInstance().GetPtr<FRoadLaneDriving>())
	{
		if (LaneDriving && LaneDriving->bInvertUV0)
		{
			VertexInfo.Alpha0 = 1.0 - VertexInfo.Alpha0;
		}
	}
}

const TInstancedStruct<FRoadLaneInstance>& FRoadLanePolygone::GetLaneInstance() const
{ 
	if (LaneIndex != LANE_INDEX_NONE)
	{
		return GetLane().LaneInstance;
	}
	else
	{
		static const TInstancedStruct<FRoadLaneInstance> Dummy;
		return Dummy;
	}
}

FText FRoadLanePolygone::GetDescription() const
{
	return FText::Format(LOCTEXT("RoadLanePolygone_GetDescription", "RoadLanePoly (ComponentName: \"{0}\", SectionIndex: {1}, LaneIndex: {2})"), 
		FText::FromString(GetRoadSplineCache().OriginSpline->GetName()), 
		FText::AsNumber(SectionIndex), 
		FText::AsNumber(LaneIndex)
	);
}

bool FRoadLanePolygone::SetUVLayers(FDynamicMesh3& Mesh, int TID, double UV0ScaleFactor, double UV1ScaleFactor, double UV2ScaleFactor) const
{
	check(Mesh.Attributes()->NumUVLayers() >= 2);

	double SplineLength = GetRoadSplineCache().SplineBounds.Max.X - GetRoadSplineCache().SplineBounds.Min.X;
	double UV0ScaleFactorAligned = double(FMath::RoundToInt(SplineLength * UV0ScaleFactor)) / SplineLength;
	double UV1ScaleFactorAligned = double(FMath::RoundToInt(SplineLength * UV1ScaleFactor)) / SplineLength;
	double UV2ScaleFactorAligned = double(FMath::RoundToInt(SplineLength * UV2ScaleFactor)) / SplineLength;

	const double Alpha1Mul = GetRoadSplineCache().SplineBounds.Extents().Y * UV1ScaleFactorAligned * 2.0;

	auto TriInfo = FindTri(TID);

	if (ensure(TriInfo.IsValid()))
	{
		const double SOffsetA = GetSOffset(TriInfo, TID, TriInfo.A->VID);
		const double SOffsetB = GetSOffset(TriInfo, TID, TriInfo.B->VID);
		const double SOffsetC = GetSOffset(TriInfo, TID, TriInfo.C->VID);

		if (auto* UVLayer0 = Mesh.Attributes()->GetUVLayer(0))
		{
			UVLayer0->SetTriangle(TID, FIndex3i{
				UVLayer0->AppendElement(FVector2f(TriInfo.A->Alpha0, SOffsetA * UV0ScaleFactorAligned)),
				UVLayer0->AppendElement(FVector2f(TriInfo.B->Alpha0, SOffsetB * UV0ScaleFactorAligned)),
				UVLayer0->AppendElement(FVector2f(TriInfo.C->Alpha0, SOffsetC * UV0ScaleFactorAligned))
			});
		}

		if (auto* UVLayer1 = Mesh.Attributes()->GetUVLayer(1))
		{
			UVLayer1->SetTriangle(TID, FIndex3i{
				UVLayer1->AppendElement(FVector2f((TriInfo.A->Alpha1 - 0.5) * Alpha1Mul + 0.5, SOffsetA * UV1ScaleFactorAligned)),
				UVLayer1->AppendElement(FVector2f((TriInfo.B->Alpha1 - 0.5) * Alpha1Mul + 0.5, SOffsetB * UV1ScaleFactorAligned)),
				UVLayer1->AppendElement(FVector2f((TriInfo.C->Alpha1 - 0.5) * Alpha1Mul + 0.5, SOffsetC * UV1ScaleFactorAligned))
			});
		}

		if (auto* UVLayer2 = Mesh.Attributes()->GetUVLayer(2))
		{
			
			UVLayer2->SetTriangle(TID, FIndex3i{
				UVLayer2->AppendElement(FVector2f(TriInfo.A->Alpha2 * UV2ScaleFactor, SOffsetA * UV2ScaleFactorAligned)),
				UVLayer2->AppendElement(FVector2f(TriInfo.B->Alpha2 * UV2ScaleFactor, SOffsetB * UV2ScaleFactorAligned)),
				UVLayer2->AppendElement(FVector2f(TriInfo.C->Alpha2 * UV2ScaleFactor, SOffsetC * UV2ScaleFactorAligned))
			});
		}

		return true;
	}

	return false;
}

// --------------------------------------------------------------------------------------------------------------------------------------------------
FRoadSimplePolygone::FRoadSimplePolygone(FRoadBaseOperatorData& Owner, int SplineIndex, double MaxSquareDistanceFromSpline, double MinSegmentLength)
	:FRoadPolygoneBase(Owner, SplineIndex)
{
	ResultInfo = { EGeometryResultType::InProgress };

	TArray<FRoadPosition> RoadPoints;
	if (!GetRoadSplineCache().ConvertSplineToPolyline_InDistanceRange2(0, LANE_INDEX_NONE, [](double S) { return 0.0; }, ESplineCoordinateSpace::World, MaxSquareDistanceFromSpline, MinSegmentLength, 0.0, GetRoadSplineCache().SplineCurves.GetSplineLength(), RoadPoints, true))
	{
		ResultInfo.SetFailed(FText::Format(LOCTEXT("RoadSimplePolygone_ConvertSplineToPolyline", "{0}: Can't ConvertSplineToPolyline"), GetDescription()));
		return;
	}

	TArray<FVector2D> Points2D;
	Points2D.Reserve(RoadPoints.Num());
	for (auto& It : RoadPoints)
	{
		Points2D.Add(FVector2D{ It.Location });
	}
	OpUtils::RemovedPolylineSelfIntersection(Points2D);

	int GID = 0;

	if (GetLaneInstance().GetPtr<FRoadLaneDriving>() != nullptr)
	{
		GID = GUIFlags::DrivingSurface;
	}
	else if (GetLaneInstance().GetPtr<FRoadLaneSidewalk>() != nullptr)
	{
		GID = GUIFlags::SidewalksSoft;
	}

	LineInfo.PID = this->Owner.Arrangement->Graph.AllocateEdgePolylines();
	for (int i = 0; i < Points2D.Num() - 1; ++i)
	{
		this->Owner.Arrangement->Insert(Points2D[i], Points2D[i + 1], GID, LineInfo.PID);
	}

	LineInfo.VID_A = LineInfo.VID_B = this->Owner.Arrangement->FindExistingVertex(Points2D[0]);

	if (!LineInfo.IsValid())
	{
		ResultInfo.SetFailed(FText::Format(LOCTEXT("RoadSimplePolygone_OutsideLineFaild", "{0}: polygon faild "), GetDescription()));
		return;
	}

	ResultInfo.SetSuccess();
}

bool FRoadSimplePolygone::CompleteArrangement()
{
	if (ResultInfo.HasFailed())
	{
		return false;
	}

	if (!FindPolyline(Owner.Arrangement->Graph, LineInfo, LineVertices))
	{
		ResultInfo.SetFailed(FText::Format(LOCTEXT("RoadSimplePolygone_InsideLineNotFound", "{0}: line not found after arrangement"), GetDescription()));
		return false;
	}

	for (int i = 0; i < LineVertices.Num(); ++i)
	{
		AddVertexInfo(LineVertices[i], nullptr, ERoadVertexInfoFlags::SimplePoly);
	}

	for (int i = 0; i < LineVertices.Num() - 1; ++i)
	{
		Boundary.Add({ LineVertices[i], LineVertices[i + 1] });
	}

	TArray<FVector2d> Vertex2d;
	{
		const auto& FirstInfo = Owner.Vertices3d[Boundary[0].A].Infos.Last();
		Vertex2d.Add(FVector2D{ FirstInfo.Pos.Location });
	}
	for (const auto& It : Boundary)
	{
		const auto& Info = Owner.Vertices3d[It.B].Infos.Last();
		Vertex2d.Add(FVector2D{ Info.Pos.Location });
	}
	Poly2d = UE::Geometry::FPolygon2d(Vertex2d);

	if (!Poly2d.IsClockwise())
	{
		Algo::Reverse(LineVertices);
		Algo::Reverse(Vertex2d);
		Poly2d = UE::Geometry::FPolygon2d(Vertex2d);
		Algo::Reverse(Boundary);
		for (auto& It : Boundary)
		{
			It = FIndex2i{ It.B, It.A };
		}
	}

	return true;
}

void FRoadSimplePolygone::AddVertexInfo(int VID, const FAxisAlignedBox2d* SplineBounds, ERoadVertexInfoFlags Flags) const
{
	check(Owner.Arrangement->Graph.IsVertex(VID));

	if (auto* FoundVertex = Owner.Vertices3d[VID].Infos.FindByPredicate([this](const FRoadVertexInfo& Info) { return Info.Poly == this; }))
	{
		FoundVertex->Flags = FoundVertex->Flags | Flags;
		return;
	}

	const auto& RoadSplineCash = GetRoadSplineCache();
	const auto V2d = Owner.Arrangement->Graph.GetVertex(VID);
	const auto Pos = RoadSplineCash.UpRayIntersection(V2d);

	auto& VertexInfo = Owner.Vertices3d[VID].Infos.Add_GetRef({});
	VertexInfo.Poly = this;
	VertexInfo.Pos = Pos;
	VertexInfo.Pos.Location.X = V2d.X;
	VertexInfo.Pos.Location.Y = V2d.Y;
	VertexInfo.Alpha0 = 0.0;
	VertexInfo.Alpha1 = 0.0;
	VertexInfo.Alpha2 = 0.0;
	VertexInfo.VID = VID;
	VertexInfo.Flags = Flags;
}

const TInstancedStruct<FRoadLaneInstance>& FRoadSimplePolygone::GetLaneInstance() const
{
	return GetRoadSplineCache().RoadLayout.FilledInstance;
}

FText FRoadSimplePolygone::GetDescription() const
{
	return LOCTEXT("RoadSimplePolygone_GetDescription", "SimplePoly");
}

bool FRoadSimplePolygone::SetUVLayers(FDynamicMesh3& Mesh, int TID, double /*UV0ScaleFactor*/, double /*UV1ScaleFactor*/, double /*UV2ScaleFactor*/) const
{
	check(Mesh.Attributes()->NumUVLayers() >= 2);

	const auto& Layout = GetRoadSplineCache().RoadLayout;
	const FAxisAlignedBox2d Bounds = FAxisAlignedBox2d(Poly2d.GetVertices());
	const FMatrix2f UVRotate = FMatrix2f::RotationDeg(Layout.FilledInstanceTexAngle);
	const double UVScale = 1.0 / (Bounds.Max - Bounds.Min).GetMax() * Layout.FilledInstanceTexScale;
	const auto SplineWorldLocation = GetRoadSplineCache().ComponentToWorld.GetLocation();
	const FVector2f OriginTranslate = FVector2f(SplineWorldLocation.X, SplineWorldLocation.Y) * UVScale;   //FVector2f(-Bounds.Min - (Bounds.Max - Bounds.Min) / 2.0) * UVScale;

	auto TriInfo = FindTri(TID);

	if (ensure(TriInfo.IsValid()))
	{
		const FVector2f UV_A = UVRotate * (FVector2f(TriInfo.A->Pos.Location.X, TriInfo.A->Pos.Location.Y) * UVScale) + OriginTranslate;
		const FVector2f UV_B = UVRotate * (FVector2f(TriInfo.B->Pos.Location.X, TriInfo.B->Pos.Location.Y) * UVScale) + OriginTranslate;
		const FVector2f UV_C = UVRotate * (FVector2f(TriInfo.C->Pos.Location.X, TriInfo.C->Pos.Location.Y) * UVScale) + OriginTranslate;

		if (auto* UVLayer0 = Mesh.Attributes()->GetUVLayer(0))
		{
			UVLayer0->SetTriangle(TID, FIndex3i{
				UVLayer0->AppendElement(UV_A),
				UVLayer0->AppendElement(UV_B),
				UVLayer0->AppendElement(UV_C)
			});
		}

		if (auto* UVLayer1 = Mesh.Attributes()->GetUVLayer(1))
		{
			UVLayer1->SetTriangle(TID, FIndex3i{
				UVLayer1->AppendElement(UV_A),
				UVLayer1->AppendElement(UV_B),
				UVLayer1->AppendElement(UV_C)
			});
		}

		if (auto* UVLayer2 = Mesh.Attributes()->GetUVLayer(2))
		{
			UVLayer2->SetTriangle(TID, FIndex3i{
				UVLayer2->AppendElement(UV_A),
				UVLayer2->AppendElement(UV_B),
				UVLayer2->AppendElement(UV_C)
			});
		}

		return true;
	}
	
	return false;
}

// --------------------------------------------------------------------------------------------------------------------------------------------------

TArray<FRoadPosition> RoadPolygoneUtils::MakePolyline(const TArray<FArrangementVertex3d>& Vertexes, const TArray<int>& VerticesIDs, const FRoadPolygoneBase* PolyFilter)
{
	TArray<FRoadPosition> Vertices;
	Vertices.Reserve(VerticesIDs.Num());

	for (auto VID : VerticesIDs)
	{
		auto* Info = Vertexes[VID].Infos.FindByPredicate([PolyFilter](const FRoadVertexInfo& It) { return It.Poly == PolyFilter; });
		check(Info);

		FRoadPosition Pos = Info->Pos;
		Pos.Location = Vertexes[VID].Vertex;

		// Set normal from FRoadPosition to Quat
		auto Diff = FQuat::FindBetweenNormals(Pos.Quat.GetUpVector(), Vertexes[VID].Normal);
		Pos.Quat = Diff * Pos.Quat;

		Vertices.Add(Pos);
	}

	return Vertices;
}
