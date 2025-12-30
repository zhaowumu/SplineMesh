// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Runtime/Engine/Private/Tests/AutoRTFM/AutoRTFMTestPrimitiveComponent.h"
#include "MyRoadSolidSplineComponent.generated.h"


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class SPLINEMESH_API UMyRoadSolidSplineComponent : public UPrimitiveComponent
{
	GENERATED_BODY()

public:
	UMyRoadSolidSplineComponent();

	// --- 道路几何参数 ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Road")
	FVector StartPos = FVector(0, 0, 0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Road")
	FVector StartTangent = FVector(500, 0, 0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Road")
	FVector EndPos = FVector(1000, 500, 0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Road")
	FVector EndTangent = FVector(500, 0, 0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Road")
	float RoadWidth = 400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Road")
	int32 Segments = 30; // 细分程度，越多越平滑

	// --- 材质 ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
	TObjectPtr<UMaterialInterface> RoadMaterial;

	// --- 重写函数 ---
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	virtual UMaterialInterface* GetMaterial(int32 ElementIndex) const override { return RoadMaterial ? RoadMaterial : GEngine->ClayMaterial; }

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
