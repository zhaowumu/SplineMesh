/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "CoreMinimal.h"

namespace CompVisUtils
{
	void DeselectAllExcept(const UActorComponent * TargetComponent);
	bool IsSelectedInViewport(const UActorComponent* TargetComponent);

	template<typename T>
	T* GetHitProxy(FViewport* Viewport, int32 X, int32 Y, int32 HitProxySize)
	{
		// Compute a HitProxySize x HitProxySize test region with the center at (X,Y).
		int32
			MinX = X - HitProxySize,
			MinY = Y - HitProxySize,
			MaxX = X + HitProxySize,
			MaxY = Y + HitProxySize;

		FIntPoint VPSize = Viewport->GetSizeXY();

		// Clip the region to the viewport bounds.
		MinX = FMath::Clamp(MinX, 0, VPSize.X - 1);
		MinY = FMath::Clamp(MinY, 0, VPSize.Y - 1);
		MaxX = FMath::Clamp(MaxX, 0, VPSize.X - 1);
		MaxY = FMath::Clamp(MaxY, 0, VPSize.Y - 1);

		int32 TestSizeX = MaxX - MinX + 1;
		int32 TestSizeY = MaxY - MinY + 1;
		T* HitProxy = NULL;

		if (TestSizeX > 0 && TestSizeY > 0)
		{
			// Read the hit proxy map from the device.
			TArray<HHitProxy*>	ProxyMap;
			Viewport->GetHitProxyMap(FIntRect(MinX, MinY, MaxX + 1, MaxY + 1), ProxyMap);
			check(ProxyMap.Num() == TestSizeX * TestSizeY);

			// Find the hit proxy in the test region with the highest order.
			int32 ProxyIndex = TestSizeY / 2 * TestSizeX + TestSizeX / 2;
			check(ProxyIndex < ProxyMap.Num());
			HitProxy = HitProxyCast<T>(ProxyMap[ProxyIndex]);

			bool bIsOrtho = Viewport->GetClient()->IsOrtho();
			double MinDist = TNumericLimits<double>::Max();

			for (int32 TestY = 0; TestY < TestSizeY; TestY++)
			{
				for (int32 TestX = 0; TestX < TestSizeX; TestX++)
				{
					if (T* TestProxy = HitProxyCast<T>(ProxyMap[TestY * TestSizeX + TestX]))
					{
						double Dist = FVector2D((TestX - X), (TestY - Y)).Size();

						if (!HitProxy || Dist < MinDist /* || (bIsOrtho ? TestProxy->OrthoPriority : TestProxy->Priority) >(bIsOrtho ? HitProxy->OrthoPriority : HitProxy->Priority)*/)
						{
							MinDist = Dist;
							HitProxy = TestProxy;
						}
					}
				}
			}
		}

		return HitProxy;
	}

	template <typename T, typename COMPARATOR_CLASS>
	int FindBestFit(const TArray<T>& Array, const COMPARATOR_CLASS& Comparator)
	{
		int Ind = 0;
		double Diff = Comparator(Array[0]);
		for (int i = 1; i < Array.Num(); ++i)
		{
			double CheckDiff = Comparator(Array[i]);
			if (CheckDiff < Diff)
			{
				Ind = i;
				Diff = CheckDiff;
			}
		}
		return Ind;
	}

} // UnrealDrive