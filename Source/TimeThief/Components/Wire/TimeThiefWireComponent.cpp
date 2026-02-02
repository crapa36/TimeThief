#include "Components/Wire/TimeThiefWireComponent.h"
#include "Components/Wire/TimeThiefWirePhysics.h"
#include "Components/Wire/TimeThiefWireTargeting.h"
#include "TimeThiefGameplayTags.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"

DEFINE_LOG_CATEGORY(LogWire);

UTimeThiefWireComponent::UTimeThiefWireComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
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

		if (WireMeshComponent)
		{
			WireMeshComponent->AttachToComponent(CachedCharacter->GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, WireStartSocketName);
			
			if (WireMeshTemplate)
			{
				WireMeshComponent->SetStaticMesh(WireMeshTemplate);
			}
			
			if (WireMaterial)
			{
				WireMeshComponent->SetMaterial(0, WireMaterial);
			}
		}

		if (AnchorMeshComponent)
		{
			AnchorMeshComponent->AttachToComponent(CachedCharacter->GetMesh(), FAttachmentTransformRules::KeepWorldTransform);
			
			if (AnchorMeshTemplate)
			{
				AnchorMeshComponent->SetStaticMesh(AnchorMeshTemplate);
			}
			AnchorMeshComponent->SetWorldScale3D(AnchorMeshScale);
		}
	}
}

void UTimeThiefWireComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ReleaseWire();
	Super::EndPlay(EndPlayReason);
}

void UTimeThiefWireComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UpdateCooldown(DeltaTime);

	switch (CurrentState)
	{
	case EWireState::Firing:
		UpdateFiringAnchor(DeltaTime);
		break;
	case EWireState::Attached:
		UpdateAttachedWire(DeltaTime);
		break;
	default:
		break;
	}

	UpdateWireVisuals();

	if (!ShouldTickComponent())
	{
		SetComponentTickEnabled(false);
	}
}

bool UTimeThiefWireComponent::ShouldTickComponent() const
{
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

	const FVector StartLocation = GetWireStartLocation();
	FVector TargetLocation;
	
	if (WireTargeting && WireTargeting->FindBestAnchorTarget(TargetLocation, StartLocation, GetAimDirection(), MaxWireLength))
	{
		FireDirection = (TargetLocation - StartLocation).GetSafeNormal();
	}
	else
	{
		FireDirection = GetAimDirection();
	}

	AnchorPoint = StartLocation;
	AnchorNormal = -FireDirection;
	CurrentFireDistance = 0.0f;
	StuckCheckTimer = 0.0f;
	GroundCheckTimer = 0.0f;

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
		AnchorNormal = HitResult.ImpactNormal;
		AttachedWireLength = FVector::Dist(StartLocation, AnchorPoint);
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

	CachedAirControl = CachedMovementComponent->AirControl;
	CachedMovementComponent->AirControl = AirControlOnWire;
	CachedMovementComponent->SetMovementMode(MOVE_Falling);

	const FVector WireDirection = (AnchorPoint - GetWireStartLocation()).GetSafeNormal();
	const float VelocityTowardAnchor = FVector::DotProduct(CachedMovementComponent->Velocity, WireDirection);
	if (VelocityTowardAnchor < 0)
	{
		CachedMovementComponent->Velocity -= WireDirection * VelocityTowardAnchor;
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
	const float CurrentWireDistance = FVector::Dist(AnchorPoint, WireStart);

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
		WirePhysics->ApplyWirePhysics(DeltaTime, AnchorPoint, WireStart, AttachedWireLength, MoveInput);
	}
}

float UTimeThiefWireComponent::GetCurrentWireLength() const
{
	if (CurrentState == EWireState::Idle) return 0.0f;
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

	const FVector WireDirection = (AnchorPoint - GetWireStartLocation()).GetSafeNormal();
	const FVector VelocityDir = Velocity.GetSafeNormal();
	const float DotProduct = FVector::DotProduct(VelocityDir, WireDirection);

	return DotProduct < WirePhysics->WireBreakAngleThreshold;
}

bool UTimeThiefWireComponent::IsFacingAwayFromWire() const
{
	if (!IsValid(CachedCharacter)) return false;

	const FVector LookDirection = GetAimDirection();
	const FVector WireDirection = (AnchorPoint - GetWireStartLocation()).GetSafeNormal();
	const float DotProduct = FVector::DotProduct(LookDirection, WireDirection);

	return DotProduct < WireReleaseLookDotThreshold;
}

FVector UTimeThiefWireComponent::GetAimDirection() const
{
	if (!IsValid(CachedCharacter)) return FVector::ForwardVector;

	FVector Loc;
	FRotator Rot;
	CachedCharacter->GetActorEyesViewPoint(Loc, Rot);
	return Rot.Vector();
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
	const FVector End = AnchorPoint;
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

	AnchorMeshComponent->SetWorldLocation(AnchorPoint);
	AnchorMeshComponent->SetWorldRotation(FRotationMatrix::MakeFromZ(AnchorNormal).Rotator());
}
