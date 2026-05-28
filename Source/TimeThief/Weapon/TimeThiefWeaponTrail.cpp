#include "Weapon/TimeThiefWeaponTrail.h"

#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "UObject/ConstructorHelpers.h"

UTimeThiefWeaponTrail::UTimeThiefWeaponTrail()
{
	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> DefaultHitscanTrailSystem(
		TEXT("/Game/Game/VFX/Weapons/NS_WhiteHitscanTrail.NS_WhiteHitscanTrail"));
	checkf(DefaultHitscanTrailSystem.Succeeded(), TEXT("Missing Niagara asset: /Game/Game/VFX/Weapons/NS_WhiteHitscanTrail."));
	HitscanTrailSystem = DefaultHitscanTrailSystem.Object;

	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> DefaultProjectileTrailSystem(
		TEXT("/Game/Game/VFX/Weapons/NS_WhiteProjectileTrail.NS_WhiteProjectileTrail"));
	checkf(DefaultProjectileTrailSystem.Succeeded(), TEXT("Missing Niagara asset: /Game/Game/VFX/Weapons/NS_WhiteProjectileTrail."));
	ProjectileTrailSystem = DefaultProjectileTrailSystem.Object;
}

void UTimeThiefWeaponTrail::DrawHitscanTrail(UWorld& World, ETimeThiefWeaponTrailType TrailType, const FVector& Start, const FVector& End) const
{
	const FTimeThiefWeaponTrailStyle& TrailStyle = GetTrailStyle(TrailType);
	checkf(HitscanTrailSystem, TEXT("Hitscan trail Niagara asset is not loaded."));

	UNiagaraComponent* TrailComponent = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		&World,
		HitscanTrailSystem,
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		FVector::OneVector,
		true,
		false,
		ENCPoolMethod::AutoRelease,
		false);

	if (TrailComponent)
	{
		ApplyTrailParameters(*TrailComponent, Start, End, TrailStyle, GetHitscanLifetime(Start, End, TrailStyle));
		TrailComponent->Activate();
	}
}

UNiagaraComponent* UTimeThiefWeaponTrail::StartProjectileTrail(ETimeThiefWeaponTrailType TrailType, USceneComponent& AttachComponent) const
{
	checkf(ProjectileTrailSystem, TEXT("Projectile trail Niagara asset is not loaded."));

	UNiagaraComponent* TrailComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(
		ProjectileTrailSystem,
		&AttachComponent,
		NAME_None,
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		EAttachLocation::SnapToTarget,
		true,
		false,
		ENCPoolMethod::AutoRelease,
		false);

	if (TrailComponent)
	{
		const FTimeThiefWeaponTrailStyle& TrailStyle = GetTrailStyle(TrailType);

		ApplyTrailParameters(*TrailComponent, FVector::ZeroVector, FVector::ZeroVector, TrailStyle, TrailStyle.Lifetime);
		TrailComponent->Activate();
	}

	return TrailComponent;
}

void UTimeThiefWeaponTrail::StopProjectileTrail(UNiagaraComponent* TrailComponent) const
{
	if (!IsValid(TrailComponent))
	{
		return;
	}

	TrailComponent->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	TrailComponent->Deactivate();
}

const FTimeThiefWeaponTrailStyle& UTimeThiefWeaponTrail::GetTrailStyle(ETimeThiefWeaponTrailType TrailType) const
{
	switch (TrailType)
	{
	case ETimeThiefWeaponTrailType::Rifle:
		return RifleTrail;
	case ETimeThiefWeaponTrailType::ShotgunPellet:
		return ShotgunPelletTrail;
	case ETimeThiefWeaponTrailType::Rocket:
		return RocketTrail;
	case ETimeThiefWeaponTrailType::Grenade:
		return GrenadeTrail;
	default:
		break;
	}

	return RifleTrail;
}

float UTimeThiefWeaponTrail::GetHitscanLifetime(const FVector& Start, const FVector& End, const FTimeThiefWeaponTrailStyle& Style) const
{
	const float TrailLength = FVector::Distance(Start, End);
	const float FadeSpeed = FMath::Max(Style.FadeSpeed, KINDA_SMALL_NUMBER);
	const float FadeDistance = FMath::Max(Style.FadeDistance, 0.0f);

	return FMath::Max(Style.Lifetime, (TrailLength + FadeDistance) / FadeSpeed);
}

void UTimeThiefWeaponTrail::ApplyTrailParameters(UNiagaraComponent& TrailComponent, const FVector& Start, const FVector& End, const FTimeThiefWeaponTrailStyle& Style, float Lifetime) const
{
	TrailComponent.SetVariablePosition(TrailStartParameterName, Start);
	TrailComponent.SetVariablePosition(TrailEndParameterName, End);
	TrailComponent.SetVariableLinearColor(TrailColorParameterName, Style.Color);
	TrailComponent.SetVariableFloat(TrailWidthParameterName, Style.Width);
	TrailComponent.SetVariableFloat(TrailLifetimeParameterName, FMath::Max(Lifetime, KINDA_SMALL_NUMBER));
	TrailComponent.SetVariableFloat(TrailFadeSpeedParameterName, FMath::Max(Style.FadeSpeed, KINDA_SMALL_NUMBER));
	TrailComponent.SetVariableFloat(TrailFadeDistanceParameterName, FMath::Max(Style.FadeDistance, KINDA_SMALL_NUMBER));
}
