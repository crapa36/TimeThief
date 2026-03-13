// Fill out your copyright notice in the Description page of Project Settings.


#include "TimeStormVisualActor.h"

#include "Components/System/TimeStormComponent.h"
#include "Game/TimeThiefGameState.h"


// Sets default values
ATimeStormVisualActor::ATimeStormVisualActor()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	
	ZoneMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ZoneMesh"));
	RootComponent = ZoneMesh;
	
	ZoneMesh->SetCollisionProfileName(FName{"NoCollision"});
	ZoneMesh->SetGenerateOverlapEvents(false);
	ZoneMesh->SetCastShadow(false);
}

// Called when the game starts or when spawned
void ATimeStormVisualActor::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ATimeStormVisualActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (const ATimeThiefGameState* GameState = GetWorld()->GetGameState<ATimeThiefGameState>())
	{
		if (const UTimeStormComponent* TimeStormComponent = GameState->TimeStormComponent)
		{
			FVector2D CurrCenter;
			float CurrRadius;
			TimeStormComponent->GetCurrStormZone(CurrCenter, CurrRadius);
		
			SetActorLocation(FVector{CurrCenter, -100.f});
		
			SetActorScale3D(FVector{CurrRadius, CurrRadius, 1.f});
		}
	}
}

