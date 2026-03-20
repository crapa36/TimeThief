// Fill out your copyright notice in the Description page of Project Settings.


#include "InteractionActorBase.h"

#include "Character/TimeThiefPlayerCharacter.h"
#include "Components/SphereComponent.h"


// Sets default values
AInteractionActorBase::AInteractionActorBase()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>("MeshComponent");
	SetRootComponent(MeshComponent);
	
	InteractionSphere = CreateDefaultSubobject<USphereComponent>("InteractionSphere");
	InteractionSphere->SetupAttachment(RootComponent);
	InteractionSphere->SetHiddenInGame(false);
	
	InteractionSphere->OnComponentBeginOverlap.AddUniqueDynamic(this, &ThisClass::OnBeginOverlap);
	InteractionSphere->OnComponentEndOverlap.AddUniqueDynamic(this, &ThisClass::OnEndOverlap);
}

// Called when the game starts or when spawned
void AInteractionActorBase::BeginPlay()
{
	Super::BeginPlay();
}

// Called every frame
void AInteractionActorBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AInteractionActorBase::OnBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (ATimeThiefPlayerCharacter* Player = Cast<ATimeThiefPlayerCharacter>(OtherActor))
	{
		Player->AddNearInteractionActor(this);
	}
}

void AInteractionActorBase::OnEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
                                         UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex)
{
	if (ATimeThiefPlayerCharacter* Player = Cast<ATimeThiefPlayerCharacter>(OtherActor))
	{
		Player->RemoveNearInteractionActor(this);
	}
}
