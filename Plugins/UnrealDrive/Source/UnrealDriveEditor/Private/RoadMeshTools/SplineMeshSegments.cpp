/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "RoadMeshTools/SplineMeshSegments.h"
#include "Utils/ObjectsCreationHelper.h"
#include "CustomSplineBuilder.h"
#include "RoadLaneAttributeEntries.h"

using namespace UnrealDrive;

void FSplineMeshSegments::ApplyTransform(const FTransform& Transform)
{
	for (auto& Segment : Segments)
	{
		Segment.SplineMeshParams.StartPos = Transform.TransformPosition(Segment.SplineMeshParams.StartPos);
		Segment.SplineMeshParams.EndPos = Transform.TransformPosition(Segment.SplineMeshParams.EndPos);
		Segment.SplineMeshParams.StartTangent = Transform.TransformVector(Segment.SplineMeshParams.StartTangent);
		Segment.SplineMeshParams.EndTangent = Transform.TransformVector(Segment.SplineMeshParams.EndTangent);
	}
}

void FSplineMeshSegments::ApplyTransformInverse(const FTransform& Transform)
{
	for (auto& Segment : Segments)
	{
		Segment.SplineMeshParams.StartPos = Transform.InverseTransformPosition(Segment.SplineMeshParams.StartPos);
		Segment.SplineMeshParams.EndPos = Transform.InverseTransformPosition(Segment.SplineMeshParams.EndPos);
		Segment.SplineMeshParams.StartTangent = Transform.InverseTransformVector(Segment.SplineMeshParams.StartTangent);
		Segment.SplineMeshParams.EndTangent = Transform.InverseTransformVector(Segment.SplineMeshParams.EndTangent);
	}
}

void FSplineMeshSegments::BuildComponents(AActor* TargetActor, bool bIsPreview) const
{
	for (auto& Segment : Segments)
	{
		FReferenceSplineMeshParams Params(Segment.SplineMeshParams);
		Params.bAlignWorldUpVector = Segment.bAlignWorldUpVector;

		if (auto* Entry = Segment.AttribyteEntry->GetPtr<FRoadLaneAttributeEntry>())
		{
			Entry->GenerateAsset(Params, *Segment.AttribyteEntry, Segment.AttribyteEntryName, TargetActor, bIsPreview);
		}
	}
}