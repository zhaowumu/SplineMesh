/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "LaneProxy.h"
#include "RoadSplineComponent.h"
#include "UnrealDriveSettings.h"
#include "RoadSceneProxy.h"
#include "SceneInterface.h"
#include "EngineGlobals.h"
#include "RenderingThread.h"
#include "RenderResource.h"
#include "VertexFactory.h"
#include "LocalVertexFactory.h"
#include "PrimitiveViewRelevance.h"
#include "PrimitiveSceneProxy.h"
#include "Engine/Engine.h"
#include "MaterialShared.h"
#include "Materials/Material.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ConstrainedDelaunay2.h"
#include "Polygon2.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Styling/StyleColors.h"

using namespace UE::Geometry;
using namespace UnrealDrive;

static const FColor SplineColor = FStyleColors::AccentPink.GetSpecifiedColor().ToFColor(true);

static bool BuildRoadMesh(const TArray<FSplinePositionLinearApproximation>& Side1, const TArray<FSplinePositionLinearApproximation>& Side2, FConstrainedDelaunay2d::EFillRule FillRule, const URoadSplineComponent* Component, int LaneIndex, const FRoadLaneSection& LaneSection, FDynamicMesh3& OutMesh)
{
	check(Side1.Num() == Side2.Num());

	const int N = Side1.Num();
	const double LaneDefWidth = UnrealDrive::DefaultRoadLaneWidth * 0.8;

	for (int i = 0; i < N; ++i)
	{
		const FRoadLane& Lane = LaneSection.GetLaneByIndex(LaneIndex);
		const double S = Component->GetDistanceAlongSplineAtSplineInputKey(Side1[i].SplineParam);
		const double Width = Lane.Width.Eval(S - LaneSection.SOffset);
		const bool bIsRever = ((LaneIndex < 0) ^ (Lane.Direction == ERoadLaneDirection::Invert) ^ (Component->RoadLayout.Direction == ERoadDirection::RightHand));
		FVertexInfo Vertex{};
		Vertex.Position = Side1[i].Position;
		Vertex.UV = FVector2f(0 - Width / LaneDefWidth / 2 + 0.5, S / LaneDefWidth * (bIsRever ? -1 : 1));
		Vertex.Color = FVector3f(0.0);
		Vertex.bHaveC = true;
		Vertex.bHaveUV = true;
		OutMesh.AppendVertex(Vertex);
	}

	for (int i = 0; i < N; ++i)
	{
		const FRoadLane& Lane = LaneSection.GetLaneByIndex(LaneIndex);
		const double S = Component->GetDistanceAlongSplineAtSplineInputKey(Side2[i].SplineParam);
		const double Width = Lane.Width.Eval(S - LaneSection.SOffset);
		const bool bIsRever = ((LaneIndex < 0) ^ (Lane.Direction == ERoadLaneDirection::Invert) ^ (Component->RoadLayout.Direction == ERoadDirection::RightHand));
		FVertexInfo Vertex{};
		Vertex.Position = Side2[i].Position;
		Vertex.UV = FVector2f(Width / LaneDefWidth - Width / LaneDefWidth / 2  + 0.5, S / LaneDefWidth * (bIsRever ? -1 : 1));
		Vertex.Color = FVector3f(0.0);
		Vertex.bHaveC = true;
		Vertex.bHaveUV = true;
		OutMesh.AppendVertex(Vertex);
	}

	for (int i = 0; i < N - 1; ++i)
	{
		OutMesh.AppendTriangle({ i, i + 1, i + N });
		OutMesh.AppendTriangle({ i + N , i + 1, i + 1 + N });
	}

	return true;
}

