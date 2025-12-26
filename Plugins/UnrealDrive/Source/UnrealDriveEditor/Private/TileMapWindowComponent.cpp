
/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "TileMapWindowComponent.h"
#include "ImageUtils.h"
#include "DynamicMeshBuilder.h"
#include "Materials/MaterialRenderProxy.h"
#include "ImageCoreUtils.h"
#include "PhysicsEngine/BodySetup.h"
#include "UnrealDriveEditorSettings.h"
#include "UnrealDriveEditorModule.h"
#include "PrimitiveSceneProxyDesc.h"
#include "UnrealDrive.h"

/*
void DrawPlaneXY(FPrimitiveDrawInterface* PDI, const FMatrix& Matrix, const FBox& Box, const FLinearColor& Color, uint8 DepthPriority, float Thickness, float DepthBias, bool bScreenSpace)
{
	FVector B[2];
	B[0] = Box.Min;
	B[1] = Box.Max;

	for (int i = 0; i < 2; i++)
	{
		for (int j = 0; j < 2; j++)
		{
			FVector P, Q;

			P.X = B[i].X; Q.X = B[i].X;
			P.Y = B[j].Y; Q.Y = B[j].Y;
			P.Z = B[0].Z; Q.Z = B[1].Z;
			P = Matrix.TransformPosition(P); Q = Matrix.TransformPosition(Q);
			PDI->DrawLine(P, Q, Color, DepthPriority, Thickness, DepthBias, bScreenSpace);

			P.Y = B[i].Y; Q.Y = B[i].Y;
			P.Z = B[j].Z; Q.Z = B[j].Z;
			P.X = B[0].X; Q.X = B[1].X;
			P = Matrix.TransformPosition(P); Q = Matrix.TransformPosition(Q);
			PDI->DrawLine(P, Q, Color, DepthPriority, Thickness, DepthBias, bScreenSpace);

			P.Z = B[i].Z; Q.Z = B[i].Z;
			P.X = B[j].X; Q.X = B[j].X;
			P.Y = B[0].Y; Q.Y = B[1].Y;
			P = Matrix.TransformPosition(P); Q = Matrix.TransformPosition(Q);
			PDI->DrawLine(P, Q, Color, DepthPriority, Thickness, DepthBias, bScreenSpace);
		}
	}
}
*/

static void DrawPlaneXY(FPrimitiveDrawInterface* PDI, const FMatrix& Matrix, const FBox& Box, const FLinearColor& Color, uint8 DepthPriority, float Thickness, float DepthBias, bool bScreenSpace)
{
	FVector Min = Box.Min;
	FVector Max = Box.Max;

	Min = Matrix.TransformPosition(Min); 
	Max = Matrix.TransformPosition(Max);

	double Z = Matrix.GetOrigin().Z;

	PDI->DrawLine(FVector(Min.X, Min.Y, Z), FVector(Max.X, Min.Y, Z), Color, DepthPriority, Thickness, DepthBias, bScreenSpace);
	PDI->DrawLine(FVector(Max.X, Min.Y, Z), FVector(Max.X, Max.Y, Z), Color, DepthPriority, Thickness, DepthBias, bScreenSpace);
	PDI->DrawLine(FVector(Max.X, Max.Y, Z), FVector(Min.X, Max.Y, Z), Color, DepthPriority, Thickness, DepthBias, bScreenSpace);
	PDI->DrawLine(FVector(Min.X, Max.Y, Z), FVector(Min.X, Min.Y, Z), Color, DepthPriority, Thickness, DepthBias, bScreenSpace);
}


