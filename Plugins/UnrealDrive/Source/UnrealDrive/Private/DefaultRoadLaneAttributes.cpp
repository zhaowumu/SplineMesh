/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "DefaultRoadLaneAttributes.h"
#include "UnrealDrivePresetBase.h"

#define LOCTEXT_NAMESPACE "DefaultRoadLaneAttributes"

/*
FText FRoadLaneAttributeMark::GetLabel() const
{
	return LOCTEXT("LaneMark_Label", "Mark");
}

const FSlateIcon& FRoadLaneAttributeMark::GetIcon() const
{
	static auto Icon = FSlateIcon("UnrealDriveEditor", "RoadEditor.RoadLaneMarkMode");
	return  Icon;
}

FText FRoadLaneAttributeSpeed::GetLabel() const
{
	return LOCTEXT("LaneSpeed_Label", "Speed");
}

const FSlateIcon& FRoadLaneAttributeSpeed::GetIcon() const
{
	static auto Icon = FSlateIcon("UnrealDriveEditor", "RoadEditor.RoadLaneSpeedMode");
	return  Icon;
}

FText FRoadLaneAttributeDecalChannelR::GetLabel() const
{
	return LOCTEXT("LaneChannelR_Label", "Decal R");
}

const FSlateIcon& FRoadLaneAttributeDecalChannelR::GetIcon() const
{
	static auto Icon = FSlateIcon("UnrealDriveEditor", "RoadEditor.RoadLaneDecalRMode");
	return  Icon;
}

FText FRoadLaneAttributeDecalChannelG::GetLabel() const
{
	return LOCTEXT("LaneChannelG_Label", "Decal G");
}

const FSlateIcon& FRoadLaneAttributeDecalChannelG::GetIcon() const
{
	static auto Icon = FSlateIcon("UnrealDriveEditor", "RoadEditor.RoadLaneDecalGMode");
	return  Icon;
}

FText FRoadLaneAttributeDecalChannelB::GetLabel() const
{
	return LOCTEXT("LaneChannelB_Label", "Decal B");
}

const FSlateIcon& FRoadLaneAttributeDecalChannelB::GetIcon() const
{
	static auto Icon = FSlateIcon("UnrealDriveEditor", "RoadEditor.RoadLaneDecalBMode");
	return  Icon;
}

FText FRoadLaneAttributeDecalChannelA::GetLabel() const
{
	return LOCTEXT("LaneChannelA_Label", "Decal A");
}

const FSlateIcon& FRoadLaneAttributeDecalChannelA::GetIcon() const
{
	static auto Icon = FSlateIcon("UnrealDriveEditor", "RoadEditor.RoadLaneDecalAMode");
	return  Icon;
}


*/

#if WITH_EDITOR

namespace Local
{
	static FRoadLaneAttributeValue::FDrawStyle DefaultStyle{ FRoadLaneAttributeValue::FDrawStyle::DefaultColor, FRoadLaneAttributeValue::FDrawStyle::DefaultColor };

	static FColor GetColor(const TInstancedStruct<FRoadLaneMarkProfile>& Profile)
	{
		if (auto* AsSolid = Profile.GetPtr<FRoadLaneMarkProfileSolid>())
		{
			return AsSolid->VertexColor;
		}
		if (auto* AsBroked = Profile.GetPtr<FRoadLaneMarkProfileBroked>())
		{
			return AsBroked->VertexColor;
		}
		if (auto* AsDouble = Profile.GetPtr<FRoadLaneMarkProfileDouble>())
		{
			return GetColor(AsDouble->Left);
		}
		return FRoadLaneAttributeValue::FDrawStyle::DefaultColor;
	}

	static FRoadLaneAttributeValue::FDrawStyle MakeFromProfile(const TInstancedStruct<FRoadLaneMarkProfile>& Profile)
	{
		if (!Profile.IsValid())
		{
			return DefaultStyle;
		}

		auto* ProfilePtr = Profile.GetPtr<FRoadLaneMarkProfile>();
		switch (ProfilePtr->Type)
		{
		case ERoadLaneMark::None:
		case ERoadLaneMark::Custom:
			return DefaultStyle;

		case ERoadLaneMark::Solid:
		case ERoadLaneMark::DoubleSolid:
		case ERoadLaneMark::SolidBroked:
		case ERoadLaneMark::BrokedSolid:
			return FRoadLaneAttributeValue::FDrawStyle(GetColor(Profile));

		case ERoadLaneMark::Broked:
		case ERoadLaneMark::DoubleBroked:
		{
			FRoadLaneAttributeValue::FDrawStyle DrawStyle(GetColor(Profile));
			DrawStyle.Color2.A = 0;
			return DrawStyle;
		}
		}

		return DefaultStyle;
	}
}

const FRoadLaneAttributeValue::FDrawStyle& FRoadLaneMark::GetDrawStyle() const
{
	if (ProfileSource == ERoadLaneMarkProfile::UseCustom)
	{
		if (CustomProfile.IsValid())
		{
			auto Type = CustomProfile.Get<FRoadLaneMarkProfile>().Type;
			if (Type != CachedRoadLaneType)
			{
				CachedRoadLaneType = Type;
				CachedDrawStyle = Local::MakeFromProfile(CustomProfile);
			}
			return CachedDrawStyle;
		}

		return Local::DefaultStyle;
	}

	if (CachedProfileName != ProfileName)
	{
		CachedProfileName = ProfileName;
		auto Profiles = UUnrealDrivePresetBase::GetAllProfiles(&UUnrealDrivePresetBase::LaneMarkProfiles);
		if (auto* FoundProfile = Profiles.Find(ProfileName))
		{
			if (FoundProfile->IsValid())
			{
				CachedDrawStyle = Local::MakeFromProfile(*FoundProfile);
			}
			else
			{
				CachedDrawStyle = Local::DefaultStyle;
			}
		}
		else
		{
			CachedDrawStyle = Local::DefaultStyle;
		}
	}
	return CachedDrawStyle;
}
#endif




#undef LOCTEXT_NAMESPACE