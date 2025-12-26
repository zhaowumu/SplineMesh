/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GameFramework/Actor.h"
#include "TileMapWindow.generated.h"

 /**
  * TileMapWindow is an actor with a UTileMapWindowComponent.
  *
  * @see UTileMapWindowComponent
  */
UCLASS(hideCategories = (Input), showCategories = ("Input|MouseInput", "Input|TouchInput"), ConversionRoot, ComponentWrapperClass, meta = (ChildCanTick), MinimalAPI)
class ATileMapWindow : public AActor
{
	GENERATED_UCLASS_BODY()

private:
	UPROPERTY(Category = TileMapWindow, VisibleAnywhere, BlueprintReadOnly, meta = (ExposeFunctionCategories = "Mesh,Rendering,Physics,Components|StaticMesh,Components|TileMapWindow", AllowPrivateAccess = "true"))
	TObjectPtr<class UTileMapWindowComponent> TileMapWindowComponent;

public:
	class UTileMapWindowComponent* GetTileMapWindowComponent() const;
};