static UTexture2D* CreateTexture2DFromImage(const FImageView& Image, UTexture2D* PrevTexture)
{
	ERawImageFormat::Type PixelFormatRawFormat;
	EPixelFormat PixelFormat = FImageCoreUtils::GetPixelFormatForRawImageFormat(Image.Format, &PixelFormatRawFormat);

	UTexture2D* NewTexture;
	if (PrevTexture /* && PrevTexture->GetPixelFormat() == PixelFormat && PrevTexture->GetSizeX() == Image.SizeX && PrevTexture->GetSizeY() == Image.SizeY*/)
	{
		NewTexture = PrevTexture;
	}
	else
	{
		TPromise<void> Promise;
		AsyncTask(ENamedThreads::GameThread, [&Promise, &NewTexture, &Image, &PixelFormat]()
		{
			NewTexture = UTexture2D::CreateTransient(Image.SizeX, Image.SizeY, PixelFormat);
			Promise.SetValue();
		});
		Promise.GetFuture().Get();
	}

	if (NewTexture == nullptr)
	{
		UE_LOG(LogUnrealDrive, Warning, TEXT("Error in CreateTransient"));
		return nullptr;
	}

	NewTexture->bNotOfflineProcessed = true;

	uint8* MipData = static_cast<uint8*>(NewTexture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE));
	check(MipData != nullptr);
	int64 MipDataSize = NewTexture->GetPlatformData()->Mips[0].BulkData.GetBulkDataSize();

	FImageView MipImage(MipData, Image.SizeX, Image.SizeY, 1, PixelFormatRawFormat, Image.GammaSpace);
	check(MipImage.GetImageSizeBytes() <= MipDataSize); // is it exactly == ?

	// copy into texture and convert if necessary :
	FImageCore::CopyImage(Image, MipImage);

	NewTexture->GetPlatformData()->Mips[0].BulkData.Unlock();

	//NewTexture->UpdateResource();

	return NewTexture;
}

static FPrimitiveSceneProxyDesc MakePrimitiveSceneProxyDes(const UPrimitiveComponent* Component)
{
	FPrimitiveSceneProxyDesc Desc(Component);
#if WITH_EDITOR
	Desc.bIsVisibleEditor = FUnrealDriveEditorModule::IsTileRendersVisibleInEditor();
#endif
	return Desc;
}

class FTileMapWindowSceneProxy final : public FPrimitiveSceneProxy
{
public:
	SIZE_T GetTypeHash() const override
	{
		static size_t UniquePointer;
		return reinterpret_cast<size_t>(&UniquePointer);
	}

	FTileMapWindowSceneProxy(UTileMapWindowComponent* Component)
		: FPrimitiveSceneProxy(MakePrimitiveSceneProxyDes(Component), NAME_None)
		, VertexFactory(GetScene().GetFeatureLevel(), "FTileMapWindowSceneProxy")
		, bUseWorldCoordinateSpace(Component->bUseWorldCoordinateSpace)
		, BodySetup(Component->GetBodySetup())
		, Box(Component->GetBox())
		, OriginOffset(Component->GetOriginOffset())
	{
		bWillEverBeLit = false;
		if (IsValid(Component->Material))
		{
			MaterialRelevance = Component->Material->GetRelevance_Concurrent(GetScene().GetFeatureLevel());
		}
		else
		{
			MaterialRelevance = {};
		}

		TArray<FDynamicMeshVertex> Verts;
		Verts.Reserve(4);
		Verts.Emplace(FVector3f{ 0.0, 0.0, 0.0 }, FVector2f{ 0.0, 0.0 }, FColor::Black);
		Verts.Emplace(FVector3f{ 1.0, 0.0, 0.0 }, FVector2f{ 1.0, 0.0 }, FColor::Black);
		Verts.Emplace(FVector3f{ 1.0, 1.0, 0.0 }, FVector2f{ 1.0, 1.0 }, FColor::Black);
		Verts.Emplace(FVector3f{ 0.0, 1.0, 0.0 }, FVector2f{ 0.0, 1.0 }, FColor::Black);

		VertexBuffers.InitFromDynamicVertex(&VertexFactory, Verts);

		IndexBuffer.Indices = { 2, 1, 0, 3, 2, 0 };
		BeginInitResource(&IndexBuffer);

		TilesData.Reserve(Component->GetGrid().Num());
		for (auto& It : Component->GetGrid())
		{
			check(It.Matrtial);
			TilesData.Add({ It.Matrtial->GetRenderProxy(), It.Transform.ToMatrixWithScale() });
		}
	}

