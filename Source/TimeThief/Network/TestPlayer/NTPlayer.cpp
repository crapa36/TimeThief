


#include "NTPlayer.h"

#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NTLocalPlayer.h"


// Sets default values
ANTPlayer::ANTPlayer()
{
	GetCapsuleComponent()->InitCapsuleSize(50.f, 100.f);
	
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;
	
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 45.0f, 0.0f);
	
	GetCharacterMovement()->JumpZVelocity = 600.f;
	GetCharacterMovement()->AirControl = 0.2f;
	GetCharacterMovement()->MaxWalkSpeed = 600.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.f;
	
	GetCharacterMovement()->bRunPhysicsWithNoController = true;
	
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
		DestRotation = GetActorRotation();
	}
}

// Called every frame
void ANTPlayer::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	{
		NowPosition = GetActorLocation();
		NowRotation = GetActorRotation();
	}
	
	if (IsLocalPlayer()) return;
	
	// 내가 조종하는 Local Player가 아니라면
	// 목표값(목적지)까지 자동 이동
	{
		
		const bool bCloseEnoughPos = FVector::DistSquared2D(NowPosition, DestPosition) <= FMath::Square(PositionTolerance);
		const bool bCloseEnoughRot = FMath::Abs(FMath::FindDeltaAngleDegrees(NowRotation.Yaw, DestRotation.Yaw)) <= RotationTolerance;
		
		if (bCloseEnoughPos && bCloseEnoughRot)
		{
			SetActorLocation(NowPosition);
			SetActorRotation(NowRotation);
			return;
		}
		
		const float MaxStep = GetCharacterMovement()->MaxWalkSpeed;
		const FVector ToDest = DestPosition - NowPosition;
		
		FVector NewPosition = NowPosition;
		
		const float Dist2D = FVector(ToDest.X, ToDest.Y, 0.0f).Size();
		if (Dist2D <= KINDA_SMALL_NUMBER)
		{
			// 거의 같은 위치에 있는 경우, 바로 목적지로 이동
			NewPosition.X = DestPosition.X;
			NewPosition.Y = DestPosition.Y;
		}
		else
		{
			const FVector Dir2D = FVector(ToDest.X, ToDest.Y, 0.0f).GetSafeNormal();
			NewPosition += Dir2D * MaxStep;
			NewPosition.Z = FMath::FInterpTo(NowPosition.Z, DestPosition.Z, DeltaTime, 10.f);
		}
		
		const float RotationSpeedDegPerSec = GetCharacterMovement()->RotationRate.Yaw;
		FRotator NewRotation = NowRotation;
		NewRotation.Yaw = FMath::FixedTurn(NowRotation.Yaw, DestRotation.Yaw, RotationSpeedDegPerSec * DeltaTime);
		
		NewRotation.Pitch = DestRotation.Pitch;
		
		SetActorLocation(NewPosition);
		SetActorRotation(NewRotation);
	}
}

bool ANTPlayer::IsLocalPlayer() const
{
	return Cast<ANTLocalPlayer>(this) != nullptr;
	// 이 방법도 좋지만 boolean 변수를 하나 두거나 virtual로 처리해도 된다
}

// Called to bind functionality to input
void ANTPlayer::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

