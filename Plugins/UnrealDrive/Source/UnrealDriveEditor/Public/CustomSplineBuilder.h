/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "CoreMinimal.h"
#include "StructUtils/InstancedStruct.h"
#include "RoadLaneAttributeEntries.h"
#include "Components/SplineMeshComponent.h"
#include "CustomSplineBuilder.generated.h"

class UActorComponent;
class AActor;


USTRUCT(BlueprintType)
struct FReferenceSplineMeshParams
{
	GENERATED_USTRUCT_BODY()

	FReferenceSplineMeshParams() = default;
	FReferenceSplineMeshParams(const FSplineMeshParams& Other);

	/** Start location of spline, in component space. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SplineMesh)
	FVector StartPos{};

	/** Start tangent of spline, in component space. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SplineMesh)
	FVector StartTangent{};

	/** X and Y scale applied to mesh at start of spline. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SplineMesh, AdvancedDisplay)
	FVector2D StartScale{ 1.0 };

	/** Roll around spline applied at start, in radians. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SplineMesh, AdvancedDisplay)
	float StartRoll{};

	/** Roll around spline applied at end, in radians. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SplineMesh, AdvancedDisplay, meta = (DisplayAfter = "EndTangent"))
	float EndRoll{};

	/** Starting offset of the mesh from the spline, in component space. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SplineMesh, AdvancedDisplay)
	FVector2D StartOffset{};

	/** End location of spline, in component space. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SplineMesh)
	FVector EndPos{};

	/** X and Y scale applied to mesh at end of spline. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SplineMesh, AdvancedDisplay)
	FVector2D EndScale{ 1.0 };

	/** End tangent of spline, in component space. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SplineMesh)
	FVector EndTangent{};

	/** Ending offset of the mesh from the spline, in component space. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SplineMesh, AdvancedDisplay)
	FVector2D EndOffset{};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SplineMesh)
	bool bAlignWorldUpVector = false;

	operator FSplineMeshParams() const;
};


/**
 * UCustomSplineBuilder
 */
UCLASS(abstract, BlueprintType,  Blueprintable, meta = (BlueprintSpawnableComponent) )
class UNREALDRIVEEDITOR_API UCustomSplineBuilder : public UObject
{
	GENERATED_BODY()

public:
	virtual void GenerateAsset(const FReferenceSplineMeshParams& SplineMeshParams, const  TInstancedStruct<FRoadLaneAttributeEntry>& AttributeEntry, FName AttributeEntryName, AActor* TargetActor, bool bIsPreview) const
	{ 
		ReceiveGenerateAsset(SplineMeshParams, AttributeEntry, AttributeEntryName, TargetActor, bIsPreview);
	}

	UFUNCTION(BlueprintImplementableEvent, Category = "CustomSplineBuilder", meta = (DisplayName = "Generate Asset"))
	void ReceiveGenerateAsset(const FReferenceSplineMeshParams& SplineMeshParams, const  TInstancedStruct<FRoadLaneAttributeEntry>& AttributeEntry, FName AttributeEntryName, AActor* TargetActor, bool bIsPreview) const;

	UFUNCTION(BlueprintCallable, Category = "CustomSplineBuilder")
	static FTransform CalcSliceTransformAtSplineOffset(const FReferenceSplineMeshParams& SplineMeshParams, const float Alpha, const float MinT = 0.0, const float MaxT = 1.0);

};