#include "Smoke/TimeThiefSmokeWorldSubsystem.h"

#include "Actors/TimeThiefSmokeVolume.h"
#include "Engine/Engine.h"
#include "Stats/Stats.h"
#include "TimeThiefSmokeRendererSubsystem.h"

namespace TimeThiefSmoke
{
	constexpr int32 MaxBulletTracesPerSmokePerTick = 18;

	ETimeThiefSmokeRendererInteractionType ToRendererType(ESmokeInteractionType Type)
	{
		switch (Type)
		{
		case ESmokeInteractionType::ExplosionShock:
			return ETimeThiefSmokeRendererInteractionType::ExplosionShock;
		case ESmokeInteractionType::ActorPush:
			return ETimeThiefSmokeRendererInteractionType::ActorPush;
		case ESmokeInteractionType::BulletWake:
		default:
			return ETimeThiefSmokeRendererInteractionType::BulletWake;
		}
	}

	ETimeThiefSmokeRendererInteractionShape ToRendererShape(ESmokeInteractionShape Shape)
	{
		switch (Shape)
		{
		case ESmokeInteractionShape::Capsule:
			return ETimeThiefSmokeRendererInteractionShape::Capsule;
		case ESmokeInteractionShape::Box:
			return ETimeThiefSmokeRendererInteractionShape::Box;
		case ESmokeInteractionShape::LineWake:
			return ETimeThiefSmokeRendererInteractionShape::LineWake;
		case ESmokeInteractionShape::Sphere:
		default:
			return ETimeThiefSmokeRendererInteractionShape::Sphere;
		}
	}

	FTimeThiefSmokeRendererSettings ToRendererSettings(const FTimeThiefSmokeRuntimeSettings& Settings)
	{
		FTimeThiefSmokeRendererSettings RendererSettings;
		RendererSettings.SmokeGridResolution = Settings.SmokeGridResolution;
		RendererSettings.PressureIterations = Settings.PressureIterations;
		RendererSettings.RenderStepCount = Settings.RenderStepCount;
		RendererSettings.Extinction = Settings.Extinction;
		RendererSettings.ScatteringAlbedo = Settings.ScatteringAlbedo;
		RendererSettings.ScatteringAnisotropy = Settings.ScatteringAnisotropy;
		RendererSettings.DensityDissipation = Settings.DensityDissipation;
		RendererSettings.VelocityDamping = Settings.VelocityDamping;
		RendererSettings.VorticityStrength = Settings.VorticityStrength;
		RendererSettings.BulletWakeMaxVisibleLife = Settings.BulletWakeMaxVisibleLife;
		RendererSettings.bUseMacCormackAdvection = Settings.bUseMacCormackAdvection;
		RendererSettings.CarrierParticleCount = Settings.CarrierParticleCount;
		RendererSettings.CarrierParticleRadius = Settings.CarrierParticleRadius;
		RendererSettings.CarrierParticleDriftSpeed = Settings.CarrierParticleDriftSpeed;
		RendererSettings.CarrierParticleInteractionStrength = Settings.CarrierParticleInteractionStrength;
		RendererSettings.MaxGPUEventsPerSmokePerFrame = Settings.MaxGPUEventsPerSmokePerFrame;
		RendererSettings.InitialDensity = Settings.InitialDensity;
		RendererSettings.SmokeFadeOutDuration = Settings.SmokeFadeOutDuration;
		RendererSettings.PlumeEmissionDuration = Settings.PlumeEmissionDuration;
		RendererSettings.PlumeSourceRadius = Settings.PlumeSourceRadius;
		RendererSettings.PlumeExpansionVelocity = Settings.PlumeExpansionVelocity;
		RendererSettings.PlumeRiseVelocity = Settings.PlumeRiseVelocity;
		return RendererSettings;
	}

	FTimeThiefSmokeRendererEvent ToRendererEvent(const FTimeThiefSmokeInteractionEvent& Event)
	{
		FTimeThiefSmokeRendererEvent RendererEvent;
		RendererEvent.SmokeId = Event.SmokeId;
		RendererEvent.Type = ToRendererType(Event.Type);
		RendererEvent.Shape = ToRendererShape(Event.Shape);
		RendererEvent.Position = FVector3f(Event.Position);
		RendererEvent.Direction = FVector3f(Event.Direction);
		RendererEvent.Rotation = FQuat4f(Event.Rotation);
		RendererEvent.Extents = FVector3f(Event.Extents);
		RendererEvent.Radius = Event.Radius;
		RendererEvent.Length = Event.Length;
		RendererEvent.Strength = Event.Strength;
		RendererEvent.NormalizedAge = Event.NormalizedAge;
		RendererEvent.Seed = Event.Seed;
		return RendererEvent;
	}
}

void UTimeThiefSmokeWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UTimeThiefSmokeWorldSubsystem::Deinitialize()
{
	ActiveSmokeVolumes.Reset();
	ActiveImpulses.Reset();
	PendingRendererEvents.Reset();
	Super::Deinitialize();
}

void UTimeThiefSmokeWorldSubsystem::Tick(float DeltaTime)
{
	CompactSmokeVolumes();

	for (int32 Index = ActiveImpulses.Num() - 1; Index >= 0; --Index)
	{
		FTimeThiefActiveSmokeImpulse& ActiveImpulse = ActiveImpulses[Index];
		ATimeThiefSmokeVolume* SmokeVolume = ActiveImpulse.SmokeVolume.Get();
		if (!SmokeVolume || ActiveImpulse.Duration <= KINDA_SMALL_NUMBER)
		{
			ActiveImpulses.RemoveAtSwap(Index);
			continue;
		}

		ActiveImpulse.Age += DeltaTime;
		const float NormalizedAge = FMath::Clamp(ActiveImpulse.Age / ActiveImpulse.Duration, 0.0f, 1.0f);
		const float StrengthScale = 1.0f - (NormalizedAge * NormalizedAge * (3.0f - 2.0f * NormalizedAge));

		if (NormalizedAge >= 1.0f || StrengthScale <= KINDA_SMALL_NUMBER)
		{
			ActiveImpulses.RemoveAtSwap(Index);
			continue;
		}

		FTimeThiefSmokeInteractionEvent Event = ActiveImpulse.Event;
		Event.NormalizedAge = NormalizedAge;
		Event.Strength *= StrengthScale;

		SmokeVolume->ApplyInteractionEvent(Event);
	}

	PublishRendererFrame(DeltaTime);

	BulletTraceCountsThisTick.Reset();
}

TStatId UTimeThiefSmokeWorldSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UTimeThiefSmokeWorldSubsystem, STATGROUP_Tickables);
}

void UTimeThiefSmokeWorldSubsystem::RegisterSmokeVolume(ATimeThiefSmokeVolume* SmokeVolume)
{
	if (!SmokeVolume)
	{
		return;
	}

	ActiveSmokeVolumes.AddUnique(SmokeVolume);
}

void UTimeThiefSmokeWorldSubsystem::UnregisterSmokeVolume(ATimeThiefSmokeVolume* SmokeVolume)
{
	ActiveSmokeVolumes.Remove(SmokeVolume);

	for (int32 Index = ActiveImpulses.Num() - 1; Index >= 0; --Index)
	{
		if (ActiveImpulses[Index].SmokeVolume.Get() == SmokeVolume)
		{
			ActiveImpulses.RemoveAtSwap(Index);
		}
	}
}

void UTimeThiefSmokeWorldSubsystem::SubmitBulletTrace(const FVector& TraceStart, const FVector& TraceEnd, float Strength, int32 Seed)
{
	if (TraceStart.Equals(TraceEnd))
	{
		return;
	}

	CompactSmokeVolumes();

	for (const TWeakObjectPtr<ATimeThiefSmokeVolume>& SmokeVolumePtr : ActiveSmokeVolumes)
	{
		ATimeThiefSmokeVolume* SmokeVolume = SmokeVolumePtr.Get();
		if (!SmokeVolume)
		{
			continue;
		}

		FVector EntryPoint = FVector::ZeroVector;
		FVector ExitPoint = FVector::ZeroVector;
		if (SmokeVolume->IntersectTraceSegment(TraceStart, TraceEnd, EntryPoint, ExitPoint))
		{
			int32& TraceCount = BulletTraceCountsThisTick.FindOrAdd(SmokeVolume);
			if (TraceCount >= TimeThiefSmoke::MaxBulletTracesPerSmokePerTick)
			{
				continue;
			}

			++TraceCount;
			SmokeVolume->HandleBulletTrace(EntryPoint, ExitPoint, Strength, Seed);
		}
	}
}

