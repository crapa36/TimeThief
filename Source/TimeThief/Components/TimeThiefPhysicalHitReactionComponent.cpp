#include "Components/TimeThiefPhysicalHitReactionComponent.h"

#include "Character/TimeThiefCharacterBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Actor.h"
#include "MorphingMesh/MorphingMeshComponent.h"
#include "PhysicsEngine/BodyInstance.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "TimerManager.h"

namespace
{
	constexpr float ReactionHoldDuration = 0.25f;
	constexpr float ReactionRecoveryDuration = 0.16f;
	constexpr float MinReactionInterval = 0.035f;
	constexpr float PhysicsBlendWeight = 0.7f;
	constexpr float CommonBlendWeightScale = 0.6f;
	constexpr float BaseImpulseMagnitude = 1000.0f;
	constexpr float MaxPointImpulseMagnitude = 20000.0f;
	constexpr float MaxRadialImpulseMagnitude = 100000.0f;
	constexpr int32 MaxReactionBoneDistance = 4;
	constexpr float AdjacentBodyWeightScale = 0.5f;
	constexpr float SecondaryBodyWeightScale = 0.33f;

	const FName NAME_PhysicalHitReactionSimulationMesh(TEXT("PhysicalHitReactionSimulationMesh"));

	const FName Bone_Pelvis(TEXT("pelvis"));
	const FName Bone_HandL(TEXT("hand_l"));
	const FName Bone_HandR(TEXT("hand_r"));
	const FName Bone_FootL(TEXT("foot_l"));
	const FName Bone_FootR(TEXT("foot_r"));
}

UTimeThiefPhysicalHitReactionComponent::UTimeThiefPhysicalHitReactionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UTimeThiefPhysicalHitReactionComponent::BeginPlay()
{
	Super::BeginPlay();

	if (AActor* Owner = GetOwner())
	{
		Owner->OnTakePointDamage.AddDynamic(this, &ThisClass::OnTakePointDamageCallback);
		Owner->OnTakeRadialDamage.AddDynamic(this, &ThisClass::OnTakeRadialDamageCallback);
	}
}

void UTimeThiefPhysicalHitReactionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopReaction();

	if (AActor* Owner = GetOwner())
	{
		Owner->OnTakePointDamage.RemoveDynamic(this, &ThisClass::OnTakePointDamageCallback);
		Owner->OnTakeRadialDamage.RemoveDynamic(this, &ThisClass::OnTakeRadialDamageCallback);
	}

	Super::EndPlay(EndPlayReason);
}

void UTimeThiefPhysicalHitReactionComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                                           FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bReactionActive && IsMorphingInProgress(CachedMorphingComponent))
	{
		StopReaction();
		return;
	}

	if (bReactionRecovering)
	{
		ReactionRecoveryElapsedTime += DeltaTime;

		const float RecoveryAlpha = FMath::Clamp(ReactionRecoveryElapsedTime / ReactionRecoveryDuration, 0.0f, 1.0f);
		const float SmoothedRecoveryAlpha = RecoveryAlpha * RecoveryAlpha * (3.0f - 2.0f * RecoveryAlpha);
		ApplyActiveBodyBlend(1.0f - SmoothedRecoveryAlpha);
		DisableCenterBodyPhysics();

		if (RecoveryAlpha >= 1.0f)
		{
			StopReaction();
		}
	}
}

void UTimeThiefPhysicalHitReactionComponent::OnTakePointDamageCallback(
	AActor* DamagedActor,
	float Damage,
	AController* InstigatedBy,
	FVector HitLocation,
	UPrimitiveComponent* HitComponent,
	FName BoneName,
	FVector ShotFromDirection,
	const UDamageType* DamageType,
	AActor* DamageCauser)
{
	PlayHitReaction(Damage, HitLocation, ShotFromDirection, BoneName, false);
}

