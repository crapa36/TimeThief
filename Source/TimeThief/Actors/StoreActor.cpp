// Fill out your copyright notice in the Description page of Project Settings.


#include "StoreActor.h"

#include "Character/TimeThiefPlayerCharacter.h"
#include "Character/TimeThiefPlayerController.h"
#include "Components/SphereComponent.h"


// Sets default values
AStoreActor::AStoreActor()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>("MeshComponent");
	RootComponent = MeshComponent;
	
	InteractionSphere = CreateDefaultSubobject<USphereComponent>("InteractionSphere");
	InteractionSphere->SetupAttachment(RootComponent);
	
	InteractionSphere->OnComponentBeginOverlap.AddDynamic(this, &AStoreActor::OnBeginOverlap);
	InteractionSphere->OnComponentEndOverlap.AddDynamic(this, &AStoreActor::OnEndOverlap);
}

// Called when the game starts or when spawned
void AStoreActor::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AStoreActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AStoreActor::OnBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (ATimeThiefPlayerCharacter* Player = Cast<ATimeThiefPlayerCharacter>(OtherActor))
	{
		Player->SetNearStore(this);
		if (ATimeThiefPlayerController* PC = Cast<ATimeThiefPlayerController>(Player->GetController()))
		{
			PC->SetStoreVisibility(true);
		}
	}
}

void AStoreActor::OnEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex)
{
	if (ATimeThiefPlayerCharacter* Player = Cast<ATimeThiefPlayerCharacter>(OtherActor))
	{
		if (Player->GetNearStore() == this)
		{
			Player->SetNearStore(nullptr);
			if (ATimeThiefPlayerController* PC = Cast<ATimeThiefPlayerController>(Player->GetController()))
			{
				PC->SetStoreVisibility(false);
			}
		}
	}
}



