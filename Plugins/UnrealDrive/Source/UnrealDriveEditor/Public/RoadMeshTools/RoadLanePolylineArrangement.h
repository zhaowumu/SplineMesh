/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "CoreMinimal.h"
#include "RoadSplineComponent.h"

namespace UnrealDrive 
{
	enum class EAppandMode
	{
		Normal,
		OtherRevers,
		SelfRevers,
		BothRevers
	};

	/*
	 * TRoadLanePolyline
	 */
	template <typename TPosition, typename TSeft>
	struct TRoadLanePolyline
	{

		TRoadLanePolyline() = default;
		TRoadLanePolyline(const TArray<TPosition>& Vertices)
			: Vertices(Vertices)
		{
		}

		TRoadLanePolyline(TArray<TPosition>&& Vertices)
			: Vertices(MoveTemp(Vertices))
		{
		}

		virtual ~TRoadLanePolyline() {}

		const TPosition& operator[] (int i) const
		{
			return Vertices[i];
		}

		inline TPosition& operator[] (int i)
		{
			return Vertices[i];
		}

		inline int Num() const { return Vertices.Num(); }

		virtual bool CanAppend(const TSeft& Other, EAppandMode AppandMode, double Tolerance) const
		{
			if (AppandMode == EAppandMode::Normal)
			{
				double Dist = FVector::Distance(Vertices.Last().Location, Other.Vertices[0].Location);
				if (FMath::IsNearlyZero(Dist, Tolerance))
				{
					const auto DirA = (Vertices[Vertices.Num() - 1].Location - Vertices[Vertices.Num() - 2].Location).GetSafeNormal();
					const auto DirB = (Other.Vertices[1].Location - Other.Vertices[0].Location).GetSafeNormal();
					if ((DirA | DirB) > 0) 
					{
						return true;
					}
				}
			}
			else if (AppandMode == EAppandMode::OtherRevers)
			{
				double Dist = FVector::Distance(Vertices.Last().Location, Other.Vertices.Last().Location);
				if (FMath::IsNearlyZero(Dist, Tolerance))
				{
					const auto DirA = (Vertices[Vertices.Num() - 1].Location - Vertices[Vertices.Num() - 2].Location).GetSafeNormal();
					const auto DirB = (Other.Vertices[Other.Vertices.Num() - 2].Location - Other.Vertices[Other.Vertices.Num() - 1].Location).GetSafeNormal();
					if ((DirA | DirB) > 0)
					{
						return true;
					}
				}
			}
			else if (AppandMode == EAppandMode::SelfRevers)
			{
				double Dist = FVector::Distance(Vertices[0].Location, Other.Vertices[0].Location);
				if (FMath::IsNearlyZero(Dist, Tolerance))
				{
					const auto DirA = (Vertices[0].Location - Vertices[1].Location).GetSafeNormal();
					const auto DirB = (Other.Vertices[1].Location - Other.Vertices[0].Location).GetSafeNormal();
					if ((DirA | DirB) > 0)
					{
						return true;
					}
				}
			}
			else // AppandMode == EAppandMode::BothRevers
			{
				double Dist = FVector::Distance(Vertices[0].Location, Other.Vertices.Last().Location);
				if (FMath::IsNearlyZero(Dist, Tolerance))
				{
					const auto DirA = (Vertices[0].Location - Vertices[1].Location).GetSafeNormal();
					const auto DirB = (Other.Vertices[Other.Vertices.Num() - 2].Location - Other.Vertices[Other.Vertices.Num() - 1].Location).GetSafeNormal();
					if ((DirA | DirB) > 0)
					{
						return true;
					}
				}
			}
			return false;
		};

		virtual void Append(TArray<TPosition>&& Other)
		{
			Vertices.RemoveAt(Vertices.Num() - 1, EAllowShrinking::No);
			Vertices.Append(MoveTemp(Other));
		};

		virtual void Reverse()
		{
			Algo::Reverse(Vertices);
		}

		TArray<TPosition> Vertices;
	};


	/*
	 * TRoadLanePolylineArrangement
	 */
	template <typename TPolyline>
	struct TRoadLanePolylineArrangement
	{
		virtual ~TRoadLanePolylineArrangement() {}

