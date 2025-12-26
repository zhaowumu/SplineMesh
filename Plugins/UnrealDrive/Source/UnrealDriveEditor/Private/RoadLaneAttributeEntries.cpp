/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */


#include "RoadLaneAttributeEntries.h"
#include "Components/SplineMeshComponent.h"
#include "Utils/ObjectsCreationHelper.h"
#include "CustomSplineBuilder.h"

void FRoadLaneAttributeEntrySplineMesh::GenerateAsset(const FReferenceSplineMeshParams& SplineMeshParams, const  TInstancedStruct<FRoadLaneAttributeEntry>& AttributeEntry, FName AttributeEntryName, AActor* TargetActor, bool bIsPreview) const
{
	if (StaticMesh)
	{
		USplineMeshComponent* NewComponent = NewObject<USplineMeshComponent>(TargetActor, *ObjectsCreationHelper::GenerateValidComponentName(AttributeEntryName.ToString(), TargetActor), bIsPreview ? RF_NoFlags : RF_Transactional);
		NewComponent->SetupAttachment(TargetActor->GetRootComponent());
		TargetActor->AddInstanceComponent(NewComponent);
		NewComponent->SetMobility(EComponentMobility::Static);
		NewComponent->OnComponentCreated();
		if (bIsPreview) NewComponent->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		NewComponent->RegisterComponent();
		NewComponent->SplineParams = SplineMeshParams;
		NewComponent->SetStaticMesh(StaticMesh.Get());
		NewComponent->UpdateRenderStateAndCollision();
	}
}

void FRoadLaneAttributeEntryComponentTemplate::GenerateAsset(const FReferenceSplineMeshParams& SplineMeshParams, const  TInstancedStruct<FRoadLaneAttributeEntry>& AttributeEntry, FName AttributeEntryName, AActor* TargetActor, bool bIsPreview) const
{
	if (ComponentTemplate != nullptr)
	{
		const FTransform Transform = UCustomSplineBuilder::CalcSliceTransformAtSplineOffset(SplineMeshParams, ComponentToSegmentAlign);
		USceneComponent* NewComponent = NewObject<USceneComponent>(TargetActor, ComponentTemplate.Get(), *ObjectsCreationHelper::GenerateValidComponentName(AttributeEntryName.ToString(), TargetActor), bIsPreview ? RF_NoFlags : RF_Transactional);
		NewComponent->SetupAttachment(TargetActor->GetRootComponent());
		TargetActor->AddInstanceComponent(NewComponent);
		NewComponent->SetMobility(EComponentMobility::Static);
		NewComponent->OnComponentCreated();
		if (bIsPreview)
		{
			if (UPrimitiveComponent* MewPrimitiveComponent = Cast<UPrimitiveComponent>(NewComponent))
			{
				MewPrimitiveComponent->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
			}
		}
		NewComponent->RegisterComponent();
		if (USplineMeshComponent* NewSplineComponent = Cast<USplineMeshComponent>(NewComponent))
		{
			NewSplineComponent->SplineParams = SplineMeshParams;
			NewSplineComponent->UpdateRenderStateAndCollision();
		}
		NewComponent->SetRelativeTransform(Transform);
	}
}

void FRoadLaneAttributeEntryCustomBuilder::GenerateAsset(const FReferenceSplineMeshParams& SplineMeshParams, const  TInstancedStruct<FRoadLaneAttributeEntry>& AttributeEntry, FName AttributeEntryName, AActor* TargetActor, bool bIsPreview) const
{
	if (CustomBuilder)
	{
		CastChecked<UCustomSplineBuilder>(CustomBuilder->GetDefaultObject())->GenerateAsset(SplineMeshParams, AttributeEntry, AttributeEntryName, TargetActor, bIsPreview);
	}
}