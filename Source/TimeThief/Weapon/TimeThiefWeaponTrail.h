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
	FTimeThiefWeaponTrailStyle(float InWidth, float InLifetime, const FLinearColor& InColor = FLinearColor::White)
		: Width(InWidth)
		, Lifetime(InLifetime)
		, Color(InColor)
	{
	}

	float Width = 0.0f;
	float Lifetime = 0.0f;
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

	inline static const FTimeThiefWeaponTrailStyle RifleTrail{4.0f, 0.5f};
	inline static const FTimeThiefWeaponTrailStyle ShotgunPelletTrail{4.0f, 0.5f};
	inline static const FTimeThiefWeaponTrailStyle RocketTrail{5.0f, 2.0f};
	inline static const FTimeThiefWeaponTrailStyle GrenadeTrail{3.0f, 2.0f};

	const FTimeThiefWeaponTrailStyle& GetTrailStyle(ETimeThiefWeaponTrailType TrailType) const;
	void ApplyTrailParameters(UNiagaraComponent& TrailComponent, const FVector& Start, const FVector& End, const FTimeThiefWeaponTrailStyle& Style) const;
};
