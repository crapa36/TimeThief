


#include "NTLocalPlayer.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"

// Sets default values
ANTLocalPlayer::ANTLocalPlayer()
{
	// Camera 세팅
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f; // 카메라와 캐릭터 사이의 거리
	CameraBoom->bUsePawnControlRotation = true; // 캐릭터의 회전에 따라 카메라가 회전하도록 설정
	
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); // 카메라를 CameraBoom에 부착
	FollowCamera->bUsePawnControlRotation = false; // 카메라가 회전하지 않도록 설정
	
	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void ANTLocalPlayer::BeginPlay()
{
	Super::BeginPlay();
}

// Called every frame
void ANTLocalPlayer::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
}

// Called to bind functionality to input
void ANTLocalPlayer::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	
	if (APlayerController* PC = Cast<APlayerController>(Controller))
	{
		if (ULocalPlayer* LP = PC->GetLocalPlayer())
		{
			if (UEnhancedInputLocalPlayerSubsystem* Subsystem = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
			{
				Subsystem->ClearAllMappings();
				if (IMC_Default)
				{
					Subsystem->AddMappingContext(IMC_Default, 0);
				}
			}
		}
	}
	
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (JumpAction)
		{
			EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ANTLocalPlayer::DoJumpStart);
			EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ANTLocalPlayer::DoJumpEnd);
		}
		
		if (MoveAction)
		{
			EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ANTLocalPlayer::Move);
			EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Completed, this, &ANTLocalPlayer::Move);
		}
		
		if (MouseLookAction)
		{
			EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &ANTLocalPlayer::Look);
		}
		
		if (LookAction)
		{
			EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ANTLocalPlayer::Look);
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("'%s' Faild to find an Enhanced Input Component!"), *GetNameSafe(this));
	}
}

void ANTLocalPlayer::Move(const FInputActionValue& Value)
{
	FVector2D MovementValue = Value.Get<FVector2D>();
	
	DoMove(MovementValue.X, MovementValue.Y);
}

void ANTLocalPlayer::Look(const FInputActionValue& Value)
{
	FVector2D LookAxisValue = Value.Get<FVector2D>();
	
	DoLook(LookAxisValue.X, LookAxisValue.Y);
}

void ANTLocalPlayer::DoMove(float Right, float Forward)
{
	if (GetController())
	{
		const FRotator Rotation = GetController()->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);
		
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
		
		AddMovementInput(ForwardDirection, Forward);
		AddMovementInput(RightDirection, Right);
	}
}

void ANTLocalPlayer::DoLook(float Yaw, float Pitch)
{
	if (GetController())
	{
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

void ANTLocalPlayer::DoJumpStart()
{
	Jump();
}

void ANTLocalPlayer::DoJumpEnd()
{
	StopJumping();
}

