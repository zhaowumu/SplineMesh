/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "ModelingTools/TriangulateRoadTool.h"
#include "Utils/ObjectsCreationHelper.h"
#include "InteractiveToolManager.h"
#include "ToolBuilderUtil.h"
#include "ToolSetupUtil.h"
#include "Selection/ToolSelectionUtil.h"
#include "DynamicMesh/MeshTransforms.h"
#include "Properties/MeshMaterialProperties.h"
#include "Engine/World.h"
#include "Components/SplineComponent.h"
#include "ActorFactories/ActorFactoryEmptyActor.h"
#include "UnrealDriveEditorModule.h"
#include "Ops/TriangulateRoadOp.h"
#include "UnrealDrive.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(TriangulateRoadTool)

#define LOCTEXT_NAMESPACE "UTriangulateRoadTool"

using namespace UE::Geometry;
using namespace UnrealDrive;


static UE::Geometry::EBackgroundComputeTaskStatus GetLastComputeStatus(TGenericDataBackgroundCompute<UnrealDrive::FRoadBaseOperatorData>& Compute)
{
	class FAccessor : public TGenericDataBackgroundCompute<UnrealDrive::FRoadBaseOperatorData>
	{
	public:
		UE::Geometry::EBackgroundComputeTaskStatus GetLastComputeStatus() const { return this->BackgroundCompute->CheckStatus().TaskStatus; }
	};

	return static_cast<FAccessor*>(&Compute)->GetLastComputeStatus();
}

void FRoadActorComputeScope::ShowReport() const
{
	int StatNumTringles = 0;
	int StatNewVertexes = 0;
	auto AddStat = [&StatNumTringles, &StatNewVertexes](const IRoadOpCompute& RoadOpCompute)
	{
		if (RoadOpCompute.HaveValidNonEmptyResult())
		{
			StatNewVertexes += RoadOpCompute.GetNumVertices();
			StatNumTringles += RoadOpCompute.GetNumTriangles();
		}
	};

	for (auto& It : OpComputes)
	{
		AddStat(*It);
	}

	UE_LOG(LogUnrealDrive, Log, TEXT("----------------- Generation Report: %s ----------------"), *TargetActor->GetActorLabel());
	UE_LOG(LogUnrealDrive, Log, TEXT("\t\t Num tringle: %i"), StatNumTringles);
	UE_LOG(LogUnrealDrive, Log, TEXT("\t\t Num vertexes: %i"), StatNewVertexes);
	//float ComputeTime = RoadComputeScope.ComputeBase->GetElapsedComputeTime();
	//UE_LOG(LogUnrealDrive, Log, TEXT("\t\t Compute time: %fs"), ComputeTime);
	if (ResultInfo.Errors.Num())
	{
		UE_LOG(LogUnrealDrive, Log,  TEXT("\t\t Error messages: "));
		for (auto& It : ResultInfo.Errors)
		{
			UE_LOG(LogUnrealDrive, Log, TEXT("\t\t\t %s"), *It.Message.ToString());
		}
	}
	if (ResultInfo.Warnings.Num())
	{
		UE_LOG(LogUnrealDrive, Log, TEXT("\t\t Warning messages: "));
		for (auto& It : ResultInfo.Warnings)
		{
			UE_LOG(LogUnrealDrive, Log, TEXT("\t\t\t %s"), *It.Message.ToString());
		}
	}
}

void FRoadActorComputeScope::NotifyRebuildOne(IRoadOpCompute& Preview)
{
	bNeedGenerateReport = true;
	ResultInfo = { EGeometryResultType::InProgress };
	Preview.CancelCompute();
	//Preview.SetVisibility(true);
	Preview.InvalidateResult();
}

void FRoadActorComputeScope::NotifyRebuildAll()
{
	BaseOpCompute->Cancel();

	for (auto& It : OpComputes)
	{
		It->CancelCompute();
	}

	ResultInfo = EGeometryResultType::InProgress;
	BaseData.Reset();
	bNeedGenerateReport = true;
	BaseOpCompute->InvalidateResult();
}

void FRoadActorComputeScope::AppendResultInfo(const FGeometryResult& InResult)
{
	ResultInfo.Errors.Append(InResult.Errors);
	ResultInfo.Warnings.Append(InResult.Warnings);
	ResultInfo.Result = FMath::Max(ResultInfo.Result, InResult.Result);
}

// -----------------------------------------------------------------------------------------------------


UTriangulateRoadTool::UTriangulateRoadTool()
{
}

