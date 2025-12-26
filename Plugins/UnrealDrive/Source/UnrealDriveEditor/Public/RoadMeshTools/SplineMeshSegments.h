/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "CoreMinimal.h"
#include "Components/SplineMeshComponent.h"
#include "StructUtils/InstancedStruct.h"
#include "RoadLaneAttributeEntries.h"

namespace UnrealDrive
{
	struct UNREALDRIVEEDITOR_API FSplineMeshSegments
	{
		FSplineMeshSegments() = default;
		FSplineMeshSegments(const FSplineMeshSegments& Other) = default;
		FSplineMeshSegments(FSplineMeshSegments&& Other) = default;
		
		const FSplineMeshSegments& operator=(const FSplineMeshSegments& Other)
		{
			AttribyteEntries = Other.AttribyteEntries;
			Segments = Other.Segments;
			return *this;
		}

		const FSplineMeshSegments& operator=(FSplineMeshSegments&& Other)
		{
			AttribyteEntries = MoveTemp(Other.AttribyteEntries);
			Segments = MoveTemp(Other.Segments);
			return *this;
		}
		
		void ApplyTransform(const FTransform& Transform);
		void ApplyTransformInverse(const FTransform& Transform);
		void BuildComponents(AActor* TargetActor, bool bIsPreview) const;
		
		struct FSegment
		{
			bool bAlignWorldUpVector;
			FSplineMeshParams SplineMeshParams;
			const TInstancedStruct<FRoadLaneAttributeEntry>* AttribyteEntry;
			FName AttribyteEntryName;
		};
		
		TArray<FSegment> Segments;
		TMap<FName, TInstancedStruct<FRoadLaneAttributeEntry>> AttribyteEntries;
	};
}