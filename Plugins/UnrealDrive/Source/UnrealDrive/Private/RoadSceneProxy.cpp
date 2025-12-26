/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "RoadSceneProxy.h"
#include "RoadSplineComponent.h"
#include "UnrealDriveSettings.h"
#include "LaneProxy.h"
#include "Math/Color.h"
#include "EngineGlobals.h"
#include "RHI.h"
#include "RenderingThread.h"
#include "RenderResource.h"
#include "VertexFactory.h"
#include "LocalVertexFactory.h"
#include "PrimitiveViewRelevance.h"
#include "PrimitiveSceneProxy.h"
#include "Engine/Engine.h"
#include "MaterialShared.h"
#include "Materials/Material.h"
#include "Engine/CollisionProfile.h"
#include "SceneManagement.h"
#include "DynamicMeshBuilder.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "StaticMeshResources.h"
#include "PrimitiveSceneProxyDesc.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UnrealDrive.h"
#include "ConstrainedDelaunay2.h"
#include "Polygon2.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Materials/MaterialRenderProxy.h"
#include "UnrealDriveSubsystem.h"
#include "SceneInterface.h"
#include "SceneView.h"

using namespace UE::Geometry;


#if WITH_EDITOR

IMPLEMENT_HIT_PROXY(HRoadSplineVisProxy, HComponentVisProxy);
IMPLEMENT_HIT_PROXY(HRoadSectionVisProxy, HRoadSplineVisProxy);
IMPLEMENT_HIT_PROXY(HRoadLaneVisProxy, HRoadSectionVisProxy);

#endif

namespace UnrealDrive
{

class FOpacityMaterialRenderProxy : public FMaterialRenderProxy
{
public:
	const FMaterialRenderProxy* const Parent;
	float Opacity;
	FName ParamName;

	FOpacityMaterialRenderProxy(const FMaterialRenderProxy* InParent, float InOpacity, FName InParamName = "Opacity")
		: FMaterialRenderProxy(InParent ? InParent->GetMaterialName() : TEXT("FOpacityMaterialRenderProxy"))
		, Parent(InParent)
		, Opacity(InOpacity)
		, ParamName(InParamName)
	{
	}

	const FMaterial* GetMaterialNoFallback(ERHIFeatureLevel::Type InFeatureLevel) const
	{
		return Parent->GetMaterialNoFallback(InFeatureLevel);
	}

	const FMaterialRenderProxy* GetFallback(ERHIFeatureLevel::Type InFeatureLevel) const
	{
		return Parent->GetFallback(InFeatureLevel);
	}

	bool GetParameterValue(EMaterialParameterType Type, const FHashedMaterialParameterInfo& ParameterInfo, FMaterialParameterValue& OutValue, const FMaterialRenderContext& Context) const
	{
		if (Type == EMaterialParameterType::Scalar && ParameterInfo.Name == ParamName)
		{
			OutValue = Opacity;
			return true;
		}
		else
		{
			return Parent->GetParameterValue(Type, ParameterInfo, OutValue, Context);
		}
	}
};

struct FTriProxy
{
	FTriProxy(const FTransform& Transform, ERHIFeatureLevel::Type InFeatureLevel)
		: LocalTransform(Transform.ToMatrixNoScale())
		, VertexFactory(InFeatureLevel, "FTriProxy")
	{
		static const float Width = 20;
		static const float Height = 20;

		TArray<FDynamicMeshVertex> Vertices;
		Vertices.SetNum(3);

		Vertices[0].Position = { 0.0 , -Height / 2.0, 0.0 };
		Vertices[1].Position = { 0.0 , +Height / 2.0, 0.0 };
		Vertices[2].Position = { Width + 0.0, 0.0, 0.0 };

		Vertices[0].Color = FColor::White;
		Vertices[1].Color = FColor::White;
		Vertices[2].Color = FColor::White;

		IndexBuffer.Indices = { 0, 1, 2 };
		VertexBuffers.InitFromDynamicVertex(&VertexFactory, Vertices);
		BeginInitResource(&VertexBuffers.PositionVertexBuffer);
		BeginInitResource(&VertexBuffers.StaticMeshVertexBuffer);
		BeginInitResource(&VertexBuffers.ColorVertexBuffer);
		BeginInitResource(&VertexFactory);
		BeginInitResource(&IndexBuffer);

		Material = GetDefault<UUnrealDriveSettings>()->SplineArrowMatrtial->GetRenderProxy();
	}

