#include "Components/Wire/TimeThiefWireComponent.h"
#include "Components/Wire/TimeThiefWirePhysics.h"
#include "Components/Wire/TimeThiefWireTargeting.h"
#include "Character/TimeThiefCharacterBase.h"
#include "TimeThiefGameplayTags.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "CableComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/StaticMesh.h"
#include "Sound/SoundBase.h"
#include "Particles/ParticleSystem.h"
#include "Kismet/GameplayStatics.h"
#include "Utils/TimeThiefAimStatics.h"
#include "Rendering/DrawElementTypes.h"
#include "Widgets/SLeafWidget.h"

DEFINE_LOG_CATEGORY(LogWire);

namespace
{
	class SWireAnchorTargetIndicator : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SWireAnchorTargetIndicator) {}
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
		}

		virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
			FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override
		{
			const FVector2D LocalSize = AllottedGeometry.GetLocalSize();
			const float Width = static_cast<float>(LocalSize.X);
			const float Height = static_cast<float>(LocalSize.Y);
			const float Padding = 4.0f;
			const float AvailableWidth = FMath::Max(Width - Padding * 2.0f, 0.0f);
			const float AvailableHeight = FMath::Max(Height - Padding * 2.0f, 0.0f);
			const float HalfBase = FMath::Min(AvailableWidth * 0.5f, AvailableHeight / UE_SQRT_3);
			if (HalfBase <= 0.0f)
			{
				return LayerId;
			}

			const float TriangleHeight = HalfBase * UE_SQRT_3;
			const float CenterX = Width * 0.5f;
			const float TopY = (Height - TriangleHeight) * 0.5f;
			const FVector2f TopLeft(CenterX - HalfBase, TopY);
			const FVector2f TopRight(CenterX + HalfBase, TopY);
			const FVector2f Bottom(CenterX, TopY + TriangleHeight);
			const float CornerRadius = HalfBase * 0.32f;
			const float LegLength = HalfBase * 0.35f;
			const FLinearColor IndicatorColor(1.0f, 0.02f, 0.04f, 0.95f);

			auto Normalize = [](const FVector2f& Vector) -> FVector2f
			{
				const float Length = FMath::Sqrt(Vector.X * Vector.X + Vector.Y * Vector.Y);
				return Length > KINDA_SMALL_NUMBER ? Vector / Length : FVector2f::ZeroVector;
			};

			auto MakeBezierPoint = [](const FVector2f& Start, const FVector2f& Control, const FVector2f& End, float Alpha) -> FVector2f
			{
				const float InverseAlpha = 1.0f - Alpha;
				return Start * InverseAlpha * InverseAlpha + Control * 2.0f * InverseAlpha * Alpha + End * Alpha * Alpha;
			};

			auto DrawCorner = [&](const FVector2f& Previous, const FVector2f& Corner, const FVector2f& Next)
			{
				const FVector2f ToPrevious = Normalize(Previous - Corner);
				const FVector2f ToNext = Normalize(Next - Corner);
				const FVector2f ArcStart = Corner + ToPrevious * CornerRadius;
				const FVector2f ArcEnd = Corner + ToNext * CornerRadius;

				TArray<FVector2f> Points;
				Points.Reserve(12);
				Points.Add(Corner + ToPrevious * (CornerRadius + LegLength));
				Points.Add(ArcStart);

				for (int32 Index = 1; Index < 8; ++Index)
				{
					const float Alpha = static_cast<float>(Index) / 8.0f;
					Points.Add(MakeBezierPoint(ArcStart, Corner, ArcEnd, Alpha));
				}

				Points.Add(ArcEnd);
				Points.Add(Corner + ToNext * (CornerRadius + LegLength));

				FSlateDrawElement::MakeLines(
					OutDrawElements,
					LayerId,
					AllottedGeometry.ToPaintGeometry(),
					MoveTemp(Points),
					ESlateDrawEffect::None,
					IndicatorColor,
					true,
					3.0f);
			};

			DrawCorner(Bottom, TopLeft, TopRight);
			DrawCorner(TopLeft, TopRight, Bottom);
			DrawCorner(TopRight, Bottom, TopLeft);

			return LayerId;
		}

		virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override
		{
			return FVector2D(48.0f, 48.0f);
		}
	};
}