void UTimeThiefPhysicalHitReactionComponent::OnTakeRadialDamageCallback(
	AActor* DamagedActor,
	float Damage,
	const UDamageType* DamageType,
	FVector Origin,
	const FHitResult& HitInfo,
	AController* InstigatedBy,
	AActor* DamageCauser)
{
	const FVector HitLocation = HitInfo.bBlockingHit ? FVector(HitInfo.ImpactPoint) : GetOwner()->GetActorLocation();
	const FVector IncomingDirection = (HitLocation - Origin).GetSafeNormal();
	PlayHitReaction(Damage, HitLocation, IncomingDirection, HitInfo.BoneName, true);
}

void UTimeThiefPhysicalHitReactionComponent::PlayHitReaction(
	float Damage,
	const FVector& HitLocation,
	const FVector& IncomingDirection,
	FName HitBoneName,
	bool bRadialDamage)
{
	if (Damage <= 0.0f)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float CurrentTime = World->GetTimeSeconds();
	if (CurrentTime - LastReactionTime < MinReactionInterval)
	{
		return;
	}

	if (bReactionActive)
	{
		StopReaction();
	}

	USkeletalMeshComponent* SourceMesh = nullptr;
	UMorphingMeshComponent* MorphingComponent = nullptr;
	if (!EnsureSimulationReady(SourceMesh, MorphingComponent))
	{
		return;
	}

	SyncSimulationMeshTransform(SourceMesh);

	FName HitBodyBone = ResolveHitBodyBone(SourceMesh, HitBoneName, HitLocation);
	if (HitBodyBone.IsNone())
	{
		HitBodyBone = ResolveClosestReactionBodyBone(SourceMesh, HitLocation);
	}
	if (HitBodyBone.IsNone())
	{
		return;
	}

	const FVector SafeIncomingDirection = IncomingDirection.IsNearlyZero()
		                                      ? (HitLocation - SourceMesh->GetComponentLocation()).GetSafeNormal()
		                                      : IncomingDirection.GetSafeNormal();
	if (SafeIncomingDirection.IsNearlyZero())
	{
		return;
	}

	GatherActiveReactionBodies(SourceMesh, HitBodyBone);
	if (ActiveReactionBodies.IsEmpty())
	{
		return;
	}

	for (const FActiveReactionBody& ActiveBody : ActiveReactionBodies)
	{
		SimulationMesh->SetBodySimulatePhysics(ActiveBody.BoneName, true);
	}

	ApplyActiveBodyBlend(1.0f);
	DisableCenterBodyPhysics();
	if (!EnsureSimulationPoseReady(SimulationMesh) || !EnsureSimulationPhysicsBodiesReady())
	{
		StopReaction();
		return;
	}

	CachedSourceMesh = SourceMesh;
	CachedMorphingComponent = MorphingComponent;
	BeginSimulationMeshPresentation(SourceMesh);
	if (MorphingComponent)
	{
		MorphingComponent->SetBoneMatrixSourceSkeletalMeshComponent(SimulationMesh);
	}

	const FVector Impulse = SafeIncomingDirection * ResolveImpulseMagnitude(Damage, bRadialDamage);
	SimulationMesh->AddImpulseAtLocation(Impulse, HitLocation, HitBodyBone);

	bReactionActive = true;
	LastReactionTime = CurrentTime;
	SetComponentTickEnabled(true);

	World->GetTimerManager().SetTimer(
		ReactionHoldTimerHandle,
		this,
		&ThisClass::StartReactionRecovery,
		ReactionHoldDuration,
		false);
}

void UTimeThiefPhysicalHitReactionComponent::StartReactionRecovery()
{
	if (!bReactionActive || bReactionRecovering)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ReactionHoldTimerHandle);
	}

	bReactionRecovering = true;
	ReactionRecoveryElapsedTime = 0.0f;
	SetComponentTickEnabled(true);
}

void UTimeThiefPhysicalHitReactionComponent::StopReaction()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ReactionHoldTimerHandle);
	}

	if (SimulationMesh)
	{
		ApplyActiveBodyBlend(0.0f);
		for (const FActiveReactionBody& ActiveBody : ActiveReactionBodies)
		{
			SimulationMesh->SetBodySimulatePhysics(ActiveBody.BoneName, false);
		}
		DisableCenterBodyPhysics();
	}

	if (CachedMorphingComponent)
	{
		CachedMorphingComponent->SetBoneMatrixSourceSkeletalMeshComponent(nullptr);
	}

	EndSimulationMeshPresentation();

	CachedSourceMesh = nullptr;
	CachedMorphingComponent = nullptr;
	ActiveReactionBodies.Reset();
	bReactionActive = false;
	bReactionRecovering = false;
	ReactionRecoveryElapsedTime = 0.0f;
	SetComponentTickEnabled(false);
}