	virtual ~FTriProxy()
	{ 
		ReleaseResources(); 
	}

	virtual void ReleaseResources()
	{
		VertexBuffers.PositionVertexBuffer.ReleaseResource();
		VertexBuffers.StaticMeshVertexBuffer.ReleaseResource();
		VertexBuffers.ColorVertexBuffer.ReleaseResource();
		VertexFactory.ReleaseResource();
		IndexBuffer.ReleaseResource();
	}

	virtual FMeshBatch* GetDynamicMeshElements(const class FPrimitiveSceneProxy& SceneProxy, const FSceneView*& View, const FSceneViewFamily& ViewFamily, FPrimitiveDrawInterface* PDI, FMeshElementCollector& Collector) const
	{
		float ViewScale = static_cast<float>(View->WorldToScreen(SceneProxy.GetLocalToWorld().TransformPosition(LocalTransform.GetOrigin())).W * (4.0f / View->UnscaledViewRect.Width() / View->ViewMatrices.GetProjectionMatrix().M[0][0]));
		ViewScale = FMath::Clamp(ViewScale, 1.f, 30.f);
		FMatrix MyLocalToWorld = (LocalTransform * SceneProxy.GetLocalToWorld()).ApplyScale(ViewScale);

		FDynamicPrimitiveUniformBuffer& DynamicPrimitiveUniformBuffer = Collector.AllocateOneFrameResource<FDynamicPrimitiveUniformBuffer>();
		{
			bool bHasPrecomputedVolumetricLightmap;
			FMatrix PreviousLocalToWorld;
			int32 SingleCaptureIndex;
			bool bOutputVelocity;
			SceneProxy.GetScene().GetPrimitiveUniformShaderParameters_RenderThread(SceneProxy.GetPrimitiveSceneInfo(), bHasPrecomputedVolumetricLightmap, PreviousLocalToWorld, SingleCaptureIndex, bOutputVelocity);
			DynamicPrimitiveUniformBuffer.Set(Collector.GetRHICommandList(), MyLocalToWorld, PreviousLocalToWorld, SceneProxy.GetBounds(), SceneProxy.GetLocalBounds(), SceneProxy.GetLocalBounds(), true, bHasPrecomputedVolumetricLightmap, bOutputVelocity, SceneProxy.GetCustomPrimitiveData());
		}

		FMeshBatch& MeshBatch = Collector.AllocateMesh();
		MeshBatch.MaterialRenderProxy = Material;
		MeshBatch.VertexFactory = &VertexFactory;
		MeshBatch.ReverseCulling = SceneProxy.IsLocalToWorldDeterminantNegative();
		MeshBatch.Type = PT_TriangleList;
		MeshBatch.DepthPriorityGroup = SDPG_World;//SDPG_Foreground;
		MeshBatch.bCanApplyViewModeOverrides = false;

		FMeshBatchElement& BatchElement = MeshBatch.Elements[0];
		BatchElement.IndexBuffer = &IndexBuffer;
		BatchElement.FirstIndex = 0;
		BatchElement.NumPrimitives = IndexBuffer.Indices.Num() / 3;
		BatchElement.MinVertexIndex = 0;
		BatchElement.MaxVertexIndex = VertexBuffers.PositionVertexBuffer.GetNumVertices() - 1;
		BatchElement.PrimitiveUniformBufferResource = &DynamicPrimitiveUniformBuffer.UniformBuffer;

		return &MeshBatch;
	}
	FMatrix LocalTransform;
	FStaticMeshVertexBuffers VertexBuffers;
	FDynamicMeshIndexBuffer32 IndexBuffer;
	FLocalVertexFactory VertexFactory;
	FMaterialRenderProxy* Material;
};

}

static FPrimitiveSceneProxyDesc MakePrimitiveSceneProxyDes(const UPrimitiveComponent* Component)
{
	FPrimitiveSceneProxyDesc Desc(Component);
#if WITH_EDITOR
	Desc.bIsVisibleEditor = FUnrealDriveModule::IsRoadSplinesVisibleInEditor();
#endif
	return Desc;
}

