/*
 * Copyright (c) 2025 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */


#include "Utils/CompVisUtils.h"
#include "EngineUtils.h"
#include "Selection.h"

namespace CompVisUtils
{
	void DeselectAllExcept(const UActorComponent* TargetComponent)
	{
		for (FSelectionIterator It(GEditor->GetSelectedComponentIterator()); It; ++It)
		{
			auto* Comp = Cast<UActorComponent>(*It);
			if (Comp && Comp != TargetComponent)
			{
				GEditor->GetSelectedComponents()->Deselect(Comp);
			}
		}
		for (FSelectionIterator It(GEditor->GetSelectedActorIterator()); It; ++It)
		{
			if (*It != TargetComponent->GetOwner())
			{
				GEditor->GetSelectedActors()->Deselect(*It);
			}
		}
	}

	bool IsSelectedInViewport(const UActorComponent* TargetComponent)
	{
		USelection* SelectedComponents = GEditor->GetSelectedComponents();
		for (FSelectionIterator Iter(*SelectedComponents); Iter; ++Iter)
		{
			if (Cast<UActorComponent>(*Iter) == TargetComponent)
			{
				return true;
			}
		}
		return false;
	}

} // CompVisUtils