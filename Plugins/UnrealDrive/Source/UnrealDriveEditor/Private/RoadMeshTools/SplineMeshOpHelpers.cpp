/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "RoadMeshTools/SplineMeshOpHelpers.h"
#include "ModelingTools/TriangulateRoadTool.h"
#include "ModelingTools/Ops/TriangulateRoadOp.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/Notifications/NotificationManager.h"
#include "UnrealDrive.h"

#define LOCTEXT_NAMESPACE "MeshOpPreviewHelpers"

using namespace UE::Geometry;

namespace UE::Private::MeshOpPreviewLocal
{
	static void DisplayCriticalWarningMessage(const FText& InMessage, float ExpireDuration = 5.0f)
	{
#if WITH_EDITOR
		FNotificationInfo Info(InMessage);
		Info.ExpireDuration = ExpireDuration;
		FSlateNotificationManager::Get().AddNotification(Info);
#endif

		UE_LOG(LogUnrealDrive, Warning, TEXT("%s"), *InMessage.ToString());
	}

	static TAutoConsoleVariable<int32> CVarOverrideMaxBackgroundTasks(
		TEXT("modeling.MaxBackgroundTasksOverride"), 0,
		TEXT("Optional override for maximum allowed background tasks when generating preview results in tools. 0 to use default values. [def: 0]"));

	int32 MaxActiveBackgroundTasksWithOverride(int32 MaxWithoutOverride)
	{
		int32 Override = CVarOverrideMaxBackgroundTasks.GetValueOnAnyThread();
		return Override > 0 ? Override : MaxWithoutOverride;
	}
}


void USplineMeshOpPreviewWithBackgroundCompute::Setup(UWorld* InWorld)
{
	PreviewMesh = NewObject<USplineMeshPreview>(this, TEXT("PreviewMesh"));
	PreviewMesh->CreateInWorld(InWorld, FTransform::Identity);
	PreviewWorld = InWorld;
	bResultValid = false;
	bMeshInitialized = false;
}

void USplineMeshOpPreviewWithBackgroundCompute::Setup(UWorld* InWorld, UnrealDrive::ISplineMeshOperatorFactory* OpGenerator)
{
	Setup(InWorld);
	BackgroundCompute = MakeUnique<UnrealDrive::FBackgroundSplineMeshComputeSource>(OpGenerator);
	BackgroundCompute->MaxActiveTaskCount = UE::Private::MeshOpPreviewLocal::MaxActiveBackgroundTasksWithOverride(MaxActiveBackgroundTasks);
}

void USplineMeshOpPreviewWithBackgroundCompute::ChangeOpFactory(UnrealDrive::ISplineMeshOperatorFactory* OpGenerator)
{
	CancelCompute();
	BackgroundCompute = MakeUnique<UnrealDrive::FBackgroundSplineMeshComputeSource>(OpGenerator);
	BackgroundCompute->MaxActiveTaskCount = UE::Private::MeshOpPreviewLocal::MaxActiveBackgroundTasksWithOverride(MaxActiveBackgroundTasks);
	bResultValid = false;
	bMeshInitialized = false;
}

void USplineMeshOpPreviewWithBackgroundCompute::ClearOpFactory()
{
	CancelCompute();
	BackgroundCompute = nullptr;
	bResultValid = false;
	bMeshInitialized = false;
}


UnrealDrive::FSplineMeshOpResult USplineMeshOpPreviewWithBackgroundCompute::Shutdown()
{
	CancelCompute();

	UnrealDrive::FSplineMeshOpResult Result;
	Result.MeshSegments = PreviewMesh->ExtractMeshSegments();
	Result.Transform = FTransformSRT3d(PreviewMesh->GetTransform());

	PreviewMesh->SetVisible(false);
	PreviewMesh->Disconnect();
	PreviewMesh = nullptr;

	PreviewWorld = nullptr;

	return Result;
}

void USplineMeshOpPreviewWithBackgroundCompute::CancelCompute()
{
	if (BackgroundCompute)
	{
		BackgroundCompute->CancelActiveCompute();
	}
}

void USplineMeshOpPreviewWithBackgroundCompute::Cancel()
{
	CancelCompute();

	PreviewMesh->SetVisible(false);
	PreviewMesh->Disconnect();
	PreviewMesh = nullptr;
}

void USplineMeshOpPreviewWithBackgroundCompute::Tick(float DeltaTime)
{
	if (BackgroundCompute)
	{
		BackgroundCompute->Tick(DeltaTime);
		UpdateResults();
	}

}

void USplineMeshOpPreviewWithBackgroundCompute::SetMaxActiveBackgroundTasks(int32 InMaxActiveBackgroundTasks)
{
	MaxActiveBackgroundTasks = InMaxActiveBackgroundTasks;
	if (BackgroundCompute)
	{
		BackgroundCompute->MaxActiveTaskCount = UE::Private::MeshOpPreviewLocal::MaxActiveBackgroundTasksWithOverride(MaxActiveBackgroundTasks);
	}
}