void UTimeThiefSmokeWorldSubsystem::SubmitExplosion(const FVector& Center, float Radius, float Strength, int32 Seed)
{
	CompactSmokeVolumes();

	for (const TWeakObjectPtr<ATimeThiefSmokeVolume>& SmokeVolumePtr : ActiveSmokeVolumes)
	{
		ATimeThiefSmokeVolume* SmokeVolume = SmokeVolumePtr.Get();
		if (!SmokeVolume || !SmokeVolume->IntersectsExplosion(Center, Radius))
		{
			continue;
		}

		SmokeVolume->HandleExplosionShock(Center, Radius, Strength, Seed);
	}
}

void UTimeThiefSmokeWorldSubsystem::AddTimedInteractionEvent(ATimeThiefSmokeVolume* SmokeVolume, const FTimeThiefSmokeInteractionEvent& Event, float Duration)
{
	if (!SmokeVolume)
	{
		return;
	}

	SmokeVolume->ApplyInteractionEvent(Event);

	FTimeThiefActiveSmokeImpulse ActiveImpulse;
	ActiveImpulse.SmokeVolume = SmokeVolume;
	ActiveImpulse.Event = Event;
	ActiveImpulse.Age = 0.0f;
	ActiveImpulse.Duration = FMath::Max(0.01f, Duration);
	ActiveImpulses.Add(ActiveImpulse);
}

void UTimeThiefSmokeWorldSubsystem::RecordRendererEvent(const FTimeThiefSmokeInteractionEvent& Event)
{
	PendingRendererEvents.Add(Event);
}

void UTimeThiefSmokeWorldSubsystem::CompactSmokeVolumes()
{
	for (int32 Index = ActiveSmokeVolumes.Num() - 1; Index >= 0; --Index)
	{
		if (!ActiveSmokeVolumes[Index].IsValid())
		{
			ActiveSmokeVolumes.RemoveAtSwap(Index);
		}
	}
}

void UTimeThiefSmokeWorldSubsystem::PublishRendererFrame(float DeltaTime)
{
	if (!GEngine)
	{
		PendingRendererEvents.Reset();
		return;
	}

	UTimeThiefSmokeRendererSubsystem* RendererSubsystem = GEngine->GetEngineSubsystem<UTimeThiefSmokeRendererSubsystem>();
	if (!RendererSubsystem)
	{
		PendingRendererEvents.Reset();
		return;
	}

	FTimeThiefSmokeRendererFrame Frame;
	Frame.DeltaSeconds = DeltaTime;
	Frame.Volumes.Reserve(ActiveSmokeVolumes.Num());
	Frame.Events.Reserve(PendingRendererEvents.Num());

	for (const TWeakObjectPtr<ATimeThiefSmokeVolume>& SmokeVolumePtr : ActiveSmokeVolumes)
	{
		const ATimeThiefSmokeVolume* SmokeVolume = SmokeVolumePtr.Get();
		if (!SmokeVolume)
		{
			continue;
		}

		const FTimeThiefSmokeRuntimeSettings& Settings = SmokeVolume->GetSmokeSettings();
		FTimeThiefSmokeRendererVolume RendererVolume;
		RendererVolume.SmokeId = SmokeVolume->GetSmokeId();
		RendererVolume.LocalToWorld = FTransform3f(SmokeVolume->GetActorTransform());
		RendererVolume.BoundsExtent = FVector3f(SmokeVolume->GetCurrentSmokeBoundsExtent());
		RendererVolume.AgeSeconds = SmokeVolume->GetSmokeAgeSeconds();
		RendererVolume.DurationSeconds = Settings.SmokeDuration;
		RendererVolume.ObstacleMaskResolution = SmokeVolume->GetObstacleMaskResolution();
		RendererVolume.ObstacleMaskRevision = SmokeVolume->GetObstacleMaskRevision();
		RendererVolume.ObstacleMask = SmokeVolume->GetObstacleMask();
		RendererVolume.Settings = TimeThiefSmoke::ToRendererSettings(Settings);
		Frame.Volumes.Add(RendererVolume);
	}

	for (const FTimeThiefSmokeInteractionEvent& Event : PendingRendererEvents)
	{
		Frame.Events.Add(TimeThiefSmoke::ToRendererEvent(Event));
	}

	RendererSubsystem->SubmitFrame(Frame);
	PendingRendererEvents.Reset();
}