bool UTimeThiefPhysicalHitReactionComponent::EnsureSimulationReady(
	USkeletalMeshComponent*& OutSourceMesh,
	UMorphingMeshComponent*& OutMorphingComponent)
{
	OutSourceMesh = nullptr;
	OutMorphingComponent = nullptr;

	ATimeThiefCharacterBase* Character = Cast<ATimeThiefCharacterBase>(GetOwner());
	if (!Character || Character->bIsDead)
	{
		return false;
	}

	UMorphingMeshComponent* MorphingComponent = Character->GetMorphingMeshComponent();
	if (IsMorphingInProgress(MorphingComponent))
	{
		StopReaction();
		return false;
	}

	USkeletalMeshComponent* SourceMesh = Character->GetThirdPersonMesh();
	if (!SourceMesh || !SourceMesh->GetSkeletalMeshAsset() || !SourceMesh->GetPhysicsAsset())
	{
		return false;
	}

	if (!SimulationMesh)
	{
		AActor* Owner = GetOwner();
		SimulationMesh = NewObject<USkeletalMeshComponent>(Owner, NAME_PhysicalHitReactionSimulationMesh);
		Owner->AddInstanceComponent(SimulationMesh);
		SimulationMesh->SetupAttachment(Owner->GetRootComponent());
		SimulationMesh->RegisterComponent();

		SimulationMesh->SetHiddenInGame(true, true);
		SimulationMesh->SetVisibility(false, true);
		SimulationMesh->SetCastShadow(false);
		SimulationMesh->SetGenerateOverlapEvents(false);
		SimulationMesh->SetCanEverAffectNavigation(false);
		SimulationMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		SimulationMesh->SetCollisionObjectType(ECC_PhysicsBody);
		SimulationMesh->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
		SimulationMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	}

	ConfigureSimulationMesh(SourceMesh);
	if (!EnsureSimulationPoseReady(SimulationMesh) || !EnsureSimulationPhysicsBodiesReady())
	{
		return false;
	}

	OutSourceMesh = SourceMesh;
	OutMorphingComponent = MorphingComponent;
	return true;
}

void UTimeThiefPhysicalHitReactionComponent::ConfigureSimulationMesh(USkeletalMeshComponent* SourceMesh)
{
	if (SimulationMesh->GetSkeletalMeshAsset() != SourceMesh->GetSkeletalMeshAsset())
	{
		SimulationMesh->SetSkeletalMesh(SourceMesh->GetSkeletalMeshAsset(), true);
	}

	if (SimulationMesh->GetPhysicsAsset() != SourceMesh->GetPhysicsAsset())
	{
		SimulationMesh->SetPhysicsAsset(SourceMesh->GetPhysicsAsset(), true);
	}

	const EAnimationMode::Type SourceAnimationMode = SourceMesh->GetAnimationMode();
	if (SimulationMesh->GetAnimationMode() != SourceAnimationMode)
	{
		SimulationMesh->SetAnimationMode(SourceAnimationMode);
	}

	if (SourceAnimationMode == EAnimationMode::AnimationBlueprint
		&& SimulationMesh->GetAnimClass() != SourceMesh->GetAnimClass())
	{
		SimulationMesh->SetAnimInstanceClass(SourceMesh->GetAnimClass());
	}

	SimulationMesh->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
	SimulationMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
}

void UTimeThiefPhysicalHitReactionComponent::SyncSimulationMeshTransform(USkeletalMeshComponent* SourceMesh)
{
	SimulationMesh->AttachToComponent(
		SourceMesh->GetAttachParent() ? SourceMesh->GetAttachParent() : SourceMesh,
		FAttachmentTransformRules::KeepWorldTransform);
	SimulationMesh->SetWorldTransform(
		SourceMesh->GetComponentTransform(),
		false,
		nullptr,
		ETeleportType::TeleportPhysics);
}

