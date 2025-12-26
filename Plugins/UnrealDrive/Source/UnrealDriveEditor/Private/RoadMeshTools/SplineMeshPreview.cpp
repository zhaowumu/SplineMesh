/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "RoadMeshTools/SplineMeshPreview.h"
#include "Engine/World.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SplineMeshPreview)

using namespace UE::Geometry;


USplineMeshPreview::USplineMeshPreview()
{
}

USplineMeshPreview::~USplineMeshPreview()
{
	checkf(TemporaryParentActor == nullptr, TEXT("You must explicitly Disconnect() USplineMeshPreview before it is GCd"));
}

void USplineMeshPreview::CreateInWorld(UWorld* World, const FTransform& WithTransform)
{
	FRotator Rotation(0.0f, 0.0f, 0.0f);
	FActorSpawnParameters SpawnInfo;
	TemporaryParentActor = World->SpawnActor<APreviewMeshActor>(FVector::ZeroVector, Rotation, SpawnInfo);

	USceneComponent* RootComponent = NewObject<USceneComponent>(TemporaryParentActor);
	RootComponent->SetMobility(EComponentMobility::Static);
	TemporaryParentActor->AddOwnedComponent(RootComponent);
	TemporaryParentActor->SetRootComponent(RootComponent);
	RootComponent->RegisterComponent();

	TemporaryParentActor->SetActorTransform(WithTransform);
}


void USplineMeshPreview::Disconnect()
{

	if (TemporaryParentActor != nullptr)
	{
		TArray<AActor*> AttachedActors;
		TemporaryParentActor->GetAttachedActors(AttachedActors, true, true);
		for (auto& It : AttachedActors)
		{
			It->Destroy();
		}

		TArray<USceneComponent*> SceneComponents;
		TemporaryParentActor->GetComponents(SceneComponents);
		for (auto* Component : SceneComponents)
		{
			if (Component != TemporaryParentActor->GetRootComponent())
			{
				TemporaryParentActor->RemoveOwnedComponent(Component);
				Component->UnregisterComponent();
				Component->DestroyComponent();
			}
		}

		TemporaryParentActor->RemoveOwnedComponent(TemporaryParentActor->GetRootComponent());
		TemporaryParentActor->GetRootComponent()->UnregisterComponent();
		TemporaryParentActor->GetRootComponent()->DestroyComponent();

		TemporaryParentActor->Destroy();
		TemporaryParentActor = nullptr;
	}
}

FTransform USplineMeshPreview::GetTransform() const
{
	if (TemporaryParentActor != nullptr)
	{
		return TemporaryParentActor->GetTransform();
	}
	return FTransform();
}

void USplineMeshPreview::SetTransform(const FTransform& UseTransform)
{
	if (TemporaryParentActor != nullptr)
	{
		if (!TemporaryParentActor->GetActorTransform().Equals(UseTransform))
		{
			TemporaryParentActor->SetActorTransform(UseTransform);
			NotifyWorldPathTracedOutputInvalidated();
		}
	}
}

void USplineMeshPreview::SetVisible(bool bVisible)
{
	TemporaryParentActor->ForEachComponent<USceneComponent>(true, [this, bVisible](USceneComponent* Component)
	{
		if (Component->IsVisible() != bVisible && Component != TemporaryParentActor->GetRootComponent())
		{
			Component->SetVisibility(bVisible, true);
			NotifyWorldPathTracedOutputInvalidated();
		}
	});
}

/*
bool USplineMeshPreview::IsVisible() const
{
	if (DynamicMeshComponent != nullptr)
	{
		return DynamicMeshComponent->IsVisible();
	}
	return false;
}
*/

void USplineMeshPreview::ClearPreview() 
{
	UpdatePreview({});
}


void USplineMeshPreview::UpdatePreview(TUniquePtr<UnrealDrive::FSplineMeshSegments>&& InMeshSegments)
{
	MeshSegments = MoveTemp(InMeshSegments);
	UpdatePreview();
}

void USplineMeshPreview::NotifyWorldPathTracedOutputInvalidated()
{
	if (TemporaryParentActor != nullptr)
	{
		UWorld* World = TemporaryParentActor->GetWorld();
		if (World && World->Scene && FApp::CanEverRender())
		{
			World->Scene->InvalidatePathTracedOutput();
		}
	}
}

void USplineMeshPreview::UpdatePreview()
{
	TArray<USceneComponent*> SceneComponents;
	TemporaryParentActor->GetComponents(SceneComponents);
	for(auto* Component: SceneComponents)
	{
		if (Component != TemporaryParentActor->GetRootComponent())
		{
			TemporaryParentActor->RemoveOwnedComponent(Component);
			Component->UnregisterComponent();
			Component->DestroyComponent();
		}
	}
	MeshSegments->BuildComponents(TemporaryParentActor, true);
}