UTimeThiefWireComponent::UTimeThiefWireComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;

	WirePhysics = CreateDefaultSubobject<UTimeThiefWirePhysics>(TEXT("WirePhysics"));
	WireTargeting = CreateDefaultSubobject<UTimeThiefWireTargeting>(TEXT("WireTargeting"));

	WireCable = CreateDefaultSubobject<UCableComponent>(TEXT("WireCable"));
	WireCable->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WireCable->SetCastShadow(false);
	WireCable->SetVisibility(false);
	WireCable->bAttachStart = true;
	WireCable->bAttachEnd = true;
	WireCable->CableLength = 0.0f;
	WireCable->CableWidth = WireThickness;
	WireCable->EndLocation = FVector::ZeroVector;
	WireCable->SolverIterations = FMath::Clamp(FiringCableSolverIterations, 1, 16);
	WireCable->bEnableStiffness = bFiringCableEnableStiffness;
	WireCable->CableGravityScale = FiringCableGravityScale;
	WireCable->bSkipCableUpdateWhenNotVisible = true;
	ApplyWireCableStaticSettings(false);

	AnchorMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AnchorMeshComponent"));
	AnchorMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	AnchorMeshComponent->SetCastShadow(false);
	AnchorMeshComponent->SetVisibility(false);

	TargetIndicatorComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("TargetIndicatorComponent"));
	TargetIndicatorComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TargetIndicatorComponent->SetGenerateOverlapEvents(false);
	TargetIndicatorComponent->SetWidgetSpace(EWidgetSpace::Screen);
	TargetIndicatorComponent->SetDrawSize(FVector2D(48.0f, 48.0f));
	TargetIndicatorComponent->SetPivot(FVector2D(0.5f, 0.5f));
	TargetIndicatorComponent->SetDrawAtDesiredSize(false);
	TargetIndicatorComponent->SetVisibility(false);
	TargetIndicatorComponent->SetHiddenInGame(true);
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
		CachedCharacter->ReceiveControllerChangedDelegate.AddUniqueDynamic(this, &UTimeThiefWireComponent::OnPawnControllerChanged);

		USkeletalMeshComponent* SkeletalMesh = CachedCharacter->GetMesh();
		if (SkeletalMesh && SkeletalMesh->DoesSocketExist(WireStartSocketName))
		{
			WireCable->AttachToComponent(SkeletalMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, WireStartSocketName);
		}
		else if (USceneComponent* RootComponent = CachedCharacter->GetRootComponent())
		{
			WireCable->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform);
		}

		if (WireMaterial)
		{
			WireCable->SetMaterial(0, WireMaterial);
		}
		WireCable->AddTickPrerequisiteComponent(this);

		if (USceneComponent* RootComponent = CachedCharacter->GetRootComponent())
		{
			AnchorMeshComponent->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform);
			TargetIndicatorComponent->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform);
		}
		WireCable->SetAttachEndToComponent(AnchorMeshComponent);
		WireCable->EndLocation = FVector::ZeroVector;
		AnchorMeshComponent->SetAbsolute(true, true, true);
		TargetIndicatorComponent->SetAbsolute(true, true, true);
		TargetIndicatorComponent->SetSlateWidget(SNew(SWireAnchorTargetIndicator));
		ApplyWireCableStaticSettings(true);

		if (AnchorMeshTemplate)
		{
			AnchorMeshComponent->SetStaticMesh(AnchorMeshTemplate);
		}
		AnchorMeshComponent->SetWorldScale3D(AnchorMeshScale);

		RefreshLocalControllerState();
	}
}

void UTimeThiefWireComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(CachedCharacter))
	{
		CachedCharacter->ReceiveControllerChangedDelegate.RemoveDynamic(this, &UTimeThiefWireComponent::OnPawnControllerChanged);
	}

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
		UpdateTargetIndicator(DeltaTime);
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

	if (AttachParticle)
	{
		UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), AttachParticle, AnchorPoint, AttachedAnchorRotation);
	}

	if (AttachSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, AttachSound, AnchorPoint);
	}

	PlayAttachedWireMontage();

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
	FireStartLocation = FVector::ZeroVector;
	FireTargetLocation = FVector::ZeroVector;
	CurrentFireDistance = 0.0f;
	AttachedWireLength = 0.0f;
	UpdateWireVisuals();
	SetComponentTickEnabled(false);
}

