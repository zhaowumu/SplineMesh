#include "MyRoadSolidSplineComponent.h"
#include "DynamicMeshBuilder.h"
#include "MeshElementCollector.h"
#include "MaterialShared.h"

/**
 * 道路渲染代理
 */
class FMyRoadSolidSceneProxy final : public FPrimitiveSceneProxy
{
public:
	FMyRoadSolidSceneProxy(UMyRoadSolidSplineComponent* Component)
		: FPrimitiveSceneProxy(Component)
		  , SPos(Component->StartPos)
		  , STan(Component->StartTangent)
		  , EPos(Component->EndPos)
		  , ETan(Component->EndTangent)
		  , Width(Component->RoadWidth)
		  , Segments(FMath::Max(1, Component->Segments))
		  , Material(Component->GetMaterial(0))
	{
	}

	virtual SIZE_T GetTypeHash() const override
	{
		static size_t UniquePointer;
		return reinterpret_cast<size_t>(&UniquePointer);
	}

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily,
	                                    uint32 VisibilityMap, FMeshElementCollector& Collector) const override
	{
		// 1. 准备材质代理 (如果为空则使用默认材质)
		FMaterialRenderProxy* MaterialProxy = Material
			                                      ? Material->GetRenderProxy()
			                                      : GEngine->DebugMeshMaterial->GetRenderProxy();
		const FMatrix& L2W = GetLocalToWorld();

		// Views: 虚幻支持多窗口渲染（比如：主视口、小地图、分屏游戏、阴影贴图渲染）。
		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			if (VisibilityMap & (1 << ViewIndex))
			{
				// 获取 PDI 用于绘制调试骨架
				FPrimitiveDrawInterface* PDI = Collector.GetPDI(ViewIndex);

				// 2. 初始化 MeshBuilder
				FDynamicMeshBuilder MeshBuilder(ViewFamily.GetFeatureLevel());

				float HalfWidth = Width * 0.5f;

				// 3. 生成顶点数据
				for (int32 i = 0; i <= Segments; ++i)
				{
					float t = (float)i / Segments;

					// --- 核心数学逻辑 ---
					// 计算当前中心点位置和切线方向
					FVector CurrPos = FMath::CubicInterp(SPos, STan, EPos, ETan, t);
					FVector CurrDir = FMath::CubicInterpDerivative(SPos, STan, EPos, ETan, t).GetSafeNormal();

					// 计算向右的向量：通过切线和上方向做外积
					FVector RightDir = FVector::CrossProduct(CurrDir, FVector::UpVector).GetSafeNormal();

					// 防御性检查：如果路是垂直往上的，RightDir 会失效，此时手动指定一个方向
					if (RightDir.IsNearlyZero())
					{
						RightDir = FVector::CrossProduct(CurrDir, FVector::ForwardVector).GetSafeNormal();
					}

					// --- 调试绘图 (PDI) ---
					// 把局部坐标转为世界坐标，画出蓝色的路面“肋骨”
					FVector WorldLeftEdge = L2W.TransformPosition(CurrPos - RightDir * HalfWidth);
					FVector WorldRightEdge = L2W.TransformPosition(CurrPos + RightDir * HalfWidth);
					PDI->DrawLine(WorldLeftEdge, WorldRightEdge, FColor::Blue, SDPG_World, 2.0f);

					// --- 构建顶点 (FDynamicMeshVertex) ---
					float V_Coord = t * 5.0f; // UV 沿路长重复 5 次

					FDynamicMeshVertex VLeft;
					VLeft.Position = (FVector3f)(CurrPos - RightDir * HalfWidth);
					VLeft.TextureCoordinate[0] = FVector2f(0.0f, V_Coord);
					VLeft.TangentX = (FVector3f)CurrDir;
					VLeft.TangentZ = (FVector3f)FVector::UpVector; // 法线朝上
					VLeft.Color = FColor::White;

					FDynamicMeshVertex VRight;
					VRight.Position = (FVector3f)(CurrPos + RightDir * HalfWidth);
					VRight.TextureCoordinate[0] = FVector2f(1.0f, V_Coord);
					VRight.TangentX = (FVector3f)CurrDir;
					VRight.TangentZ = (FVector3f)FVector::UpVector;
					VRight.Color = FColor::White;

					MeshBuilder.AddVertex(VLeft);
					MeshBuilder.AddVertex(VRight);

					// 4. 构建索引（连接成面）
					if (i > 0)
					{
						int32 Index_PrevLeft = (i - 1) * 2;
						int32 Index_PrevRight = (i - 1) * 2 + 1;
						int32 Index_CurLeft = i * 2;
						int32 Index_CurRight = i * 2 + 1;

						// 为了防止绕序问题导致看不见，我们先画正面，再画反面（双面渲染测试）
						// 正面 (顺时针)
						MeshBuilder.AddTriangle(Index_PrevLeft, Index_CurLeft, Index_PrevRight);
						MeshBuilder.AddTriangle(Index_PrevRight, Index_CurLeft, Index_CurRight);

						// 反面 (逆时针)
						MeshBuilder.AddTriangle(Index_PrevLeft, Index_PrevRight, Index_CurLeft);
						MeshBuilder.AddTriangle(Index_PrevRight, Index_CurRight, Index_CurLeft);
					}
				}

				// 5. 提交网格给渲染管线
				MeshBuilder.GetMesh(L2W, MaterialProxy, SDPG_World, false, false, ViewIndex, Collector);
			}
		}
	}

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
	{
		FPrimitiveViewRelevance Result;
		Result.bDrawRelevance = IsShown(View);
		Result.bDynamicRelevance = true;
		Result.bOpaque = true;
		Result.bNormalTranslucency = true;
		Result.bEditorPrimitiveRelevance = true;
		return Result;
	}

	virtual uint32 GetMemoryFootprint() const override { return sizeof(*this) + GetAllocatedSize(); }

private:
	FVector SPos, STan, EPos, ETan;
	float Width;
	int32 Segments;
	UMaterialInterface* Material;
};

// --- Component 实现 ---

UMyRoadSolidSplineComponent::UMyRoadSolidSplineComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	// 默认给点数据，防止一开始看不见
	StartPos = FVector(0, 0, 0);
	StartTangent = FVector(500, 0, 0);
	EndPos = FVector(1000, 500, 0);
	EndTangent = FVector(500, 0, 0);
	RoadWidth = 200.0f;
	Segments = 20;
}

FPrimitiveSceneProxy* UMyRoadSolidSplineComponent::CreateSceneProxy()
{
	return new FMyRoadSolidSceneProxy(this);
}

FBoxSphereBounds UMyRoadSolidSplineComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	// 这里非常重要：给一个足够大的包围盒！
	return FBoxSphereBounds(LocalToWorld.GetLocation(), FVector(2000.f, 2000.f, 1000.f), 2500.f);
}

#if WITH_EDITOR
void UMyRoadSolidSplineComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	MarkRenderStateDirty();
}
#endif
