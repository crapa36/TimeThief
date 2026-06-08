#include "UI/TimeThiefControlGuideWidget.h"

#include "Components/TextBlock.h"
#include "Components/TimeThiefHeroComponent.h"
#include "EnhancedActionKeyMapping.h"
#include "Input/TimeThiefInputConfig.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "TimeThiefGameplayTags.h"

UTimeThiefControlGuideWidget::UTimeThiefControlGuideWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InputConfigOverride = TSoftObjectPtr<UTimeThiefInputConfig>(FSoftObjectPath(TEXT("/Game/Game/Input/DA_InputConfig.DA_InputConfig")));
	InputMappingContexts.Add(TSoftObjectPtr<UInputMappingContext>(FSoftObjectPath(TEXT("/Game/Game/Input/IMC_Default.IMC_Default"))));
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UTimeThiefControlGuideWidget::NativeConstruct()
{
	Super::NativeConstruct();

	RebuildGuideEntries();
	RefreshControlGuide();
}

void UTimeThiefControlGuideWidget::RefreshControlGuide()
{
	if (GuideEntries.IsEmpty())
	{
		RebuildGuideEntries();
	}

	for (FGuideEntry& Entry : GuideEntries)
	{
		if (Entry.KeyText.IsValid())
		{
			Entry.KeyText->SetText(ResolveKeysForTags(Entry.InputTags));
		}
	}
}

void UTimeThiefControlGuideWidget::RebuildGuideEntries()
{
	const FTimeThiefGameplayTags& Tags = FTimeThiefGameplayTags::Get();

	GuideEntries.Reset();
	GuideEntries.Add({ { Tags.InputTag_Action_Move }, Move_KeyText });
	GuideEntries.Add({ { Tags.InputTag_Action_Jump }, Jump_KeyText });
	GuideEntries.Add({ { Tags.InputTag_Action_Wire }, Wire_KeyText });
	GuideEntries.Add({ { Tags.InputTag_Action_Fire }, Fire_KeyText });
	GuideEntries.Add({ { Tags.InputTag_Action_Aim }, Aim_KeyText });
	GuideEntries.Add({ { Tags.InputTag_Action_Reload }, Reload_KeyText });
	GuideEntries.Add({ { Tags.InputTag_Action_EquipRifle, Tags.InputTag_Action_EquipShotgun, Tags.InputTag_Action_EquipRocketLauncher }, WeaponSelect_KeyText });
	GuideEntries.Add({ { Tags.InputTag_Action_Throw }, Throw_KeyText });
	GuideEntries.Add({ { Tags.InputTag_Action_Inventory }, Inventory_KeyText });
	GuideEntries.Add({ { Tags.InputTag_Action_SavePoint }, SavePoint_KeyText });
	GuideEntries.Add({ { Tags.InputTag_Action_ToggleMinimap }, Minimap_KeyText });
	GuideEntries.Add({ { Tags.InputTag_Action_Interact }, Interact_KeyText });
}

const UTimeThiefInputConfig* UTimeThiefControlGuideWidget::ResolveInputConfig() const
{
	if (const APlayerController* PlayerController = GetOwningPlayer())
	{
		if (const APawn* Pawn = PlayerController->GetPawn())
		{
			if (const UTimeThiefHeroComponent* HeroComponent = UTimeThiefHeroComponent::FindHeroComponent(Pawn))
			{
				if (const UTimeThiefInputConfig* InputConfig = HeroComponent->GetInputConfig())
				{
					return InputConfig;
				}
			}
		}
	}

	return InputConfigOverride.LoadSynchronous();
}

void UTimeThiefControlGuideWidget::ResolveInputMappingContexts(TArray<const UInputMappingContext*>& OutMappingContexts) const
{
	OutMappingContexts.Reset();

	if (const APlayerController* PlayerController = GetOwningPlayer())
	{
		if (const APawn* Pawn = PlayerController->GetPawn())
		{
			if (const UTimeThiefHeroComponent* HeroComponent = UTimeThiefHeroComponent::FindHeroComponent(Pawn))
			{
				HeroComponent->GetInputMappingContexts(OutMappingContexts);
				if (!OutMappingContexts.IsEmpty())
				{
					return;
				}
			}
		}
	}

	for (const TSoftObjectPtr<UInputMappingContext>& MappingContext : InputMappingContexts)
	{
		if (const UInputMappingContext* LoadedMappingContext = MappingContext.LoadSynchronous())
		{
			OutMappingContexts.Add(LoadedMappingContext);
		}
	}
}

const UInputAction* UTimeThiefControlGuideWidget::FindInputActionForTag(const FGameplayTag& InputTag) const
{
	if (const UTimeThiefInputConfig* InputConfig = ResolveInputConfig())
	{
		if (const UInputAction* NativeAction = InputConfig->FindNativeInputActionForTag(InputTag, false))
		{
			return NativeAction;
		}

		return InputConfig->FindAbilityInputActionForTag(InputTag, false);
	}

	return nullptr;
}

FText UTimeThiefControlGuideWidget::ResolveKeysForTags(const TArray<FGameplayTag>& InputTags) const
{
	TArray<const UInputMappingContext*> MappingContexts;
	ResolveInputMappingContexts(MappingContexts);

	TArray<FString> KeyLabels;
	for (const FGameplayTag& InputTag : InputTags)
	{
		const UInputAction* InputAction = FindInputActionForTag(InputTag);
		if (!InputAction)
		{
			continue;
		}

		for (const UInputMappingContext* MappingContext : MappingContexts)
		{
			if (!MappingContext)
			{
				continue;
			}

			for (const FEnhancedActionKeyMapping& Mapping : MappingContext->GetMappings())
			{
				if (Mapping.Action == InputAction && Mapping.Key.IsValid())
				{
					KeyLabels.AddUnique(FormatKeyLabel(Mapping.Key));
				}
			}
		}
	}

	return FText::FromString(FormatKeyLabels(MoveTemp(KeyLabels)));
}

FString UTimeThiefControlGuideWidget::FormatKeyLabel(const FKey& Key)
{
	if (Key == EKeys::LeftMouseButton)
	{
		return TEXT("LMB");
	}
	if (Key == EKeys::RightMouseButton)
	{
		return TEXT("RMB");
	}
	if (Key == EKeys::SpaceBar)
	{
		return TEXT("SPACE");
	}
	if (Key == EKeys::Tab)
	{
		return TEXT("TAB");
	}
	if (Key == EKeys::LeftShift || Key == EKeys::RightShift)
	{
		return TEXT("SHIFT");
	}
	if (Key == EKeys::One)
	{
		return TEXT("1");
	}
	if (Key == EKeys::Two)
	{
		return TEXT("2");
	}
	if (Key == EKeys::Three)
	{
		return TEXT("3");
	}

	return Key.GetDisplayName(false).ToString().ToUpper();
}

FString UTimeThiefControlGuideWidget::FormatKeyLabels(TArray<FString>&& KeyLabels)
{
	if (KeyLabels.IsEmpty())
	{
		return TEXT("-");
	}

	KeyLabels.Sort();

	if (KeyLabels.Contains(TEXT("A")) && KeyLabels.Contains(TEXT("D")) && KeyLabels.Contains(TEXT("S")) && KeyLabels.Contains(TEXT("W")))
	{
		return TEXT("WASD");
	}

	if (KeyLabels.Num() == 3 && KeyLabels.Contains(TEXT("1")) && KeyLabels.Contains(TEXT("2")) && KeyLabels.Contains(TEXT("3")))
	{
		return TEXT("1~3");
	}

	return FString::Join(KeyLabels, TEXT(" / "));
}