void UTimeThiefWireComponent::SimulateLaunch(const FVector& RemoteStartPosition, const FVector& RemoteDirection)
{
	AnchorPoint = RemoteStartPosition;
	FireDirection = RemoteDirection.GetSafeNormal();
	FireStartLocation = RemoteStartPosition;
	FireTargetLocation = FireStartLocation + FireDirection * MaxWireLength;
	CurrentFireDistance = 0.0f;
	StuckCheckTimer = 0.0f;
	GroundCheckTimer = 0.0f;

	if (FireSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, FireSound, RemoteStartPosition);
	}

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
	return CurrentState == EWireState::Idle && CooldownRemaining <= 0.0f && !bPendingWireFire;
}

float UTimeThiefWireComponent::GetCooldownPercent() const
{
	if (WireCooldown <= 0.0f)
	{
		return 0.0f;
	}

	return FMath::Clamp(CooldownRemaining / WireCooldown, 0.0f, 1.0f);
}

void UTimeThiefWireComponent::FireWire()
{
	if (CurrentState != EWireState::Idle) return;
	if (CooldownRemaining > 0.0f || bPendingWireFire) return;

	bPendingWireFire = true;

	bool bHasFireNotify = false;
	if (!PlayWireFireMontage(bHasFireNotify))
	{
		CancelWireFire();
		LaunchWire();
		return;
	}

	bFireOnMontageEnded = !bHasFireNotify;
}

void UTimeThiefWireComponent::ConfirmWireFire()
{
	if (!bPendingWireFire) return;

	ClearWireFireAnimation(true);

	if (CurrentState != EWireState::Idle) return;

	LaunchWire();
}

void UTimeThiefWireComponent::CancelWireFire()
{
	if (!bPendingWireFire) return;

	ClearWireFireAnimation(true);
}

void UTimeThiefWireComponent::LaunchWire()
{
	if (CurrentState != EWireState::Idle) return;

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

	FVector TargetLocation = FVector::ZeroVector;
	bool bHasTargetLocation = false;
	const float RetargetDotThreshold = FMath::Cos(FMath::DegreesToRadians(FMath::Max(TargetIndicatorRetargetAngleDegrees, 0.0f)));
	if (bHasCachedTargetIndicator && bHasCachedTargetAimDirection
		&& FVector::DotProduct(CachedTargetAimDirection, AimDirection) >= RetargetDotThreshold)
	{
		TargetLocation = CachedTargetIndicatorLocation;
		bHasTargetLocation = true;
	}
	else if (WireTargeting && WireTargeting->FindBestAnchorTarget(TargetLocation, CamLoc, AimDirection, MaxWireLength))
	{
		CachedTargetIndicatorLocation = TargetLocation;
		CachedTargetAimDirection = AimDirection;
		bHasCachedTargetIndicator = true;
		bHasCachedTargetAimDirection = true;
		bHasTargetLocation = true;
	}

	FireStartLocation = WireStartLocation;
	FireTargetLocation = bHasTargetLocation ? TargetLocation : FireStartLocation + AimDirection * MaxWireLength;

	if (bHasTargetLocation)
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

	AnchorPoint = FireStartLocation;
	CurrentFireDistance = 0.0f;
	StuckCheckTimer = 0.0f;
	GroundCheckTimer = 0.0f;
	SetTargetIndicatorVisible(false);

	if (FireSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, FireSound, WireStartLocation);
	}

	OnWireLaunched.Broadcast(WireStartLocation, FireDirection);

	SetComponentTickEnabled(true);
	SetWireState(EWireState::Firing);
}

bool UTimeThiefWireComponent::PlayWireFireMontage(bool& bOutHasFireNotify)
{
	bOutHasFireNotify = false;
	if (!FireMontage) return false;

	UAnimInstance* AnimInstance = GetWireMontageAnimInstance();
	if (!AnimInstance) return false;

	const float MontageLength = AnimInstance->Montage_Play(FireMontage);
	if (MontageLength <= 0.0f) return false;

	PlayMontageOnWireMeshes(FireMontage, AnimInstance);
	bOutHasFireNotify = DoesMontageContainWireFireNotify(FireMontage);
	BindWireFireAnimation(AnimInstance, FireMontage);
	return true;
}