	virtual ~FTileMapWindowSceneProxy()
	{
		VertexBuffers.PositionVertexBuffer.ReleaseResource();
		VertexBuffers.StaticMeshVertexBuffer.ReleaseResource();
		VertexBuffers.ColorVertexBuffer.ReleaseResource();
		IndexBuffer.ReleaseResource();
		VertexFactory.ReleaseResource();
	}

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_ArrowSceneProxy_DrawDynamicElements);

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			if (VisibilityMap & (1 << ViewIndex))
			{
				const FSceneView* View = Views[ViewIndex];

				for (auto& Tile : TilesData)
				{
					FMatrix TileLocalToWorld = bUseWorldCoordinateSpace ? Tile.Transform * FTransform(GetLocalToWorld().GetOrigin()).ToMatrixNoScale() : Tile.Transform * GetLocalToWorld();
					FMeshBatch& Mesh = Collector.AllocateMesh();
					FMeshBatchElement& BatchElement = Mesh.Elements[0];
					BatchElement.IndexBuffer = &IndexBuffer;
					Mesh.bWireframe = false;
					Mesh.VertexFactory = &VertexFactory;
					Mesh.MaterialRenderProxy = Tile.Material;

					FDynamicPrimitiveUniformBuffer& DynamicPrimitiveUniformBuffer = Collector.AllocateOneFrameResource<FDynamicPrimitiveUniformBuffer>();
					DynamicPrimitiveUniformBuffer.Set(Collector.GetRHICommandList(), TileLocalToWorld, TileLocalToWorld, GetBounds(), GetLocalBounds(), false, false, false);
					BatchElement.PrimitiveUniformBufferResource = &DynamicPrimitiveUniformBuffer.UniformBuffer;

					BatchElement.FirstIndex = 0;
					BatchElement.NumPrimitives = IndexBuffer.Indices.Num() / 3;
					BatchElement.MinVertexIndex = 0;
					BatchElement.MaxVertexIndex = VertexBuffers.PositionVertexBuffer.GetNumVertices() - 1;
					Mesh.ReverseCulling = IsLocalToWorldDeterminantNegative();
					Mesh.Type = PT_TriangleList;
					Mesh.DepthPriorityGroup = SDPG_World;
					Mesh.bCanApplyViewModeOverrides = false;
					Collector.AddMesh(ViewIndex, Mesh);
				}
			}
		}

		FMatrix BoxWorldTransform;
		if (bUseWorldCoordinateSpace)
		{
			BoxWorldTransform = FMatrix::Identity;
			BoxWorldTransform.SetOrigin(GetLocalToWorld().GetOrigin() + OriginOffset);
		}
		else
		{
			BoxWorldTransform = GetLocalToWorld();
			BoxWorldTransform.SetOrigin(BoxWorldTransform.GetOrigin() + BoxWorldTransform.TransformVector(OriginOffset));
		}

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			if (VisibilityMap & (1 << ViewIndex))
			{
				DrawPlaneXY(Collector.GetPDI(ViewIndex), BoxWorldTransform, Box, FColor::White, SDPG_World, 0.0, 0.0, false);
			}
		}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			if (VisibilityMap & (1 << ViewIndex))
			{
				RenderCollision(BodySetup, Collector, ViewIndex, ViewFamily.EngineShowFlags, GetBounds(), IsSelected());
				RenderBounds(Collector.GetPDI(ViewIndex), ViewFamily.EngineShowFlags, GetBounds(), IsSelected());
			}
		}
