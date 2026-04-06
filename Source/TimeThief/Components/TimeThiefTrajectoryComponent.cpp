#include "Components/TimeThiefTrajectoryComponent.h"
#include "Character/TimeThiefNetworkCharacterBase.h"
#include "Network/NetworkEntityComponent.h"
#include "Network/State/NetworkControlType.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/TrajectoryTypes.h"

static constexpr float StopVelocityThresholdSq = 16.0f;
static constexpr int32 HistoryCleanupThreshold = 20;

static FORCEINLINE FVector ConvertToTrajectoryLocalSpace(const FVector& V)
{
	return FVector(V.Y, -V.X, V.Z);
}

FRemoteTrajectoryHistory::FRemoteTrajectoryHistory(double InMaxHistorySeconds)
	: MaxHistorySeconds(InMaxHistorySeconds)
{
}

void FRemoteTrajectoryHistory::SetMaxHistorySeconds(double InMaxHistorySeconds)
{
	MaxHistorySeconds = InMaxHistorySeconds;
}

void FRemoteTrajectoryHistory::AddSample(double TimeSeconds, const FVector& Position, float YawDeg, const FVector& Velocity2D)
{
	FRemoteNetSample Sample;
	Sample.TimeSeconds = TimeSeconds;
	Sample.Position = Position;
	Sample.YawDeg = YawDeg;
	Sample.Velocity2D = Velocity2D;
	Samples.Add(Sample);

	const double MinTime = TimeSeconds - MaxHistorySeconds;
	int32 FirstValidIdx = 0;
	while (FirstValidIdx < Samples.Num() && Samples[FirstValidIdx].TimeSeconds < MinTime)
	{
		++FirstValidIdx;
	}
	
	if (FirstValidIdx > HistoryCleanupThreshold)
	{
		Samples.RemoveAt(0, FirstValidIdx, EAllowShrinking::No);
	}
}

bool FRemoteTrajectoryHistory::SampleAt(double QueryTime, FVector& OutPos, float& OutYawDeg, FVector& OutVelocity2D) const
{
	if (Samples.Num() == 0)
	{
		return false;
	}

	if (QueryTime <= Samples[0].TimeSeconds)
	{
		OutPos = Samples[0].Position;
		OutYawDeg = Samples[0].YawDeg;
		OutVelocity2D = Samples[0].Velocity2D;
		return true;
	}

	if (QueryTime >= Samples.Last().TimeSeconds)
	{
		OutPos = Samples.Last().Position;
		OutYawDeg = Samples.Last().YawDeg;
		OutVelocity2D = Samples.Last().Velocity2D;
		return true;
	}

	int32 High = Samples.Num() - 1;
	int32 Low = 0;
	while (High - Low > 1)
	{
		int32 Mid = (Low + High) / 2;
		if (Samples[Mid].TimeSeconds < QueryTime)
		{
			Low = Mid;
		}
		else
		{
			High = Mid;
		}
	}

	const FRemoteNetSample& A = Samples[Low];
	const FRemoteNetSample& B = Samples[High];
	const double Interval = B.TimeSeconds - A.TimeSeconds;

	if (Interval <= KINDA_SMALL_NUMBER)
	{
		OutPos = A.Position;
		OutYawDeg = A.YawDeg;
		OutVelocity2D = A.Velocity2D;
		return true;
	}

	const double Alpha = (QueryTime - A.TimeSeconds) / Interval;
	
	const FVector T0 = A.Velocity2D * static_cast<float>(Interval);
	const FVector T1 = B.Velocity2D * static_cast<float>(Interval);
	OutPos = FMath::CubicInterp(A.Position, T0, B.Position, T1, static_cast<float>(Alpha));
	
	const float DeltaYaw = FMath::FindDeltaAngleDegrees(A.YawDeg, B.YawDeg);
	OutYawDeg = FRotator::NormalizeAxis(A.YawDeg + (DeltaYaw * static_cast<float>(Alpha)));

	OutVelocity2D = FMath::Lerp(A.Velocity2D, B.Velocity2D, static_cast<float>(Alpha));

	return true;
}