void UTimeThiefWireComponent::PlayAttachedWireMontage()
{
	if (!AttachedMontage) return;

	UAnimInstance* AnimInstance = GetWireMontageAnimInstance();
	if (!AnimInstance) return;

	if (AnimInstance->Montage_Play(AttachedMontage) > 0.0f)
	{
		PlayMontageOnWireMeshes(AttachedMontage, AnimInstance);
	}
}

bool UTimeThiefWireComponent::DoesMontageContainWireFireNotify(const UAnimMontage* Montage) const
{
	if (!Montage || WireFireNotifyName.IsNone()) return false;

	const FString NotifyNameString = WireFireNotifyName.ToString();
	const FName NotifyEventName = GetWireFireNotifyEventName();
	const bool bCanHandleNamedNotify = FindFunction(NotifyEventName) != nullptr;

	for (const FAnimNotifyEvent& NotifyEvent : Montage->Notifies)
	{
		if (bCanHandleNamedNotify && NotifyEvent.NotifyName == WireFireNotifyName)
		{
			return true;
		}

		if (NotifyEvent.Notify && NotifyEvent.Notify->GetNotifyName() == NotifyNameString)
		{
			return true;
		}

		if (NotifyEvent.NotifyStateClass && NotifyEvent.NotifyStateClass->GetNotifyName() == NotifyNameString)
		{
			return true;
		}
	}

	return false;
}

UAnimInstance* UTimeThiefWireComponent::GetWireMontageAnimInstance() const
{
	if (!IsValid(CachedCharacter)) return nullptr;

	USkeletalMeshComponent* MontageMesh = CachedCharacter->GetMesh();
	if (const ATimeThiefCharacterBase* CharacterBase = Cast<ATimeThiefCharacterBase>(CachedCharacter))
	{
		MontageMesh = CharacterBase->GetMontagePlaybackMesh();
	}

	return MontageMesh ? MontageMesh->GetAnimInstance() : nullptr;
}

void UTimeThiefWireComponent::PlayMontageOnWireMeshes(UAnimMontage* Montage, UAnimInstance* PrimaryAnimInstance)
{
	if (!Montage || !IsValid(CachedCharacter)) return;

	if (const ATimeThiefCharacterBase* CharacterBase = Cast<ATimeThiefCharacterBase>(CachedCharacter))
	{
		TArray<USkeletalMeshComponent*, TInlineAllocator<2>> Meshes;
		Meshes.Add(CharacterBase->GetThirdPersonMesh());
		Meshes.AddUnique(CharacterBase->GetFirstPersonMesh());

		for (USkeletalMeshComponent* Mesh : Meshes)
		{
			UAnimInstance* AnimInstance = Mesh ? Mesh->GetAnimInstance() : nullptr;
			if (AnimInstance && AnimInstance != PrimaryAnimInstance)
			{
				AnimInstance->Montage_Play(Montage);
			}
		}
		return;
	}

	if (USkeletalMeshComponent* Mesh = CachedCharacter->GetMesh())
	{
		if (UAnimInstance* AnimInstance = Mesh->GetAnimInstance(); AnimInstance && AnimInstance != PrimaryAnimInstance)
		{
			AnimInstance->Montage_Play(Montage);
		}
	}
}

void UTimeThiefWireComponent::BindWireFireAnimation(UAnimInstance* AnimInstance, UAnimMontage* Montage)
{
	PendingFireAnimInstance = AnimInstance;
	PendingFireMontage = Montage;
	PendingWireFireNotifyName = WireFireNotifyName;
	PendingWireFireNotifyEventName = GetWireFireNotifyEventName();

	AnimInstance->OnPlayMontageNotifyBegin.AddDynamic(this, &UTimeThiefWireComponent::OnWireFireMontageNotify);

	if (FindFunction(PendingWireFireNotifyEventName))
	{
		AnimInstance->AddExternalNotifyHandler(this, PendingWireFireNotifyEventName);
	}

	FOnMontageEnded MontageEndedDelegate;
	MontageEndedDelegate.BindUObject(this, &UTimeThiefWireComponent::OnWireFireMontageEnded);
	AnimInstance->Montage_SetEndDelegate(MontageEndedDelegate, Montage);
}

