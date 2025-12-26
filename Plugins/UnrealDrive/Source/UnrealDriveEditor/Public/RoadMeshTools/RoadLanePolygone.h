/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "CoreMinimal.h"
#include "RoadSplineComponent.h"
#include "RoadSplineCache.h"
#include "Polygon2.h"
#include "CompGeom/Delaunay2.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Util/ProgressCancel.h"

namespace UnrealDrive 
{

	using namespace UE::Geometry;

	struct FRoadPolygoneBase;
	struct FRoadBaseOperatorData;

	/**
	 * FRoadLanePolyline
	 */
	namespace GUIFlags
	{
		static uint8 DrivingSurface = 1 << 0;
		static uint8 SidewalksSoft  = 1 << 1;
		static uint8 SidewalksHard  = 1 << 2;
		static uint8 CenterLine     = 1 << 3;
	}

	/**
	 * ERoadVertexInfoType
	 */
	enum class ERoadVertexInfoFlags: int32
	{
		Inside = (1U << 0),
		Outside = (1U << 1),
		EndCap = (1U << 2),
		BeginCap = (1U << 3),
		LoopSeam = (1U << 4),
		OverlapPoly = (1U << 5),
		SimplePoly = (1U << 6),
	};
	ENUM_CLASS_FLAGS(ERoadVertexInfoFlags)

	/**
	 * FRoadVertexInfo
	 */
	struct FRoadVertexInfo
	{
		const FRoadPolygoneBase * Poly;
		FRoadPosition Pos;
		double Alpha0;
		double Alpha1;
		double Alpha2;
		int VID;
		ERoadVertexInfoFlags Flags;
	};

	/**
	 * FArrangementVertex3d
	 */
	struct UNREALDRIVEEDITOR_API FArrangementVertex3d
	{
		TArray<FRoadVertexInfo> Infos;
		FVector Vertex;
		FVector Normal; 
	};

	/**
	 * ERoadPolygoneType
	 */
	enum ERoadPolygoneType
	{
		Simple,
		RoadLane,
		//Polyline
	};

	/*
	 * FLineInfo 
	 * Notes:
	 * If VID_B == -1 means the line contain only one point (VID_A)
	 * If VID_A == VID_B means the line is loop
	 * if VID_A == -1 means line is not valid
	 */
	struct FLineInfo
	{
		int PID = -1;
		int VID_A = -1;
		int VID_B = -1;

		inline bool IsValid() const { return VID_A != -1; }
		inline bool IsLoop() const { return IsValid() && VID_A == VID_B; }
	};

	/**
	 * FRoadPolygoneBase 
	 */
	struct UNREALDRIVEEDITOR_API FRoadPolygoneBase
	{
		struct FTriInfo
		{
			FRoadVertexInfo* A;
			FRoadVertexInfo* B;
			FRoadVertexInfo* C;

			inline bool IsValid() const { return A && B && C; }
			inline FRoadVertexInfo& GetVertexInfo(int VID) const
			{
				check(IsValid());
				auto& Ret = A->VID == VID ? *A : (B->VID == VID ? *B : *C); 
				check(Ret.VID == VID);
				return Ret;
			}
		};

		FRoadPolygoneBase(FRoadBaseOperatorData& Owner, int SplineIndex)
			: Owner(Owner)
			, SplineIndex(SplineIndex)
		{
		}
		virtual ~FRoadPolygoneBase()
		{
		}

		virtual ERoadPolygoneType GetType() const = 0;
		virtual bool CompleteArrangement() = 0;
		virtual void AddVertexInfo(int VID, const FAxisAlignedBox2d* RoadSplineBounds, ERoadVertexInfoFlags Falgs) const = 0;
		virtual const TInstancedStruct<FRoadLaneInstance>& GetLaneInstance() const = 0;
		virtual FText GetDescription() const = 0;
		virtual bool SetUVLayers(FDynamicMesh3& Mesh, int TID, double UV0ScaleFactor, double UV1ScaleFactor, double UV2ScaleFactor) const = 0;

