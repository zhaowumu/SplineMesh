/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "Components/PrimitiveComponent.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "TileMapWindowComponent.generated.h" 

USTRUCT()
struct FTileData
{
	GENERATED_USTRUCT_BODY()

	FString URL{};
	FTransform Transform{};

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> Matrtial;

	UPROPERTY()
	TObjectPtr<UTexture2D> Texture;
};


UCLASS(hidecategories = (Object, LOD, Physics, Lighting, TextureStreaming, Activation, "Components|Activation", Collision, HLOD, Navigation), meta = (BlueprintSpawnableComponent))
class UTileMapWindowComponent : public UPrimitiveComponent
{
	GENERATED_UCLASS_BODY()

	class FTileLoading;
	friend FTileLoading;

public:
	UPROPERTY(EditAnywhere, Category = TileMapWindow, meta = (GetOptions = "GetTileSourcesNames", NoResetToDefault))
	FName Source;

	UPROPERTY(EditAnywhere, Category = TileMapWindow)
	bool bUseWorldCoordinateSpace = false;

	/** Corresponds to the X axis */
	UPROPERTY(EditAnywhere, Category = TileMapWindow)
	double Longitude = 30.249716;

	/** Corresponds to the Y axis  */
	UPROPERTY(EditAnywhere, Category = TileMapWindow)
	double Latitude = 59.984076;

	UPROPERTY(EditAnywhere, Category = TileMapWindow, meta=(EditCondition = "bUseWorldCoordinateSpace==true"))
	FVector WorldOrignOffset{};

	UPROPERTY(EditAnywhere, Category = TileMapWindow, meta = (UIMin = 1, UIMax = 30, ClampMin = 1, ClampMax = 30))
	FUintVector2 WindowSize{ 10, 10 };

	UPROPERTY(EditAnywhere, Category = TileMapWindow, meta = (UIMin = 1, UIMax = 24, ClampMin = 1, ClampMax = 24))
	int Zoom = 18;

	// Height of collision box [cm]
	UPROPERTY(EditAnywhere, Category = TileMapWindow, meta = (UIMin = 0.01, ClampMin = 0.01))
	double BodyHeight = 100;

	UPROPERTY(EditAnywhere, Category = TileMapWindow)
	TObjectPtr<UMaterial> Material;

	UPROPERTY(Transient)
	TObjectPtr<UTexture> EmptyTexture;

	UPROPERTY(Transient, DuplicateTransient)
	TObjectPtr<class UBodySetup> BodySetup;

public:
	UFUNCTION(Category = TileMapWindow)
	void UpdateGrid(bool bMarkRenderStateDirty);

	UFUNCTION(CallInEditor, Category = TileMapWindow)
	static void ClearCache();

	UFUNCTION(CallInEditor, Category = TileMapWindow, meta = (DisplayName = "UpdateGrid"))
	void MarkTileGridDirty() { bTileGridIsDirty = true; }

	const auto& GetGrid() const { return Grid; }
	double GetTileSize() const { return TileSize; }
	const FVector& GetOriginOffset() const { return OriginOffset; }
	FBox GetBox() const;
	FTransform GetBoxToWorld(const FTransform& ComponentLocalToWorld) const;
	FTransform GetBoxToLocal(const FTransform& ComponentLocalToWorld) const;

	void SetTexture(int Index, UTexture2D* Texture);
	void UpdateBodySetup(bool bForce);
	static FString GetTileHacheDir();

	UFUNCTION()
	static TArray<FName> GetTileSourcesNames();

public:
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	virtual void GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials = false) const override;
	virtual void BeginDestroy() override;
	virtual void OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport = ETeleportType::None) override;
	void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
#if WITH_EDITOR
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
#endif
	virtual UBodySetup* GetBodySetup() override;
	virtual void OnComponentCreated() override;
	virtual void PostLoad() override;

protected:

	UPROPERTY(Transient)
	TArray<FTileData> Grid;
	double TileSize = 100;
	FVector OriginOffset{};
	TSharedPtr<FTileLoading> TileLoading;

	bool bTileGridIsDirty = false;
};