#endif
	}

	void RenderCollision(UBodySetup* InBodySetup, FMeshElementCollector& Collector, int32 ViewIndex, const FEngineShowFlags& EngineShowFlags, const FBoxSphereBounds& InBounds, bool bRenderInEditor) const
	{
		if (InBodySetup)
		{
			bool bDrawCollision = EngineShowFlags.Collision && IsCollisionEnabled();

			if (bDrawCollision && AllowDebugViewmodes())
			{
				// Draw simple collision as wireframe if 'show collision', collision is enabled, and we are not using the complex as the simple
				const bool bDrawSimpleWireframeCollision = InBodySetup->CollisionTraceFlag != ECollisionTraceFlag::CTF_UseComplexAsSimple;

				if (FMath::Abs(GetLocalToWorld().Determinant()) < SMALL_NUMBER)
				{
					// Catch this here or otherwise GeomTransform below will assert
					// This spams so commented out
					//UE_LOG(LogUnrealDrive, Log, TEXT("Zero scaling not supported (%s)"), *StaticMesh->GetPathName());
				}
				else
				{
					const bool bDrawSolid = !bDrawSimpleWireframeCollision;
					const bool bProxyIsSelected = IsSelected();

					if (bDrawSolid)
					{
						// Make a material for drawing solid collision stuff
						auto SolidMaterialInstance = new FColoredMaterialRenderProxy(
							GEngine->ShadedLevelColorationUnlitMaterial->GetRenderProxy(),
							GetWireframeColor()
						);

						Collector.RegisterOneFrameMaterialProxy(SolidMaterialInstance);

						FTransform GeomTransform(GetLocalToWorld());
						InBodySetup->AggGeom.GetAggGeom(GeomTransform, GetWireframeColor().ToFColor(true), SolidMaterialInstance, false, true, AlwaysHasVelocity(), ViewIndex, Collector);
					}
					// wireframe
					else
					{
						FColor CollisionColor = FColor(157, 149, 223, 255);
						FTransform GeomTransform(GetLocalToWorld());
						InBodySetup->AggGeom.GetAggGeom(GeomTransform, GetSelectionColor(CollisionColor, bProxyIsSelected, IsHovered()).ToFColor(true), nullptr, false, false, AlwaysHasVelocity(), ViewIndex, Collector);
					}
				}
			}
		}
	}

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
	{
		FPrimitiveViewRelevance Result;
		Result.bDynamicRelevance = true;
		Result.bDrawRelevance = IsShown(View);
		Result.bShadowRelevance = false;// IsShadowCast(View);
		Result.bVelocityRelevance = DrawsVelocity() && Result.bOpaque && Result.bRenderInMainPass;
		Result.bEditorPrimitiveRelevance = UseEditorCompositing(View);
		MaterialRelevance.SetPrimitiveViewRelevance(Result);

		return Result;
	}


	virtual uint32 GetMemoryFootprint(void) const override { return(sizeof(*this) + GetAllocatedSize()); }
	uint32 GetAllocatedSize(void) const { return(FPrimitiveSceneProxy::GetAllocatedSize()); }

private:
	FStaticMeshVertexBuffers VertexBuffers;
	FDynamicMeshIndexBuffer32 IndexBuffer;
	FLocalVertexFactory VertexFactory;
	FMaterialRelevance MaterialRelevance;
	struct FTileData
	{
		FMaterialRenderProxy* Material = nullptr;
		FMatrix Transform;
	};
	TArray<FTileData> TilesData;
	bool bUseWorldCoordinateSpace;
	UBodySetup* BodySetup;
	FBox Box;
	FVector OriginOffset;

};

//-----------------------------------------------------------------------------------------------------------------------------------------------

class UTileMapWindowComponent::FTileLoading : public FRunnable
{
public:
	FTileLoading(UTileMapWindowComponent * Component)
		: Component(Component)
	{
		Requests.Reserve(Component->GetGrid().Num());
		for (auto& It : Component->GetGrid())
		{
			Requests.Add({ It.URL, FMD5::HashAnsiString(*It.URL), nullptr, It.Texture.Get()});
		}
		Thread.Reset(FRunnableThread::Create(this, TEXT("FTileLoading")));
	}

	virtual ~FTileLoading()
	{
		bShutdown = true;
		if (Thread)
		{
			Thread->WaitForCompletion();
		}
		for (auto& It : Requests)
		{
			if (It.Request)
			{
				It.Request->CancelRequest();
			}
		}
	}