void UTimeThiefWireComponent::ClearWireFireAnimation(bool bClearMontageEndDelegate)
{
	if (PendingFireAnimInstance)
	{
		PendingFireAnimInstance->OnPlayMontageNotifyBegin.RemoveDynamic(this, &UTimeThiefWireComponent::OnWireFireMontageNotify);

		if (FindFunction(PendingWireFireNotifyEventName))
		{
			PendingFireAnimInstance->RemoveExternalNotifyHandler(this, PendingWireFireNotifyEventName);
		}

		if (bClearMontageEndDelegate && PendingFireMontage)
		{
			FOnMontageEnded EmptyDelegate;
			PendingFireAnimInstance->Montage_SetEndDelegate(EmptyDelegate, PendingFireMontage);
		}
	}

	PendingFireAnimInstance = nullptr;
	PendingFireMontage = nullptr;
	PendingWireFireNotifyName = NAME_None;
	PendingWireFireNotifyEventName = NAME_None;
	bPendingWireFire = false;
	bFireOnMontageEnded = false;
}

FName UTimeThiefWireComponent::GetWireFireNotifyEventName() const
{
	return FName(*FString::Printf(TEXT("AnimNotify_%s"), *WireFireNotifyName.ToString()));
}

void UTimeThiefWireComponent::OnWireFireMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (Montage != PendingFireMontage) return;

	const bool bShouldFire = bPendingWireFire && bFireOnMontageEnded && !bInterrupted;
	ClearWireFireAnimation(false);

	if (bShouldFire && CurrentState == EWireState::Idle)
	{
		LaunchWire();
	}
}

void UTimeThiefWireComponent::OnWireFireMontageNotify(FName NotifyName, const FBranchingPointNotifyPayload& BranchingPointNotifyPayload)
{
	if (!bPendingWireFire) return;
	if (NotifyName != PendingWireFireNotifyName) return;
	if (BranchingPointNotifyPayload.SequenceAsset != PendingFireMontage) return;

	ConfirmWireFire();
}

void UTimeThiefWireComponent::AnimNotify_WireFire()
{
	ConfirmWireFire();
}

void UTimeThiefWireComponent::OnPawnControllerChanged(APawn* Pawn, AController* OldController, AController* NewController)
{
	if (Pawn != CachedCharacter)
	{
		return;
	}

	RefreshLocalControllerState();
}

void UTimeThiefWireComponent::ReleaseWire()
{
	CancelWireFire();

	if (CurrentState == EWireState::Idle) return;

	TargetIndicatorRefreshTimer = 0.0f;
	bHasCachedTargetIndicator = false;
	bHasCachedTargetAimDirection = false;
	SetTargetIndicatorVisible(false);

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
	FireStartLocation = FVector::ZeroVector;
	FireTargetLocation = FVector::ZeroVector;
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
		AnchorPoint = FireStartLocation + FireDirection * MaxWireLength;
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
	TargetIndicatorRefreshTimer = 0.0f;
	bHasCachedTargetIndicator = false;
	bHasCachedTargetAimDirection = false;
	SetTargetIndicatorVisible(false);

	if (!IsValid(CachedMovementComponent)) return;

	PlayAttachedWireMontage();

	if (AttachSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, AttachSound, AnchorPoint);
	}

	if (AttachParticle)
	{
		UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), AttachParticle, AnchorPoint, AttachedAnchorRotation);
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

