// Fill out your copyright notice in the Description page of Project Settings.


#include "ItemBase.h"

#include "ChannelCommons.h"
#include "Character/TimeThiefPlayerCharacter.h"
#include "Components/SphereComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/System/InventorySystemComponent.h"
#include "Game/ItemSettings.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Network/NetworkGameInstanceSubsystem.h"
#include "NiagaraComponent.h"
#include "UI/PromptWidget.h"


// Sets default values
AItemBase::AItemBase()
{
	PrimaryActorTick.bCanEverTick = false;
	
	LookingSphere = CreateDefaultSubobject<USphereComponent>("LookingSphere");
	LookingSphere->SetCollisionResponseToChannel(ECC_InteractTrace, ECR_Block);
	LookingSphere->SetupAttachment(RootComponent);

	IdleFXComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("IdleFXComponent"));
	IdleFXComponent->SetupAttachment(RootComponent);
	IdleFXComponent->SetAutoActivate(false);
}

// Called when the game starts or when spawned
void AItemBase::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AItemBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AItemBase::Interact(const ATimeThiefPlayerCharacter* Player)
{
	if (auto GI = UNetworkGameInstanceSubsystem::Get(this); GI && GI->IsConnected())
	{
		TryRequestServer();
	}
	else
	{
		if (UInventorySystemComponent* Inven = Player->GetInventoryComponent())
		{
			Inven->AddItem(ItemID, Quantity);
			Disable();
		}
	}
}

void AItemBase::OnBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (ATimeThiefPlayerCharacter* Player = Cast<ATimeThiefPlayerCharacter>(OtherActor))
	{
		Player->AddVicinityItem(this);
	}
}

void AItemBase::OnEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex)
{
	if (ATimeThiefPlayerCharacter* Player = Cast<ATimeThiefPlayerCharacter>(OtherActor))
	{
		Player->RemoveVicinityItem(this);
	}
}

void AItemBase::Enable()
{
	bIsEnabled = true;
}

void AItemBase::Disable()
{
	LookingSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	InteractionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComponent->SetVisibility(false);
	DeactivateIdleFX();
	bIsEnabled = false;
}

void AItemBase::ApplySpawnRuntimeState(const FNetworkEntityState& EntityState)
{
	Super::ApplySpawnRuntimeState(EntityState);
	
	SetItemStack(static_cast<EItemID>(EntityState.TemplateId), EntityState.ItemCount);
}

void AItemBase::TryRequestServer()
{
	if (UNetworkGameInstanceSubsystem* NGIS = UNetworkGameInstanceSubsystem::Get(this))
	{
		NGIS->SendItemPickUp(GetEntityId());
	}
}

void AItemBase::ActivateIdleFX()
{
	if (!IdleFXComponent || !IdleFXSystem)
	{
		return;
	}

	IdleFXComponent->SetAsset(IdleFXSystem);
	IdleFXComponent->SetRelativeLocation(IdleFXOffset);
	IdleFXComponent->Activate(true);
}

void AItemBase::DeactivateIdleFX()
{
	if (IdleFXComponent)
	{
		IdleFXComponent->Deactivate();
	}
}

void AItemBase::SetItemStack(EItemID NewItemID, int NewQuantity)
{
	ItemID = NewItemID;
	Quantity = NewQuantity;
	
	LookingSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	MeshComponent->SetVisibility(true);
	MeshComponent->EmptyOverrideMaterials();
	MeshComponent->SetStaticMesh(GetDefault<UItemSettings>()->GetItemMesh(ItemID));
	ActivateIdleFX();
	
	InteractionWidgetComponent->SetRelativeLocation(FVector{0, 0, MeshComponent->Bounds.BoxExtent.Z + 20});
	
	InteractionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	
	if (auto Prompt = Cast<UPromptWidget>(InteractionWidgetComponent->GetWidget()))
	{
		Prompt->SetPromptText(FText::FromString(GetDefault<UItemSettings>()->GetItemName(ItemID) + FString::Printf(TEXT(" 줍기"))));
	}
}