	virtual uint32 Run() override
	{
		for (int Index = 0; Index < Requests.Num(); ++Index)
		{
			auto& It = Requests[Index];
			FString CacheFile = UTileMapWindowComponent::GetTileHacheDir() / It.Hash;
			if (FPaths::FileExists(CacheFile))
			{
				FImage Image;
				if (FImageUtils::LoadImage(*CacheFile, Image))
				{
					if (UTexture2D* Texture = CreateTexture2DFromImage(Image, It.Texture))
					{
						AsyncTask(ENamedThreads::GameThread, [Component = Component, Index, Texture, thisPtr=this]()
						{
							if (IsValid(Component) && Component->TileLoading.Get() == thisPtr)
							{
								Texture->UpdateResource();
								Component->SetTexture(Index, Texture);
							}
						});
						continue;
					}
					else
					{
						UE_LOG(LogUnrealDrive, Error, TEXT("Can't create texture for '%s'"), *CacheFile);
					}
				}
				else
				{
					UE_LOG(LogUnrealDrive, Error, TEXT("Can't load image '%s'"), *CacheFile);
				}
			}

			It.Request = FHttpModule::Get().CreateRequest();
			//It.Request->OnProcessRequestComplete().BindUObject(this, &FTileLoading::OnPostReceivedResponse, Index);
			It.Request->SetURL(It.URL);
			It.Request->SetVerb("GET");
			It.Request->ProcessRequest();

			if (bShutdown)
			{
				return 0;
			}
		}

		bool AreNotFinishedRequests = true;
		while (!bShutdown && AreNotFinishedRequests)
		{
			AreNotFinishedRequests = false;
			for (int Index = 0; Index < Requests.Num(); ++Index)
			{
				auto& It = Requests[Index];
				if (It.Request)
				{
					auto Response = It.Request->GetResponse();
					if (Response && EHttpRequestStatus::IsFinished(Response->GetStatus()))
					{
						ProcessResponse(Index);
						It.Request.Reset();
					}
					AreNotFinishedRequests = true;
				}
				if (bShutdown)
				{
					return 0;
				}
			}
			FPlatformProcess::Sleep(0.01f);
		}
		return 0;
	}

	virtual void Stop() override
	{
		bShutdown = true;
	}

	void ProcessResponse(int Index)
	{
		auto Response = Requests[Index].Request->GetResponse();

		if (Response->GetStatus() != EHttpRequestStatus::Succeeded)
		{
			UE_LOG(LogUnrealDrive, Error, TEXT("Status faild"));
			return;
		}

		if (Response->GetContentType() != TEXT("image/jpeg") && Response->GetContentType() != TEXT("image/png"))
		{
			UE_LOG(LogUnrealDrive, Error, TEXT("GetContentType == '%s'"), *Response->GetContentType());
			return;
		}

		FString FilePath = UTileMapWindowComponent::GetTileHacheDir() / Requests[Index].Hash;

		if (!FFileHelper::SaveArrayToFile(Response->GetContent(), *FilePath))
		{
			UE_LOG(LogUnrealDrive, Error, TEXT("Can't save to file '%s'"), *FilePath);
			return;
		}

		FImage Image;
		if (!FImageUtils::LoadImage(*FilePath, Image))
		{
			UE_LOG(LogUnrealDrive, Error, TEXT("Can't load image '%s'"), *FilePath);
			return;
		}

		UTexture2D* Texture = CreateTexture2DFromImage(Image, Requests[Index].Texture);
		if (!IsValid(Texture))
		{
			UE_LOG(LogUnrealDrive, Error, TEXT("Can't create texture '%s'"), *FilePath);
			return;
		}

		AsyncTask(ENamedThreads::GameThread, [Component = Component, Index, Texture, thisPtr=this]()
		{
			if (IsValid(Component) && Component->TileLoading.Get() == thisPtr)
			{
				Texture->UpdateResource();
				Component->SetTexture(Index, Texture);
			}
		});
	}


	TUniquePtr<FRunnableThread> Thread;
	std::atomic<bool> bShutdown = false;
	UTileMapWindowComponent* Component;
	struct FTileData
	{
		FString URL;
		FString Hash;
		TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> Request;
		UTexture2D* Texture;
	};
	TArray<FTileData> Requests;
};


//-----------------------------------------------------------------------------------------------------------------------------------------------

UTileMapWindowComponent::UTileMapWindowComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	static ConstructorHelpers::FObjectFinder<UMaterial> MaterialFinder(TEXT("/UnrealDrive/EditorAssets/Materials/M_TileMapWindow"));
	if (MaterialFinder.Succeeded())
	{
		Material = MaterialFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UTexture> EmptyTextureFinder(TEXT("/UnrealDrive/EditorAssets/Materials/EmptyTileMapWindowTexture.EmptyTileMapWindowTexture"));
	if (EmptyTextureFinder.Succeeded())
	{
		EmptyTexture = Cast<UTexture>(EmptyTextureFinder.Object);
	}

	PrimaryComponentTick.bCanEverTick = true;
	//PrimaryComponentTick.bStartWithTickEnabled = true;
	//PrimaryComponentTick.TickGroup = ETickingGroup::TG_PrePhysics;
	bTickInEditor = true;
}


