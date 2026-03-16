// Fill out your copyright notice in the Description page of Project Settings.


#include "ItemBase.h"

#include "Character/TimeThiefPlayerCharacter.h"
#include "Components/System/InventorySystemComponent.h"
#include "Kismet/KismetSystemLibrary.h"


// Sets default values
AItemBase::AItemBase()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
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

void AItemBase::OnBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	Super::OnBeginOverlap(OverlappedComponent, OtherActor, OtherComponent, OtherBodyIndex, bFromSweep, SweepResult);
}

void AItemBase::OnEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex)
{
	Super::OnEndOverlap(OverlappedComponent, OtherActor, OtherComponent, OtherBodyIndex);
}

void AItemBase::Interact(const ATimeThiefPlayerCharacter* Player)
{
	UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("Interacted with item: %s"), *UEnum::GetValueAsString(ItemName)));
	if (UInventorySystemComponent* Inven = Player->GetComponentByClass<UInventorySystemComponent>())
	{
		Inven->AddItem(ItemName);
		Destroy();
	}
}

