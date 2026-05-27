#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NiagaraSystem.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraFloatParamBinding.h"

#include "TimeThiefDissolveFXComponent.generated.h"


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class TIMETHIEF_API UTimeThiefDissolveFXComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UTimeThiefDissolveFXComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;
	
public:
	void PlayDisappear();
	void PlayAppear();
	
public:
	void RegisterMesh(UMeshComponent* Mesh);
	void UnregisterMesh(UMeshComponent* Mesh);
	
private:
	void SetMask(float Value);
	void Finish();
	
	void ApplyFloatParams(UNiagaraComponent* FX, const TArray<FNiagaraFloatParamBinding>& FloatParams);
	
	void RefreshTargetMeshes();
	void ApplyMaskToMeshes();
	
private:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dissolve", meta=(AllowPrivateAccess="true"))
	TArray<TObjectPtr<UMeshComponent>> TargetMeshes;
	
protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dissolve|FX")
	TObjectPtr<UNiagaraSystem> DisappearFXSystem = nullptr;
	// 사망 시 사라지는 이펙트 (실제로 사라지고 나타나는 것)
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dissolve|FX")
	TObjectPtr<UNiagaraSystem> DeathFXSystem = nullptr;
	// 사망 시 흩어지는 이펙트 (사라지는 것과 동시에 재생)
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dissolve|FX")
	TObjectPtr<UNiagaraSystem> SpawnFXSystem = nullptr;
	// 부활 시 나타나는 이펙트 (나타나는 것과 동시에 재생)
	
	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> DisappearFX = nullptr;
	
	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> DeathFX = nullptr;
	
	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> SpawnFX = nullptr;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dissolve|Niagara")
	TArray<FNiagaraFloatParamBinding> DisappearFXFloatParams;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dissolve|Niagara")
	TArray<FNiagaraFloatParamBinding> DeathFXFloatParams;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dissolve|Niagara")
	TArray<FNiagaraFloatParamBinding> SpawnFXFloatParams;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dissolve")
	float DisappearDuration = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dissolve")
	float AppearDuration = 1.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dissolve")
	FName MaskParameterName = TEXT("Mask");
	
private:
	float CurrentMask = 1.0f;		// 현재
	float TargetMask = 1.0f;		// 목표
	float ElapsedTime = 0.0f;		// 현재 효과가 재생된 시간
	float Duration = 1.0f;			// 효과 재생에 필요한 총 시간
	float StartMask = 1.0f;			// 효과 시작 시의 Mask 값
	
};