FPrimitiveSceneProxy* UTileMapWindowComponent::CreateSceneProxy()
{
	return new FTileMapWindowSceneProxy(this);
}

void UTileMapWindowComponent::BeginDestroy()
{
	Super::BeginDestroy();
	TileLoading.Reset();
	Grid.Reset();
}

#if WITH_EDITOR
void UTileMapWindowComponent::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	FProperty* MemberProperty = nullptr;
	if (PropertyChangedEvent.PropertyChain.GetActiveMemberNode())
	{
		MemberProperty = PropertyChangedEvent.PropertyChain.GetActiveMemberNode()->GetValue();
	}

	if (MemberProperty != nullptr)
	{
		static const FName SourceName = GET_MEMBER_NAME_CHECKED(UTileMapWindowComponent, Source);
		static const FName UseWorldCoordinateSpaceName = GET_MEMBER_NAME_CHECKED(UTileMapWindowComponent, bUseWorldCoordinateSpace);
		static const FName LongitudeName = GET_MEMBER_NAME_CHECKED(UTileMapWindowComponent, Longitude);
		static const FName LatitudeName = GET_MEMBER_NAME_CHECKED(UTileMapWindowComponent, Latitude);
		static const FName WorldOrignOffsetName = GET_MEMBER_NAME_CHECKED(UTileMapWindowComponent, WorldOrignOffset);
		static const FName WindowSizeName = GET_MEMBER_NAME_CHECKED(UTileMapWindowComponent, WindowSize);
		static const FName ZoomName = GET_MEMBER_NAME_CHECKED(UTileMapWindowComponent, Zoom);
		static const FName MaterialName = GET_MEMBER_NAME_CHECKED(UTileMapWindowComponent, Material);
		static const FName EmptyTextureName = GET_MEMBER_NAME_CHECKED(UTileMapWindowComponent, EmptyTexture);

		const FName PropertyName(MemberProperty->GetFName());

		if (PropertyName == SourceName ||
			PropertyName == UseWorldCoordinateSpaceName ||
			PropertyName == LongitudeName ||
			PropertyName == LatitudeName ||
			PropertyName == WorldOrignOffsetName ||
			PropertyName == WindowSizeName ||
			PropertyName == ZoomName ||
			PropertyName == MaterialName ||
			PropertyName == EmptyTextureName)
		{
			MarkTileGridDirty();
		}
	}

	Super::PostEditChangeChainProperty(PropertyChangedEvent);
}
#endif

void UTileMapWindowComponent::OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport)
{
	Super::OnUpdateTransform(UpdateTransformFlags, Teleport);
	if (bUseWorldCoordinateSpace)
	{
		MarkTileGridDirty();
	}
}

void UTileMapWindowComponent::OnComponentCreated()
{
	Super::OnComponentCreated();

	// Called when a component is created (not loaded). This can happen in the editor or during gameplay

	if (GetDefault<UUnrealDriveEditorSettings>()->TileSources.Num())
	{
		Source = GetDefault<UUnrealDriveEditorSettings>()->TileSources.begin()->Key;
	}

	MarkTileGridDirty();
}

void UTileMapWindowComponent::PostLoad()
{
	Super::PostLoad();
	MarkTileGridDirty();
}

FBox UTileMapWindowComponent::GetBox() const
{
	return FBox(
		FVector(-TileSize * WindowSize.X * 0.5, -TileSize * WindowSize.Y * 0.5, -BodyHeight * 0.5),
		FVector(+TileSize * WindowSize.X * 0.5, +TileSize * WindowSize.Y * 0.5, +BodyHeight * 0.5)
	);
}

