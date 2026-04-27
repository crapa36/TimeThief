#include "Components/Wire/TimeThiefWireComponent.h"
#include "Components/Wire/TimeThiefWirePhysics.h"
#include "Components/Wire/TimeThiefWireTargeting.h"
#include "TimeThiefGameplayTags.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"
#include "Sound/SoundBase.h"
#include "Kismet/GameplayStatics.h"
#include "Utils/TimeThiefAimStatics.h"
#include "DrawDebugHelpers.h"

DEFINE_LOG_CATEGORY(LogWire);

UTimeThiefWireComponent::UTimeThiefWireComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;

	WirePhysics = CreateDefaultSubobject<UTimeThiefWirePhysics>(TEXT("WirePhysics"));
	WireTargeting = CreateDefaultSubobject<UTimeThiefWireTargeting>(TEXT("WireTargeting"));

	WireMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WireMeshComponent"));
	WireMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WireMeshComponent->SetCastShadow(false);
	WireMeshComponent->SetVisibility(false);

	AnchorMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AnchorMeshComponent"));
	AnchorMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	AnchorMeshComponent->SetCastShadow(false);
	AnchorMeshComponent->SetVisibility(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMeshAsset(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMeshAsset.Succeeded())
	{
		WireMeshComponent->SetStaticMesh(CylinderMeshAsset.Object);
	}
}

void UTimeThiefWireComponent::BeginPlay()
{
	Super::BeginPlay();

	CachedCharacter = GetPawn<ACharacter>();
	if (IsValid(CachedCharacter))
	{
		CachedMovementComponent = CachedCharacter->GetCharacterMovement();
		
		if (WirePhysics)
		{
			WirePhysics->Initialize(CachedMovementComponent);
		}
		if (WireTargeting)
		{
			WireTargeting->Initialize(CachedCharacter);
		}

		USkeletalMeshComponent* SkeletalMesh = CachedCharacter->GetMesh();
		if (SkeletalMesh && SkeletalMesh->DoesSocketExist(WireStartSocketName))
		{
			WireMeshComponent->AttachToComponent(SkeletalMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, WireStartSocketName);
		}
		else if (USceneComponent* RootComponent = CachedCharacter->GetRootComponent())
		{
			WireMeshComponent->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform);
		}

		if (WireMeshTemplate)
		{
			WireMeshComponent->SetStaticMesh(WireMeshTemplate);
		}

		if (WireMaterial)
		{
			WireMeshComponent->SetMaterial(0, WireMaterial);
		}

		if (SkeletalMesh)
		{
			AnchorMeshComponent->AttachToComponent(SkeletalMesh, FAttachmentTransformRules::KeepWorldTransform);
		}
		else if (USceneComponent* RootComponent = CachedCharacter->GetRootComponent())
		{
			AnchorMeshComponent->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform);
		}

		if (AnchorMeshTemplate)
		{
			AnchorMeshComponent->SetStaticMesh(AnchorMeshTemplate);
		}
		AnchorMeshComponent->SetWorldScale3D(AnchorMeshScale);

		if (APlayerController* PC = Cast<APlayerController>(CachedCharacter->GetController()))
		{
			CachedCameraManager = PC->PlayerCameraManager;
			if (CachedCameraManager)
			{
				DefaultFOV = CachedCameraManager->DefaultFOV;
			}
		}
	}
}

void UTimeThiefWireComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(CachedCameraManager) && CurrentFOVOffset > 0.0f)
	{
		CachedCameraManager->UnlockFOV();
	}
	ReleaseWire();
	Super::EndPlay(EndPlayReason);
}

void UTimeThiefWireComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	const bool bIsLocallyControlled = IsValid(CachedCharacter) && CachedCharacter->IsLocallyControlled();
	if (!bIsLocallyControlled)
	{
		if (CurrentState == EWireState::Firing)
		{
			const float MoveDistance = WireFireSpeed * DeltaTime;
			CurrentFireDistance += MoveDistance;
			AnchorPoint += FireDirection * MoveDistance;

			if (CurrentFireDistance >= MaxWireLength)
			{
				ReleaseWire();
			}
			else
			{
				UpdateWireVisuals();
			}
		}
		else if (CurrentState == EWireState::Attached)
		{
			UpdateWireVisuals();
		}
		else if (CurrentState == EWireState::Idle)
		{
			SetComponentTickEnabled(false);
		}
		return;
	}

	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UpdateCooldown(DeltaTime);

	switch (CurrentState)
	{
	case EWireState::Firing:
		UpdateFiringAnchor(DeltaTime);
		break;
	case EWireState::Attached:
		UpdateAttachedWire(DeltaTime);
		UpdateWireRotation(DeltaTime);
		UpdateSpeedEffects(DeltaTime);
		break;
	default:
		UpdateTargetIndicator();
		ResetSpeedEffects(DeltaTime);
		break;
	}

	UpdateWireVisuals();

	if (!ShouldTickComponent())
	{
		SetComponentTickEnabled(false);
	}
}

void UTimeThiefWireComponent::SimulateAttach(const FVector& RemoteAnchorPoint)
{
	AnchorPoint = RemoteAnchorPoint;
	AttachedWireLength = FVector::Dist(GetWireStartLocation(), GetPullAnchorPoint());
	AttachedAnchorRotation = (GetPullAnchorPoint() - GetWireStartLocation()).GetSafeNormal().Rotation() + AnchorMeshRotationOffset;

	if (CurrentState != EWireState::Attached)
	{
		const EWireState OldState = CurrentState;
		CurrentState = EWireState::Attached;
		OnWireStateChanged.Broadcast(OldState, CurrentState);
	}

	OnWireAttached.Broadcast(AnchorPoint);
	SetComponentTickEnabled(true);
	UpdateWireVisuals();
}

void UTimeThiefWireComponent::SimulateDetach()
{
	if (CurrentState == EWireState::Idle)
	{
		return;
	}

	const EWireState OldState = CurrentState;
	CurrentState = EWireState::Idle;
	OnWireStateChanged.Broadcast(OldState, CurrentState);

	AnchorPoint = FVector::ZeroVector;
	FireDirection = FVector::ZeroVector;
	CurrentFireDistance = 0.0f;
	AttachedWireLength = 0.0f;
	UpdateWireVisuals();
	SetComponentTickEnabled(false);
}

void UTimeThiefWireComponent::SimulateLaunch(const FVector& RemoteStartPosition, const FVector& RemoteDirection)
{
	AnchorPoint = RemoteStartPosition;
	FireDirection = RemoteDirection.GetSafeNormal();
	CurrentFireDistance = 0.0f;
	StuckCheckTimer = 0.0f;
	GroundCheckTimer = 0.0f;

	SetWireState(EWireState::Firing);
	SetComponentTickEnabled(true);
	UpdateWireVisuals();
}

bool UTimeThiefWireComponent::ShouldTickComponent() const
{
	if (IsValid(CachedCharacter) && CachedCharacter->IsLocallyControlled())
	{
		return true;
	}
	return CurrentState != EWireState::Idle || CooldownRemaining > 0.0f;
}

void UTimeThiefWireComponent::UpdateCooldown(float DeltaTime)
{
	if (CooldownRemaining > 0.0f)
	{
		CooldownRemaining = FMath::Max(0.0f, CooldownRemaining - DeltaTime);
	}
}

bool UTimeThiefWireComponent::CanFireWire() const
{
	return CurrentState == EWireState::Idle && CooldownRemaining <= 0.0f;
}