bool UTimeThiefPhysicalHitReactionComponent::EnsureSimulationPoseReady(USkeletalMeshComponent* Mesh) const
{
	const USkeletalMesh* SkeletalMesh = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
	if (!SkeletalMesh)
	{
		return false;
	}

	const int32 ExpectedTransformCount = SkeletalMesh->GetRefSkeleton().GetNum();
	TArray<FTransform> BoneSpaceTransforms = Mesh->GetBoneSpaceTransforms();
	if (Mesh->GetNumComponentSpaceTransforms() != ExpectedTransformCount
		|| BoneSpaceTransforms.Num() != ExpectedTransformCount)
	{
		Mesh->TickAnimation(0.0f, false);
		Mesh->RefreshBoneTransforms();
		BoneSpaceTransforms = Mesh->GetBoneSpaceTransforms();
	}

	return Mesh->GetNumComponentSpaceTransforms() == ExpectedTransformCount
		&& BoneSpaceTransforms.Num() == ExpectedTransformCount;
}

bool UTimeThiefPhysicalHitReactionComponent::EnsureSimulationPhysicsBodiesReady() const
{
	const UPhysicsAsset* PhysicsAsset = SimulationMesh ? SimulationMesh->GetPhysicsAsset() : nullptr;
	if (!PhysicsAsset || PhysicsAsset->SkeletalBodySetups.IsEmpty())
	{
		return false;
	}

	const int32 ExpectedBodyCount = PhysicsAsset->SkeletalBodySetups.Num();
	if (SimulationMesh->Bodies.Num() < ExpectedBodyCount)
	{
		SimulationMesh->RecreatePhysicsState();
	}

	return SimulationMesh->Bodies.Num() >= ExpectedBodyCount;
}

bool UTimeThiefPhysicalHitReactionComponent::IsMorphingInProgress(const UMorphingMeshComponent* MorphingComponent) const
{
	return MorphingComponent
		&& MorphingComponent->bIsSkeletalMesh
		&& MorphingComponent->IsComponentTickEnabled()
		&& MorphingComponent->ElapsedTime < MorphingComponent->MaxMorphingTime;
}

void UTimeThiefPhysicalHitReactionComponent::BeginSimulationMeshPresentation(USkeletalMeshComponent* SourceMesh)
{
	bSourceMeshWasVisible = SourceMesh->IsVisible();
	bSourceMeshWasHiddenInGame = SourceMesh->bHiddenInGame;
	bSourceMeshVisibilityCached = true;

	SimulationMesh->SetOwnerNoSee(SourceMesh->bOwnerNoSee);
	SimulationMesh->SetOnlyOwnerSee(SourceMesh->bOnlyOwnerSee);
	SimulationMesh->SetCastShadow(SourceMesh->CastShadow);

	const bool bShowSimulationMesh = bSourceMeshWasVisible && !bSourceMeshWasHiddenInGame;
	SimulationMesh->SetHiddenInGame(!bShowSimulationMesh, true);
	SimulationMesh->SetVisibility(bShowSimulationMesh, true);

	if (bShowSimulationMesh)
	{
		SourceMesh->SetVisibility(false, false);
	}
}

void UTimeThiefPhysicalHitReactionComponent::EndSimulationMeshPresentation()
{
	if (SimulationMesh)
	{
		SimulationMesh->SetHiddenInGame(true, true);
		SimulationMesh->SetVisibility(false, true);
	}

	if (bSourceMeshVisibilityCached && CachedSourceMesh)
	{
		CachedSourceMesh->SetVisibility(bSourceMeshWasVisible, false);
		CachedSourceMesh->SetHiddenInGame(bSourceMeshWasHiddenInGame, false);
	}

	bSourceMeshVisibilityCached = false;
	bSourceMeshWasVisible = false;
	bSourceMeshWasHiddenInGame = false;
}

