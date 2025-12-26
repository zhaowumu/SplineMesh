/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "UnrealDriveSubsystem.h"
#include "RoadSplineComponent.h"
#include "EngineUtils.h"
#include "UnrealDrive.h"
#include "SceneView.h"

#if WITH_EDITOR

#include "Selection.h"

void UUnrealDriveSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	if (GetWorld()->WorldType == EWorldType::Editor)
	{
		GEngine->OnLevelActorAdded().AddUObject(this, &UUnrealDriveSubsystem::OnActorSpawned);
		GEngine->OnLevelActorDeleted().AddUObject(this, &UUnrealDriveSubsystem::OnActorDeleted, true);

		//FEditorDelegates::OnEditCutActorsBegin.AddRaw(this, &UUnrealDriveSubsystem::OnEditCutActorsBegin);
		//FEditorDelegates::OnEditCutActorsEnd.AddRaw(this, &UUnrealDriveSubsystem::OnEditCutActorsEnd);
		// 
		//FEditorDelegates::OnEditCopyActorsBegin.AddRaw(this, &UUnrealDriveSubsystem::OnEditCopyActorsBegin);
		//FEditorDelegates::OnEditCopyActorsEnd.AddRaw(this, &UUnrealDriveSubsystem::OnEditCopyActorsEnd);

		FEditorDelegates::OnDuplicateActorsBegin.AddUObject(this, &UUnrealDriveSubsystem::OnDuplicateActorsBegin);
		FEditorDelegates::OnDuplicateActorsEnd.AddUObject(this, &UUnrealDriveSubsystem::OnDuplicateActorsEnd);

		FEditorDelegates::OnEditPasteActorsBegin.AddUObject(this, &UUnrealDriveSubsystem::OnDuplicateActorsBegin);
		FEditorDelegates::OnEditPasteActorsEnd.AddUObject(this, &UUnrealDriveSubsystem::OnDuplicateActorsEnd);
	}

	bRoadSplineWasSelected = false;

	Super::Initialize(Collection);
}

void UUnrealDriveSubsystem::Deinitialize()
{
	Super::Deinitialize();

	if (GetWorld()->WorldType == EWorldType::Editor)
	{
		GEngine->OnLevelActorAdded().RemoveAll(this);
		GEngine->OnLevelActorDeleted().RemoveAll(this);

		FEditorDelegates::OnDuplicateActorsBegin.RemoveAll(this);
		FEditorDelegates::OnDuplicateActorsEnd.RemoveAll(this);

		FEditorDelegates::OnEditPasteActorsBegin.RemoveAll(this);
		FEditorDelegates::OnEditPasteActorsEnd.RemoveAll(this);
	}

	bRoadSplineWasSelected = false;

}

void UUnrealDriveSubsystem::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	bool bWasSelected = false;
	for (FSelectionIterator Iter = GEditor->GetSelectedComponentIterator(); Iter; ++Iter)
	{
		if (Cast<URoadSplineComponent>(*Iter))
		{
			bWasSelected = true;
			break;
		}
	}
	bRoadSplineWasSelected = bWasSelected;
	
}

TStatId UUnrealDriveSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UUnrealDriveSubsystem, STATGROUP_Tickables);
}

void UUnrealDriveSubsystem::OnActorSpawned(AActor* InActor)
{
	if (bDuplicationStarted)
	{
		DuplicatedActors.Add(InActor);
	}
}

void UUnrealDriveSubsystem::OnActorDeleted(AActor* InActor, bool bClearStencilIdValues)
{
}

