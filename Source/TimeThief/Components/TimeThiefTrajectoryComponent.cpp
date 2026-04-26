#include "Components/TimeThiefTrajectoryComponent.h"
#include "Character/TimeThiefNetworkCharacterBase.h"
#include "Network/NetworkEntityComponent.h"
#include "Network/State/NetworkControlType.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/TrajectoryTypes.h"

static constexpr int32 HistoryCleanupThreshold = 8;

static FORCEINLINE FVector ConvertToTrajectoryLocalSpace(const FVector& V)
{
	return FVector(-V.Y, V.X, V.Z);
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

void FRemoteTrajectoryHistory::ResetToSample(double TimeSeconds, const FVector& Position, float YawDeg, const FVector& Velocity2D)
{
	Samples.Reset();
	AddSample(TimeSeconds, Position, YawDeg, Velocity2D);
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

bool FRemoteTrajectoryHistory::GetLast(FRemoteNetSample& OutLast) const
{
	if (Samples.Num() == 0)
	{
		return false;
	}

	OutLast = Samples.Last();
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

UTimeThiefTrajectoryComponent::UTimeThiefTrajectoryComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, RemoteHistory(10.0)
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UTimeThiefTrajectoryComponent::BeginPlay()
{
	Super::BeginPlay();
	const double MaxHistorySeconds = static_cast<double>(HistoryLengthSeconds + PredictionLengthSeconds + 1.0f);
	RemoteHistory.SetMaxHistorySeconds(MaxHistorySeconds);
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
	const float CurrentYaw = NetChar->GetNetworkCharYaw();

	const FVector2D NetworkVelocity2D = NetChar->GetNetworkVelocity2D();
	const FVector NetworkPlanarVel(NetworkVelocity2D.X, NetworkVelocity2D.Y, 0.0f);
	const float HistoryVelocityThresholdSq = FMath::Square(FMath::Max(0.0f, HistoryVelocityZeroThresholdCmPerSec));
	const float PredictionVelocityThresholdSq = FMath::Square(FMath::Max(0.0f, PredictionVelocityZeroThresholdCmPerSec));
	const float MinObservedDisplacementSq = FMath::Square(FMath::Max(0.0f, MinObservedDisplacementCm));
	const float SmoothedVelocityZeroThresholdSq = FMath::Square(FMath::Max(0.0f, SmoothedVelocityZeroThresholdCmPerSec));

	FVector HistoryPlanarVel = NetworkPlanarVel;
	const UCharacterMovementComponent* CMC = NetChar->GetCharacterMovement();

	FRemoteNetSample LastSample;
	const bool bHasLastSample = RemoteHistory.GetLast(LastSample);
	FVector ObservedVelocity = FVector::ZeroVector;
	bool bHasObservedVelocity = false;

	if (bHasLastSample)
	{
		const double DtFromLast = SimulatedTime - LastSample.TimeSeconds;
		if (DtFromLast > KINDA_SMALL_NUMBER)
		{
			const FVector DeltaPos = CurrentPos - LastSample.Position;
			const FVector DeltaPlanar(DeltaPos.X, DeltaPos.Y, 0.0f);
			if (DeltaPlanar.SizeSquared() >= MinObservedDisplacementSq)
			{
				ObservedVelocity = DeltaPlanar / static_cast<float>(DtFromLast);
				bHasObservedVelocity = true;
			}
		}
	}

	if (HistoryPlanarVel.SizeSquared() < HistoryVelocityThresholdSq)
	{
		if (bHasObservedVelocity && ObservedVelocity.SizeSquared() >= HistoryVelocityThresholdSq)
		{
			HistoryPlanarVel = ObservedVelocity;
		}
		else
		{
			HistoryPlanarVel = FVector::ZeroVector;
		}
	}

	if (bHasLastSample)
	{
		const float PositionErrorCm = FVector::Dist(CurrentPos, LastSample.Position);
		const float YawErrorDeg = FMath::Abs(FMath::FindDeltaAngleDegrees(CurrentYaw, LastSample.YawDeg));
		const bool bSnapCooldownReady = (SimulatedTime - LastSnapTimeSeconds) >= static_cast<double>(SnapCooldownSeconds);

		if (bSnapCooldownReady && (PositionErrorCm >= HardSnapDistanceCm || YawErrorDeg >= HardSnapYawDeg))
		{
			const bool bForceHardReset = PositionErrorCm >= ForceHardSnapDistanceCm;
			if (bForceHardReset)
			{
				RemoteHistory.ResetToSample(SimulatedTime, CurrentPos, CurrentYaw, HistoryPlanarVel);
				LastSmoothedVelocity = HistoryPlanarVel;
			}
			else
			{
				const float SnapBlendAlpha = FMath::Clamp(HardSnapBlendAlpha, 0.0f, 1.0f);
				const FVector BlendedPos = FMath::Lerp(LastSample.Position, CurrentPos, SnapBlendAlpha);
				const float YawDeltaToCurrent = FMath::FindDeltaAngleDegrees(LastSample.YawDeg, CurrentYaw);
				const float BlendedYaw = FRotator::NormalizeAxis(LastSample.YawDeg + (YawDeltaToCurrent * SnapBlendAlpha));

				FVector BlendedVelocity = HistoryPlanarVel;
				const double Dt = SimulatedTime - LastSample.TimeSeconds;
				if (Dt > KINDA_SMALL_NUMBER)
				{
					FVector BlendedObservedVelocity = (BlendedPos - LastSample.Position) / static_cast<float>(Dt);
					BlendedObservedVelocity.Z = 0.0f;
					BlendedVelocity = FMath::Lerp(HistoryPlanarVel, BlendedObservedVelocity, 0.5f);
				}

				RemoteHistory.AddSample(SimulatedTime, BlendedPos, BlendedYaw, BlendedVelocity);
				LastSmoothedVelocity = FMath::Lerp(LastSmoothedVelocity, BlendedVelocity, 0.5f);
			}

			LastSnapTimeSeconds = SimulatedTime;
			++HardSnapCount;

			UE_LOG(LogTemp, Verbose,
				TEXT("[Trajectory] HardSnap count=%d error=%.1fcm yaw=%.1fdeg force=%d owner=%s"),
				HardSnapCount,
				PositionErrorCm,
				YawErrorDeg,
				bForceHardReset ? 1 : 0,
				*GetNameSafe(NetChar));
		}
		else
		{
			if (PositionErrorCm >= SoftSnapDistanceCm)
			{
				if (bHasObservedVelocity)
				{
					HistoryPlanarVel = FMath::Lerp(HistoryPlanarVel, ObservedVelocity, 0.65f);
				}

				++SoftSnapCount;
			}

			RemoteHistory.AddSample(SimulatedTime, CurrentPos, CurrentYaw, HistoryPlanarVel);
		}
	}
	else
	{
		RemoteHistory.AddSample(SimulatedTime, CurrentPos, CurrentYaw, HistoryPlanarVel);
	}
	
	const FQuat CurrentQuat = FRotator(0.f, CurrentYaw, 0.f).Quaternion();
	const FTransform CurrentTransform(CurrentQuat, CurrentPos, FVector::OneVector);
	const FTransform InvCurrentTransform = CurrentTransform.Inverse();


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

	FVector PredictionInputVelocity = HistoryPlanarVel;
	if (PredictionInputVelocity.SizeSquared() < PredictionVelocityThresholdSq)
	{
		PredictionInputVelocity = FVector::ZeroVector;
	}

	const float SmoothingResponse = FMath::Max(0.0f, VelocitySmoothingResponse);
	const float SmoothFactor = (SmoothingResponse > KINDA_SMALL_NUMBER)
		? (1.f - FMath::Exp(-SmoothingResponse * DeltaTime))
		: 1.f;
	LastSmoothedVelocity = FMath::Lerp(LastSmoothedVelocity, PredictionInputVelocity, SmoothFactor);

	if (LastSmoothedVelocity.SizeSquared() < SmoothedVelocityZeroThresholdSq)
	{
		LastSmoothedVelocity = FVector::ZeroVector;
	}

	float RawYawRate = 0.0f;
	FRemoteNetSample PrevYawSample, CurrYawSample;
	if (RemoteHistory.GetLastTwo(PrevYawSample, CurrYawSample))
	{
		const double Dt = CurrYawSample.TimeSeconds - PrevYawSample.TimeSeconds;
		if (Dt > KINDA_SMALL_NUMBER)
		{
			RawYawRate = FMath::FindDeltaAngleDegrees(PrevYawSample.YawDeg, CurrYawSample.YawDeg) / static_cast<float>(Dt);
		}
	}

	float YawRate = 0.0f;
	float MaxYawRateDeg = FMath::Abs(PredictionYawRateClampDegPerSec);
	if (CMC && FMath::Abs(CMC->RotationRate.Yaw) > KINDA_SMALL_NUMBER)
	{
		MaxYawRateDeg = FMath::Min(MaxYawRateDeg, FMath::Abs(CMC->RotationRate.Yaw));
	}
	if (FMath::Abs(RawYawRate) >= HighTurnYawRateThresholdDegPerSec)
	{
		MaxYawRateDeg = FMath::Min(MaxYawRateDeg, FMath::Abs(HighTurnYawRateClampDegPerSec));
	}

	if (FMath::Abs(RawYawRate) < YawRateDeadZoneDegPerSec)
	{
		YawRate = 0.0f;
	}
	else if (MaxYawRateDeg > KINDA_SMALL_NUMBER)
	{
		YawRate = FMath::Clamp(RawYawRate, -MaxYawRateDeg, MaxYawRateDeg);
	}
	else
	{
		YawRate = RawYawRate;
	}

	FVector PredictedWorldPos = CurrentPos;
	FVector PredictedWorldVel = LastSmoothedVelocity;
	float PredictedYaw = CurrentYaw;

	if (PredictedWorldVel.Size2D() < LowSpeedPredictionThresholdCmPerSec)
	{
		PredictedWorldVel *= FMath::Clamp(LowSpeedPredictionVelocityScale, 0.0f, 1.0f);
	}

	const float VelocityDampingPerStep = FMath::Exp(-FMath::Max(0.0f, PredictionVelocityDamping) * DtPred);

	for (int32 j = 1; j <= PredictionCount; ++j)
	{
		const float T = j * DtPred;

		PredictedWorldPos += PredictedWorldVel * DtPred;
		PredictedWorldVel = PredictedWorldVel.RotateAngleAxis(YawRate * DtPred, FVector::UpVector);
		PredictedWorldVel *= VelocityDampingPerStep;
		PredictedYaw = FRotator::NormalizeAxis(PredictedYaw + YawRate * DtPred);
		
		FTransformTrajectorySample Sample;
		Sample.Position = ConvertToTrajectoryLocalSpace(InvCurrentTransform.TransformPosition(PredictedWorldPos));
		
		const FQuat PredictedWorldFacing = FRotator(0.f, PredictedYaw, 0.f).Quaternion();
		Sample.Facing = InvCurrentTransform.TransformRotation(PredictedWorldFacing);
		
		Sample.TimeInSeconds = T;
		Trajectory.Samples.Add(Sample);
	}
}