float UTimeThiefWireComponent::GetWireCableTautnessAlpha(float CurrentDistance) const
{
	if (CurrentState != EWireState::Attached)
	{
		return 0.0f;
	}

	const float LengthTolerance = FMath::Max(WireLengthUpdateTolerance, 1.0f);
	const float LengthSlack = AttachedWireLength - CurrentDistance;
	const float LengthAlpha = 1.0f - FMath::Clamp(LengthSlack / LengthTolerance, 0.0f, 1.0f);

	if (!WirePhysics)
	{
		return LengthAlpha;
	}

	const float DistanceError = FMath::Max(CurrentDistance - AttachedWireLength, 0.0f);
	const float SpringForce = DistanceError * FMath::Max(WirePhysics->SpringStiffness, 0.0f);

	float DampingForce = 0.0f;
	if (IsValid(CachedMovementComponent))
	{
		const FVector WireDirection = (GetPullAnchorPoint() - GetWireStartLocation()).GetSafeNormal();
		const float RadialVelocity = FVector::DotProduct(CachedMovementComponent->Velocity, WireDirection);
		DampingForce = FMath::Max(-RadialVelocity * FMath::Max(WirePhysics->SpringDamping, 0.0f), 0.0f);
	}

	const float PullForce = FMath::Max(WirePhysics->PullForce, 0.0f);
	const float SpringReferenceForce = FMath::Max(WirePhysics->SpringStiffness, 0.0f) * LengthTolerance;
	const float ReferenceForce = FMath::Max(PullForce + SpringReferenceForce, 1.0f);
	const float ForceAlpha = FMath::Clamp((PullForce + SpringForce + DampingForce) / ReferenceForce, 0.0f, 1.0f);

	return FMath::Max(LengthAlpha, ForceAlpha);
}

void UTimeThiefWireComponent::ApplyWireCableStaticSettings(bool bRecreateSimulation)
{
	if (!WireCable)
	{
		return;
	}

	const int32 NumSegments = FMath::Clamp(WireCableNumSegments, 1, 20);
	const int32 NumSides = FMath::Clamp(WireCableNumSides, 1, 16);

	WireCable->NumSegments = NumSegments;
	WireCable->NumSides = NumSides;
	WireCable->TileMaterial = WireCableTileMaterial;
	WireCable->SubstepTime = FMath::Max(WireCableSubstepTime, 0.005f);
	WireCable->bResetAfterTeleport = bWireCableResetAfterTeleport;
	WireCable->bTeleportAfterReattach = bWireCableTeleportAfterReattach;

	if (bRecreateSimulation && WireCable->IsRegistered())
	{
		WireCable->ReregisterComponent();
	}
}

void UTimeThiefWireComponent::UpdateWireVisuals()
{
	if (!WireCable || !AnchorMeshComponent) return;

	if (CurrentState == EWireState::Idle)
	{
		WireCable->SetVisibility(false);
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
		FVector AnchorDirection = UTimeThiefAimStatics::NormalizeAimDirection(FireTargetLocation - AnchorPoint, FireDirection);
		if (FVector::DotProduct(AnchorDirection, FireDirection) < 0.0f)
		{
			AnchorDirection = FireDirection;
		}
		AnchorRotation = AnchorDirection.Rotation() + AnchorMeshRotationOffset;
	}
	
	AnchorMeshComponent->SetWorldLocation(AnchorPoint);
	AnchorMeshComponent->SetWorldRotation(AnchorRotation);

	const FVector WireAttachPoint = AnchorPoint + AnchorRotation.RotateVector(AnchorWireAttachOffset);
	const FVector End = WireAttachPoint;
	const float Distance = FVector::Dist(Start, End);

	if (Distance < KINDA_SMALL_NUMBER)
	{
		WireCable->SetVisibility(false);
		AnchorMeshComponent->SetVisibility(false);
		return;
	}

	WireCable->SetWorldLocation(Start);
	WireCable->CableWidth = FMath::Max(WireThickness, 0.01f);
	WireCable->EndLocation = AnchorMeshComponent->GetComponentTransform().InverseTransformPosition(End);

	const float TautnessAlpha = GetWireCableTautnessAlpha(Distance);
	const float SlackMultiplier = FMath::Lerp(FiringCableSlackMultiplier, TautCableSlackMultiplier, TautnessAlpha);
	const float GravityScale = FMath::Lerp(FiringCableGravityScale, TautCableGravityScale, TautnessAlpha);
	const int32 SolverIterations = FMath::RoundToInt(FMath::Lerp(static_cast<float>(FiringCableSolverIterations), static_cast<float>(TautCableSolverIterations), TautnessAlpha));
	const bool bEnableStiffness = TautnessAlpha >= 0.5f ? bTautCableEnableStiffness : bFiringCableEnableStiffness;
	const float TeleportDistanceThreshold = FMath::Lerp(
		FMath::Max(FiringCableTeleportDistanceThreshold, 0.01f),
		FMath::Max(TautCableTeleportDistanceThreshold, 0.01f),
		TautnessAlpha);

	WireCable->CableLength = Distance * FMath::Max(SlackMultiplier, 1.0f);
	WireCable->CableGravityScale = GravityScale;
	WireCable->SolverIterations = FMath::Clamp(SolverIterations, 1, 16);
	WireCable->bEnableStiffness = bEnableStiffness;
	WireCable->TeleportDistanceThreshold = TeleportDistanceThreshold;
	WireCable->SubstepTime = FMath::Max(WireCableSubstepTime, 0.005f);
	WireCable->TileMaterial = WireCableTileMaterial;
	WireCable->bResetAfterTeleport = bWireCableResetAfterTeleport;
	WireCable->bTeleportAfterReattach = bWireCableTeleportAfterReattach;
	WireCable->SetVisibility(true);
	AnchorMeshComponent->SetVisibility(true);
}