void USplineMeshOpPreviewWithBackgroundCompute::UpdateResults()
{
	if (BackgroundCompute == nullptr)
	{
		LastComputeStatus = EBackgroundComputeTaskStatus::NotComputing;
		return;
	}

	UnrealDrive::FBackgroundSplineMeshComputeSource::FStatus Status = BackgroundCompute->CheckStatus();
	LastComputeStatus = Status.TaskStatus;

	if (LastComputeStatus == EBackgroundComputeTaskStatus::ValidResultAvailable || (bAllowDirtyResultUpdates && LastComputeStatus == EBackgroundComputeTaskStatus::DirtyResultAvailable))
	{
		TUniquePtr<UnrealDrive::FSplineMeshOperator> MeshOp = BackgroundCompute->ExtractResult();
		OnOpSplineMeshCompleted.Broadcast(MeshOp.Get());

		TUniquePtr<UnrealDrive::FSplineMeshSegments> ResultMesh = MeshOp->ExtractResult();
		PreviewMesh->SetTransform((FTransform)MeshOp->GetResultTransform());


		PreviewMesh->UpdatePreview(MoveTemp(ResultMesh));
		bMeshInitialized = true;

		PreviewMesh->SetVisible(bVisible);
		bResultValid = (LastComputeStatus == EBackgroundComputeTaskStatus::ValidResultAvailable);
		ValidResultComputeTimeSeconds = Status.ElapsedTime;

		OnMeshUpdated.Broadcast(this);

		bWaitingForBackgroundTasks = false;
	}
	else if (int WaitingTaskCount; BackgroundCompute->IsWaitingForBackgroundTasks(WaitingTaskCount))
	{
		if (!bWaitingForBackgroundTasks)
		{
			UE::Private::MeshOpPreviewLocal::DisplayCriticalWarningMessage(LOCTEXT("TooManyBackgroundTasks", "Too many background tasks: Cancelling earlier tasks before generating new preview."));
			bWaitingForBackgroundTasks = true;
		}
	}
	else
	{
		bWaitingForBackgroundTasks = false;
	}
}

void USplineMeshOpPreviewWithBackgroundCompute::InvalidateResult()
{
	if (BackgroundCompute)
	{
		BackgroundCompute->NotifyActiveComputeInvalidated();
	}
	bResultValid = false;
}

void USplineMeshOpPreviewWithBackgroundCompute::SetVisibility(bool bVisibleIn)
{
	bVisible = bVisibleIn;
	PreviewMesh->SetVisible(bVisible);
}

void USplineMeshOpPreviewWithBackgroundCompute::Setup(UTriangulateRoadTool* RoadTool, TWeakPtr<UnrealDrive::FRoadActorComputeScope> RoadComputeScope, UnrealDrive::ISplineMeshOperatorFactory* OpFactory)
{
	Setup(RoadTool->GetTargetWorld(), OpFactory);
	//OpFactory->RoadTool = RoadTool;
	//OpFactory->ActorData = RoadComputeScope;
	//PreviewMesh->EnableWireframe(RoadTool->TriangulateProperties->bShowWireframe);
	OnOpSplineMeshCompleted.AddLambda(
		[this, RoadComputeScope, RoadTool = TWeakObjectPtr<UTriangulateRoadTool>(RoadTool)](const UnrealDrive::FSplineMeshOperator* Op)
		{
			if (!Op)
			{
				return;
			}

			auto& ActorData = *RoadComputeScope.Pin().Get();
			ActorData.AppendResultInfo(Op->GetResultInfo());

			if (!Op->GetResultInfo().HasResult())
			{
				return;
			}
		}
	);
	OnMeshUpdated.AddLambda(
		[RoadTool = TWeakObjectPtr<UTriangulateRoadTool>(RoadTool)](const USplineMeshOpPreviewWithBackgroundCompute* UpdatedPreview)
		{
			RoadTool->GetToolManager()->PostInvalidation();
			RoadTool->NotifyOpWasUpdated();
		}
	);
}

void USplineMeshOpPreviewWithBackgroundCompute::ShutdownAndGenerateAssets(AActor* TargetActor, const FTransform3d& ActorToWorld)
{
	if (!HaveValidNonEmptyResult())
	{
		Cancel();
		return;
	}

	auto OpResult = Shutdown();
	if (OpResult.MeshSegments.IsValid())
	{
		OpResult.MeshSegments->ApplyTransform(OpResult.Transform);
		OpResult.MeshSegments->ApplyTransformInverse(ActorToWorld);
		OpResult.MeshSegments->BuildComponents(TargetActor, false);
	}
}

#undef LOCTEXT_NAMESPACE