void UUnrealDriveSubsystem::OnDuplicateActorsBegin()
{
	bDuplicationStarted = true;

	// Generate of new GUID for all ULaneConnection on the level
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		It->ForEachComponent<URoadSplineComponent>(true, [&](const URoadSplineComponent* Comp)
		{
			for (auto& Section : Comp->GetLaneSections())
			{
				for (auto& Lane : Section.Left)
				{
					Lane.PredecessorConnection->Guid = FGuid::NewGuid();
					Lane.SuccessorConnection->Guid = FGuid::NewGuid();
				}
				for (auto& Lane : Section.Right)
				{
					Lane.PredecessorConnection->Guid = FGuid::NewGuid();
					Lane.SuccessorConnection->Guid = FGuid::NewGuid();
				}
			}
		});
	}

	// Set LaneConnectionGuid for all connected URoadConnection on the level
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		It->ForEachComponent<URoadSplineComponent>(true, [&](const URoadSplineComponent* Comp)
		{
			if(Comp->GetPredecessorConnection()->IsConnected())
			{
				Comp->GetPredecessorConnection()->LaneConnectionGuid = Comp->GetPredecessorConnection()->GetOuterConnection()->Guid;
			}
			else
			{
				Comp->GetPredecessorConnection()->LaneConnectionGuid = FGuid{};
			}

			if (Comp->GetSuccessorConnection()->IsConnected())
			{
				Comp->GetSuccessorConnection()->LaneConnectionGuid = Comp->GetSuccessorConnection()->GetOuterConnection()->Guid;
			}
			else
			{
				Comp->GetSuccessorConnection()->LaneConnectionGuid = FGuid{};
			}
		});
	}
}

void UUnrealDriveSubsystem::OnDuplicateActorsEnd()
{
	bDuplicationStarted = false;


	TMap<FGuid, ULaneConnection*> Links;

	for (auto& DupicatedActor : DuplicatedActors)
	{
		DupicatedActor->ForEachComponent<URoadSplineComponent>(true, [&](const URoadSplineComponent* Comp)
		{
			for (auto& Section : Comp->GetLaneSections())
			{
				for (auto& Lane : Section.Left)
				{
					Links.Add(Lane.PredecessorConnection->Guid, Lane.PredecessorConnection);
					Links.Add(Lane.SuccessorConnection->Guid, Lane.SuccessorConnection);
				}
				for (auto& Lane : Section.Right)
				{
					Links.Add(Lane.PredecessorConnection->Guid, Lane.PredecessorConnection);
					Links.Add(Lane.SuccessorConnection->Guid, Lane.SuccessorConnection);
				}
			}
		});
	}

	for (auto& DupicatedActor : DuplicatedActors)
	{
		DupicatedActor->ForEachComponent<URoadSplineComponent>(true, [&](const URoadSplineComponent* Comp)
		{
			if(Comp->GetPredecessorConnection()->LaneConnectionGuid.IsValid())
			{
				if (auto* Conn = Links.Find(Comp->GetPredecessorConnection()->LaneConnectionGuid))
				{
					Comp->GetPredecessorConnection()->ConnectTo(*Conn);
				}
			}
			if (Comp->GetSuccessorConnection()->LaneConnectionGuid.IsValid())
			{
				if (auto* Conn = Links.Find(Comp->GetSuccessorConnection()->LaneConnectionGuid))
				{
					Comp->GetSuccessorConnection()->ConnectTo(*Conn);
				}
			}
		});
	}

	DuplicatedActors.Empty();
}

void UUnrealDriveSubsystem::BeginCopyRoadsTransaction()
{
}

void UUnrealDriveSubsystem::EndCopyRoadTransaction()
{
}

void UUnrealDriveSubsystem::UpdateObservedConnections(const URoadSplineComponent* RoadSpline)
{
	auto* Connections =  ObservedConnections.Find(RoadSpline);
	if (!Connections)
	{
		return;
	}

	auto UpdateConnection = [Connections](ULaneConnection* ProbeConnection)
	{
		if (auto* Info = Connections->Find(ProbeConnection))
		{
			Info->Transform = ProbeConnection->EvalTransform(0.5, ESplineCoordinateSpace::World);
		}
	};

	for (auto& Section : RoadSpline->GetLaneSections())
	{
		for (auto& Lane : Section.Left)
		{
			UpdateConnection(Lane.PredecessorConnection);
			UpdateConnection(Lane.SuccessorConnection);
		}
		for (auto& Lane : Section.Right)
		{
			UpdateConnection(Lane.PredecessorConnection);
			UpdateConnection(Lane.SuccessorConnection);
		}
	}
}

void UUnrealDriveSubsystem::CleanObservedConnections() 
{ 
	ObservedConnections.Empty();
}


void UUnrealDriveSubsystem::AddObservedConnections(const URoadSplineComponent* RoadSpline, const TArray<const ULaneConnection*>& Connections)
{
	auto& ConnectionsMap = ObservedConnections.FindOrAdd(RoadSpline);
	for (auto& Connection : Connections)
	{
		ConnectionsMap.FindOrAdd(Connection) = { Connection->EvalTransform(0.5, ESplineCoordinateSpace::World), false };
	}
}

