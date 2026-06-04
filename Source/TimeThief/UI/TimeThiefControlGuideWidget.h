#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "TimeThiefControlGuideWidget.generated.h"

class UInputAction;
class UInputMappingContext;
class UTextBlock;
class UTimeThiefInputConfig;

UCLASS(Abstract)
class TIMETHIEF_API UTimeThiefControlGuideWidget : public UUserWidget
{
	GENERATED_BODY()

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Move_KeyText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Jump_KeyText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Wire_KeyText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Fire_KeyText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Aim_KeyText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Reload_KeyText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> WeaponSelect_KeyText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Throw_KeyText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Inventory_KeyText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> SavePoint_KeyText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Minimap_KeyText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Interact_KeyText;

public:
	UTimeThiefControlGuideWidget(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable, Category = "TimeThief|Control Guide")
	void RefreshControlGuide();

protected:
	virtual void NativeConstruct() override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Control Guide")
	TSoftObjectPtr<UTimeThiefInputConfig> InputConfigOverride;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Control Guide")
	TArray<TSoftObjectPtr<UInputMappingContext>> InputMappingContexts;

private:
	struct FGuideEntry
	{
		TArray<FGameplayTag> InputTags;
		TWeakObjectPtr<UTextBlock> KeyText;
	};

	void RebuildGuideEntries();

	const UTimeThiefInputConfig* ResolveInputConfig() const;
	void ResolveInputMappingContexts(TArray<const UInputMappingContext*>& OutMappingContexts) const;
	const UInputAction* FindInputActionForTag(const FGameplayTag& InputTag) const;
	FText ResolveKeysForTags(const TArray<FGameplayTag>& InputTags) const;

	static FString FormatKeyLabel(const FKey& Key);
	static FString FormatKeyLabels(TArray<FString>&& KeyLabels);

	TArray<FGuideEntry> GuideEntries;
};