void UTriangulateRoadTool::SetSplineActors(TArray<TWeakObjectPtr<AActor>> InSplineActors)
{
	for (auto& It : InSplineActors)
	{
		auto & NewItem = RoadsComputeScope.Add_GetRef(MakeShared<FRoadActorComputeScope>());
		NewItem->TargetActor = It;
	}
}

void UTriangulateRoadTool::Setup()
{
	UInteractiveTool::Setup();

	SetToolDisplayName(LOCTEXT("TriangulateRoadToolName", "Triangulate Roads"));
	TriangulateProperties = NewObject<UTriangulateRoadToolProperties>(this);
	TriangulateProperties->RestoreProperties(this);
	AddToolPropertySource(TriangulateProperties);
	//TriangulateProperties->WatchProperty(TriangulateProperties->ErrorTolerance, [this](double ErrorTolerance)
	//{
	//	PollRoadsUpdates(true);
	//});

	for (auto& RoadComputeScope: RoadsComputeScope)
	{
		auto BaseOpFactory = MakeUnique<FRoadBaseOperatorFactory>(); 
		BaseOpFactory->RoadTool = this;
		BaseOpFactory->RoadComputeScope = RoadComputeScope;
		RoadComputeScope->BaseOpCompute = MakeUnique<TGenericDataBackgroundCompute<UnrealDrive::FRoadBaseOperatorData>>();
		RoadComputeScope->BaseOpCompute->Setup(BaseOpFactory.Get());
		RoadComputeScope->BaseOpCompute->OnResultUpdated.AddLambda(
			[this , RoadComputeScopeWeak=RoadComputeScope.ToWeakPtr()](const TUniquePtr<UnrealDrive::FRoadBaseOperatorData>& Data)
			{
				GetToolManager()->PostInvalidation();
				NotifyOpWasUpdated();
				auto RoadComputeScope = RoadComputeScopeWeak.Pin();
				if (RoadComputeScope->BaseOpCompute->HaveValidResult())
				{
					RoadComputeScope->BaseData = TSharedPtr<UnrealDrive::FRoadBaseOperatorData>(RoadComputeScope->BaseOpCompute->Shutdown().Release());
					RoadComputeScope->ResultInfo = RoadComputeScope->BaseData->ResultInfo;
				}
				else
				{
					RoadComputeScope->BaseData.Reset();
					RoadComputeScope->ResultInfo = { EGeometryResultType::Failure };
				}
				for (auto& It : RoadComputeScope->OpComputes)
				{
					It->InvalidateResult();
				}
			}
		);

		auto& RoadComputeFactories = FUnrealDriveEditorModule::Get().GetRoadComputeFactories();
		RoadComputeScope->OpFactories.Add(MoveTemp(BaseOpFactory));
		RoadComputeScope->OpComputes.Reserve(RoadComputeFactories.Num());
		for (auto& [FactoryName, Factory] : RoadComputeFactories)
		{
			auto* RoadCompute = Factory.Execute(this, RoadComputeScope);
			check(RoadCompute);
			TStrongScriptInterface<IRoadOpCompute> Interface;
			Interface.SetObject(CastChecked<UObject>(RoadCompute));
			Interface.SetInterface(RoadCompute);
			RoadComputeScope->OpComputes.Add(MoveTemp(Interface));
		}
	}
}