void UUnrealDriveSubsystem::CaptureConnections(const URoadConnection* SrcConnection, const FViewCameraState& CameraState, double MaxViewDistance, double MaxOrthoWidth, TFunction<bool(const ULaneConnection*)> IsConnectionAllowed)
{
	ObservedConnections.Empty();

	if (CameraState.bIsOrthographic && CameraState.OrthoWorldCoordinateWidth > MaxOrthoWidth)
	{
		return;
	}

	int NumCaptured = 0;

	auto TryAddConnection = [&](ULaneConnection* ProbeConnection, TMap<TWeakObjectPtr<const ULaneConnection>, FConnectionInfo> & Connections)
	{
		check(ProbeConnection);
		if (!IsConnectionAllowed(ProbeConnection))
		{
			return false;
		}

		if (!SrcConnection->CanConnectTo(ProbeConnection))
		{
			return false;
		}

		FTransform Transform = ProbeConnection->EvalTransform(0.5, ESplineCoordinateSpace::World);
		if (!CameraState.bIsOrthographic)
		{
			if ((Transform.GetLocation() - CameraState.ViewPosition).Length() > MaxViewDistance)
			{
				return false;
			}
		}

		FVector2D ScreenPos;
		if (!FSceneView::ProjectWorldToScreen(Transform.GetLocation(), CameraState.ViewRect, CameraState.ViewToProj, ScreenPos))
		{
			return false;
		}

		if (ScreenPos.X < CameraState.ViewRect.Min.X || ScreenPos.X > CameraState.ViewRect.Max.X || ScreenPos.Y < CameraState.ViewRect.Min.Y || ScreenPos.Y > CameraState.ViewRect.Max.Y)
		{
			return false;
		}
		
		Connections.Emplace(ProbeConnection, { Transform, false });
		++NumCaptured;
		return true;
	};


	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		It->ForEachComponent<URoadSplineComponent>(true, [&](const URoadSplineComponent* Comp)
		{
			TMap<TWeakObjectPtr<const ULaneConnection>, FConnectionInfo> Connections;
			for (auto& Section : Comp->GetLaneSections())
			{
				for (auto& Lane : Section.Left)
				{
					TryAddConnection(Lane.PredecessorConnection, Connections);
					TryAddConnection(Lane.SuccessorConnection, Connections);
				}
				for (auto& Lane : Section.Right)
				{
					TryAddConnection(Lane.PredecessorConnection, Connections);
					TryAddConnection(Lane.SuccessorConnection, Connections);
				}
			}
			if (Connections.Num())
			{
				ObservedConnections.Add(Comp, MoveTemp(Connections));
			}
		});
	}

	UE_LOG(LogUnrealDrive, Log, TEXT("Captured %i connections"), NumCaptured);
}


void UUnrealDriveSubsystem::ForEachObservedConnection(TFunctionRef<void(const ULaneConnection*, FConnectionInfo&)> VisitorFunc)
{
	for (auto& [Spline, Connections] : ObservedConnections)
	{
		for (auto& [Connection, Info] : Connections)
		{
			if (auto* Ptr = Connection.Get())
			{
				if (IsValid(Ptr) && IsValid(Ptr->GetOwnedRoadSpline()))
				{
					VisitorFunc(Ptr, Info);
				}
			}
		}
	}
}

TPair<TWeakObjectPtr<const ULaneConnection>, UUnrealDriveSubsystem::FConnectionInfo>* UUnrealDriveSubsystem::FindObservedConnectionByPredicate(TFunctionRef<bool(const ULaneConnection*, const UUnrealDriveSubsystem::FConnectionInfo&) > VisitorFunc)
{
	for (auto& [Spline, Connections] : ObservedConnections)
	{
		
		for (auto& It : Connections)
		{
			if (auto* Ptr = It.Key.Get())
			{
				if (VisitorFunc(Ptr, It.Value))
				{
					if (IsValid(Ptr) && IsValid(Ptr->GetOwnedRoadSpline()))
					{
						return &It;
					}
				}
			}
		}
	}
	return nullptr;
}

#endif // WITH_EDITOR