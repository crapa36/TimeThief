


#include "NTPlayer.h"

#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Network/NetworkEntityComponent.h"
#include "Network/State/NetworkControlType.h"
#include "Network/State/NetworkEntityState.h"


// Sets default values
ANTPlayer::ANTPlayer()
{
	GetCapsuleComponent()->InitCapsuleSize(50.f, 100.f);
	
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;
	
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 720.0f, 0.0f);
	
	GetCharacterMovement()->JumpZVelocity = 600.f;
	GetCharacterMovement()->AirControl = 0.2f;
	GetCharacterMovement()->MaxWalkSpeed = 600.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.f;
	
	GetCharacterMovement()->bRunPhysicsWithNoController = true;
	
	GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, -100.0f));
	GetMesh()->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
	
	NetworkEntityComponent = CreateDefaultSubobject<UNetworkEntityComponent>(TEXT("NetworkEntityComponent"));
	
	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

ANTPlayer::~ANTPlayer()
{
}

// Called when the game starts or when spawned
void ANTPlayer::BeginPlay()
{
	Super::BeginPlay();
	
	{
		DestPosition = GetActorLocation();
		
		const FRotator ActorRot = GetActorRotation();
		NowYaw = ActorRot.Yaw;
		TargetYaw = ActorRot.Yaw;
		NowPitch = 0.f;
		TargetPitch = 0.f;
	}
}

// Called every frame
void ANTPlayer::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	NowPosition = GetActorLocation();
	NowYaw = GetActorRotation().Yaw;
	
	if (IsLocalPlayer())
	{
		if (Controller)
		{
			const float ControlPitch = Controller->GetControlRotation().Pitch;
			NowPitch = FRotator::NormalizeAxis(ControlPitch);
			
			if (NowPitch > 180.f) NowPitch -= 360.f;
			NowPitch = FMath::Clamp(NowPitch, -89.f, 89.f);
		}
		
		return;
	}

	if (NetworkEntityComponent == nullptr)
	{
		return;
	}
	
	// 내가 조종하는 Local Player가 아니라면
	// 목표값(목적지)까지 자동 이동
	{
		const FVector2D NowXZ(NowPosition.X, NowPosition.Z);
		const FVector2D DestXZ(DestPosition.X, DestPosition.Z);
		
		const bool bCloseEnoughPos = FVector::DistSquared2D(NowPosition, DestPosition) <= FMath::Square(PositionTolerance);
		const bool bCloseEnoughRot = FMath::Abs(FMath::FindDeltaAngleDegrees(NowYaw, TargetYaw)) <= RotationTolerance;
		const bool bCloseEnoughPitch = FMath::Abs(FMath::FindDeltaAngleDegrees(NowPitch, TargetPitch)) <= PitchTolerance;
		
		if (bCloseEnoughPos && bCloseEnoughRot && bCloseEnoughPitch)
		{
			SetActorLocation(DestPosition);
			SetYawApply(TargetYaw);
			SetPitchApply(TargetPitch);
			return;
		}
		
		InterpElapsed += DeltaTime;
		const float Alpha = FMath::Clamp(InterpElapsed / InterpDuration, 0.f, 1.f);

		const FVector NewPosition = FMath::Lerp(InterpStartPosition, InterpTargetPosition, Alpha);
		SetActorLocation(NewPosition);
		
		const float RotationSpeedDegPerSec = GetCharacterMovement()->RotationRate.Yaw;
		float NewYaw = FMath::FixedTurn(NowYaw, TargetYaw, RotationSpeedDegPerSec * DeltaTime);
		SetYawApply(NewYaw);
		
		float NewPitch = FMath::FInterpTo(NowPitch, TargetPitch, DeltaTime, 12.f);
		SetPitchApply(NewPitch);
	}
}

UNetworkEntityComponent* ANTPlayer::GetNetworkEntityComponent() const
{
	return NetworkEntityComponent;
}

bool ANTPlayer::IsLocalPlayer() const
{	
	return NetworkEntityComponent && NetworkEntityComponent->IsLocalControlled();
}

void ANTPlayer::InitializeNetworkEntity(uint32 InEntityId, ENetworkControlType InControlType)
{
	if (NetworkEntityComponent == nullptr)
	{
		return;
	}
	
	NetworkEntityComponent->SetEntityId(InEntityId);
	NetworkEntityComponent->SetControlType(InControlType);
}

void ANTPlayer::SetNetworkEntityState(const FNetworkEntityState& EntityState)
{
	SetDestPosition(EntityState.Position);
	TargetYaw = EntityState.Yaw;
	TargetPitch = EntityState.Pitch;
}

uint32 ANTPlayer::GetEntityId() const
{
	return NetworkEntityComponent ? NetworkEntityComponent->GetEntityId() : 0;
}

void ANTPlayer::SetYawApply(float InYaw)
{
	FRotator NewRotation = GetActorRotation();
	NewRotation.Yaw = FRotator::NormalizeAxis(InYaw);
	
	SetActorRotation(NewRotation);
}

void ANTPlayer::SetPitchApply(float InPitch)
{
	NowPitch = FRotator::NormalizeAxis(InPitch);
}

// Called to bind functionality to input
void ANTPlayer::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