void UTriangulateRoadTool::PollRoadsUpdates(bool bForce)
{
	for (int i = 0; i < RoadsComputeScope.Num(); ++i)
	{
		auto& RoadComputeScope = RoadsComputeScope[i];
		if (RoadComputeScope->bLostInputSpline)
		{
			continue;
		}

		bool bSplinesUpdated = false;
		bool bAttributesUpdated = false;

		int32 SplineIdx = 0;

		RoadComputeScope->TargetActor->ForEachComponent<URoadSplineComponent>(false, [&](URoadSplineComponent* SplineComponent)
		{
			uint64 RoadVersions = SplineComponent->GetSplineCurvesVersion() + SplineComponent->GetRoadLayout().GetLayoutVersion();
			uint64 RoadAttributesVersion = SplineComponent->GetRoadLayout().GetAttributesVersion();
			FTransform Transform = SplineComponent->GetComponentTransform();
			if (SplineIdx >= RoadComputeScope->SplineData.Num())
			{
				bSplinesUpdated = true;
				RoadComputeScope->SplineData.Add({ RoadVersions, RoadAttributesVersion, Transform });
			}
			else if (RoadComputeScope->SplineData[SplineIdx].LastRoadVersions != RoadVersions || !RoadComputeScope->SplineData[SplineIdx].LastSplineTransforms.Equals(Transform))
			{
				bSplinesUpdated = true;
			}

			if (!bSplinesUpdated && RoadComputeScope->SplineData[SplineIdx].LastRoadAttributesVersion != RoadAttributesVersion)
			{
				bAttributesUpdated = true;
			}

			RoadComputeScope->SplineData[SplineIdx].LastRoadVersions = RoadVersions;
			RoadComputeScope->SplineData[SplineIdx].LastRoadAttributesVersion = RoadAttributesVersion;
			RoadComputeScope->SplineData[SplineIdx].LastSplineTransforms = Transform;
			++SplineIdx;
		});

		if (RoadComputeScope->SplineData.Num() != SplineIdx)
		{
			if (SplineIdx < RoadComputeScope->SplineData.Num())
			{
				RoadComputeScope->bLostInputSpline = true;
				GetToolManager()->DisplayMessage(
					LOCTEXT("LostSpline", "Tool lost reference to an input spline; cannot respond to further spline changes."),
					EToolMessageLevel::UserWarning);
				return;
			}
			RoadComputeScope->SplineData.SetNum(SplineIdx);
			bSplinesUpdated = true;
		}

		if (bSplinesUpdated || bForce)
		{
			RoadComputeScope->NotifyRebuildAll();
		}
		else if (bAttributesUpdated)
		{
			if (RoadComputeScope->TargetActor.IsValid() && RoadComputeScope->BaseData)
			{
				// Update RoadSplinesCache
				TArray<const URoadSplineComponent*> Splines;
				RoadComputeScope->TargetActor->GetComponents(Splines);
				RoadComputeScope->BaseData->RoadSplinesCache.Empty();
				for (auto& Spline : Splines)
				{
					RoadComputeScope->BaseData->RoadSplinesCache.Emplace(Spline);
					RoadComputeScope->BaseData->RoadSplinesCache.Last().UpdateSplinesCurves2d();
				}
				RoadComputeScope->BaseData->DebugLines.Empty();

				for (auto& It : RoadComputeScope->OpComputes)
				{
					if (It->IsRoadAttribute())
					{
						RoadComputeScope->NotifyRebuildOne(*It);
					}
				}
			}
		}
	}
}


void UTriangulateRoadTool::OnTick(float DeltaTime)
{
	PollRoadsUpdates(false);

	for (auto & RoadComputeScope : RoadsComputeScope)
	{
		RoadComputeScope->BaseOpCompute->Tick(DeltaTime);

		for (auto& It : RoadComputeScope->OpComputes)
		{
			It->Tick(DeltaTime);
		}
	}

	bool bHasFaild = false;
	bool bHasWarnings = false;
	int NumTask = 0;
	bool bReportWasShown = false;

	static auto CheckStatus = [](EBackgroundComputeTaskStatus Status)
	{
		return Status != EBackgroundComputeTaskStatus::NotComputing;
	};

	for (auto& RoadComputeScope : RoadsComputeScope)
	{
		if (CheckStatus(GetLastComputeStatus(*RoadComputeScope->BaseOpCompute)))
		{
			++NumTask;
		}

		for (auto& It : RoadComputeScope->OpComputes)
		{
			if (CheckStatus(It->GetLastComputeStatus()))
			{
				++NumTask;
			}
		}

		if (NumTask == 0 && RoadComputeScope->bNeedGenerateReport)
		{
			RoadComputeScope->ShowReport();
			RoadComputeScope->bNeedGenerateReport = false;
			bReportWasShown = true;
		}

		if (RoadComputeScope->ResultInfo.HasFailed() || RoadComputeScope->ResultInfo.Errors.Num())
		{
			bHasFaild = true;
		}

		if (RoadComputeScope->ResultInfo.Warnings.Num())
		{
			bHasWarnings = true;
		}
	}

	if (bOpWasJustUpdated || bReportWasShown)
	{
		if (NumTask > 0)
		{
			GetToolManager()->DisplayMessage(FText::Format(LOCTEXT("UTriangulateRoadTool_OpInProgress", "In progress {0} ..."), NumTask), EToolMessageLevel::UserWarning);
		}
		else if (bHasFaild)
		{
			GetToolManager()->DisplayMessage(LOCTEXT("UTriangulateRoadTool_OpFail", "ERROR: See the log"), EToolMessageLevel::UserWarning);
		}
		else if (bHasWarnings)
		{
			GetToolManager()->DisplayMessage(LOCTEXT("UTriangulateRoadTool_OpFail", "WARNING: See the log"), EToolMessageLevel::UserWarning);
		}
		else
		{
			GetToolManager()->DisplayMessage(LOCTEXT("UTriangulateRoadTool_OpDone", "Success"), EToolMessageLevel::UserWarning);
		}
	}

	bOpWasJustUpdated = false;
}

