#include "Components/ItemMovementComponent.h"

#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"

UItemMovementComponent::UItemMovementComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UItemMovementComponent::BeginPlay()
{
	Super::BeginPlay();

	if (bRandomizeStartPhase)
	{
		ElapsedTime = FMath::FRandRange(0.0f, 1.0f / FMath::Max(BobFrequency, KINDA_SMALL_NUMBER));
	}

	ResetMovementOrigin();
}

void UItemMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	USceneComponent* Target = ResolveTargetComponent();
	if (!Target)
	{
		return;
	}

	const bool bTargetIsRoot = IsTargetRootComponent(Target);
	if (!bHasOrigin || (bTargetIsRoot && WasMovedExternally(Target)))
	{
		ResetMovementOrigin();
	}

	if (bSkipWhenTargetHidden && !Target->IsVisible())
	{
		return;
	}

	ElapsedTime += DeltaTime;

	const float BobOffset = FMath::Sin(ElapsedTime * UE_TWO_PI * BobFrequency) * BobAmplitude;
	const float YawOffset = FMath::Fmod(ElapsedTime * RotationSpeedDegrees, 360.0f);

	if (bTargetIsRoot)
	{
		const FVector NewLocation = InitialWorldLocation + FVector(0.0f, 0.0f, BobOffset);
		const FRotator NewRotation = InitialWorldRotation + FRotator(0.0f, YawOffset, 0.0f);
		Target->SetWorldLocationAndRotation(NewLocation, NewRotation);
		LastAppliedWorldLocation = NewLocation;
		LastAppliedWorldRotation = NewRotation;
		bHasAppliedWorldTransform = true;
		return;
	}

	const FVector NewLocation = InitialRelativeLocation + FVector(0.0f, 0.0f, BobOffset);
	const FRotator NewRotation = InitialRelativeRotation + FRotator(0.0f, YawOffset, 0.0f);
	Target->SetRelativeLocationAndRotation(NewLocation, NewRotation);
}

void UItemMovementComponent::ResetMovementOrigin()
{
	if (USceneComponent* Target = ResolveTargetComponent())
	{
		InitialRelativeLocation = Target->GetRelativeLocation();
		InitialRelativeRotation = Target->GetRelativeRotation();
		InitialWorldLocation = Target->GetComponentLocation();
		InitialWorldRotation = Target->GetComponentRotation();
		LastAppliedWorldLocation = InitialWorldLocation;
		LastAppliedWorldRotation = InitialWorldRotation;
		bHasAppliedWorldTransform = true;
		bHasOrigin = true;
	}
}

USceneComponent* UItemMovementComponent::ResolveTargetComponent() const
{
	if (TargetComponent)
	{
		return TargetComponent;
	}

	const AActor* Owner = GetOwner();
	return Owner ? Owner->GetRootComponent() : nullptr;
}

bool UItemMovementComponent::IsTargetRootComponent(const USceneComponent* Target) const
{
	const AActor* Owner = GetOwner();
	return Owner && Target && Target == Owner->GetRootComponent();
}

bool UItemMovementComponent::WasMovedExternally(const USceneComponent* Target) const
{
	if (!Target || !bHasAppliedWorldTransform)
	{
		return false;
	}

	constexpr float MoveToleranceSq = 1.0f;
	constexpr float RotateToleranceDeg = 0.1f;

	const bool bLocationChanged = FVector::DistSquared(Target->GetComponentLocation(), LastAppliedWorldLocation) > MoveToleranceSq;
	const bool bRotationChanged = !Target->GetComponentRotation().Equals(LastAppliedWorldRotation, RotateToleranceDeg);

	return bLocationChanged || bRotationChanged;
}