static void ConvertSplineToPolyLine(const USplineComponent* SplineComp, TArray<FVector>& OutPoints)
{
	const FInterpCurveVector& SplineInfo = SplineComp->GetSplinePointsPosition();
	const int32 NumPoints = SplineInfo.Points.Num();
	const int32 NumSegments = SplineInfo.bIsLooped ? NumPoints : NumPoints - 1;

	OutPoints.Add(SplineComp->GetLocationAtSplineInputKey(0.0, ESplineCoordinateSpace::Local));

	for (int32 KeyIdx = 0; KeyIdx < NumSegments + 1; KeyIdx++)
	{
		// If not the first keypoint, draw a line to the previous keypoint.
		if (KeyIdx > 0)
		{
			const int32 EndIdx = KeyIdx - 1;

			// Then draw a line for each substep.
			constexpr int32 NumSteps = 20;

			for (int32 StepIdx = 1; StepIdx <= NumSteps; StepIdx++)
			{
				const float StepRatio = StepIdx / static_cast<float>(NumSteps);
				const float Key = EndIdx + StepRatio;
				const FVector NewPos = SplineComp->GetLocationAtSplineInputKey(Key, ESplineCoordinateSpace::Local);
				OutPoints.Add(NewPos);
			}
		}
	}
}


static TSharedPtr<FLaneProxy> MakeLaneProxy(TArray<FSplinePositionLinearApproximation>& PrevPoints, const URoadSplineComponent* Component, int SectionIndex, int LaneIndex, double S0, double S1, bool bDrawStartCap, bool bDrawEndCap, ERHIFeatureLevel::Type FeatureLevel)
{
	const int NumPointPerSegmaent = GetDefault<UUnrealDriveSettings>()->NumPointPerSegmaent;
	const int NumPointPerSection = GetDefault<UUnrealDriveSettings>()->NumPointPerSection;

	auto& Section = Component->GetLaneSection(SectionIndex);
	auto& Lane = Section.GetLaneByIndex(LaneIndex);

	if ((S1 - S0) < KINDA_SMALL_NUMBER)
	{
		return {};
	}

	auto LaneProxy = MakeShared< FLaneProxy>(SectionIndex, LaneIndex, FeatureLevel);

	TArray<FSplinePositionLinearApproximation> Points;
	Component->BuildLinearApproximation(Points, [&](double S)
	{
		return Section.EvalLaneROffset(LaneIndex, S) + Component->EvalROffset(S);
	}, S0, S1, NumPointPerSegmaent, NumPointPerSection, ESplineCoordinateSpace::Local);

	check(Points.Num());

	LaneProxy->LanePoints.Reserve(Points.Num() + 2);

	if (bDrawStartCap)
	{
		LaneProxy->LanePoints.Add(PrevPoints[0].Position);
	}
	for (const auto& Pt : Points)
	{
		LaneProxy->LanePoints.Add(Pt.Position);
	}
	if (bDrawEndCap)
	{
		LaneProxy->LanePoints.Add(PrevPoints.Last().Position);
	}

	FDynamicMesh3 Mesh(false /*bWantNormals*/, true /* bWantColors*/, true /*bWantUVs*/, false /*bWantTriGroups*/);
	if (BuildRoadMesh(PrevPoints, Points, LaneIndex >= 0 ? FConstrainedDelaunay2d::EFillRule::Positive : FConstrainedDelaunay2d::EFillRule::Negative, Component, LaneIndex, Section, Mesh))
	{
		UMaterialInstance* Material = UUnrealDriveSettings::GetLaneMatrtial(Section.GetLaneByIndex(LaneIndex).LaneInstance);
		check(Material);

		LaneProxy->Material = Material->GetRenderProxy();

		TArray<FDynamicMeshVertex> MeshVertices;
		TArray<uint32> MeshIndices;

		MeshVertices.Reserve(Mesh.VertexCount());
		MeshIndices.Reserve(3 * Mesh.TriangleCount());
		for (int VertexIndex : Mesh.VertexIndicesItr())
		{
			FVertexInfo VertexInfo;
			const bool bWantColors = true;
			const bool bWantNormals = false;
			const bool bWantUVs = true;
			Mesh.GetVertex(VertexIndex, VertexInfo, bWantNormals, bWantColors, bWantUVs);

			const FVector3f LocalSpacePosition = FVector3f(VertexInfo.Position);
			FDynamicMeshVertex MeshVertex(LocalSpacePosition);

			MeshVertex.Color = FColor(255, 255, 255, 255);
			MeshVertex.TextureCoordinate[0] = VertexInfo.UV;
			MeshVertices.Add(MeshVertex);
		}
		for (FIndex3i Triangle : Mesh.TrianglesItr())
		{
			MeshIndices.Append({ (uint32)Triangle.A, (uint32)Triangle.B, (uint32)Triangle.C });
		}
		LaneProxy->InitMesh(MeshVertices, MeshIndices);
	}

	PrevPoints = MoveTemp(Points);

	return LaneProxy;
};