FName UTimeThiefPhysicalHitReactionComponent::ResolveHitBodyBone(
	USkeletalMeshComponent* SourceMesh,
	FName HitBoneName,
	const FVector& HitLocation) const
{
	if (!IsCenterLockedBone(SourceMesh, HitBoneName) && HasPhysicsBody(SourceMesh, HitBoneName))
	{
		return HitBoneName;
	}

	const FName ClosestPhysicsBone = SourceMesh->GetNumComponentSpaceTransforms() > 0
		                                  ? SourceMesh->FindClosestBone(HitLocation, nullptr, 0.0f, true)
		                                  : NAME_None;
	if (!IsCenterLockedBone(SourceMesh, ClosestPhysicsBone) && HasPhysicsBody(SourceMesh, ClosestPhysicsBone))
	{
		return ClosestPhysicsBone;
	}

	return NAME_None;
}

FName UTimeThiefPhysicalHitReactionComponent::ResolveClosestReactionBodyBone(
	USkeletalMeshComponent* SourceMesh,
	const FVector& HitLocation) const
{
	if (SourceMesh->GetNumComponentSpaceTransforms() <= 0)
	{
		return NAME_None;
	}

	const UPhysicsAsset* PhysicsAsset = SourceMesh->GetPhysicsAsset();
	FName ClosestBodyBone = NAME_None;
	float ClosestDistanceSquared = TNumericLimits<float>::Max();

	for (const TObjectPtr<USkeletalBodySetup>& BodySetup : PhysicsAsset->SkeletalBodySetups)
	{
		const FName BodyBoneName = BodySetup ? BodySetup->BoneName : NAME_None;
		if (BodyBoneName.IsNone() || IsCenterLockedBone(SourceMesh, BodyBoneName))
		{
			continue;
		}

		const float DistanceSquared = FVector::DistSquared(SourceMesh->GetBoneLocation(BodyBoneName), HitLocation);
		if (DistanceSquared < ClosestDistanceSquared)
		{
			ClosestDistanceSquared = DistanceSquared;
			ClosestBodyBone = BodyBoneName;
		}
	}

	return ClosestBodyBone;
}

void UTimeThiefPhysicalHitReactionComponent::GatherActiveReactionBodies(
	USkeletalMeshComponent* SourceMesh,
	FName HitBodyBone)
{
	ActiveReactionBodies.Reset();

	const UPhysicsAsset* PhysicsAsset = SimulationMesh->GetPhysicsAsset();

	for (const TObjectPtr<USkeletalBodySetup>& BodySetup : PhysicsAsset->SkeletalBodySetups)
	{
		const FName BodyBoneName = BodySetup ? BodySetup->BoneName : NAME_None;
		if (BodyBoneName.IsNone() || IsCenterLockedBone(SourceMesh, BodyBoneName) || !SimulationMesh->GetBodyInstance(BodyBoneName))
		{
			continue;
		}

		const float FalloffWeight = ResolveBodyFalloffWeight(SourceMesh, BodyBoneName, HitBodyBone);
		if (FalloffWeight <= 0.0f)
		{
			continue;
		}

		FActiveReactionBody& ActiveBody = ActiveReactionBodies.AddDefaulted_GetRef();
		ActiveBody.BoneName = BodyBoneName;
		ActiveBody.BlendWeight = PhysicsBlendWeight * CommonBlendWeightScale * FalloffWeight;
	}
}

void UTimeThiefPhysicalHitReactionComponent::ApplyActiveBodyBlend(float BlendAlpha)
{
	for (const FActiveReactionBody& ActiveBody : ActiveReactionBodies)
	{
		if (FBodyInstance* BodyInstance = SimulationMesh->GetBodyInstance(ActiveBody.BoneName))
		{
			BodyInstance->PhysicsBlendWeight = ActiveBody.BlendWeight * BlendAlpha;
		}
	}
}

float UTimeThiefPhysicalHitReactionComponent::ResolveBodyFalloffWeight(
	const USkeletalMeshComponent* SourceMesh,
	FName BodyBoneName,
	FName HitBodyBone) const
{
	const int32 BoneDistance = ResolveLocalBoneDistance(SourceMesh, BodyBoneName, HitBodyBone);
	if (BoneDistance == 0)
	{
		return 1.0f;
	}
	if (BoneDistance == 1)
	{
		return AdjacentBodyWeightScale;
	}
	if (BoneDistance <= MaxReactionBoneDistance)
	{
		return SecondaryBodyWeightScale;
	}

	return 0.0f;
}

