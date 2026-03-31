#include "Components/TimeThiefTrajectoryComponent.h"
#include "Character/TimeThiefNetworkCharacterBase.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Animation/TrajectoryTypes.h"

FRemoteTrajectoryHistory::FRemoteTrajectoryHistory(double InMaxHistorySeconds)
	: MaxHistorySeconds(InMaxHistorySeconds)
{
}

void FRemoteTrajectoryHistory::SetMaxHistorySeconds(double InMaxHistorySeconds)
{
	MaxHistorySeconds = InMaxHistorySeconds;
}

void FRemoteTrajectoryHistory::AddSample(double TimeSeconds, const FVector& Position, float YawDeg)
{
	FRemoteNetSample Sample;
	Sample.TimeSeconds = TimeSeconds;
	Sample.Position = Position;
	Sample.YawDeg = YawDeg;
	Samples.Add(Sample);

	const double MinTime = TimeSeconds - MaxHistorySeconds;
	int32 FirstValidIdx = 0;
	while (FirstValidIdx < Samples.Num() && Samples[FirstValidIdx].TimeSeconds < MinTime)
	{
		++FirstValidIdx;
	}
	if (FirstValidIdx > 0)
	{
		Samples.RemoveAt(0, FirstValidIdx);
	}
}

bool FRemoteTrajectoryHistory::SampleAt(double QueryTime, FVector& OutPos, float& OutYawDeg) const
{
	if (Samples.Num() == 0)
	{
		return false;
	}

	if (QueryTime <= Samples[0].TimeSeconds)
	{
		OutPos = Samples[0].Position;
		OutYawDeg = Samples[0].YawDeg;
		return true;
	}

	if (QueryTime >= Samples.Last().TimeSeconds)
	{
		OutPos = Samples.Last().Position;
		OutYawDeg = Samples.Last().YawDeg;
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
		return true;
	}

	const double Alpha = (QueryTime - A.TimeSeconds) / Interval;
	OutPos = FMath::Lerp(A.Position, B.Position, static_cast<float>(Alpha));
	
	const float DeltaYaw = FMath::FindDeltaAngleDegrees(A.YawDeg, B.YawDeg);
	OutYawDeg = FRotator::NormalizeAxis(A.YawDeg + (DeltaYaw * static_cast<float>(Alpha)));

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

void UTimeThiefTrajectoryComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	ATimeThiefNetworkCharacterBase* NetChar = Cast<ATimeThiefNetworkCharacterBase>(GetOwner());

	if (NetChar && !NetChar->IsLocalPlayer())
	{
		UActorComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);
		UpdateRemoteTrajectory(DeltaTime);
	}
	else
	{
		Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	}
}

FVector UTimeThiefTrajectoryComponent::EstimatePlanarVelocityFromHistory(const FRemoteTrajectoryHistory& History) const
{
	FRemoteNetSample Prev, Curr;
	if (!History.GetLastTwo(Prev, Curr))
	{
		return FVector::ZeroVector;
	}
	
	const double Dt = Curr.TimeSeconds - Prev.TimeSeconds;
	if (Dt <= KINDA_SMALL_NUMBER)
	{
		return FVector::ZeroVector;
	}
	
	FVector Vel = (Curr.Position - Prev.Position) / static_cast<float>(Dt);
	Vel.Z = 0.f;
	return Vel;
}

void UTimeThiefTrajectoryComponent::UpdateRemoteTrajectory(float DeltaTime)
{
	ATimeThiefNetworkCharacterBase* NetChar = Cast<ATimeThiefNetworkCharacterBase>(GetOwner());
	if (!NetChar)
	{
		return;
	}

	RemoteHistory.SetMaxHistorySeconds(HistoryLengthSeconds + 1.0);

	SimulatedTime += DeltaTime;

	const FVector CurrentPos = NetChar->GetNetworkLocation();
	const float CurrentYaw = NetChar->GetNetworkYaw();
	const FQuat CurrentQuat = FRotator(0.f, CurrentYaw, 0.f).Quaternion();
	const FTransform CurrentTransform(CurrentQuat, CurrentPos, FVector::OneVector);
	const FTransform InvCurrentTransform = CurrentTransform.Inverse();

	RemoteHistory.AddSample(SimulatedTime, CurrentPos, CurrentYaw);

	const int32 HistoryCount = FMath::FloorToInt(HistoryLengthSeconds * HistorySamplesPerSecond);
	const int32 PredictionCount = FMath::FloorToInt(PredictionLengthSeconds * PredictionSamplesPerSecond);
	const int32 TotalSamples = HistoryCount + 1 + PredictionCount;

	Trajectory.Samples.Reset();
	Trajectory.Samples.Reserve(TotalSamples);

	const float DtHist = 1.0f / FMath::Max(1, HistorySamplesPerSecond);

	for (int32 i = HistoryCount; i >= 1; --i)
	{
		const float TOffset = -i * DtHist;
		const double QueryTime = SimulatedTime + TOffset;

		FVector Pos;
		float Yaw;
		if (RemoteHistory.SampleAt(QueryTime, Pos, Yaw))
		{
			FTransformTrajectorySample Sample;
			
			Sample.Position = InvCurrentTransform.TransformPosition(Pos);
			
			const FQuat WorldFacing = FRotator(0.f, Yaw, 0.f).Quaternion();
			Sample.Facing = InvCurrentTransform.TransformRotation(WorldFacing);
			
			Sample.TimeInSeconds = TOffset;
			Trajectory.Samples.Add(Sample);
		}
	}

	FTransformTrajectorySample CurrentTrajSample;
	CurrentTrajSample.Position = FVector::ZeroVector;
	CurrentTrajSample.Facing = FQuat::Identity;
	CurrentTrajSample.TimeInSeconds = 0.0f;
	Trajectory.Samples.Add(CurrentTrajSample);

	const float DtPred = 1.0f / FMath::Max(1, PredictionSamplesPerSecond);

	const FVector WorldPlanarVel = EstimatePlanarVelocityFromHistory(RemoteHistory);
	
	const FVector LocalPlanarVel = InvCurrentTransform.TransformVectorNoScale(WorldPlanarVel);

	float MaxYawRateDeg = 360.0f;
	if (UCharacterMovementComponent* CMC = NetChar->GetCharacterMovement())
	{
		MaxYawRateDeg = CMC->RotationRate.Yaw;
	}

	float YawRate = 0.0f;
	FRemoteNetSample PrevSample, CurrSample;
	if (RemoteHistory.GetLastTwo(PrevSample, CurrSample))
	{
		const double Dt = CurrSample.TimeSeconds - PrevSample.TimeSeconds;
		if (Dt > KINDA_SMALL_NUMBER)
		{
			YawRate = FMath::FindDeltaAngleDegrees(PrevSample.YawDeg, CurrSample.YawDeg) / static_cast<float>(Dt);
		}
	}

	if (MaxYawRateDeg > 0.f)
	{
		YawRate = FMath::Clamp(YawRate, -MaxYawRateDeg, MaxYawRateDeg);
	}

	for (int32 j = 1; j <= PredictionCount; ++j)
	{
		const float T = j * DtPred;
		
		FTransformTrajectorySample Sample;
		
		Sample.Position = LocalPlanarVel * T;
		
		const float TargetLocalYaw = FRotator::NormalizeAxis(YawRate * T);
		Sample.Facing = FRotator(0.f, TargetLocalYaw, 0.f).Quaternion();
		
		Sample.TimeInSeconds = T;
		Trajectory.Samples.Add(Sample);
	}
}