		virtual double GetPriority() const;
		virtual const UE::Geometry::FGeometryResult& GetResult() const { return ResultInfo; }
		const FRoadSplineCache& GetRoadSplineCache() const;
		FRoadSplineCache& GetRoadSplineCache();

		FTriInfo FindTri(int TID) const;
		inline bool IsPolyline() const { return Boundary.Num() && Boundary[0].A != Boundary.Last().B; }

		FRoadBaseOperatorData& Owner;
		int SplineIndex;

		
		TArray<FIndex2i> Boundary; // ClockWise
		TArray<TArray<FIndex2i>> Holse; // ClockWise
		TArray<int> TrianglesIDs;
		UE::Geometry::FGeometryResult ResultInfo;
	};

	/**
	 * FRoadLanePolygone
	 */
	struct UNREALDRIVEEDITOR_API FRoadLanePolygone: public FRoadPolygoneBase
	{
		FRoadLanePolygone(FRoadBaseOperatorData& Owner, int SplineIndex, int SectionIndex, int LaneIndex, double MaxSquareDistanceFromSpline, double MaxSquareDistanceFromCap, double MinSegmentLength);

		virtual ERoadPolygoneType GetType() const override { return ERoadPolygoneType::RoadLane; }
		virtual bool CompleteArrangement() override;
		virtual void AddVertexInfo(int VID, const FAxisAlignedBox2d* SplineBounds, ERoadVertexInfoFlags Flags) const override;
		virtual const TInstancedStruct<FRoadLaneInstance>& GetLaneInstance() const override;
		virtual FText GetDescription() const override;
		virtual bool SetUVLayers(FDynamicMesh3& Mesh, int TID, double UV0ScaleFactor, double UV1ScaleFactor, double UV2ScaleFactor) const override;
		
		const FRoadLaneSection& GetSection() const;
		const FRoadLane& GetLane() const;
		const TMap<FName, FRoadLaneAttribute> & GetLaneAttributes() const;
		double GetStartOffset() const;
		double GetEndOffset() const;

		bool IsLoop() const { return bIsLoop; }


		int SectionIndex;
		int LaneIndex;

		TArray<int> InsideLineVertices;
		TArray<int> EndCapVertices;
		TArray<int> OutsideLineVertices;
		TArray<int> BeginCapVertices;

		UE::Geometry::FPolygon2d Poly2d;
		
		FAxisAlignedBox2d Bounds;
		FAxisAlignedBox2d SplineBounds; // X - SOffset, Y - ROffset


	private:
		bool ProcessPolyline(const FLineInfo& LineInfo, TArray<int>& VIDs, ERoadVertexInfoFlags Flags);

	private:
		FLineInfo InsideLineInfo;
		FLineInfo EndCapInfo;
		FLineInfo OutsideLineInfo;
		FLineInfo BeginCapInfo;
		bool bIsLoop;
	};

	/**
	 * FRoadSimplePolygone
	 */
	struct UNREALDRIVEEDITOR_API FRoadSimplePolygone : public FRoadPolygoneBase
	{
		FRoadSimplePolygone(FRoadBaseOperatorData& Owner, int SplineIndex, double MaxSquareDistanceFromSpline, double MinSegmentLength);

		virtual ERoadPolygoneType GetType() const override { return ERoadPolygoneType::Simple; }
		virtual bool CompleteArrangement() override;
		virtual void AddVertexInfo(int VID, const FAxisAlignedBox2d* SplineBounds, ERoadVertexInfoFlags Flags) const override;
		virtual const TInstancedStruct<FRoadLaneInstance>& GetLaneInstance() const override;
		virtual FText GetDescription() const override;
		virtual bool SetUVLayers(FDynamicMesh3& Mesh, int TID, double UV0ScaleFactor, double UV1ScaleFactor, double UV2ScaleFactor) const override;

		TArray<int> LineVertices;
		UE::Geometry::FPolygon2d Poly2d;

	private:
		FLineInfo LineInfo;
	};

	namespace RoadPolygoneUtils
	{
		TArray<FRoadPosition> MakePolyline(const TArray<FArrangementVertex3d>& Vertexes, const TArray<int>& VerticesIDs, const FRoadPolygoneBase* PolyFilter);
	}

} // UnrealDrive