FRoadSplineSceneProxy::FRoadSplineSceneProxy(URoadSplineComponent* Component)
	: FPrimitiveSceneProxy(MakePrimitiveSceneProxyDes(Component), NAME_None)
	, RoadSpline(Component)
	, bIsMultiRoad(Component->GetOwner()->K2_GetComponentsByClass(URoadSplineComponent::StaticClass()).Num() > 1)

{
	Subsystem = Component->GetWorld()->GetSubsystem<UUnrealDriveSubsystem>();

	bWantsSelectionOutline = false;
	
	LanesProxies = UnrealDrive::FLaneProxy::MakeLaneProxysFromSpline(Component, GetScene().GetFeatureLevel());

	if (Component->IsClosedLoop() && Component->GetRoadLayout().FilledInstance.IsValid())
	{
		LanesProxies.Add(UnrealDrive::FLaneProxy::MakeLoopProxyFromSpline(Component, GetScene().GetFeatureLevel()));
	}

	//float Step = (Component->GetSplineLength() / SPLINE_DRAW_ARROW_STEP < 1.0) ? Component->GetSplineLength() / 2 : SPLINE_DRAW_ARROW_STEP;
	//if(Step < )

	for (auto& It : GetDefault<UUnrealDriveSettings>()->DriveableLaneMatrtials)
	{
		MaterialRelevance |= It.Value->GetRelevance_Concurrent(GetScene().GetFeatureLevel());
	}

	//MaterialRelevance |= GetDefault<UUnrealDriveSettings>()->CurbLaneMatrtial->GetRelevance_Concurrent(GetScene().GetFeatureLevel());
	
	MaterialRelevance |= GetDefault<UUnrealDriveSettings>()->SplineArrowMatrtial->GetRelevance_Concurrent(GetScene().GetFeatureLevel());
	MaterialRelevance |= GetDefault<UUnrealDriveSettings>()->SidewalkMatrtial->GetRelevance_Concurrent(GetScene().GetFeatureLevel());
	MaterialRelevance |= GetDefault<UUnrealDriveSettings>()->SelectedLaneMatrtial->GetRelevance_Concurrent(GetScene().GetFeatureLevel());
	MaterialRelevance |= GetDefault<UUnrealDriveSettings>()->EmptyLaneMatrtial->GetRelevance_Concurrent(GetScene().GetFeatureLevel());
	MaterialRelevance |= GetDefault<UUnrealDriveSettings>()->HiddenLaneMatrtial->GetRelevance_Concurrent(GetScene().GetFeatureLevel());

	/*
	static const int NumStep = 6;
	const float Step = Component->GetSplineLength() / NumStep;

	for (int i = 1; i < NumStep; ++i)
	{
		FTransform Transform = Component->GetTransformAtDistanceAlongSpline(i * Step, ESplineCoordinateSpace::Local);
		ArrawLines.Add({ Transform.GetLocation(), (Transform.GetRotation() * FQuat::MakeFromRotator(FRotator(0, +150, 0))).GetForwardVector() });
		ArrawLines.Add({ Transform.GetLocation(), (Transform.GetRotation() * FQuat::MakeFromRotator(FRotator(0, -150, 0))).GetForwardVector() });
	}
	*/

	TriProxy = MakeShared<UnrealDrive::FTriProxy>(Component->GetTransformAtDistanceAlongSpline(0.0, ESplineCoordinateSpace::Local), GetScene().GetFeatureLevel());
}

FRoadSplineSceneProxy::~FRoadSplineSceneProxy()
{
}

