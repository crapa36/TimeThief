// Fill out your copyright notice in the Description page of Project Settings.


#include "TimeStormVisualActor.h"

#include "Components/System/TimeStormComponent.h"
#include "Game/TimeThiefGameState.h"
#include "Kismet/GameplayStatics.h"


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
	
	DynamicPostProcessMaterial = UMaterialInstanceDynamic::Create(BasePostProcessMaterial, this);
	
	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), APostProcessVolume::StaticClass(), FoundActors);

	if (FoundActors.Num() > 0)
	{
		TargetPPVolume = Cast<APostProcessVolume>(FoundActors[0]);
        
		if (TargetPPVolume && DynamicPostProcessMaterial)
		{
			TargetPPVolume->AddOrUpdateBlendable(DynamicPostProcessMaterial, 1.0f);
			TargetPPVolume->bUnbound = true;
		}
	}
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
		
			SetActorLocation(FVector{CurrCenter, -1000.f});
		
			SetActorScale3D(FVector{CurrRadius, CurrRadius, 1.f});
			
			if (auto PC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
			{
				if (auto Player = PC->GetPawn())
				{
					FVector PlayerLocation = Player->GetActorLocation();
					FVector2D PlayerXY = FVector2D(PlayerLocation.X, PlayerLocation.Y);
					
					float Distance = FVector2D::Distance(PlayerXY, CurrCenter);
					
					float Weight = Distance < CurrRadius ? 0.f : 1.f;
					
					if (TargetPPVolume)
					{
						TargetPPVolume->AddOrUpdateBlendable(DynamicPostProcessMaterial, Weight);
					}
				}
			}
			
		}
	}
}