int32 UTimeThiefPhysicalHitReactionComponent::ResolveLocalBoneDistance(
	const USkeletalMeshComponent* SourceMesh,
	FName FirstBoneName,
	FName SecondBoneName) const
{
	const USkeletalMesh* SkeletalMesh = SourceMesh->GetSkeletalMeshAsset();
	const FReferenceSkeleton& RefSkeleton = SkeletalMesh->GetRefSkeleton();
	const int32 FirstBoneIndex = RefSkeleton.FindBoneIndex(FirstBoneName);
	const int32 SecondBoneIndex = RefSkeleton.FindBoneIndex(SecondBoneName);
	if (FirstBoneIndex == INDEX_NONE || SecondBoneIndex == INDEX_NONE)
	{
		return TNumericLimits<int32>::Max();
	}

	TMap<int32, int32> FirstAncestors;
	int32 DistanceFromFirst = 0;
	for (int32 BoneIndex = FirstBoneIndex; BoneIndex != INDEX_NONE; BoneIndex = RefSkeleton.GetParentIndex(BoneIndex))
	{
		FirstAncestors.Add(BoneIndex, DistanceFromFirst++);
	}

	int32 DistanceFromSecond = 0;
	for (int32 BoneIndex = SecondBoneIndex; BoneIndex != INDEX_NONE; BoneIndex = RefSkeleton.GetParentIndex(BoneIndex))
	{
		if (const int32* FirstDistance = FirstAncestors.Find(BoneIndex))
		{
			return *FirstDistance + DistanceFromSecond;
		}

		++DistanceFromSecond;
	}

	return TNumericLimits<int32>::Max();
}

FName UTimeThiefPhysicalHitReactionComponent::GetSkeletonRootBoneName(const USkeletalMeshComponent* Mesh) const
{
	const USkeletalMesh* SkeletalMesh = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
	if (!SkeletalMesh)
	{
		return NAME_None;
	}

	const FReferenceSkeleton& RefSkeleton = SkeletalMesh->GetRefSkeleton();
	return RefSkeleton.GetNum() > 0 ? RefSkeleton.GetBoneName(0) : NAME_None;
}

bool UTimeThiefPhysicalHitReactionComponent::IsCenterLockedBone(
	const USkeletalMeshComponent* Mesh,
	FName BoneName) const
{
	return BoneName == Bone_Pelvis
		|| BoneName == Bone_HandL
		|| BoneName == Bone_HandR
		|| BoneName == Bone_FootL
		|| BoneName == Bone_FootR
		|| BoneName == GetSkeletonRootBoneName(Mesh);
}

void UTimeThiefPhysicalHitReactionComponent::DisableCenterBodyPhysics()
{
	const FName CenterBones[] =
	{
		GetSkeletonRootBoneName(SimulationMesh),
		Bone_Pelvis,
		Bone_HandL,
		Bone_HandR,
		Bone_FootL,
		Bone_FootR,
	};
	for (const FName CenterBone : CenterBones)
	{
		if (FBodyInstance* BodyInstance = SimulationMesh->GetBodyInstance(CenterBone))
		{
			BodyInstance->PhysicsBlendWeight = 0.0f;
			SimulationMesh->SetBodySimulatePhysics(CenterBone, false);
		}
	}
}

bool UTimeThiefPhysicalHitReactionComponent::HasPhysicsBody(const USkeletalMeshComponent* Mesh, FName BoneName) const
{
	return Mesh->GetPhysicsAsset()->FindBodyIndex(BoneName) != INDEX_NONE;
}

float UTimeThiefPhysicalHitReactionComponent::ResolveImpulseMagnitude(float Damage, bool bRadialDamage) const
{
	const float DamageAlpha = FMath::Clamp(Damage / 100.0f, 0.25f, 1.0f);
	const float MaxImpulse = bRadialDamage ? MaxRadialImpulseMagnitude : MaxPointImpulseMagnitude;
	return FMath::Max(BaseImpulseMagnitude, MaxImpulse * DamageAlpha);
}
