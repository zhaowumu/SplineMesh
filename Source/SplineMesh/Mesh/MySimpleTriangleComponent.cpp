#include "MySimpleTriangleComponent.h"

#include "PrimitiveSceneProxy.h"
#include "DynamicMeshBuilder.h"
#include "Materials/Material.h"
#include "SceneManagement.h"

//////////////////////////////////////////////////////////////////////////

class FMySimpleTriangleSceneProxy final : public FPrimitiveSceneProxy
{
public:
	FMySimpleTriangleSceneProxy(const UMySimpleTriangleComponent* Comp)
		: FPrimitiveSceneProxy(Comp)
		  , Size(Comp->Size)
	{
		Material = Comp->Material
			           ? Comp->Material
			           : UMaterial::GetDefaultMaterial(MD_Surface);
	}

	virtual SIZE_T GetTypeHash() const override
	{
		static size_t UniquePointer;
		return reinterpret_cast<SIZE_T>(&UniquePointer);
	}

	virtual void GetDynamicMeshElements(
		const TArray<const FSceneView*>& Views,
		const FSceneViewFamily& ViewFamily,
		uint32 VisibilityMap,
		FMeshElementCollector& Collector
	) const override
	{
		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			if (!(VisibilityMap & (1 << ViewIndex)))
				continue;

			FDynamicMeshBuilder MeshBuilder(ViewFamily.GetFeatureLevel());

			MeshBuilder.AddVertex(
				FVector3f(0, 0, 0),
				FVector2f(0, 0),
				FVector3f(1, 0, 0),
				FVector3f(0, 1, 0),
				FVector3f(0, 0, 1),
				FColor::Red);

			MeshBuilder.AddVertex(
				FVector3f(Size, 0, 0),
				FVector2f(1, 0),
				FVector3f(1, 0, 0),
				FVector3f(0, 1, 0),
				FVector3f(0, 0, 1),
				FColor::Red
			);

			MeshBuilder.AddVertex(
				FVector3f(0, 0, Size),
				FVector2f(0, 1),
				FVector3f(1, 0, 0),
				FVector3f(0, 1, 0),
				FVector3f(0, 0, 1),
				FColor::Red
			);

			MeshBuilder.AddTriangle(0, 1, 2);

			MeshBuilder.GetMesh(
				GetLocalToWorld(),
				Material->GetRenderProxy(),
				SDPG_World,
				false,
				false,
				ViewIndex,
				Collector
			);
		}
	}

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
	{
		FPrimitiveViewRelevance R;
		R.bDrawRelevance = true;
		R.bDynamicRelevance = true;
		R.bRenderInMainPass = true;
		R.bOpaque = true;
		return R;
	}

	virtual uint32 GetMemoryFootprint() const override
	{
		return sizeof(*this);
	}

private:
	float Size;
	UMaterialInterface* Material;
};

//////////////////////////////////////////////////////////////////////////

UMySimpleTriangleComponent::UMySimpleTriangleComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

FPrimitiveSceneProxy* UMySimpleTriangleComponent::CreateSceneProxy()
{
	return new FMySimpleTriangleSceneProxy(this);
}

FBoxSphereBounds UMySimpleTriangleComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	return FBoxSphereBounds(FBox(
		FVector(0, -10, 0),
		FVector(Size, 10, Size)
	)).TransformBy(LocalToWorld);
}

void UMySimpleTriangleComponent::GetUsedMaterials(
	TArray<UMaterialInterface*>& OutMaterials,
	bool bGetDebugMaterials
) const
{
	OutMaterials.Add(
		Material ? Material : UMaterial::GetDefaultMaterial(MD_Surface)
	);
}

#if WITH_EDITOR
void UMySimpleTriangleComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	MarkRenderStateDirty();
}
#endif
