#pragma once

#include "CoreMinimal.h"
#include "TimeThiefWireTargeting.generated.h"

class ACharacter;

UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced)
class TIMETHIEF_API UTimeThiefWireTargeting : public UObject
{
	GENERATED_BODY()

public:
	UTimeThiefWireTargeting();

	virtual void Initialize(ACharacter* InCharacter);
	virtual bool ShouldRefreshTarget(float DeltaTime, const FVector& AimDirection);
	virtual void ResetTargetRefresh();

	virtual bool FindBestAnchorTarget(FVector& OutTargetLocation, const FVector& StartLocation, const FVector& AimDirection, float MaxLength);
	virtual bool CheckAnchorCollision(const FVector& Start, const FVector& End, FHitResult& OutHit, AActor* IgnoredActor);

public:

	// 앵커 충돌 반경
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Targeting")
	float AnchorCollisionRadius = 5.0f;

	// 캐릭터 기준 타게팅 최소 거리
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Targeting")
	float MinTargetDistance = 250.0f;

	// 렛지 판정 높이
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Targeting")
	float LedgeCheckHeight = 250.0f;

	// 렛지 상단으로 인정할 최소 Up 노멀(Z) 값
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Targeting")
	float LedgeMinNormalZ = 0.85f;

	// 벽 히트점 대비 렛지 히트점의 최소 높이 차
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Targeting")
	float LedgeMinHeightDelta = 10.0f;

	// 노말 방향 투영 두께에 곱해지는 프로브 거리 스케일
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Targeting")
	float ProbeDistanceScale = 0.3f;

	// 렛지 프로브 거리의 최소 클램프 값
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Targeting")
	float ProbeDistanceMin = 3.0f;

	// 렛지 프로브 거리의 최대 클램프 값
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Targeting")
	float ProbeDistanceMax = 12.0f;

	// 화면 중심 기준 샘플 픽셀 간격
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Targeting")
	float ScreenSamplePixelStep = 4.0f;

	// 타겟 재검사 주기
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Targeting", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float TargetRefreshInterval = 0.2f;

	// 에임 변화로 즉시 재검사할 각도
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Targeting", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float RetargetAngleDegrees = 3.0f;
	
	// 필터링에 사용하는 기본 픽셀 반경
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Targeting")
	float ScreenTargetingRadiusPx = 180.0f;

	// 에임 주변 얇은 타겟 후보를 찾는 월드 스윕 반경
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Targeting")
	float ThinTargetObjectProbeRadius = 80.0f;

	// 얇은 타겟 후보 주변 정밀 샘플 픽셀 반경
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Targeting")
	float ThinTargetRefineRadiusPx = 24.0f;

	// 얇은 타겟 후보 주변 정밀 샘플 픽셀 간격
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Targeting")
	float ThinTargetRefinePixelStep = 1.5f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Targeting")
	TArray<TEnumAsByte<EObjectTypeQuery>> CollisionObjectTypes;

protected:
	UPROPERTY(Transient)
	TObjectPtr<ACharacter> CachedCharacter;

	float TargetRefreshTimer = 0.0f;
	FVector CachedTargetAimDirection = FVector::ForwardVector;
	bool bHasCachedTargetAimDirection = false;
};
