// Fill out your copyright notice in the Description page of Project Settings.


#include "InteractionActorBase.h"

#include "Blueprint/UserWidget.h"
#include "Character/TimeThiefPlayerCharacter.h"
#include "Components/SphereComponent.h"
#include "Components/WidgetComponent.h"
#include "ChannelCommons.h"

// Sets default values
AInteractionActorBase::AInteractionActorBase()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>("MeshComponent");
	MeshComponent->SetCollisionResponseToChannel(ECC_InteractTrace, ECR_Block);
	SetRootComponent(MeshComponent);
	
	InteractionSphere = CreateDefaultSubobject<USphereComponent>("InteractionSphere");
	InteractionSphere->SetupAttachment(RootComponent);
	InteractionSphere->OnComponentBeginOverlap.AddUniqueDynamic(this, &ThisClass::OnBeginOverlap);
	InteractionSphere->OnComponentEndOverlap.AddUniqueDynamic(this, &ThisClass::OnEndOverlap);
	
	InteractionWidgetComponent = CreateDefaultSubobject<UWidgetComponent>("InteractionWidgetComponent");
	InteractionWidgetComponent->SetWidgetSpace(EWidgetSpace::Screen);
	InteractionWidgetComponent->SetupAttachment(RootComponent);
	InteractionWidgetComponent->SetVisibility(false);
	InteractionWidgetComponent->SetDrawAtDesiredSize(true);
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

void AInteractionActorBase::SetVisibilityInteractionUI(bool bShow)
{
	InteractionWidgetComponent->SetVisibility(bShow);
}