void UTriangulateRoadTool::Render(IToolsContextRenderAPI* RenderAPI)
{
	
	for (auto& RoadComputeScope : RoadsComputeScope)
	{
		if (RoadComputeScope->BaseData)
		{
			FScopeLock Lock(&RoadComputeScope->BaseData->RenderAPIMutex);
			for (auto& DebugLine : RoadComputeScope->BaseData->DebugLines)
			{
				for (auto& Segment : DebugLine.Lines)
				{
					RenderAPI->GetPrimitiveDrawInterface()->DrawTranslucentLine(Segment.Key, Segment.Value, DebugLine.Color, SDPG_Foreground, DebugLine.Thickness, 10000.0, true);
					RenderAPI->GetPrimitiveDrawInterface()->DrawPoint(Segment.Key, FColor::White, 10, 10000.0);
					RenderAPI->GetPrimitiveDrawInterface()->DrawPoint(Segment.Value, FColor::White, 10, 10000.0);
				}
			}
		}
	}
	
}

void UTriangulateRoadTool::SetWorld(UWorld* World)
{
	TargetWorld = World;
}

UWorld* UTriangulateRoadTool::GetTargetWorld()
{
	return TargetWorld.Get();
}

void UTriangulateRoadTool::OnPropertyModified(UObject* PropertySet, FProperty* Property)
{
	static FName RebuildAll = "RebuildAll";

	if (!Property)
	{
		return;
	}

	if (Property->HasMetaData(RebuildAll))
	{
		for (auto& RoadComputeScope : RoadsComputeScope)
		{
			RoadComputeScope->NotifyRebuildAll();
		}
	}
	else
	{
		for (auto& RoadComputeScope : RoadsComputeScope)
		{
			for (auto& It : RoadComputeScope->OpComputes)
			{
				for (auto Tag : It->RebuildTags)
				{
					if (Property->HasMetaData(Tag))
					{
						RoadComputeScope->NotifyRebuildOne(*It);
					}
				}

			}
		}

	}

	if (Property->GetFName() == GET_MEMBER_NAME_CHECKED(UTriangulateRoadToolProperties, bShowWireframe))
	{
		for (auto& RoadComputeScope : RoadsComputeScope)
		{
			for (auto& It : RoadComputeScope->OpComputes)
			{
				It->EnableWireframe(TriangulateProperties->bShowWireframe);
			}
		}
	}
}

void UTriangulateRoadTool::AddToolPropertySource(UObject* PropertyObject)
{
	Super::AddToolPropertySource(PropertyObject);
}

void UTriangulateRoadTool::AddToolPropertySource(UInteractiveToolPropertySet* PropertySet)
{
	Super::AddToolPropertySource(PropertySet);
}

UInteractiveToolPropertySet* UTriangulateRoadTool::SetupPropertySet(const TSubclassOf<UInteractiveToolPropertySet>& PropertySet)
{
	auto* Found = ToolPropertyObjects.FindByPredicate([&PropertySet](auto& Obj)
	{
		return Obj->GetClass() == PropertySet.Get();
	});

	if (Found && Cast<UInteractiveToolPropertySet>(*Found))
	{
		return Cast<UInteractiveToolPropertySet>(*Found);
	}

	UInteractiveToolPropertySet* Properties = NewObject<UInteractiveToolPropertySet>(this, PropertySet);
	Properties->RestoreProperties(this);
	AddToolPropertySource(Properties);
	return Properties;
}

