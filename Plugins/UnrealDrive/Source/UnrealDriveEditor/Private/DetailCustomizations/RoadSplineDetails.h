/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "RoadSplineComponent.h"
#include "IDetailChildrenBuilder.h"
#include "IDetailCustomNodeBuilder.h"
#include "ComponentVisualizers/RoadSectionComponentVisualizer.h"
#include "Widgets/SWidget.h"

class ISplineMetadataDetails;

class FRoadSplineDetails : public IDetailCustomNodeBuilder, public TSharedFromThis<FRoadSplineDetails>
{
public:
	FRoadSplineDetails(URoadSplineComponent* InOwningSplineComponent);

	//~ Begin IDetailCustomNodeBuilder interface
	virtual void SetOnRebuildChildren(FSimpleDelegate InOnRegenerateChildren) override;
	virtual void GenerateHeaderRowContent(FDetailWidgetRow& NodeRow) override;
	virtual void GenerateChildContent(IDetailChildrenBuilder& ChildrenBuilder) override;
	virtual void Tick(float DeltaTime) override;
	virtual bool RequiresTick() const override { return true; }
	virtual bool InitiallyCollapsed() const override { return false; }
	virtual FName GetName() const override;
	//~ End IDetailCustomNodeBuilder interface

	static bool bAlreadyWarnedInvalidIndex;

private:

	template <typename T>
	struct TSharedValue
	{
		TSharedValue() : bInitialized(false) {}

		void Reset()
		{
			bInitialized = false;
		}

		void Add(T InValue)
		{
			if (!bInitialized)
			{
				Value = InValue;
				bInitialized = true;
			}
			else
			{
				if (Value.IsSet() && InValue != Value.GetValue()) { Value.Reset(); }
			}
		}

		TOptional<T> Value;
		bool bInitialized;
	};

	struct FSharedVectorValue
	{
		FSharedVectorValue() : bInitialized(false) {}

		void Reset()
		{
			bInitialized = false;
		}

		bool IsValid() const { return bInitialized; }

		void Add(const FVector& V)
		{
			if (!bInitialized)
			{
				X = V.X;
				Y = V.Y;
				Z = V.Z;
				bInitialized = true;
			}
			else
			{
				if (X.IsSet() && V.X != X.GetValue()) { X.Reset(); }
				if (Y.IsSet() && V.Y != Y.GetValue()) { Y.Reset(); }
				if (Z.IsSet() && V.Z != Z.GetValue()) { Z.Reset(); }
			}
		}

		TOptional<float> X;
		TOptional<float> Y;
		TOptional<float> Z;
		bool bInitialized;
	};

	struct FSharedRotatorValue
	{
		FSharedRotatorValue() : bInitialized(false) {}

		void Reset()
		{
			bInitialized = false;
		}

		bool IsValid() const { return bInitialized; }

		void Add(const FRotator& R)
		{
			if (!bInitialized)
			{
				Roll = R.Roll;
				Pitch = R.Pitch;
				Yaw = R.Yaw;
				bInitialized = true;
			}
			else
			{
				if (Roll.IsSet() && R.Roll != Roll.GetValue()) { Roll.Reset(); }
				if (Pitch.IsSet() && R.Pitch != Pitch.GetValue()) { Pitch.Reset(); }
				if (Yaw.IsSet() && R.Yaw != Yaw.GetValue()) { Yaw.Reset(); }
			}
		}

		TOptional<float> Roll;
		TOptional<float> Pitch;
		TOptional<float> Yaw;
		bool bInitialized;
	};