void UTimeThiefWireComponent::FireWire()
{
	if (!CanFireWire()) return;

	const FVector WireStartLocation = GetWireStartLocation();

	FVector CamLoc = WireStartLocation;
	FVector AimDirection = UTimeThiefAimStatics::NormalizeAimDirection(
		GetOwner() ? GetOwner()->GetActorForwardVector() : FVector::ForwardVector);

	if (IsValid(CachedCharacter))
	{
		if (!UTimeThiefAimStatics::ResolveAimView(CachedCharacter, CamLoc, AimDirection))
		{
			CamLoc = CachedCharacter->GetPawnViewLocation();
			AimDirection = UTimeThiefAimStatics::NormalizeAimDirection(CachedCharacter->GetActorForwardVector());
		}
	}

	AimDirection = UTimeThiefAimStatics::NormalizeAimDirection(AimDirection);

	FVector TargetLocation;
	
	if (WireTargeting && WireTargeting->FindBestAnchorTarget(TargetLocation, CamLoc, AimDirection, MaxWireLength))
	{
		FireDirection = UTimeThiefAimStatics::ResolveAimDirectionToTarget(
			WireStartLocation,
			TargetLocation,
			AimDirection);
	}
	else
	{
		FireDirection = AimDirection;
	}

	AnchorPoint = WireStartLocation;
	CurrentFireDistance = 0.0f;
	StuckCheckTimer = 0.0f;
	GroundCheckTimer = 0.0f;

	if (FireSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, FireSound, WireStartLocation);
	}

	OnWireLaunched.Broadcast(WireStartLocation, FireDirection);

	SetComponentTickEnabled(true);
	SetWireState(EWireState::Firing);
}

void UTimeThiefWireComponent::ReleaseWire()
{
	if (CurrentState == EWireState::Idle) return;

	if (CurrentState == EWireState::Attached)
	{
		if (IsValid(CachedMovementComponent))
		{
			CachedMovementComponent->AirControl = CachedAirControl;
		}
		RestoreRotationMode();
	}

	CooldownRemaining = WireCooldown;

	SetWireState(EWireState::Idle);

	AnchorPoint = FVector::ZeroVector;
	FireDirection = FVector::ZeroVector;
	CurrentFireDistance = 0.0f;
	AttachedWireLength = 0.0f;
}

void UTimeThiefWireComponent::Jump()
{
	if (IsWireAttached())
	{
		ReleaseWire();
		
		if (IsValid(CachedCharacter) && IsValid(CachedMovementComponent))
		{
			const float JumpZ = CachedMovementComponent->JumpZVelocity;
			CachedCharacter->LaunchCharacter(FVector(0.0f, 0.0f, JumpZ), false, true);
		}
	}
}

void UTimeThiefWireComponent::SetWireState(EWireState NewState)
{
	if (CurrentState == NewState) return;
	
	const EWireState OldState = CurrentState;
	CurrentState = NewState;
	OnWireStateChanged.Broadcast(OldState, NewState);

	if (CurrentState == EWireState::Attached)
	{
		OnAnchorAttached();
	}
}

void UTimeThiefWireComponent::UpdateFiringAnchor(float DeltaTime)
{
	const FVector StartLocation = GetWireStartLocation();
	const FVector PreviousAnchorPoint = AnchorPoint;
	const float MoveDistance = WireFireSpeed * DeltaTime;
	
	CurrentFireDistance += MoveDistance;
	AnchorPoint += FireDirection * MoveDistance;

	if (CurrentFireDistance > MaxWireLength)
	{
		AnchorPoint = StartLocation + FireDirection * MaxWireLength;
	}

	FHitResult HitResult;
	if (WireTargeting && WireTargeting->CheckAnchorCollision(PreviousAnchorPoint, AnchorPoint, HitResult, GetOwner()))
	{
		AnchorPoint = HitResult.ImpactPoint;
		AttachedWireLength = FVector::Dist(StartLocation, GetPullAnchorPoint());
		
		AttachedAnchorRotation = FireDirection.Rotation() + AnchorMeshRotationOffset;
		
		SetWireState(EWireState::Attached);
		OnWireAttached.Broadcast(AnchorPoint);
		return;
	}
	
	if (CurrentFireDistance >= MaxWireLength)
	{
		ReleaseWire();
	}
}