FTransform UTileMapWindowComponent::GetBoxToWorld(const FTransform& LocalToWorld) const
{
	if (bUseWorldCoordinateSpace)
	{
		return FTransform(FQuat::Identity, LocalToWorld.GetLocation() + OriginOffset);
	}
	else
	{
		FTransform Transform = LocalToWorld;
		Transform.AddToTranslation(Transform.GetRotation().RotateVector(OriginOffset));
		return Transform;
	}
}

FTransform UTileMapWindowComponent::GetBoxToLocal(const FTransform& ComponentLocalToWorld) const
{
	if (bUseWorldCoordinateSpace)
	{
		auto Quat = ComponentLocalToWorld.GetRotation().Inverse();
		FTransform Transform;
		Transform.SetRotation(Quat);
		Transform.SetLocation(Quat.RotateVector(OriginOffset));
		return Transform;
	}
	else
	{
		return FTransform(OriginOffset);
	}
}

FBoxSphereBounds UTileMapWindowComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	auto Box = GetBox();
	return Box.TransformBy(GetBoxToWorld(LocalToWorld));
}

UBodySetup* UTileMapWindowComponent::GetBodySetup()
{
	UpdateBodySetup(false);
	return BodySetup;
}

void UTileMapWindowComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bTileGridIsDirty)
	{
		bTileGridIsDirty = false;
		UpdateGrid(true);
	}
}

void UTileMapWindowComponent::UpdateBodySetup(bool bForce)
{
	if (!BodySetup || bForce)
	{
		BodySetup = NewObject<UBodySetup>(this);
		BodySetup->CollisionTraceFlag = CTF_UseSimpleAsComplex;
		//BodySetup->CollisionReponse = EBodyCollisionResponse::BodyCollision_Enabled;
		BodySetup->DefaultInstance.SetCollisionProfileName(UCollisionProfile::BlockAllDynamic_ProfileName);

		BodySetup->AggGeom.BoxElems.Add(FKBoxElem());

		auto Box = GetBox();
		FVector BoxSize = Box.GetSize();

		FKBoxElem* BoxElem = BodySetup->AggGeom.BoxElems.GetData();
		BoxElem->X = BoxSize.X;
		BoxElem->Y = BoxSize.Y;
		BoxElem->Z = BoxSize.Z;
		BoxElem->SetTransform(GetBoxToLocal(GetComponentToWorld()));
	}
}

static constexpr double WGS84_A = 6378137.0;
static constexpr double WGS84_B = 6356752.3142;
static constexpr double WGS84_E = 0.081819191;
static constexpr double WGS84_EqLen = WGS84_A * 2.0 * UE_PI;

/*
 * Mercator projection usefull links
 *   - https://en.wikipedia.org/wiki/Web_Mercator_projection
 *   - https://en.wikipedia.org/wiki/Mercator_projection
 *   - https://wiki.openstreetmap.org/wiki/Slippy_map_tilenames
 */

static TTuple<double, double, double> LatLon2Tile_WebMeraktor(double Longitude, double Latitude)
{
	const double XTile = ((Longitude + 180) / 360);
	const double YTile = (1.0 - (FMath::Loge(FMath::Tan(FMath::DegreesToRadians(Latitude) / 2 + UE_PI / 4)) / UE_PI)) / 2.0;
	const double K = 1.0 / FMath::Cos(FMath::DegreesToRadians(Latitude));

	return { XTile, YTile , K };
}

static TTuple<double, double, double> LatLon2Tile_WorldMeraktor(double Longitude, double Latitude)
{
	double CosLat = FMath::Cos(FMath::DegreesToRadians(Latitude));
	double SinLat = FMath::Sin(FMath::DegreesToRadians(Latitude));

	const double XTile = ((Longitude + 180) / 360);
	const double YTile = (1.0 - (FMath::Loge(FMath::Tan(FMath::DegreesToRadians(Latitude) / 2 + UE_PI / 4) * FMath::Pow((1.0 - WGS84_E * SinLat) / (1.0 + WGS84_E * SinLat), WGS84_E / 2)) / UE_PI)) / 2;
	const double K = 1.0 / CosLat * FMath::Sqrt(1.0 - WGS84_E * WGS84_E * SinLat * SinLat);

	return { XTile, YTile , K };
}

