
/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "TileMapWindow.h"
#include "TileMapWindowComponent.h"
#include "Engine/CollisionProfile.h"
#include "Logging/MessageLog.h"
#include "Misc/UObjectToken.h"
#include "Misc/MapErrors.h"
#include "Engine/StaticMesh.h"

//#include UE_INLINE_GENERATED_CPP_BY_NAME(TileMapWindow)

#define LOCTEXT_NAMESPACE "TileMapWindow"

ATileMapWindow::ATileMapWindow(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetCanBeDamaged(false);

	TileMapWindowComponent = ObjectInitializer.CreateDefaultSubobject<UTileMapWindowComponent>(this, TEXT("TileMapWindowComponent"));
	TileMapWindowComponent->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	TileMapWindowComponent->Mobility = EComponentMobility::Static;
	TileMapWindowComponent->SetGenerateOverlapEvents(false);

	RootComponent = TileMapWindowComponent;
}

/** Returns TileMapWindowComponent subobject **/
UTileMapWindowComponent* ATileMapWindow::GetTileMapWindowComponent() const { return TileMapWindowComponent; }

#undef LOCTEXT_NAMESPACE