void UTimeThiefWireComponent::OnAnchorAttached()
{
	if (!IsValid(CachedMovementComponent)) return;

	if (AttachSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, AttachSound, AnchorPoint);
	}

	CachedAirControl = CachedMovementComponent->AirControl;
	CachedMovementComponent->AirControl = AirControlOnWire;
	CachedMovementComponent->SetMovementMode(MOVE_Falling);

	ApplyWireRotationMode();

	const FVector WireDirection = (GetPullAnchorPoint() - GetWireStartLocation()).GetSafeNormal();
	const float VelocityTowardAnchor = FVector::DotProduct(CachedMovementComponent->Velocity, WireDirection);
	if (VelocityTowardAnchor < 0)
	{
		CachedMovementComponent->Velocity -= WireDirection * VelocityTowardAnchor;
	}

	const float GravityZ = CachedMovementComponent->GetGravityZ();
	if (!FMath::IsNearlyZero(GravityZ))
	{
		const FVector GravityDirection = FVector::UpVector * FMath::Sign(GravityZ);
		const float VelocityTowardGravity = FVector::DotProduct(CachedMovementComponent->Velocity, GravityDirection);
		if (VelocityTowardGravity > 0.0f)
		{
			CachedMovementComponent->Velocity -= GravityDirection * VelocityTowardGravity;
		}
	}
}

void UTimeThiefWireComponent::UpdateAttachedWire(float DeltaTime)
{
	if (!IsValid(CachedCharacter) || !IsValid(CachedMovementComponent))
	{
		ReleaseWire();
		return;
	}

	if (!CachedMovementComponent->IsFalling())
	{
		CachedMovementComponent->SetMovementMode(MOVE_Falling);
	}

	const FVector WireStart = GetWireStartLocation();
	const FVector PullAnchorPoint = GetPullAnchorPoint();

	const float CurrentWireDistance = FVector::Dist(PullAnchorPoint, WireStart);

	if (CurrentWireDistance <= ArrivalDistance || ShouldRelease(DeltaTime))
	{
		ReleaseWire();
		return;
	}

	if (CurrentWireDistance < AttachedWireLength - WireLengthUpdateTolerance)
	{
		AttachedWireLength = CurrentWireDistance;
	}

	if (WirePhysics)
	{
		WirePhysics->ApplyWirePhysics(DeltaTime, PullAnchorPoint, WireStart, AttachedWireLength, MoveInput);
	}
}

float UTimeThiefWireComponent::GetCurrentWireLength() const
{
	if (CurrentState == EWireState::Idle) return 0.0f;
	if (CurrentState == EWireState::Attached)
	{
		return FVector::Dist(GetWireStartLocation(), GetPullAnchorPoint());
	}
	return FVector::Dist(GetWireStartLocation(), AnchorPoint);
}

void UTimeThiefWireComponent::HandleInputPressed(FGameplayTag InputTag)
{
	const FTimeThiefGameplayTags& Tags = FTimeThiefGameplayTags::Get();

	if (InputTag == Tags.InputTag_Action_Wire)
	{
		if (CurrentState == EWireState::Idle)
		{
			FireWire();
		}
		else
		{
			ReleaseWire();
		}
	}
	else if (InputTag == Tags.InputTag_Action_Jump)
	{
		Jump();
	}
}

bool UTimeThiefWireComponent::ShouldRelease(float DeltaTime)
{
	return IsStuck(DeltaTime) || IsOnGroundTooLong(DeltaTime) || IsWireSnapping() || IsFacingAwayFromWire();
}

bool UTimeThiefWireComponent::IsStuck(float DeltaTime)
{
	if (!IsValid(CachedMovementComponent)) return true;

	const float SpeedSquared = CachedMovementComponent->Velocity.SizeSquared();
	const float ThresholdSquared = StuckSpeedThreshold * StuckSpeedThreshold;

	if (SpeedSquared < ThresholdSquared)
	{
		StuckCheckTimer += DeltaTime;
		if (StuckCheckTimer >= StuckCheckDelay)
		{
			return true;
		}
	}
	else
	{
		StuckCheckTimer = 0.0f;
	}
	return false;
}

