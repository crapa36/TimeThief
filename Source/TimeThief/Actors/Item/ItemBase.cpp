// Fill out your copyright notice in the Description page of Project Settings.


#include "ItemBase.h"

#include "ChannelCommons.h"
#include "Character/TimeThiefPlayerCharacter.h"
#include "Components/SphereComponent.h"
#include "Components/System/InventorySystemComponent.h"
#include "Kismet/KismetSystemLibrary.h"


// Sets default values
AItemBase::AItemBase()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	
	LookingSphere = CreateDefaultSubobject<USphereComponent>("LookingSphere");
	LookingSphere->SetCollisionResponseToChannel(ECC_InteractTrace, ECR_Block);
	LookingSphere->SetupAttachment(RootComponent);
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
	if (UInventorySystemComponent* Inven = Player->GetInventoryComponent())
	{
		Inven->AddItem(ItemID, Quantity);
		Destroy();
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