void FRoadSplineSceneProxy::GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const
{
	const FMatrix& MyLocalToWorld = GetLocalToWorld();

	TTuple<int, int> SelectedLane =  RoadSpline->GetSelectedLane();

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		if (VisibilityMap & (1 << ViewIndex))
		{
			const FSceneView* View = Views[ViewIndex];
			FPrimitiveDrawInterface* PDI = Collector.GetPDI(ViewIndex);

			for (const auto& Lane : LanesProxies)
			{
				if (FMeshBatch* MeshBatch = Lane->GetDynamicMeshElements(*this, View, ViewFamily, PDI, Collector))
				{
					if (bIsMultiRoad && IsParentSelected() && !IsIndividuallySelected())
					{
						MeshBatch->MaterialRenderProxy = GetDefault<UUnrealDriveSettings>()->HiddenLaneMatrtial->GetRenderProxy();
					}
					else if(Lane->SectionIndex != INDEX_NONE && Lane->SectionIndex == SelectedLane.Get<0>() && Lane->LaneIndex == SelectedLane.Get<1>())
					{
						MeshBatch->MaterialRenderProxy = GetDefault<UUnrealDriveSettings>()->SelectedLaneMatrtial->GetRenderProxy();
					}
#if WITH_EDITOR
					else if (!IsParentSelected() && Subsystem && Subsystem->GetRoadSplineWasSelected())
					{
						auto UnselectedRoadMAterial = new UnrealDrive::FOpacityMaterialRenderProxy(MeshBatch->MaterialRenderProxy, 0.3);
						Collector.RegisterOneFrameMaterialProxy(UnselectedRoadMAterial);
						MeshBatch->MaterialRenderProxy = UnselectedRoadMAterial;
					}
#endif
					MeshBatch->DepthPriorityGroup = GetDepthPriorityGroup(View);

					Collector.AddMesh(ViewIndex, *MeshBatch);
				}

				Lane->DrawLines(MyLocalToWorld, PDI, IsSelected());
			}

			

			if (IsIndividuallySelected())
			{
				Collector.AddMesh(ViewIndex, *TriProxy->GetDynamicMeshElements(*this, View, ViewFamily, PDI, Collector));

				/*
				for (const auto& [Pos, Dir] : ArrawLines)
				{
					const FVector WorldPos1 = MyLocalToWorld.TransformPosition(Pos);
					const float ViewScale = static_cast<float>(View->WorldToScreen(WorldPos1).W * (4.0f / View->UnscaledViewRect.Width() / View->ViewMatrices.GetProjectionMatrix().M[0][0]));
					static const double ArrowSize = 7;
					const FVector WorldPos2 = MyLocalToWorld.TransformPosition(Pos + Dir * ArrowSize * ViewScale);
					PDI->DrawLine(WorldPos1, WorldPos2, FLinearColor::White, SDPG_Foreground, 0.0, 0.f, true);
				}
				*/
			}
		}
	}
}

void FRoadSplineSceneProxy::DrawStaticElements(FStaticPrimitiveDrawInterface* PDI)
{
	/*
	PDI->ReserveMemoryForMeshes(LanesProxies.Num());
	for (const auto& Lane : LanesProxies)
	{
		Lane->DrawStaticElements(*this, PDI);
	}
	*/
}

FPrimitiveViewRelevance FRoadSplineSceneProxy::GetViewRelevance(const FSceneView* View) const
{
	FPrimitiveViewRelevance Result;
	Result.bDrawRelevance = IsShown(View);
	Result.bShadowRelevance = false;// IsShadowCast(View);
	Result.bDynamicRelevance = true;
	//Result.bStaticRelevance = true;
	Result.bRenderInMainPass = ShouldRenderInMainPass();
	//Result.bUsesLightingChannels = GetLightingChannelMask() != GetDefaultLightingChannelMask();
	Result.bRenderCustomDepth = ShouldRenderCustomDepth();
	Result.bTranslucentSelfShadow = false;// bCastVolumetricTranslucentShadow;
	Result.bVelocityRelevance = false;// DrawsVelocity() && Result.bOpaque&& Result.bRenderInMainPass;
	//Result.bEditorNoDepthTestPrimitiveRelevance = true;
	Result.bEditorPrimitiveRelevance = UseEditorCompositing(View);
	//Result.bEditorStaticSelectionRelevance = true;
	MaterialRelevance.SetPrimitiveViewRelevance(Result);

	return Result;
}

bool FRoadSplineSceneProxy::CanBeOccluded() const
{
	return !MaterialRelevance.bDisableDepthTest;
}

HHitProxy* FRoadSplineSceneProxy::CreateHitProxies(UPrimitiveComponent* Component, TArray<TRefCountPtr<HHitProxy> >& OutHitProxies)
{
	HHitProxy* DefaultHitProxy = FPrimitiveSceneProxy::CreateHitProxies(Component, OutHitProxies);

#if WITH_EDITOR
	OutHitProxies.Reserve(OutHitProxies.Num() + LanesProxies.Num());

	for (auto & It : LanesProxies)
	{
		if (HRoadSplineVisProxy* HitProxy = It->CreateHitProxy(CastChecked<URoadSplineComponent>(Component)))
		{
			OutHitProxies.Add(HitProxy);
		}
	}
#endif
	return DefaultHitProxy;
}