void UTimeThiefWireComponent::UpdateTargetIndicator(float DeltaTime)
{
	if (CurrentState != EWireState::Idle || CooldownRemaining > 0.0f)
	{
		TargetIndicatorRefreshTimer = 0.0f;
		bHasCachedTargetIndicator = false;
		bHasCachedTargetAimDirection = false;
		SetTargetIndicatorVisible(false);
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

	TargetIndicatorRefreshTimer = FMath::Max(TargetIndicatorRefreshTimer - DeltaTime, 0.0f);
	const float RetargetDotThreshold = FMath::Cos(FMath::DegreesToRadians(FMath::Max(TargetIndicatorRetargetAngleDegrees, 0.0f)));
	const bool bShouldRefreshTarget = !bHasCachedTargetAimDirection
		|| FVector::DotProduct(CachedTargetAimDirection, AimDirection) < RetargetDotThreshold;

	if (bShouldRefreshTarget && TargetIndicatorRefreshTimer <= 0.0f)
	{
		FVector TargetLocation = FVector::ZeroVector;
		bHasCachedTargetIndicator = WireTargeting && WireTargeting->FindBestAnchorTarget(TargetLocation, CamLoc, AimDirection, MaxWireLength);
		if (bHasCachedTargetIndicator)
		{
			CachedTargetIndicatorLocation = TargetLocation;
		}
		CachedTargetAimDirection = AimDirection;
		bHasCachedTargetAimDirection = true;

		TargetIndicatorRefreshTimer = FMath::Max(TargetIndicatorUpdateInterval, 0.0f);
	}

	if (bHasCachedTargetIndicator)
	{
		if (TargetIndicatorComponent)
		{
			TargetIndicatorComponent->SetWorldLocation(CachedTargetIndicatorLocation);
		}
		SetTargetIndicatorVisible(true);
	}
	else
	{
		SetTargetIndicatorVisible(false);
	}
}

void UTimeThiefWireComponent::SetTargetIndicatorVisible(bool bVisible)
{
	if (!TargetIndicatorComponent)
	{
		return;
	}

	TargetIndicatorComponent->SetVisibility(bVisible);
	TargetIndicatorComponent->SetHiddenInGame(!bVisible);
}

void UTimeThiefWireComponent::RefreshLocalControllerState()
{
	if (!IsValid(CachedCharacter) || !CachedCharacter->IsLocallyControlled())
	{
		bHasCachedTargetIndicator = false;
		bHasCachedTargetAimDirection = false;
		SetTargetIndicatorVisible(false);
		return;
	}

	if (APlayerController* PC = Cast<APlayerController>(CachedCharacter->GetController()))
	{
		CachedCameraManager = PC->PlayerCameraManager;
		if (TargetIndicatorComponent)
		{
			TargetIndicatorComponent->SetOwnerPlayer(PC->GetLocalPlayer());
		}
		if (CachedCameraManager)
		{
			DefaultFOV = CachedCameraManager->DefaultFOV;
		}
	}

	TargetIndicatorRefreshTimer = 0.0f;
	bHasCachedTargetAimDirection = false;
	SetComponentTickEnabled(true);
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

	const float MaxSpeed = SpeedEffectThreshold * 2.0f;
	const float Alpha = FMath::Clamp((Speed - SpeedEffectThreshold) / (MaxSpeed - SpeedEffectThreshold), 0.0f, 1.0f);
	return FMath::Pow(Alpha, 0.5f);
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

	if (EffectAlpha > 0.1f && WireSpeedShake && IsValid(CachedCharacter))
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
