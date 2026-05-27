


#include "TimeThiefDissolveFXComponent.h"


// Sets default values for this component's properties
UTimeThiefDissolveFXComponent::UTimeThiefDissolveFXComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}


// Called when the game starts
void UTimeThiefDissolveFXComponent::BeginPlay()
{
	Super::BeginPlay();

	RefreshTargetMeshes();
	
	AActor* Owner = GetOwner();
	if (Owner)
	{
		if (DisappearFXSystem)
		{
			DisappearFX = UNiagaraFunctionLibrary::SpawnSystemAttached(
				DisappearFXSystem,
				Owner->GetRootComponent(),
				NAME_None,
				FVector::ZeroVector,
				FRotator::ZeroRotator,
				EAttachLocation::KeepRelativeOffset,
				false
			);
		}

		if (DeathFXSystem)
		{
			DeathFX = UNiagaraFunctionLibrary::SpawnSystemAttached(
				DeathFXSystem,
				Owner->GetRootComponent(),
				NAME_None,
				FVector::ZeroVector,
				FRotator::ZeroRotator,
				EAttachLocation::KeepRelativeOffset,
				false
			);
		}

		if (SpawnFXSystem)
		{
			SpawnFX = UNiagaraFunctionLibrary::SpawnSystemAttached(
				SpawnFXSystem,
				Owner->GetRootComponent(),
				NAME_None,
				FVector::ZeroVector,
				FRotator::ZeroRotator,
				EAttachLocation::KeepRelativeOffset,
				false
			);
		}
	}
	
	ApplyFloatParams(DeathFX, DeathFXFloatParams);
	ApplyFloatParams(SpawnFX, SpawnFXFloatParams);
	ApplyFloatParams(DisappearFX, DisappearFXFloatParams);
	
	SetMask(CurrentMask);

	SetComponentTickEnabled(false);
}


// Called every frame
void UTimeThiefDissolveFXComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                                  FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	ElapsedTime += DeltaTime;

	const float Alpha = Duration > 0.0f
		? FMath::Clamp(ElapsedTime / Duration, 0.0f, 1.0f)
		: 1.0f;

	const float NewMask = FMath::Lerp(StartMask, TargetMask, Alpha);
	SetMask(NewMask);

	if (Alpha >= 1.0f)
	{
		Finish();
	}
}

void UTimeThiefDissolveFXComponent::SetMask(float Value)
{
	CurrentMask = Value;

	ApplyMaskToMeshes();

	if (DisappearFX)
	{
		FName VN = MaskParameterName;
		if (DisappearFXFloatParams.Num() > 0)
		{
			VN = DisappearFXFloatParams[0].ParameterName;
		}
		
		DisappearFX->SetVariableFloat(VN, CurrentMask);
	}
}

void UTimeThiefDissolveFXComponent::PlayDisappear()
{
	StartMask = CurrentMask;
	TargetMask = 0.0f;
	ElapsedTime = 0.0f;
	Duration = FMath::Max(DisappearDuration, 0.01f);

	if (SpawnFX)
	{
		SpawnFX->Deactivate();
	}
	
	if (DisappearFX)
	{
		DisappearFX->Activate(true);
	}

	if (DeathFX)
	{
		DeathFX->Activate(true);
	}

	SetComponentTickEnabled(true);
}

void UTimeThiefDissolveFXComponent::PlayAppear()
{
	StartMask = CurrentMask;
	TargetMask = 1.0f;
	ElapsedTime = 0.0f;
	Duration = FMath::Max(AppearDuration, 0.01f);

	if (DeathFX)
	{
		DeathFX->Deactivate();
	}
	
	if (DisappearFX)
	{
		DisappearFX->Activate(true);
	}

	if (SpawnFX)
	{
		SpawnFX->Activate(true);
	}

	SetComponentTickEnabled(true);
}

void UTimeThiefDissolveFXComponent::RegisterMesh(UMeshComponent* Mesh)
{
	if (!IsValid(Mesh))
	{
		return;
	}

	TargetMeshes.AddUnique(Mesh);

	SetMask(CurrentMask);
}

void UTimeThiefDissolveFXComponent::UnregisterMesh(UMeshComponent* Mesh)
{
	if (!IsValid(Mesh))
	{
		return;
	}

	TargetMeshes.Remove(Mesh);
}

void UTimeThiefDissolveFXComponent::Finish()
{
	SetMask(TargetMask);

	ElapsedTime = 0.0f;
	Duration = 1.0f;
	StartMask = TargetMask;
	CurrentMask = TargetMask;
	
	if (DisappearFX)
	{
		DisappearFX->Deactivate();
	}

	SetComponentTickEnabled(false);
}

void UTimeThiefDissolveFXComponent::ApplyFloatParams(UNiagaraComponent* FX,
	const TArray<FNiagaraFloatParamBinding>& FloatParams)
{
	if (!IsValid(FX))
	{
		return;
	}

	for (const FNiagaraFloatParamBinding& Param : FloatParams)
	{
		if (!Param.ParameterName.IsNone())
		{
			FX->SetVariableFloat(Param.ParameterName, Param.Value);
		}
	}
}

void UTimeThiefDissolveFXComponent::RefreshTargetMeshes()
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner))
	{
		return;
	}

	if (TargetMeshes.Num() > 0)
	{
		return;
	}

	TArray<UMeshComponent*> MeshComponents;
	Owner->GetComponents<UMeshComponent>(MeshComponents);

	for (UMeshComponent* Mesh : MeshComponents)
	{
		if (IsValid(Mesh))
		{
			TargetMeshes.Add(Mesh);
		}
	}
}

void UTimeThiefDissolveFXComponent::ApplyMaskToMeshes()
{
	for (UMeshComponent* Mesh : TargetMeshes)
	{
		if (!IsValid(Mesh))
		{
			continue;
		}

		const int32 MatCount = Mesh->GetNumMaterials();

		for (int32 i = 0; i < MatCount; ++i)
		{
			UMaterialInstanceDynamic* MID = Mesh->CreateAndSetMaterialInstanceDynamic(i);
			if (MID)
			{
				MID->SetScalarParameterValue(MaskParameterName, CurrentMask);
			}
		}
	}
}
