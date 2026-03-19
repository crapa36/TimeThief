


#include "NTPlayer.h"

#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NTLocalPlayer.h"
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
		NowYaw = ActorRot.Pitch;
		TargetYaw = ActorRot.Yaw;
		NowPitch = 0.f;
		TargetPitch = 0.f;
	}
}

// Called every frame
void ANTPlayer::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	{
		NowPosition = GetActorLocation();
		NowYaw = GetActorRotation().Yaw;
	}
	
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

		FVector NewPosition = FMath::Lerp(InterpStartPosition, InterpTargetPosition, Alpha);
		SetActorLocation(NewPosition);
		
		const float RotationSpeedDegPerSec = GetCharacterMovement()->RotationRate.Yaw;
		float NewYaw = FMath::FixedTurn(NowYaw, TargetYaw, RotationSpeedDegPerSec * DeltaTime);
		SetYawApply(NewYaw);
		
		float NewPitch = FMath::FInterpTo(NowPitch, TargetPitch, DeltaTime, 12.f);
		SetPitchApply(NewPitch);
	}
}

bool ANTPlayer::IsLocalPlayer() const
{
	return Cast<ANTLocalPlayer>(this) != nullptr;
	// 이 방법도 좋지만 boolean 변수를 하나 두거나 virtual로 처리해도 된다
}

void ANTPlayer::SetNetworkEntityState(const FNetworkEntityState& EntityState)
{
	DestPosition = EntityState.Position;
	TargetYaw = EntityState.Yaw;
	TargetPitch = EntityState.Pitch;
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