bool UTimeThiefWireComponent::IsOnGroundTooLong(float DeltaTime)
{
	if (!IsValid(CachedMovementComponent)) return false;

	const bool bOnGround = CachedMovementComponent->CurrentFloor.bBlockingHit;

	if (bOnGround)
	{
		GroundCheckTimer += DeltaTime;
		
		if (WirePhysics && GroundCheckTimer >= WirePhysics->MaxGroundTime)
		{
			return true;
		}
	}
	else
	{
		GroundCheckTimer = 0.0f;
	}
	return false;
}

bool UTimeThiefWireComponent::IsWireSnapping() const
{
	if (!IsValid(CachedMovementComponent) || !WirePhysics) return false;

	const FVector Velocity = CachedMovementComponent->Velocity;
	const float SpeedSquared = Velocity.SizeSquared();
	const float ThresholdSquared = WirePhysics->WireBreakSpeedThreshold * WirePhysics->WireBreakSpeedThreshold;

	if (SpeedSquared < ThresholdSquared)
	{
		return false;
	}

	const FVector WireDirection = (GetPullAnchorPoint() - GetWireStartLocation()).GetSafeNormal();
	const FVector VelocityDir = Velocity.GetSafeNormal();
	const float DotProduct = FVector::DotProduct(VelocityDir, WireDirection);

	return DotProduct < WirePhysics->WireBreakAngleThreshold;
}

bool UTimeThiefWireComponent::IsFacingAwayFromWire() const
{
	if (!IsValid(CachedCharacter)) return false;

	const FVector LookDirection = GetAimDirection();
	const FVector WireDirection = (GetPullAnchorPoint() - GetWireStartLocation()).GetSafeNormal();
	const float DotProduct = FVector::DotProduct(LookDirection, WireDirection);

	return DotProduct < WireReleaseLookDotThreshold;
}

FVector UTimeThiefWireComponent::GetPullAnchorPoint() const
{
	if (IsValid(CachedCharacter) && CachedCharacter->GetActorLocation().Z > AnchorPoint.Z)
	{
		return AnchorPoint;
	}

	return AnchorPoint + FVector(0.0f, 0.0f, PullAnchorHeightOffset);
}

FVector UTimeThiefWireComponent::GetAimDirection() const
{
	if (!IsValid(CachedCharacter)) return FVector::ForwardVector;

	FVector ViewLocation = FVector::ZeroVector;
	FVector ViewDirection = FVector::ForwardVector;
	if (UTimeThiefAimStatics::ResolveAimView(CachedCharacter, ViewLocation, ViewDirection))
	{
		return UTimeThiefAimStatics::NormalizeAimDirection(ViewDirection);
	}

	return UTimeThiefAimStatics::NormalizeAimDirection(CachedCharacter->GetActorForwardVector());
}

FVector UTimeThiefWireComponent::GetWireStartLocation() const
{
	if (!IsValid(CachedCharacter))
	{
		const AActor* Owner = GetOwner();
		return Owner ? Owner->GetActorLocation() : FVector::ZeroVector;
	}

	USkeletalMeshComponent* Mesh = CachedCharacter->GetMesh();
	if (Mesh && Mesh->DoesSocketExist(WireStartSocketName))
	{
		return Mesh->GetSocketLocation(WireStartSocketName);
	}

	return CachedCharacter->GetActorLocation();
}