void UTriangulateRoadTool::Shutdown(EToolShutdownType ShutdownType)
{
	for (auto& It : ToolPropertyObjects)
	{
		if (auto* PropertySet = Cast<UInteractiveToolPropertySet>(It))
		{
			PropertySet->SaveProperties(this);
		}
	}

	if (ShutdownType == EToolShutdownType::Accept)
	{
		
		GetToolManager()->BeginUndoTransaction(LOCTEXT("SweepSplineAction", "Spline Triangulation"));

		for (auto& RoadComputeScope : RoadsComputeScope)
		{
			if (!RoadComputeScope->TargetActor.IsValid())
			{
				UE_LOG(LogUnrealDrive, Error, TEXT(" UTriangulateRoadTool::Shutdown(); TargetActor is lost"));
				continue;
			}

			FTransform3d ActorToWorld = (FTransform3d)RoadComputeScope->TargetActor->GetTransform();
			AActor* TargetActor;
			if (TriangulateProperties->OutputActor == ERoadActorOutput::CreateNewActor)
			{
				FString ActorName = RoadComputeScope->TargetActor->GetActorLabel() + TEXT("_Gen");
				UActorFactoryEmptyActor* EmptyActorFactory = NewObject<UActorFactoryEmptyActor>();
				FAssetData AssetData(EmptyActorFactory->GetDefaultActorClass(FAssetData()));
				FActorSpawnParameters SpawnParams;
				SpawnParams.Name = *ActorName;
				SpawnParams.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;
				TargetActor = EmptyActorFactory->CreateActor(AssetData.GetAsset(), TargetWorld->GetCurrentLevel(), (FTransform)ActorToWorld, SpawnParams);
				FActorLabelUtilities::SetActorLabelUnique(TargetActor, ActorName);
				TargetActor->GetRootComponent()->SetMobility(EComponentMobility::Static);
			}
			else
			{
				TargetActor = RoadComputeScope->TargetActor.Get();
			}

			if (!TargetActor)
			{
				UE_LOG(LogUnrealDrive, Error, TEXT(" UTriangulateRoadTool::Shutdown(); Can't create TargetActor"));
				continue;
			}

			for (auto& It : RoadComputeScope->OpComputes)
			{
				It->ShutdownAndGenerateAssets(TargetActor, ActorToWorld);
			}
			
			ToolSelectionUtil::SetNewActorSelection(GetToolManager(), TargetActor);
		}

		GetToolManager()->EndUndoTransaction();
	}
	else
	{
		for (auto& RoadComputeScope : RoadsComputeScope)
		{
			RoadComputeScope->BaseOpCompute->Cancel();

			for (auto& It : RoadComputeScope->OpComputes)
			{
				It->Cancel();
			}
		}
	}

	TargetWorld = nullptr;
	RoadsComputeScope.Reset();

	//CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	
	Super::Shutdown(ShutdownType);
}

bool UTriangulateRoadTool::CanAccept() const
{
	for (auto& RoadComputeScope : RoadsComputeScope)
	{
		for (auto& It : RoadComputeScope->OpComputes)
		{
			if (It->HaveValidNonEmptyResult())
			{
				return true;
			}
		}
	}
	return false;
}

bool UTriangulateRoadToolBuilder::CanBuildTool(const FToolBuilderState& SceneState) const
{
	int32 NumSplines = ToolBuilderUtil::CountComponents(SceneState, [&](UActorComponent* Object) -> bool
	{
		return Cast<URoadSplineComponent>(Object) != nullptr;
	});
	FIndex2i SupportedRange = GetSupportedSplineCountRange();
	return (NumSplines >= SupportedRange.A && (SupportedRange.B == -1 || NumSplines <= SupportedRange.B));
}

void UTriangulateRoadToolBuilder::InitializeNewTool(UTriangulateRoadTool* NewTool, const FToolBuilderState& SceneState) const
{
	TArray<UActorComponent*> Components = ToolBuilderUtil::FindAllComponents(SceneState, [&](UActorComponent* Object)
	{
		return Cast<URoadSplineComponent>(Object) != nullptr;
	});
	TArray<TWeakObjectPtr<AActor>> ActorsWithSplines;
	TSet<AActor*> FoundActors;
	for (UActorComponent* Component : Components)
	{
		AActor* ActorWithSpline = Component->GetOwner();
		if (!FoundActors.Contains(ActorWithSpline))
		{
			FoundActors.Add(ActorWithSpline);
			ActorsWithSplines.Add(ActorWithSpline);
		}
	}
	NewTool->SetSplineActors(MoveTemp(ActorsWithSplines));
	NewTool->SetWorld(SceneState.World);
}

UInteractiveTool* UTriangulateRoadToolBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	UTriangulateRoadTool* NewTool = NewObject<UTriangulateRoadTool>(SceneState.ToolManager);
	InitializeNewTool(NewTool, SceneState);
	return NewTool;
}


#undef LOCTEXT_NAMESPACE