void UTileMapWindowComponent::UpdateGrid(bool bMarkRenderStateDirty)
{
	TileLoading.Reset();

	auto* SourceDesc = GetDefault<UUnrealDriveEditorSettings>()->TileSources.Find(Source);

	if (!SourceDesc)
	{
		UE_LOG(LogUnrealDrive, Error, TEXT("Can't find tile map source '%s'"), *Source.ToString());
		return;
	}

	Grid.SetNum(WindowSize.Y * WindowSize.X, EAllowShrinking::No);
	for (auto& It : Grid)
	{
		if (!IsValid(It.Matrtial))
		{
			It.Matrtial = UMaterialInstanceDynamic::Create(Material, this);
		}
		/*
		if (It.Texture.IsValid())
		{
			It.Texture->ConditionalBeginDestroy();
			It.Texture.Reset();
		}
		*/
		It.Matrtial->SetTextureParameterValue(TEXT("Texture"), EmptyTexture);
	}

	const double N = FMath::Pow(2.0,  Zoom);

	auto [XTile, YTile, K] = SourceDesc->Projection == ETileMapProjection::WorldMercator
		? LatLon2Tile_WorldMeraktor(Longitude, Latitude)
		: LatLon2Tile_WebMeraktor(Longitude, Latitude);

	XTile *= N;
	YTile *= N;


	TileSize = WGS84_EqLen / K / N * 100.0; // 100.0 - m to cm

	if (bUseWorldCoordinateSpace)
	{
		FVector Location = GetComponentToWorld().GetLocation() - WorldOrignOffset;
		XTile += Location.X / TileSize;
		YTile += Location.Y / TileSize;
	}

	OriginOffset = {
		-FMath::Frac(XTile) * TileSize + (WindowSize.X % 2 ? TileSize * 0.5 : 0.0),
		-FMath::Frac(YTile) * TileSize + (WindowSize.Y % 2 ? TileSize * 0.5 : 0.0),
		0.0
	};

	const FVector Offset = FVector{
		-TileSize * WindowSize.X * 0.5,
		-TileSize * WindowSize.Y * 0.5,
		0.0
	} + OriginOffset;

	for (uint32 Row = 0; Row < WindowSize.Y; ++Row)
	{
		for (uint32 Col = 0; Col < WindowSize.X; ++Col)
		{
			int Index = Row * WindowSize.X + Col;
			auto& It = Grid[Index];
			It.URL = FString::Format(*SourceDesc->URL, { {TEXT("x"), (int)XTile + Col - WindowSize.X / 2} , {TEXT("y"), (int)YTile + Row - WindowSize.Y / 2}, {TEXT("z"), Zoom} });
			It.Transform = FTransform(FRotator::ZeroRotator, FVector(TileSize * Col, TileSize * Row, 0.0) + Offset, FVector(TileSize));
		}
	}

	TileLoading = MakeShared<FTileLoading>(this);
	

	if (bMarkRenderStateDirty)
	{
		MarkRenderStateDirty();
	}

	UpdateBodySetup(true);
	RecreatePhysicsState();
}

void UTileMapWindowComponent::SetTexture(int Index, UTexture2D* Texture)
{
	check(Index >= 0 && Index < Grid.Num());
	check(IsValid(Grid[Index].Matrtial));

	Grid[Index].Texture = Texture;
	Grid[Index].Matrtial->SetTextureParameterValue(TEXT("Texture"), Texture);
}

void UTileMapWindowComponent::GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials) const
{
	Super::GetUsedMaterials(OutMaterials, bGetDebugMaterials);

	for (auto& It : Grid)
	{
		if (IsValid(It.Matrtial))
		{
			OutMaterials.Add(It.Matrtial.Get());
		}
	}
}

FString UTileMapWindowComponent::GetTileHacheDir()
{
	return FPaths::ProjectSavedDir() / TEXT("TileCache");
}

void UTileMapWindowComponent::ClearCache()
{
	IFileManager::Get().DeleteDirectory(*GetTileHacheDir(), false, true);
}

TArray<FName> UTileMapWindowComponent::GetTileSourcesNames()
{
	TArray<FName> Keys;
	GetDefault<UUnrealDriveEditorSettings>()->TileSources.GetKeys(Keys);
	return Keys;
}