void UTimeThiefWireComponent::UpdateWireVisuals()
{
	if (!WireMeshComponent || !AnchorMeshComponent) return;

	if (CurrentState == EWireState::Idle)
	{
		WireMeshComponent->SetVisibility(false);
		AnchorMeshComponent->SetVisibility(false);
		return;
	}

	const FVector Start = GetWireStartLocation();
	
	FRotator AnchorRotation;
	if (CurrentState == EWireState::Attached)
	{
		AnchorRotation = AttachedAnchorRotation;
	}
	else
	{
		const FVector AnchorDirection = (AnchorPoint - Start).GetSafeNormal();
		AnchorRotation = AnchorDirection.Rotation() + AnchorMeshRotationOffset;
	}
	
	AnchorMeshComponent->SetWorldLocation(AnchorPoint);
	AnchorMeshComponent->SetWorldRotation(AnchorRotation);

	const FVector WireAttachPoint = AnchorPoint + AnchorRotation.RotateVector(AnchorWireAttachOffset);
	const FVector End = WireAttachPoint;
	const float Distance = FVector::Dist(Start, End);

	if (Distance < KINDA_SMALL_NUMBER)
	{
		WireMeshComponent->SetVisibility(false);
		AnchorMeshComponent->SetVisibility(false);
		return;
	}

	WireMeshComponent->SetVisibility(true);
	AnchorMeshComponent->SetVisibility(true);

	const FVector Direction = (End - Start) / Distance;
	const FVector CenterLocation = (Start + End) * 0.5f;
	const FRotator Rotation = Direction.Rotation() + FRotator(-90.0f, 0.0f, 0.0f);
	
	const float LengthScale = Distance / 100.0f;
	const float ThicknessScale = WireThickness / 100.0f;
	const FVector Scale = FVector(ThicknessScale, ThicknessScale, LengthScale);

	WireMeshComponent->SetWorldLocation(CenterLocation);
	WireMeshComponent->SetWorldRotation(Rotation);
	WireMeshComponent->SetWorldScale3D(Scale);
}

void UTimeThiefWireComponent::UpdateTargetIndicator()
{
	if (!CanFireWire())
	{
		return;
	}

	FVector CamLoc = GetWireStartLocation();
	FVector AimDirection = UTimeThiefAimStatics::NormalizeAimDirection(
		GetOwner() ? GetOwner()->GetActorForwardVector() : FVector::ForwardVector);

	if (IsValid(CachedCharacter))
	{
		if (!UTimeThiefAimStatics::ResolveAimView(CachedCharacter, CamLoc, AimDirection))
		{
			CamLoc = CachedCharacter->GetPawnViewLocation();
			AimDirection = UTimeThiefAimStatics::NormalizeAimDirection(CachedCharacter->GetActorForwardVector());
		}
	}

	AimDirection = UTimeThiefAimStatics::NormalizeAimDirection(AimDirection);

	FVector TargetLocation;
	if (WireTargeting && WireTargeting->FindBestAnchorTarget(TargetLocation, CamLoc, AimDirection, MaxWireLength))
	{
		DrawDebugPoint(GetWorld(), TargetLocation, 20.0f, FColor::Red, false, -1.0f, 0);
	}
}

float UTimeThiefWireComponent::GetSpeedEffectAlpha() const
{
	if (!IsValid(CachedMovementComponent))
	{
		return 0.0f;
	}

	const float Speed = CachedMovementComponent->Velocity.Size();
	if (Speed <= SpeedEffectThreshold)
	{
		return 0.0f;
	}

	const float MaxSpeed = SpeedEffectThreshold * 2.5f;
	return FMath::Clamp((Speed - SpeedEffectThreshold) / (MaxSpeed - SpeedEffectThreshold), 0.0f, 1.0f);
}

void UTimeThiefWireComponent::UpdateSpeedEffects(float DeltaTime)
{
	const float EffectAlpha = GetSpeedEffectAlpha();

	if (IsValid(CachedCameraManager))
	{
		const float TargetFOVOffset = EffectAlpha * MaxFOVIncrease;
		CurrentFOVOffset = FMath::FInterpTo(CurrentFOVOffset, TargetFOVOffset, DeltaTime, FOVInterpSpeed);
		
		if (CurrentFOVOffset > 0.1f)
		{
			CachedCameraManager->SetFOV(DefaultFOV + CurrentFOVOffset);
		}
		else
		{
			CurrentFOVOffset = 0.0f;
			CachedCameraManager->UnlockFOV();
		}
	}

	if (EffectAlpha > 0.3f && WireSpeedShake && IsValid(CachedCharacter))
	{
		if (APlayerController* PC = Cast<APlayerController>(CachedCharacter->GetController()))
		{
			PC->ClientStartCameraShake(WireSpeedShake, CameraShakeScale * EffectAlpha);
		}
	}
}