		TArray<TPolyline> Polylines;

		void Insert(TPolyline&& Polyline, double Tolerance)
		{
			if (Polylines.Num() == 0)
			{
				Polylines.Add(MoveTemp(Polyline));
				return;
			}

			const int AppendIndex = TryAppend(MoveTemp(Polyline), INDEX_NONE, Tolerance);
			if (AppendIndex != INDEX_NONE)
			{
				const int AppendIndex2 = TryAppend(MoveTemp(Polylines[AppendIndex]), AppendIndex, Tolerance);
				if (AppendIndex2 != INDEX_NONE)
				{
					Polylines.RemoveAt(AppendIndex);
				}
			}
			else
			{
				Polylines.Add(MoveTemp(Polyline));
			}
		}

		int TryAppend(TPolyline&& Polyline, int SkipIndex, double Tolerance)
		{
			for (int i = 0; i < Polylines.Num(); ++i)
			{
				if (i == SkipIndex)
				{
					continue;
				}
				auto& It = Polylines[i];
				if (It.CanAppend(Polyline, EAppandMode::Normal, Tolerance))
				{
					It.Append(MoveTemp(Polyline.Vertices));
					return i;
				}
				else if (It.CanAppend(Polyline, EAppandMode::OtherRevers, Tolerance))
				{
					Polyline.Reverse();
					It.Append(MoveTemp(Polyline.Vertices));
					return i;
				}
				else if (Polyline.CanAppend(It, EAppandMode::Normal, Tolerance))
				{
					Polyline.Append(MoveTemp(It.Vertices));
					It.Vertices = MoveTemp(Polyline.Vertices);
					return i;
				}
				else if (Polyline.CanAppend(It, EAppandMode::SelfRevers, Tolerance))
				{
					Polyline.Reverse();
					Polyline.Append(MoveTemp(It.Vertices));
					It.Vertices = MoveTemp(Polyline.Vertices);
					return i;
				}
			}
			return INDEX_NONE;
		}
	};

	template <typename TPosition>
	void GetThreeVectors(const TArray<TPosition>& Vertices, int Index, FVector& OutRightVector, FVector& OutUpVector, FVector& OutForwardVector, double& OutSinA)
	{
		check(Vertices.Num() > 1);

		OutUpVector = Vertices[Index].Quat.GetUpVector();

		if (Index == 0)
		{
			auto& PtB = Vertices[Index].Location;
			auto& PtC = Vertices[Index + 1].Location;
			FVector ForwardVector1 = (PtC - PtB).GetSafeNormal();
			OutRightVector = FVector::CrossProduct(OutUpVector, ForwardVector1).GetSafeNormal();
			OutForwardVector = ForwardVector1;
			OutSinA = 1.0;

		}
		else if (Index == Vertices.Num() - 1)
		{
			auto& PtA = Vertices[Index - 1].Location;
			auto& PtB = Vertices[Index].Location;
			FVector ForwardVector0 = (PtB - PtA).GetSafeNormal();
			OutRightVector = FVector::CrossProduct(OutUpVector, ForwardVector0).GetSafeNormal();
			OutForwardVector = ForwardVector0;
			OutSinA = 1.0;
		}
		else
		{
			auto& PtA = Vertices[Index - 1].Location;
			auto& PtB = Vertices[Index].Location;
			auto& PtC = Vertices[Index + 1].Location;
			FVector ForwardVector0 = (PtB - PtA).GetSafeNormal();
			FVector ForwardVector1 = (PtC - PtB).GetSafeNormal();
			OutSinA = FMath::Sin(OpUtils::AngleBetweenNormals(-ForwardVector0, ForwardVector1) * 0.5);
			OutForwardVector = (ForwardVector0 + ForwardVector1).GetSafeNormal();

			//check(FMath::Abs(SinA) > 0.1);

			FVector RightVector0 = FVector::CrossProduct(OutUpVector, ForwardVector0).GetSafeNormal();
			FVector RightVector1 = FVector::CrossProduct(OutUpVector, ForwardVector1).GetSafeNormal();
			OutRightVector = (RightVector1 + RightVector0).GetSafeNormal();
		}
	}

} // UnrealDrive