static bool HasLeftSide(ERoadLaneSectionSide Side)
{
	return Side == ERoadLaneSectionSide::Both || Side == ERoadLaneSectionSide::Left;
}

static bool HasRightSide(ERoadLaneSectionSide Side)
{
	return Side == ERoadLaneSectionSide::Both || Side == ERoadLaneSectionSide::Right;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------

void FLaneProxy::InitMesh(TArray<FDynamicMeshVertex>& Vertices, TArray<uint32>& Indices)
{
	if (Indices.Num() > 3)
	{
		IndexBuffer.Indices = Indices;
		VertexBuffers.InitFromDynamicVertex(&VertexFactory, Vertices);
		BeginInitResource(&VertexBuffers.PositionVertexBuffer);
		BeginInitResource(&VertexBuffers.StaticMeshVertexBuffer);
		BeginInitResource(&VertexBuffers.ColorVertexBuffer);
		BeginInitResource(&VertexFactory);
		BeginInitResource(&IndexBuffer);
	}
}

void FLaneProxy::ReleaseResources()
{
	VertexBuffers.PositionVertexBuffer.ReleaseResource();
	VertexBuffers.StaticMeshVertexBuffer.ReleaseResource();
	VertexBuffers.ColorVertexBuffer.ReleaseResource();
	VertexFactory.ReleaseResource();
	IndexBuffer.ReleaseResource();
}

FMeshBatch * FLaneProxy::GetDynamicMeshElements(const class FPrimitiveSceneProxy& SceneProxy, const FSceneView*& View, const FSceneViewFamily& ViewFamily, FPrimitiveDrawInterface* PDI, FMeshElementCollector& Collector) const
{
	const FMatrix& MyLocalToWorld = SceneProxy.GetLocalToWorld();

	if (IndexBuffer.Indices.Num() > 3)
	{
		FDynamicPrimitiveUniformBuffer& DynamicPrimitiveUniformBuffer = Collector.AllocateOneFrameResource<FDynamicPrimitiveUniformBuffer>();
		{
			bool bHasPrecomputedVolumetricLightmap;
			FMatrix PreviousLocalToWorld;
			int32 SingleCaptureIndex;
			bool bOutputVelocity;
			SceneProxy.GetScene().GetPrimitiveUniformShaderParameters_RenderThread(SceneProxy.GetPrimitiveSceneInfo(), bHasPrecomputedVolumetricLightmap, PreviousLocalToWorld, SingleCaptureIndex, bOutputVelocity);
			DynamicPrimitiveUniformBuffer.Set(Collector.GetRHICommandList(), SceneProxy.GetLocalToWorld(), PreviousLocalToWorld, SceneProxy.GetBounds(), SceneProxy.GetLocalBounds(), SceneProxy.GetLocalBounds(), true, bHasPrecomputedVolumetricLightmap, bOutputVelocity, SceneProxy.GetCustomPrimitiveData());
		}

		FMeshBatch& MeshBatch = Collector.AllocateMesh();
		MeshBatch.MaterialRenderProxy = Material;
		MeshBatch.VertexFactory = &VertexFactory;
		MeshBatch.ReverseCulling = SceneProxy.IsLocalToWorldDeterminantNegative();
		MeshBatch.Type = PT_TriangleList;
		MeshBatch.DepthPriorityGroup = SDPG_World;//SDPG_Foreground;
		MeshBatch.bCanApplyViewModeOverrides = false;
		//MeshBatch.bWireframe = bWireframe;

#if WITH_EDITOR
		if (/*sSelected()&&*/  HitProxy.IsValid())
		{
			MeshBatch.BatchHitProxyId = HitProxy->Id;
		}
#endif

		FMeshBatchElement& BatchElement = MeshBatch.Elements[0];
		BatchElement.IndexBuffer = &IndexBuffer;
		BatchElement.FirstIndex = 0;
		BatchElement.NumPrimitives = IndexBuffer.Indices.Num() / 3;
		BatchElement.MinVertexIndex = 0;
		BatchElement.MaxVertexIndex = VertexBuffers.PositionVertexBuffer.GetNumVertices() - 1;
		BatchElement.PrimitiveUniformBufferResource = &DynamicPrimitiveUniformBuffer.UniformBuffer;

		return &MeshBatch;
	}

	return nullptr;
}

void FLaneProxy::DrawLines(const FMatrix& LocalToWorld, FPrimitiveDrawInterface* PDI, bool bIsSelected) const
{
#if WITH_EDITOR
	PDI->SetHitProxy(HitProxy);
#endif
	for (int i = 0; i < LanePoints.Num() - 1; ++i)
	{
		PDI->DrawLine(
			LocalToWorld.TransformPosition(LanePoints[i]),
			LocalToWorld.TransformPosition(LanePoints[i + 1]),
			bIsSelected ? FStyleColors::AccentOrange.GetSpecifiedColor() : LineColor,
			SDPG_Foreground, 
			0.0f,
			0.0,//bIsSelected ? 0.0f : 0.1f, 
			false);
	}
#if WITH_EDITOR
	PDI->SetHitProxy(nullptr);
#endif
}

#if WITH_EDITOR
HRoadSplineVisProxy* FLaneProxy::CreateHitProxy(const URoadSplineComponent* Component)
{
	HitProxy = new HRoadLaneVisProxy(Component, SectionIndex, LaneIndex);
	return HitProxy;	
}
#endif

TArray<TSharedPtr<FLaneProxy>> FLaneProxy::MakeLaneProxysFromSpline(URoadSplineComponent* Component, ERHIFeatureLevel::Type FeatureLevel)
{
	const int NumPointPerSegmaent = GetDefault<UUnrealDriveSettings>()->NumPointPerSegmaent;
	const int NumPointPerSection = GetDefault<UUnrealDriveSettings>()->NumPointPerSection;

	TArray<TSharedPtr<FLaneProxy>> LanesProxy;

	int PreLeftSectionIndex = -1;
	int PreRighSectionIndex = -1;

	for (int SectionIndex = 0; SectionIndex < Component->GetLaneSectionsNum(); ++SectionIndex)
	{
		auto& Section = Component->GetLaneSection(SectionIndex);

		const double S0 = Section.SOffset;
		const double S1 = Section.SOffsetEnd_Cashed;
		if ((S1 - S0) < KINDA_SMALL_NUMBER)
		{
			continue;
		}

		ERoadLaneSectionSide NextSide = ERoadLaneSectionSide::Both;
		if (SectionIndex != Component->GetLaneSectionsNum() - 1)
		{
			NextSide = Component->GetLaneSection(SectionIndex + 1).Side;
		}

		TArray<FSplinePositionLinearApproximation> PrevPoints;

		// Add center lane
		TArray<FSplinePositionLinearApproximation> CenterPoints;
		Component->BuildLinearApproximation(CenterPoints, [&](double S) { return Component->EvalROffset(S); }, S0, S1, NumPointPerSegmaent, NumPointPerSection, ESplineCoordinateSpace::Local);
		FLaneProxy& LaneProxy = *LanesProxy.Add_GetRef(MakeShared< FLaneProxy>(SectionIndex, 0, FeatureLevel));
		LaneProxy.LanePoints.SetNum(CenterPoints.Num());
		LaneProxy.LineColor = SplineColor;
		for (int i = 0; i < CenterPoints.Num(); ++i) LaneProxy.LanePoints[i] = CenterPoints[i].Position;


		// Add right lanes
		PrevPoints = CenterPoints;
		if (HasRightSide(Section.Side))
		{
			for (int i = 0; i < Section.Right.Num(); ++i)
			{
				auto& Lane = Section.Right[i];
				if (auto NewProxy = MakeLaneProxy(PrevPoints, Component, SectionIndex, +i + 1, S0, S1, true, HasRightSide(NextSide), FeatureLevel))
				{
					LanesProxy.Add(MoveTemp(NewProxy));
				}
				else
				{
					break;
				}
			}
			PreRighSectionIndex = SectionIndex;
		}
		else if (PreRighSectionIndex >= 0)
		{
			for (int i = 0; i < Component->GetLaneSection(PreRighSectionIndex).Right.Num(); ++i)
			{
				if (auto NewProxy = MakeLaneProxy(PrevPoints, Component, PreRighSectionIndex, +i + 1, S0, S1, false, HasRightSide(NextSide), FeatureLevel))
				{
					LanesProxy.Add(MoveTemp(NewProxy));
				}
				else
				{
					break;
				}
			}
		}

		// Add left lanes
		PrevPoints = CenterPoints;
		if (HasLeftSide(Section.Side))
		{
			for (int i = 0; i < Section.Left.Num(); ++i)
			{
				auto& Lane = Section.Left[i];
				if (auto NewProxy = MakeLaneProxy(PrevPoints, Component, SectionIndex, -i - 1, S0, S1, true, HasLeftSide(NextSide), FeatureLevel))
				{
					LanesProxy.Add(MoveTemp(NewProxy));
				}
				else
				{
					break;
				}
			}
			PreLeftSectionIndex = SectionIndex;
		}
		else if (PreLeftSectionIndex >= 0)
		{
			for (int i = 0; i < Component->GetLaneSection(PreLeftSectionIndex).Left.Num(); ++i)
			{
				if (auto NewProxy = MakeLaneProxy(PrevPoints, Component, PreLeftSectionIndex, -i - 1, S0, S1, false, HasLeftSide(NextSide), FeatureLevel))
				{
					LanesProxy.Add(MoveTemp(NewProxy));
				}
				else
				{
					break;
				}
			}
		}
	}

	return LanesProxy;
}

TSharedPtr<FLaneProxy> FLaneProxy::MakeLoopProxyFromSpline(URoadSplineComponent* Component, ERHIFeatureLevel::Type FeatureLevel)
{

	struct FLaneProxyLoop : public FLaneProxy
	{
		FLaneProxyLoop(ERHIFeatureLevel::Type FeatureLevel)
			: FLaneProxy(INDEX_NONE, 0, FeatureLevel)
		{
		}
#if WITH_EDITOR
		virtual HRoadSplineVisProxy* CreateHitProxy(const URoadSplineComponent* Component) override
		{
			HitProxy = new HRoadSplineVisProxy(Component);
			return HitProxy;
		}
#endif
	};

	auto LaneProxy = MakeShared< FLaneProxyLoop>(FeatureLevel);

	UMaterialInstance* Material = UUnrealDriveSettings::GetLaneMatrtial(Component->GetRoadLayout().FilledInstance);
	check(Material);
	LaneProxy->Material = Material->GetRenderProxy();
	LaneProxy->LineColor = SplineColor;
	ConvertSplineToPolyLine(Component, LaneProxy->LanePoints);

	TArray<FDynamicMeshVertex> Verts;
	Verts.Reserve(LaneProxy->LanePoints.Num());
	for (int i = 0; i < LaneProxy->LanePoints.Num(); ++i)
	{
		Verts.Emplace(FVector3f(LaneProxy->LanePoints[i]), FVector2f{ 0.0, 0.0 }, FColor::Black);
	}

	FPolygon2d Polygon(LaneProxy->LanePoints);
	FConstrainedDelaunay2d Triangulation;
	Triangulation.FillRule = Polygon.IsClockwise() ? FConstrainedDelaunay2d::EFillRule::Negative : FConstrainedDelaunay2d::EFillRule::Positive;
	Triangulation.Add(Polygon);
	bool bTriangulationSuccess = Triangulation.Triangulate();
	TArray<uint32> Indices;
	if (Triangulation.Triangles.Num())
	{
		Indices.Reserve(3 * Triangulation.Triangles.Num());
		for (const FIndex3i& Triangle : Triangulation.Triangles)
		{
			Indices.Append({ (uint32)Triangle.A, (uint32)Triangle.B, (uint32)Triangle.C });
		}
	}

	LaneProxy->InitMesh(Verts, Indices);
	return MoveTemp(LaneProxy);
}