void UTimeThiefWireComponent::ResetSpeedEffects(float DeltaTime)
{
	if (!IsValid(CachedCameraManager))
	{
		return;
	}

	if (CurrentFOVOffset > 0.0f)
	{
		CurrentFOVOffset = FMath::FInterpTo(CurrentFOVOffset, 0.0f, DeltaTime, FOVInterpSpeed * 2.0f);
		
		if (CurrentFOVOffset > 0.1f)
		{
			CachedCameraManager->SetFOV(DefaultFOV + CurrentFOVOffset);
		}
		else
		{
			CurrentFOVOffset = 0.0f;
			CachedCameraManager->UnlockFOV();
		}
	}
}

void UTimeThiefWireComponent::ApplyWireRotationMode()
{
	if (!bOrientToVelocityOnWire) return;
	if (!IsValid(CachedCharacter) || !IsValid(CachedMovementComponent)) return;

	CachedOrientRotationToMovement = CachedMovementComponent->bOrientRotationToMovement;
	CachedUseControllerDesiredRotation = CachedMovementComponent->bUseControllerDesiredRotation;
	CachedUseControllerRotationYaw = CachedCharacter->bUseControllerRotationYaw;

	CachedMovementComponent->bOrientRotationToMovement = false;
	CachedMovementComponent->bUseControllerDesiredRotation = false;
	CachedCharacter->bUseControllerRotationYaw = false;
}

void UTimeThiefWireComponent::RestoreRotationMode()
{
	if (!bOrientToVelocityOnWire) return;
	if (!IsValid(CachedCharacter) || !IsValid(CachedMovementComponent)) return;

	CachedMovementComponent->bOrientRotationToMovement = CachedOrientRotationToMovement;
	CachedMovementComponent->bUseControllerDesiredRotation = CachedUseControllerDesiredRotation;
	CachedCharacter->bUseControllerRotationYaw = CachedUseControllerRotationYaw;
}

void UTimeThiefWireComponent::UpdateWireRotation(float DeltaTime)
{
	if (!bOrientToVelocityOnWire) return;
	if (!IsValid(CachedCharacter) || !IsValid(CachedMovementComponent)) return;

	const FVector WireVector = GetPullAnchorPoint() - GetWireStartLocation();
	const FVector LateralWireDirection(WireVector.X, WireVector.Y, 0.0f);
	if (LateralWireDirection.IsNearlyZero())
	{
		return;
	}

	const float CurrentYaw = CachedCharacter->GetActorRotation().Yaw;
	const float WireYaw = LateralWireDirection.GetSafeNormal().Rotation().Yaw;
	const float WireAngleDelta = FMath::Abs(FMath::FindDeltaAngleDegrees(CurrentYaw, WireYaw));
	const bool bForceRotateToWire = WireAngleDelta > WireRotationForceAngleThreshold;

	const FVector Velocity = CachedMovementComponent->Velocity;
	const FVector LateralVelocity = FVector(Velocity.X, Velocity.Y, 0.0f);
	const float LateralSpeed = LateralVelocity.Size();

	if (!bForceRotateToWire && LateralSpeed < WireRotationMinSpeed)
	{
		return;
	}

	const FRotator CurrentRotation = CachedCharacter->GetActorRotation();
	const float TargetYaw = bForceRotateToWire ? WireYaw : LateralVelocity.Rotation().Yaw;
	const FRotator TargetRotation = FRotator(0.0f, TargetYaw, 0.0f);

	const FRotator NewRotation = FMath::RInterpTo(CurrentRotation, TargetRotation, DeltaTime, WireRotationInterpSpeed);
	CachedCharacter->SetActorRotation(FRotator(CurrentRotation.Pitch, NewRotation.Yaw, CurrentRotation.Roll));
}