	EVisibility IsEnabled() const { return (SelectedKeys.Num() > 0) ? EVisibility::Visible : EVisibility::Collapsed; }
	EVisibility IsDisabled() const { return (SelectedKeys.Num() == 0) ? EVisibility::Visible : EVisibility::Collapsed; }
	bool IsOnePointSelected() const { return SelectedKeys.Num() == 1; }
	bool ArePointsSelected() const { return (SelectedKeys.Num() > 0); };
	bool AreNoPointsSelected() const { return (SelectedKeys.Num() == 0); };
	TOptional<float> GetInputKey() const { return InputKey.Value; }
	TOptional<float> GetPositionX() const { return Position.X; }
	TOptional<float> GetPositionY() const { return Position.Y; }
	TOptional<float> GetPositionZ() const { return Position.Z; }
	TOptional<float> GetArriveTangentX() const { return ArriveTangent.X; }
	TOptional<float> GetArriveTangentY() const { return ArriveTangent.Y; }
	TOptional<float> GetArriveTangentZ() const { return ArriveTangent.Z; }
	TOptional<float> GetLeaveTangentX() const { return LeaveTangent.X; }
	TOptional<float> GetLeaveTangentY() const { return LeaveTangent.Y; }
	TOptional<float> GetLeaveTangentZ() const { return LeaveTangent.Z; }
	TOptional<float> GetRotationRoll() const { return Rotation.Roll; }
	TOptional<float> GetRotationPitch() const { return Rotation.Pitch; }
	TOptional<float> GetRotationYaw() const { return Rotation.Yaw; }
	TOptional<float> GetScaleX() const { return Scale.X; }
	TOptional<float> GetScaleY() const { return Scale.Y; }
	TOptional<float> GetScaleZ() const { return Scale.Z; }
	void OnSetInputKey(float NewValue, ETextCommit::Type CommitInfo);
	void OnSetPosition(float NewValue, ETextCommit::Type CommitInfo, EAxis::Type Axis);
	void OnSetArriveTangent(float NewValue, ETextCommit::Type CommitInfo, EAxis::Type Axis);
	void OnSetLeaveTangent(float NewValue, ETextCommit::Type CommitInfo, EAxis::Type Axis);
	void OnSetRotation(float NewValue, ETextCommit::Type CommitInfo, EAxis::Type Axis);
	void OnSetScale(float NewValue, ETextCommit::Type CommitInfo, EAxis::Type Axis);
	FText GetPointType() const;
	void OnSplinePointTypeChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo);
	TSharedRef<SWidget> OnGenerateComboWidget(TSharedPtr<FString> InComboString);

	void GenerateSplinePointSelectionControls(IDetailChildrenBuilder& ChildrenBuilder);
	FReply OnSelectFirstLastSplinePoint(bool bFirst);
	FReply OnSelectPrevNextSplinePoint(bool bNext, bool bAddToSelection);
	FReply OnSelectAllSplinePoints();

	URoadSplineComponent* GetSplineComponentToVisualize() const;

	void UpdateValues();

	enum class ESplinePointProperty
	{
		Location,
		Rotation,
		Scale,
		ArriveTangent,
		LeaveTangent
	};

	TSharedRef<SWidget> BuildSplinePointPropertyLabel(ESplinePointProperty SplinePointProp);
	void OnSetTransformEditingAbsolute(ESplinePointProperty SplinePointProp, bool bIsAbsolute);
	bool IsTransformEditingAbsolute(ESplinePointProperty SplinePointProperty) const;
	bool IsTransformEditingRelative(ESplinePointProperty SplinePointProperty) const;
	FText GetSplinePointPropertyText(ESplinePointProperty SplinePointProp) const;
	void SetSplinePointProperty(ESplinePointProperty SplinePointProp, FVector NewValue, EAxisList::Type Axis, bool bCommitted);

	FUIAction CreateCopyAction(ESplinePointProperty SplinePointProp);
	FUIAction CreatePasteAction(ESplinePointProperty SplinePointProp);

	bool OnCanCopy(ESplinePointProperty SplinePointProp) const { return true; }
	void OnCopy(ESplinePointProperty SplinePointProp);
	void OnPaste(ESplinePointProperty SplinePointProp);

	void OnPasteFromText(const FString& InTag, const FString& InText, const TOptional<FGuid>& InOperationId, ESplinePointProperty SplinePointProp);
	void PasteFromText(const FString& InTag, const FString& InText, ESplinePointProperty SplinePointProp);

	void OnBeginPositionSlider();
	void OnBeginScaleSlider();
	void OnEndSlider(float);

	URoadSplineComponent* SplineComp;
	URoadSplineComponent* SplineCompArchetype;
	TSet<int32> SelectedKeys;

	TSharedValue<float> InputKey;
	FSharedVectorValue Position;
	FSharedVectorValue ArriveTangent;
	FSharedVectorValue LeaveTangent;
	FSharedVectorValue Scale;
	FSharedRotatorValue Rotation;
	TSharedValue<ESplinePointType::Type> PointType;

	TSharedPtr<class FRoadSplineComponentVisualizer> SplineVisualizer;
	FProperty* SplineCurvesProperty;
	TArray<TSharedPtr<FString>> SplinePointTypes;
	TSharedPtr<ISplineMetadataDetails> SplineMetaDataDetails;
	FSimpleDelegate OnRegenerateChildren;

	bool bEditingLocationAbsolute = false;
	bool bEditingRotationAbsolute = false;

	bool bInSliderTransaction = false;
};