bool FRemoteTrajectoryHistory::GetLastTwo(FRemoteNetSample& OutPrev, FRemoteNetSample& OutCurr) const
{
	if (Samples.Num() < 2)
	{
		return false;
	}
	OutPrev = Samples[Samples.Num() - 2];
	OutCurr = Samples[Samples.Num() - 1];
	return true;
}

bool FRemoteTrajectoryHistory::GetLastThree(FRemoteNetSample& OutPrev2, FRemoteNetSample& OutPrev, FRemoteNetSample& OutCurr) const
{
	if (Samples.Num() < 3)
	{
		return false;
	}

	OutPrev2 = Samples[Samples.Num() - 3];
	OutPrev = Samples[Samples.Num() - 2];
	OutCurr = Samples[Samples.Num() - 1];
	return true;
}

UTimeThiefTrajectoryComponent::UTimeThiefTrajectoryComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, RemoteHistory(10.0)
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UTimeThiefTrajectoryComponent::BeginPlay()
{
	Super::BeginPlay();
	RemoteHistory.SetMaxHistorySeconds(HistoryLengthSeconds + 1.0);
}

void UTimeThiefTrajectoryComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	ATimeThiefNetworkCharacterBase* NetChar = Cast<ATimeThiefNetworkCharacterBase>(GetOwner());

	if (NetChar)
	{
		if (NetChar->IsLocallyControlled())
		{
			Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
			return;
		}

		const UNetworkEntityComponent* NetEntityComp = NetChar->GetNetworkEntityComponent();
		const ENetworkControlType ControlType = NetEntityComp ? NetEntityComp->GetControlType() : ENetworkControlType::None;
		const bool bUseRemoteTrajectory = (ControlType == ENetworkControlType::Remote) || (ControlType == ENetworkControlType::ServerAuth);
		if (bUseRemoteTrajectory)
		{
			UActorComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);
			UpdateRemoteTrajectory(DeltaTime);
			return;
		}
	}

	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void UTimeThiefTrajectoryComponent::UpdateRemoteTrajectory(float DeltaTime)
{
	ATimeThiefNetworkCharacterBase* NetChar = Cast<ATimeThiefNetworkCharacterBase>(GetOwner());
	if (!NetChar)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (World)
	{
		SimulatedTime = World->GetTimeSeconds();
	}
	else
	{
		SimulatedTime += DeltaTime;
	}

	const FVector CurrentPos = NetChar->GetNetworkLocation();
	const float CurrentYaw = NetChar->GetNetworkYaw();

	FVector WorldPlanarVel = NetChar->GetMoveStep();
	WorldPlanarVel.Z = 0.f;
	
	const FQuat CurrentQuat = FRotator(0.f, CurrentYaw, 0.f).Quaternion();
	const FTransform CurrentTransform(CurrentQuat, CurrentPos, FVector::OneVector);
	const FTransform InvCurrentTransform = CurrentTransform.Inverse();

	RemoteHistory.AddSample(SimulatedTime, CurrentPos, CurrentYaw, WorldPlanarVel);

	const int32 HistoryCount = FMath::CeilToInt32(HistoryLengthSeconds * HistorySamplesPerSecond);
	const int32 PredictionCount = FMath::CeilToInt32(PredictionLengthSeconds * PredictionSamplesPerSecond);
	const int32 TotalSamples = HistoryCount + 1 + PredictionCount;

	Trajectory.Samples.Reset();
	Trajectory.Samples.Reserve(TotalSamples);

	const double DtHist = 1.0 / static_cast<double>(FMath::Max(1, HistorySamplesPerSecond));

	for (int32 i = HistoryCount; i >= 1; --i)
	{
		const double TOffset = -static_cast<double>(i) * DtHist;
		const double QueryTime = SimulatedTime + TOffset;

		FVector Pos;
		float Yaw;
		FVector HistVel;
		if (RemoteHistory.SampleAt(QueryTime, Pos, Yaw, HistVel))
		{
			FTransformTrajectorySample Sample;
			Sample.Position = ConvertToTrajectoryLocalSpace(InvCurrentTransform.TransformPosition(Pos));
			
			const FQuat WorldFacing = FRotator(0.f, Yaw, 0.f).Quaternion();
			Sample.Facing = InvCurrentTransform.TransformRotation(WorldFacing);
			
			Sample.TimeInSeconds = static_cast<float>(TOffset);
			Trajectory.Samples.Add(Sample);
		}
	}

	FTransformTrajectorySample CurrentTrajSample;
	CurrentTrajSample.Position = FVector::ZeroVector;
	CurrentTrajSample.Facing = FQuat::Identity;
	CurrentTrajSample.TimeInSeconds = 0.0f;
	Trajectory.Samples.Add(CurrentTrajSample);

	const float DtPred = 1.0f / FMath::Max(1, PredictionSamplesPerSecond);

	if (WorldPlanarVel.SizeSquared() < StopVelocityThresholdSq)
	{
		WorldPlanarVel = FVector::ZeroVector;
		LastSmoothedVelocity = FVector::ZeroVector;
	}
	else
	{
		const float SmoothFactor = 1.f - FMath::Exp(-5.f * DeltaTime);
		LastSmoothedVelocity = FMath::Lerp(LastSmoothedVelocity, WorldPlanarVel, SmoothFactor);
	}

	if (LastSmoothedVelocity.SizeSquared() < 1.0f)
	{
		LastSmoothedVelocity = FVector::ZeroVector;
	}

	float MaxYawRateDeg = 360.0f;
	if (UCharacterMovementComponent* CMC = NetChar->GetCharacterMovement())
	{
		MaxYawRateDeg = CMC->RotationRate.Yaw;
	}

	float YawRate = 0.0f;
	FRemoteNetSample PrevYawSample, CurrYawSample;
	if (RemoteHistory.GetLastTwo(PrevYawSample, CurrYawSample))
	{
		const double Dt = CurrYawSample.TimeSeconds - PrevYawSample.TimeSeconds;
		if (Dt > KINDA_SMALL_NUMBER)
		{
			YawRate = FMath::FindDeltaAngleDegrees(PrevYawSample.YawDeg, CurrYawSample.YawDeg) / static_cast<float>(Dt);
		}
	}

	if (FMath::Abs(YawRate) < 1.0f)
	{
		YawRate = 0.0f;
	}
	else if (FMath::Abs(MaxYawRateDeg) > KINDA_SMALL_NUMBER)
	{
		YawRate = FMath::Clamp(YawRate, -FMath::Abs(MaxYawRateDeg), FMath::Abs(MaxYawRateDeg));
	}

	FVector PredictedWorldPos = CurrentPos;
	FVector PredictedWorldVel = LastSmoothedVelocity;
	float PredictedYaw = CurrentYaw;

	for (int32 j = 1; j <= PredictionCount; ++j)
	{
		const float T = j * DtPred;

		PredictedWorldPos += PredictedWorldVel * DtPred;
		PredictedWorldVel = PredictedWorldVel.RotateAngleAxis(YawRate * DtPred, FVector::UpVector);
		PredictedYaw = FRotator::NormalizeAxis(PredictedYaw + YawRate * DtPred);
		
		FTransformTrajectorySample Sample;
		Sample.Position = ConvertToTrajectoryLocalSpace(InvCurrentTransform.TransformPosition(PredictedWorldPos));
		
		const FQuat PredictedWorldFacing = FRotator(0.f, PredictedYaw, 0.f).Quaternion();
		Sample.Facing = InvCurrentTransform.TransformRotation(PredictedWorldFacing);
		
		Sample.TimeInSeconds = T;
		Trajectory.Samples.Add(Sample);
	}
}