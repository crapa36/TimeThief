// Fill out your copyright notice in the Description page of Project Settings.


#include "StoreActor.h"

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

