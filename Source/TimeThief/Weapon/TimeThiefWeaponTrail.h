#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "TimeThiefWeaponTrail.generated.h"

class UNiagaraComponent;
class UNiagaraSystem;
class USceneComponent;
class UWorld;

enum class ETimeThiefWeaponTrailType : uint8
{
	Rifle,
	ShotgunPellet,
	Rocket,
	Grenade
};

struct FTimeThiefWeaponTrailStyle
{
	FTimeThiefWeaponTrailStyle() = default;
	FTimeThiefWeaponTrailStyle(
		float InWidth,
		float InLifetime,
		float InFadeSpeed = 8000.0f,
		float InFadeDistance = 800.0f,
		const FLinearColor& InColor = FLinearColor::White)
		: Width(InWidth)
		, Lifetime(InLifetime)
		, FadeSpeed(InFadeSpeed)
		, FadeDistance(InFadeDistance)
		, Color(InColor)
	{
	}

	float Width = 0.0f;
	float Lifetime = 0.0f;
	float FadeSpeed = 8000.0f;
	float FadeDistance = 800.0f;
	FLinearColor Color = FLinearColor::White;
};

UCLASS()
class TIMETHIEF_API UTimeThiefWeaponTrail : public UObject
{
	GENERATED_BODY()

public:
	UTimeThiefWeaponTrail();

	void DrawHitscanTrail(UWorld& World, ETimeThiefWeaponTrailType TrailType, const FVector& Start, const FVector& End) const;
	UNiagaraComponent* StartProjectileTrail(ETimeThiefWeaponTrailType TrailType, USceneComponent& AttachComponent) const;
	void StopProjectileTrail(UNiagaraComponent* TrailComponent) const;

private:
	UPROPERTY()
	TObjectPtr<UNiagaraSystem> HitscanTrailSystem;

	UPROPERTY()
	TObjectPtr<UNiagaraSystem> ProjectileTrailSystem;

	inline static const FName TrailStartParameterName{TEXT("User.TrailStart")};
	inline static const FName TrailEndParameterName{TEXT("User.TrailEnd")};
	inline static const FName TrailColorParameterName{TEXT("User.TrailColor")};
	inline static const FName TrailWidthParameterName{TEXT("User.TrailWidth")};
	inline static const FName TrailLifetimeParameterName{TEXT("User.TrailLifetime")};
	inline static const FName TrailFadeSpeedParameterName{TEXT("User.TrailFadeSpeed")};
	inline static const FName TrailFadeDistanceParameterName{TEXT("User.TrailFadeDistance")};

	inline static const FTimeThiefWeaponTrailStyle RifleTrail{5.0f, 0.15f, 8000.0f, 800.0f};
	inline static const FTimeThiefWeaponTrailStyle ShotgunPelletTrail{5.0f, 0.15f, 8000.0f, 800.0f};
	inline static const FTimeThiefWeaponTrailStyle RocketTrail{5.0f, 2.0f};
	inline static const FTimeThiefWeaponTrailStyle GrenadeTrail{3.0f, 2.0f};

	const FTimeThiefWeaponTrailStyle& GetTrailStyle(ETimeThiefWeaponTrailType TrailType) const;
	float GetHitscanLifetime(const FVector& Start, const FVector& End, const FTimeThiefWeaponTrailStyle& Style) const;
	void ApplyTrailParameters(UNiagaraComponent& TrailComponent, const FVector& Start, const FVector& End, const FTimeThiefWeaponTrailStyle& Style, float Lifetime) const;
};
