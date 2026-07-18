#include "TimeThiefSmokeTestRecorder.h"

#include "TimeThiefSmokeTestScenario.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
	TSharedPtr<FJsonValueArray> VectorJson(const FVector& Value)
	{
		TArray<TSharedPtr<FJsonValue>> Values;
		Values.Add(MakeShared<FJsonValueNumber>(Value.X));
		Values.Add(MakeShared<FJsonValueNumber>(Value.Y));
		Values.Add(MakeShared<FJsonValueNumber>(Value.Z));
		return MakeShared<FJsonValueArray>(MoveTemp(Values));
	}

	TArray<TSharedPtr<FJsonValue>> IntArrayJson(const TArray<int32>& Values)
	{
		TArray<TSharedPtr<FJsonValue>> Result;
		Result.Reserve(Values.Num());
		for (const int32 Value : Values)
		{
			Result.Add(MakeShared<FJsonValueNumber>(Value));
		}
		return Result;
	}

	FString JsonString(const TSharedRef<FJsonObject>& Object, bool bPretty = false)
	{
		FString Result;
		if (bPretty)
		{
			const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Result);
			FJsonSerializer::Serialize(Object, Writer);
		}
		else
		{
			const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Result);
			FJsonSerializer::Serialize(Object, Writer);
		}
		return Result;
	}

	double Percentile(TArray<double> Values, double Fraction)
	{
		if (Values.IsEmpty())
		{
			return 0.0;
		}
		Values.Sort();
		const double Index = FMath::Clamp(Fraction, 0.0, 1.0) * (Values.Num() - 1);
		const int32 Lower = FMath::FloorToInt(Index);
		const int32 Upper = FMath::CeilToInt(Index);
		return FMath::Lerp(Values[Lower], Values[Upper], Index - Lower);
	}

	bool ProbeMetric(const FTimeThiefSmokeTestProbeResult& Result, const FString& Name, double& OutValue)
	{
		if (Name == TEXT("natural_density_sum")) OutValue = Result.NaturalDensitySum;
		else if (Name == TEXT("displaced_density_sum") || Name == TEXT("density_sum")) OutValue = Result.DisplacedDensitySum;
		else if (Name == TEXT("max_velocity")) OutValue = Result.MaxVelocity;
		else if (Name == TEXT("bullet_cutout_max")) OutValue = Result.BulletCutoutMax;
		else if (Name == TEXT("bullet_sink_max")) OutValue = Result.BulletSinkMax;
		else if (Name == TEXT("active_density_voxels")) OutValue = Result.ActiveDensityVoxels;
		else if (Name == TEXT("active_bullet_voxels")) OutValue = Result.ActiveBulletVoxels;
		else if (Name == TEXT("density_inside_obstacle")) OutValue = Result.DensityInsideObstacle;
		else if (Name == TEXT("max_natural_density")) OutValue = Result.MaxNaturalDensity;
		else if (Name == TEXT("max_displaced_density")) OutValue = Result.MaxDisplacedDensity;
		else if (Name == TEXT("max_combined_density")) OutValue = Result.MaxCombinedDensity;
		else if (Name == TEXT("density_clamp_violation_voxels")) OutValue = Result.DensityClampViolationVoxels;
		else if (Name == TEXT("solid_obstacle_voxels")) OutValue = Result.SolidObstacleVoxels;
		else return false;
		return true;
	}

	bool PhasePassMetric(const FTimeThiefSmokeTestPhasePassSamples& Samples, const FString& Name, double& OutValue)
	{
		if (Name == TEXT("count")) OutValue = Samples.Durations.Num();
		else if (Name == TEXT("median_ms")) OutValue = Percentile(Samples.Durations, 0.5);
		else if (Name == TEXT("p95_ms")) OutValue = Percentile(Samples.Durations, 0.95);
		else if (Name == TEXT("draw_fraction_median") && !Samples.DrawFractions.IsEmpty()) OutValue = Percentile(Samples.DrawFractions, 0.5);
		else if (Name == TEXT("camera_inside_count_median") && !Samples.CameraInsideCounts.IsEmpty()) OutValue = Percentile(Samples.CameraInsideCounts, 0.5);
		else if (Name == TEXT("surface_distance_median") && !Samples.SurfaceDistances.IsEmpty()) OutValue = Percentile(Samples.SurfaceDistances, 0.5);
		else return false;
		return true;
	}
}

FTimeThiefSmokeTestRecorder::~FTimeThiefSmokeTestRecorder()
{
	EventsArchive.Reset();
	GpuArchive.Reset();
	ProbesArchive.Reset();
	TimelineArchive.Reset();
}

bool FTimeThiefSmokeTestRecorder::Initialize(const FString& InOutputDirectory, FString& OutError)
{
	OutputDirectory = InOutputDirectory;
	if (!IFileManager::Get().MakeDirectory(*OutputDirectory, true))
	{
		OutError = FString::Printf(TEXT("Cannot create output directory: %s"), *OutputDirectory);
		return false;
	}

	EventsArchive.Reset(IFileManager::Get().CreateFileWriter(*FPaths::Combine(OutputDirectory, TEXT("events.jsonl"))));
	GpuArchive.Reset(IFileManager::Get().CreateFileWriter(*FPaths::Combine(OutputDirectory, TEXT("gpu_passes.jsonl"))));
	ProbesArchive.Reset(IFileManager::Get().CreateFileWriter(*FPaths::Combine(OutputDirectory, TEXT("probes.jsonl"))));
	TimelineArchive.Reset(IFileManager::Get().CreateFileWriter(*FPaths::Combine(OutputDirectory, TEXT("timeline.log"))));
	if (!EventsArchive || !GpuArchive || !ProbesArchive || !TimelineArchive)
	{
		OutError = FString::Printf(TEXT("Cannot open output files in: %s"), *OutputDirectory);
		return false;
	}
	return true;
}

void FTimeThiefSmokeTestRecorder::SetScenarioLoaded(int32 ActionCount)
{
	Execution.bScenarioLoaded = true;
	Execution.ActionsRequested = ActionCount;
}

void FTimeThiefSmokeTestRecorder::EnqueueEvent(const FTimeThiefSmokeTestEvent& Event)
{
	FPendingRecord Record;
	Record.Type = ERecordType::Event;
	Record.Event = Event;
	PendingRecords.Enqueue(MoveTemp(Record));
}

void FTimeThiefSmokeTestRecorder::EnqueueGpuPass(const FTimeThiefSmokeTestGpuPassResult& Result)
{
	FPendingRecord Record;
	Record.Type = ERecordType::GpuPass;
	Record.GpuPass = Result;
	PendingRecords.Enqueue(MoveTemp(Record));
}

void FTimeThiefSmokeTestRecorder::EnqueueProbe(const FTimeThiefSmokeTestProbeResult& Result)
{
	FPendingRecord Record;
	Record.Type = ERecordType::Probe;
	Record.Probe = Result;
	PendingRecords.Enqueue(MoveTemp(Record));
}

void FTimeThiefSmokeTestRecorder::Drain(double ScenarioTimeSeconds)
{
	FPendingRecord Record;
	while (PendingRecords.Dequeue(Record))
	{
		switch (Record.Type)
		{
		case ERecordType::Event: WriteEvent(Record.Event, ScenarioTimeSeconds); break;
		case ERecordType::GpuPass: WriteGpuPass(Record.GpuPass, ScenarioTimeSeconds); break;
		case ERecordType::Probe: WriteProbe(Record.Probe, ScenarioTimeSeconds); break;
		}
	}
	if (EventsArchive) EventsArchive->Flush();
	if (GpuArchive) GpuArchive->Flush();
	if (ProbesArchive) ProbesArchive->Flush();
	if (TimelineArchive) TimelineArchive->Flush();
}

bool FTimeThiefSmokeTestRecorder::IsEmpty() const
{
	return PendingRecords.IsEmpty();
}

void FTimeThiefSmokeTestRecorder::WriteLine(FArchive* Archive, const FString& Line)
{
	if (!Archive)
	{
		return;
	}
	FTCHARToUTF8 Utf8(*(Line + LINE_TERMINATOR));
	Archive->Serialize(const_cast<void*>(static_cast<const void*>(Utf8.Get())), Utf8.Length());
}

void FTimeThiefSmokeTestRecorder::WriteEvent(const FTimeThiefSmokeTestEvent& Event, double TimeSeconds)
{
	const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetNumberField(TEXT("sequence"), ++Sequence);
	Json->SetNumberField(TEXT("time"), TimeSeconds);
	Json->SetNumberField(TEXT("frame"), Event.FrameId != 0 ? Event.FrameId : GFrameCounter);
	Json->SetStringField(TEXT("type"), Event.Type);
	if (!Event.ActionId.IsEmpty()) Json->SetStringField(TEXT("action"), Event.ActionId);
	if (!Event.EntityId.IsEmpty()) Json->SetStringField(TEXT("entity"), Event.EntityId);
	if (!Event.Label.IsEmpty()) Json->SetStringField(TEXT("label"), Event.Label);
	if (!Event.Shape.IsEmpty()) Json->SetStringField(TEXT("shape"), Event.Shape);
	if (!Event.ActorName.IsEmpty()) Json->SetStringField(TEXT("actor"), Event.ActorName);
	if (!Event.ComponentName.IsEmpty()) Json->SetStringField(TEXT("component"), Event.ComponentName);
	if (Event.SmokeId != INDEX_NONE) Json->SetNumberField(TEXT("smoke"), Event.SmokeId);
	if (Event.ItemIndex != INDEX_NONE) Json->SetNumberField(TEXT("item"), Event.ItemIndex);
	if (Event.Count != 0) Json->SetNumberField(TEXT("count"), Event.Count);
	if (Event.Seed != 0) Json->SetNumberField(TEXT("seed"), Event.Seed);
	if (!Event.Start.IsNearlyZero()) Json->SetField(TEXT("start"), VectorJson(Event.Start));
	if (!Event.End.IsNearlyZero()) Json->SetField(TEXT("end"), VectorJson(Event.End));
	if (!Event.Entry.IsNearlyZero()) Json->SetField(TEXT("entry"), VectorJson(Event.Entry));
	if (!Event.Exit.IsNearlyZero()) Json->SetField(TEXT("exit"), VectorJson(Event.Exit));
	if (!Event.Position.IsNearlyZero()) Json->SetField(TEXT("position"), VectorJson(Event.Position));
	if (!Event.PreviousPosition.IsNearlyZero()) Json->SetField(TEXT("previous_position"), VectorJson(Event.PreviousPosition));
	if (!Event.Direction.IsNearlyZero()) Json->SetField(TEXT("direction"), VectorJson(Event.Direction));
	if (!Event.Extents.IsNearlyZero()) Json->SetField(TEXT("extents"), VectorJson(Event.Extents));
	if (Event.Radius != 0.0) Json->SetNumberField(TEXT("radius"), Event.Radius);
	if (Event.Length != 0.0) Json->SetNumberField(TEXT("length"), Event.Length);
	if (Event.Strength != 0.0) Json->SetNumberField(TEXT("strength"), Event.Strength);
	if (Event.Speed != 0.0) Json->SetNumberField(TEXT("speed"), Event.Speed);
	if (!Event.SmokeIds.IsEmpty()) Json->SetArrayField(TEXT("smoke_ids"), IntArrayJson(Event.SmokeIds));
	WriteLine(EventsArchive.Get(), JsonString(Json));
	WriteLine(TimelineArchive.Get(), FString::Printf(TEXT("[%07.3fs] %s action=%s entity=%s smoke=%d"), TimeSeconds, *Event.Type, *Event.ActionId, *Event.EntityId, Event.SmokeId));

	if (Event.Type == TEXT("action_completed")) ++Execution.ActionsCompleted;
	else if (Event.Type == TEXT("action_failed")) ++Execution.ActionsFailed;
	else if (Event.Type == TEXT("smoke_detonated")) ++Execution.SmokesRequested;
	else if (Event.Type == TEXT("smoke_spawned")) ++Execution.SmokesSpawned;
	else if (Event.Type == TEXT("smoke_registered"))
	{
		++Execution.SmokesRegistered;
		if (Event.SmokeId != INDEX_NONE) RegisteredSmokeIds.Add(Event.SmokeId);
	}
	else if (Event.Type == TEXT("renderer_frame_received")) ++Execution.RendererFramesReceived;
	else if (Event.Type == TEXT("probe_requested")) Execution.ProbesRequested += FMath::Max(1, Event.SmokeIds.Num());
	else if (Event.Type == TEXT("bullet_fired")) ++Situation.BulletsFired;
	else if (Event.Type == TEXT("bullet_entered_smoke")) ++Situation.BulletIntersections;
	else if (Event.Type == TEXT("bullet_event_accepted")) ++Situation.BulletEventsAccepted;
	else if (Event.Type == TEXT("bullet_event_rejected")) ++Situation.BulletEventsRejected;
	else if (Event.Type == TEXT("explosion_submitted")) ++Situation.Explosions;
	else if (Event.Type == TEXT("explosion_intersected_smoke")) ++Situation.ExplosionAffectedSmokes;
	else if (Event.Type == TEXT("actor_push_event_queued"))
	{
		++Situation.ActorPushEvents;
		if (Event.Shape == TEXT("sphere")) ++Situation.ActorSpherePushEvents;
		else if (Event.Shape == TEXT("capsule")) ++Situation.ActorCapsulePushEvents;
		else if (Event.Shape == TEXT("box")) ++Situation.ActorBoxPushEvents;
	}
	else if (Event.Type == TEXT("missing_smoke_id")) ++Situation.MissingSmokeIds;
	else if (Event.Type == TEXT("renderer_clear_frame_submitted")) ++Situation.RendererClearFrames;
	else if (Event.Type == TEXT("bullet_fields_activated")) ++Situation.BulletFieldActivations;
	else if (Event.Type == TEXT("bullet_fields_cleared")) ++Situation.BulletFieldClears;
}

void FTimeThiefSmokeTestRecorder::WriteGpuPass(const FTimeThiefSmokeTestGpuPassResult& Result, double TimeSeconds)
{
	const FString Pass = Result.PassName.ToString();
	const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetNumberField(TEXT("time"), TimeSeconds);
	Json->SetNumberField(TEXT("frame"), Result.FrameId);
	Json->SetStringField(TEXT("phase"), Result.Phase);
	Json->SetStringField(TEXT("pass"), Pass);
	if (Result.SmokeId != INDEX_NONE) Json->SetNumberField(TEXT("smoke"), Result.SmokeId);
	if (Result.BatchIndex != INDEX_NONE) Json->SetNumberField(TEXT("batch_index"), Result.BatchIndex);
	if (Result.BatchCount > 0) Json->SetNumberField(TEXT("batch_count"), Result.BatchCount);
	if (Result.IterationIndex != INDEX_NONE) Json->SetNumberField(TEXT("iteration"), Result.IterationIndex);
	if (Result.EventCount != 0) Json->SetNumberField(TEXT("event_count"), Result.EventCount);
	if (Result.SmokeCount != 0) Json->SetNumberField(TEXT("smoke_count"), Result.SmokeCount);
	if (Result.BulletEventCount != 0) Json->SetNumberField(TEXT("bullet_events"), Result.BulletEventCount);
	if (Result.ExplosionEventCount != 0) Json->SetNumberField(TEXT("explosion_events"), Result.ExplosionEventCount);
	if (Result.ActorEventCount != 0) Json->SetNumberField(TEXT("actor_events"), Result.ActorEventCount);
	if (Result.BrickEventBitCount != INDEX_NONE) Json->SetNumberField(TEXT("brick_event_bit_count"), Result.BrickEventBitCount);
	if (Result.VisitedEventCount != INDEX_NONE) Json->SetNumberField(TEXT("visited_event_count"), Result.VisitedEventCount);
	if (Result.SkippedEventCount != INDEX_NONE) Json->SetNumberField(TEXT("skipped_event_count"), Result.SkippedEventCount);
	if (Result.VortexParticleCount != INDEX_NONE) Json->SetNumberField(TEXT("vortex_particle_count"), Result.VortexParticleCount);
	if (Result.VortexBrickCount != INDEX_NONE) Json->SetNumberField(TEXT("vortex_brick_count"), Result.VortexBrickCount);
	if (Result.VortexParticleBrickPairs != INDEX_NONE) Json->SetNumberField(TEXT("vortex_particle_brick_pairs"), Result.VortexParticleBrickPairs);
	if (!Result.ObstacleStencilMode.IsEmpty()) Json->SetStringField(TEXT("obstacle_stencil_mode"), Result.ObstacleStencilMode);
	if (Result.FaceOpenSampleCount != INDEX_NONE) Json->SetNumberField(TEXT("face_open_sample_count"), Result.FaceOpenSampleCount);
	Json->SetBoolField(TEXT("pass_executed"), Result.bPassExecuted);
	if (Result.DispatchGroupCount != INDEX_NONE) Json->SetNumberField(TEXT("dispatch_group_count"), Result.DispatchGroupCount);
	if (Result.ViewportPixelCount > 0)
	{
		Json->SetNumberField(TEXT("draw_pixels"), Result.DrawPixelCount);
		Json->SetNumberField(TEXT("viewport_pixels"), Result.ViewportPixelCount);
		Json->SetNumberField(TEXT("draw_fraction"), static_cast<double>(Result.DrawPixelCount) / Result.ViewportPixelCount);
	}
	if (Result.DrawRect.Area() > 0)
	{
		Json->SetNumberField(TEXT("draw_rect_min_x"), Result.DrawRect.Min.X);
		Json->SetNumberField(TEXT("draw_rect_min_y"), Result.DrawRect.Min.Y);
		Json->SetNumberField(TEXT("draw_rect_max_x"), Result.DrawRect.Max.X);
		Json->SetNumberField(TEXT("draw_rect_max_y"), Result.DrawRect.Max.Y);
	}
	if (Result.TileCount > 0)
	{
		Json->SetNumberField(TEXT("tile_count"), Result.TileCount);
		Json->SetNumberField(TEXT("empty_tile_count"), Result.EmptyTileCount);
		Json->SetNumberField(TEXT("max_smokes_per_tile"), Result.MaxSmokesPerTile);
		Json->SetNumberField(TEXT("average_smokes_per_nonempty_tile"), Result.AverageSmokesPerNonEmptyTile);
		Json->SetArrayField(TEXT("tile_smoke_count_histogram"), IntArrayJson(Result.TileSmokeCountHistogram));
	}
	if (Result.EstimatedFullRaySteps > 0)
	{
		Json->SetNumberField(TEXT("estimated_full_ray_steps"), Result.EstimatedFullRaySteps);
		Json->SetNumberField(TEXT("target_step_length"), Result.TargetStepLength);
		if (Result.ActualResolvedStepMax > 0)
		{
			Json->SetNumberField(TEXT("actual_resolved_step_min"), Result.ActualResolvedStepMin);
			Json->SetNumberField(TEXT("actual_resolved_step_max"), Result.ActualResolvedStepMax);
			Json->SetNumberField(TEXT("actual_resolved_step_average"), Result.ActualResolvedStepAverage);
			Json->SetNumberField(TEXT("actual_executed_step_min"), Result.ActualExecutedStepMin);
			Json->SetNumberField(TEXT("actual_executed_step_max"), Result.ActualExecutedStepMax);
			Json->SetNumberField(TEXT("actual_executed_step_average"), Result.ActualExecutedStepAverage);
		}
		if (!Result.SampleGridMode.IsEmpty()) Json->SetStringField(TEXT("sample_grid_mode"), Result.SampleGridMode);
		if (Result.WorldStepLength > 0.0f) Json->SetNumberField(TEXT("world_step_length"), Result.WorldStepLength);
		Json->SetNumberField(TEXT("sample_phase_hash"), Result.SamplePhaseHash);
		Json->SetNumberField(TEXT("segment_count"), Result.SegmentCount);
		Json->SetNumberField(TEXT("stable_sample_count"), Result.StableSampleCount);
		Json->SetNumberField(TEXT("sparse_skip_step_count"), Result.SparseSkipStepCount);
		Json->SetNumberField(TEXT("combined_medium_sample_count"), Result.CombinedMediumSampleCount);
		Json->SetNumberField(TEXT("combined_shadow_evaluation_count"), Result.CombinedShadowEvaluationCount);
		Json->SetNumberField(TEXT("filtered_noise_octave_count"), Result.FilteredNoiseOctaveCount);
		Json->SetNumberField(TEXT("boundary_noise_octave_count"), Result.BoundaryNoiseOctaveCount);
		Json->SetNumberField(TEXT("density_noise_octave_count"), Result.DensityNoiseOctaveCount);
		Json->SetNumberField(TEXT("filament_noise_octave_count"), Result.FilamentNoiseOctaveCount);
		Json->SetNumberField(TEXT("shadow_noise_octave_count"), Result.ShadowNoiseOctaveCount);
		Json->SetNumberField(TEXT("combined_shadow_step_sample_count"), Result.CombinedShadowStepSampleCount);
		Json->SetNumberField(TEXT("combined_shadow_medium_sample_count"), Result.CombinedShadowMediumSampleCount);
		Json->SetNumberField(TEXT("boundary_evaluation_count"), Result.BoundaryEvaluationCount);
		Json->SetNumberField(TEXT("boundary_coarse_noise_evaluation_count"), Result.BoundaryCoarseNoiseEvaluationCount);
		Json->SetNumberField(TEXT("boundary_fine_noise_evaluation_count"), Result.BoundaryFineNoiseEvaluationCount);
		Json->SetNumberField(TEXT("boundary_interior_skip_count"), Result.BoundaryInteriorSkipCount);
		Json->SetNumberField(TEXT("boundary_exterior_reject_count"), Result.BoundaryExteriorRejectCount);
		Json->SetNumberField(TEXT("shadow_interval_skip_count"), Result.ShadowIntervalSkipCount);
		Json->SetNumberField(TEXT("shadow_optical_depth_early_out_count"), Result.ShadowOpticalDepthEarlyOutCount);
		Json->SetNumberField(TEXT("shadow_adaptive_double_step_count"), Result.ShadowAdaptiveDoubleStepCount);
		Json->SetNumberField(TEXT("shadow_ellipsoid_reject_count"), Result.ShadowEllipsoidRejectCount);
		Json->SetNumberField(TEXT("combined_shadow_step_count"), Result.CombinedShadowStepCount);
		Json->SetNumberField(TEXT("combined_shadow_step_length"), Result.CombinedShadowStepLength);
		Json->SetBoolField(TEXT("order_independent_integrator"), Result.bOrderIndependentIntegrator);
		Json->SetNumberField(TEXT("sparse_smoke_count"), Result.SparseSmokeCount);
		Json->SetNumberField(TEXT("packed_dense_smoke_count"), Result.PackedDenseSmokeCount);
		Json->SetNumberField(TEXT("dense_smoke_count"), FMath::Max(Result.SmokeCount - Result.SparseSmokeCount, 0));
		Json->SetNumberField(TEXT("bullet_field_active_smoke_count"), Result.BulletFieldActiveSmokeCount);
		Json->SetBoolField(TEXT("half_resolution"), Result.bHalfResolution);
		Json->SetBoolField(TEXT("fast_filament"), Result.bFastFilament);
		Json->SetBoolField(TEXT("boundary_shell_gate"), Result.bBoundaryShellGate);
		Json->SetBoolField(TEXT("single_smoke_shader"), Result.bSingleSmokeShader);
	}
	if (Result.CameraInsideSmokeCount != INDEX_NONE) Json->SetNumberField(TEXT("camera_inside_smokes"), Result.CameraInsideSmokeCount);
	if (Result.NearestSmokeSurfaceDistance >= 0.0f) Json->SetNumberField(TEXT("nearest_smoke_surface_distance"), Result.NearestSmokeSurfaceDistance);
	if (!Result.SmokeIds.IsEmpty()) Json->SetArrayField(TEXT("smoke_ids"), IntArrayJson(Result.SmokeIds));
	Json->SetNumberField(TEXT("duration_ms"), Result.DurationMilliseconds);
	WriteLine(GpuArchive.Get(), JsonString(Json));
	++Execution.GpuPassSamples;
	if (Pass == TEXT("Composite.Raymarch")) ++Situation.CompositeBatches;
	if (Pass == TEXT("Composite.Raymarch"))
	{
		TSet<int32> PassSmokeIds;
		for (const int32 SmokeId : Result.SmokeIds)
		{
			if (PassSmokeIds.Contains(SmokeId)) ++DuplicateRenderedSmokeIds;
			PassSmokeIds.Add(SmokeId);
			RenderedSmokeIds.Add(SmokeId);
		}
	}
	PassDurations.FindOrAdd(Pass).Add(Result.DurationMilliseconds);
	FTimeThiefSmokeTestPhasePassSamples& PhaseSamples = PhasePassSamples.FindOrAdd(Result.Phase).FindOrAdd(Pass);
	PhaseSamples.Durations.Add(Result.DurationMilliseconds);
	if (Result.ViewportPixelCount > 0) PhaseSamples.DrawFractions.Add(static_cast<double>(Result.DrawPixelCount) / Result.ViewportPixelCount);
	if (Result.CameraInsideSmokeCount != INDEX_NONE) PhaseSamples.CameraInsideCounts.Add(Result.CameraInsideSmokeCount);
	if (Result.NearestSmokeSurfaceDistance >= 0.0f) PhaseSamples.SurfaceDistances.Add(Result.NearestSmokeSurfaceDistance);
	ExecutedPasses.Add(Pass);
	if (Pass.StartsWith(TEXT("Pressure.")) && Pass != TEXT("Pressure.Total"))
	{
		const uint64 Key = (Result.FrameId << 32) ^ static_cast<uint32>(Result.SmokeId);
		PressureDurationByFrameAndSmoke.FindOrAdd(Key) += Result.DurationMilliseconds;
	}
}

void FTimeThiefSmokeTestRecorder::WriteProbe(const FTimeThiefSmokeTestProbeResult& Result, double TimeSeconds)
{
	const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetNumberField(TEXT("time"), TimeSeconds);
	Json->SetNumberField(TEXT("request_id"), Result.RequestId);
	Json->SetStringField(TEXT("label"), Result.Label);
	Json->SetNumberField(TEXT("smoke"), Result.SmokeId);
	Json->SetNumberField(TEXT("natural_density_sum"), Result.NaturalDensitySum);
	Json->SetNumberField(TEXT("displaced_density_sum"), Result.DisplacedDensitySum);
	Json->SetField(TEXT("density_centroid"), VectorJson(Result.DensityCentroid));
	Json->SetNumberField(TEXT("max_velocity"), Result.MaxVelocity);
	Json->SetNumberField(TEXT("bullet_cutout_max"), Result.BulletCutoutMax);
	Json->SetNumberField(TEXT("bullet_sink_max"), Result.BulletSinkMax);
	Json->SetNumberField(TEXT("active_density_voxels"), Result.ActiveDensityVoxels);
	Json->SetNumberField(TEXT("active_bullet_voxels"), Result.ActiveBulletVoxels);
	Json->SetNumberField(TEXT("density_inside_obstacle"), Result.DensityInsideObstacle);
	Json->SetNumberField(TEXT("max_natural_density"), Result.MaxNaturalDensity);
	Json->SetNumberField(TEXT("max_displaced_density"), Result.MaxDisplacedDensity);
	Json->SetNumberField(TEXT("max_combined_density"), Result.MaxCombinedDensity);
	Json->SetNumberField(TEXT("density_clamp_violation_voxels"), Result.DensityClampViolationVoxels);
	Json->SetNumberField(TEXT("solid_obstacle_voxels"), Result.SolidObstacleVoxels);
	WriteLine(ProbesArchive.Get(), JsonString(Json));
	++Execution.ProbesCompleted;
	ProbeResults.FindOrAdd(Result.Label).Add(Result.SmokeId, Result);
}

bool FTimeThiefSmokeTestRecorder::WriteResult(const FTimeThiefSmokeTestScenario& Scenario, const TArray<FString>& ExecutionErrors, FString& OutError)
{
	Drain(Scenario.DurationSeconds);
	if (!PressureDurationByFrameAndSmoke.IsEmpty())
	{
		TArray<double>& PressureTotals = PassDurations.FindOrAdd(TEXT("Pressure.Total"));
		PressureDurationByFrameAndSmoke.GenerateValueArray(PressureTotals);
		ExecutedPasses.Add(TEXT("Pressure.Total"));
	}
	EventsArchive.Reset();
	GpuArchive.Reset();
	ProbesArchive.Reset();
	TimelineArchive.Reset();

	TArray<FString> ValidityFailures = ExecutionErrors;
	if (!Execution.bScenarioLoaded) ValidityFailures.Add(TEXT("scenario_not_loaded"));
	if (Execution.ActionsCompleted + Execution.ActionsFailed != Execution.ActionsRequested) ValidityFailures.Add(TEXT("actions_incomplete"));
	if (Execution.ActionsFailed > 0) ValidityFailures.Add(TEXT("action_execution_failed"));
	if (Execution.SmokesSpawned != Execution.SmokesRequested) ValidityFailures.Add(TEXT("smoke_spawn_count_mismatch"));
	if (Execution.SmokesRegistered != Execution.SmokesSpawned) ValidityFailures.Add(TEXT("smoke_registration_count_mismatch"));
	if (Execution.SmokesSpawned > 0 && Execution.RendererFramesReceived == 0) ValidityFailures.Add(TEXT("renderer_frame_not_received"));
	if (Execution.ProbesCompleted != Execution.ProbesRequested) ValidityFailures.Add(TEXT("probe_incomplete"));

	TArray<FString> ExpectationFailures;
	auto CheckCounter = [&ExpectationFailures, &Scenario](const TCHAR* Name, int32 Actual)
	{
		double Expected = 0.0;
		if (Scenario.Expectations->TryGetNumberField(Name, Expected) && Actual != static_cast<int32>(Expected))
		{
			ExpectationFailures.Add(FString::Printf(TEXT("%s expected %.0f, got %d"), Name, Expected, Actual));
		}
		const FString MinName = FString(Name) + TEXT("_min");
		if (Scenario.Expectations->TryGetNumberField(MinName, Expected) && Actual < static_cast<int32>(Expected))
		{
			ExpectationFailures.Add(FString::Printf(TEXT("%s expected >= %.0f, got %d"), Name, Expected, Actual));
		}
		const FString MaxName = FString(Name) + TEXT("_max");
		if (Scenario.Expectations->TryGetNumberField(MaxName, Expected) && Actual > static_cast<int32>(Expected))
		{
			ExpectationFailures.Add(FString::Printf(TEXT("%s expected <= %.0f, got %d"), Name, Expected, Actual));
		}
	};
	CheckCounter(TEXT("smokes_spawned"), Execution.SmokesSpawned);
	CheckCounter(TEXT("smokes_registered"), Execution.SmokesRegistered);
	CheckCounter(TEXT("bullets_fired"), Situation.BulletsFired);
	CheckCounter(TEXT("bullet_intersections"), Situation.BulletIntersections);
	CheckCounter(TEXT("accepted_bullet_events"), Situation.BulletEventsAccepted);
	CheckCounter(TEXT("rejected_bullet_events"), Situation.BulletEventsRejected);
	CheckCounter(TEXT("explosions"), Situation.Explosions);
	CheckCounter(TEXT("explosion_affected_smokes"), Situation.ExplosionAffectedSmokes);
	CheckCounter(TEXT("actor_push_events"), Situation.ActorPushEvents);
	CheckCounter(TEXT("actor_sphere_push_events"), Situation.ActorSpherePushEvents);
	CheckCounter(TEXT("actor_capsule_push_events"), Situation.ActorCapsulePushEvents);
	CheckCounter(TEXT("actor_box_push_events"), Situation.ActorBoxPushEvents);
	CheckCounter(TEXT("composite_batches"), Situation.CompositeBatches);
	CheckCounter(TEXT("missing_smoke_ids"), Situation.MissingSmokeIds);
	CheckCounter(TEXT("renderer_clear_frames"), Situation.RendererClearFrames);
	CheckCounter(TEXT("bullet_field_activations"), Situation.BulletFieldActivations);
	CheckCounter(TEXT("bullet_field_clears"), Situation.BulletFieldClears);
	CheckCounter(TEXT("rendered_unique_smokes"), RenderedSmokeIds.Num());
	CheckCounter(TEXT("duplicate_rendered_smoke_ids"), DuplicateRenderedSmokeIds);
	int32 UnrenderedRegisteredSmokeIds = 0;
	for (const int32 SmokeId : RegisteredSmokeIds)
	{
		UnrenderedRegisteredSmokeIds += RenderedSmokeIds.Contains(SmokeId) ? 0 : 1;
	}
	CheckCounter(TEXT("unrendered_registered_smokes"), UnrenderedRegisteredSmokeIds);

	const TArray<TSharedPtr<FJsonValue>>* RequiredPasses = nullptr;
	if (Scenario.Expectations->TryGetArrayField(TEXT("pass_executed"), RequiredPasses))
	{
		for (const TSharedPtr<FJsonValue>& Value : *RequiredPasses)
		{
			const FString Name = Value->AsString();
			if (!ExecutedPasses.Contains(Name)) ExpectationFailures.Add(FString::Printf(TEXT("pass not executed: %s"), *Name));
		}
	}
	const TArray<TSharedPtr<FJsonValue>>* SkippedPasses = nullptr;
	if (Scenario.Expectations->TryGetArrayField(TEXT("pass_skipped"), SkippedPasses))
	{
		for (const TSharedPtr<FJsonValue>& Value : *SkippedPasses)
		{
			const FString Name = Value->AsString();
			if (ExecutedPasses.Contains(Name)) ExpectationFailures.Add(FString::Printf(TEXT("pass unexpectedly executed: %s"), *Name));
		}
	}

	const TSharedPtr<FJsonObject>* ProbeExpectations = nullptr;
	if (Scenario.Expectations->TryGetObjectField(TEXT("probe"), ProbeExpectations) && ProbeExpectations && ProbeExpectations->IsValid())
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& LabelPair : (*ProbeExpectations)->Values)
		{
			const TSharedPtr<FJsonObject> Metrics = LabelPair.Value->AsObject();
			const TMap<int32, FTimeThiefSmokeTestProbeResult>* Results = ProbeResults.Find(LabelPair.Key);
			if (!Metrics.IsValid() || !Results || Results->IsEmpty())
			{
				ExpectationFailures.Add(FString::Printf(TEXT("probe result missing: %s"), *LabelPair.Key));
				continue;
			}
			for (const TPair<FString, TSharedPtr<FJsonValue>>& MetricPair : Metrics->Values)
			{
				const bool bMin = MetricPair.Key.EndsWith(TEXT("_min"));
				const bool bMax = MetricPair.Key.EndsWith(TEXT("_max"));
				const FString MetricName = MetricPair.Key.LeftChop(bMin || bMax ? 4 : 0);
				double Aggregate = bMin ? TNumericLimits<double>::Max() : TNumericLimits<double>::Lowest();
				bool bKnown = false;
				for (const TPair<int32, FTimeThiefSmokeTestProbeResult>& ResultPair : *Results)
				{
					double Value = 0.0;
					if (ProbeMetric(ResultPair.Value, MetricName, Value))
					{
						bKnown = true;
						Aggregate = bMin ? FMath::Min(Aggregate, Value) : FMath::Max(Aggregate, Value);
					}
				}
				if (!bKnown || (!bMin && !bMax))
				{
					ExpectationFailures.Add(FString::Printf(TEXT("unsupported probe expectation: %s.%s"), *LabelPair.Key, *MetricPair.Key));
					continue;
				}
				const double Limit = MetricPair.Value->AsNumber();
				if ((bMin && Aggregate < Limit) || (bMax && Aggregate > Limit))
				{
					ExpectationFailures.Add(FString::Printf(TEXT("probe %s.%s expected %s %.6f, got %.6f"), *LabelPair.Key, *MetricName, bMin ? TEXT(">=") : TEXT("<="), Limit, Aggregate));
				}
			}
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* ProbeComparisons = nullptr;
	if (Scenario.Expectations->TryGetArrayField(TEXT("probe_compare"), ProbeComparisons))
	{
		auto ReadProbeMaximum = [this](const FString& Label, const FString& Metric, double& OutValue)
		{
			const TMap<int32, FTimeThiefSmokeTestProbeResult>* Results = ProbeResults.Find(Label);
			if (!Results || Results->IsEmpty()) return false;
			OutValue = TNumericLimits<double>::Lowest();
			bool bFound = false;
			for (const TPair<int32, FTimeThiefSmokeTestProbeResult>& Pair : *Results)
			{
				double Value = 0.0;
				if (ProbeMetric(Pair.Value, Metric, Value))
				{
					OutValue = FMath::Max(OutValue, Value);
					bFound = true;
				}
			}
			return bFound;
		};

		for (const TSharedPtr<FJsonValue>& Value : *ProbeComparisons)
		{
			const TSharedPtr<FJsonObject> Comparison = Value->AsObject();
			FString LeftLabel;
			FString RightLabel;
			FString Metric;
			FString Operator;
			if (!Comparison.IsValid() ||
				!Comparison->TryGetStringField(TEXT("left"), LeftLabel) ||
				!Comparison->TryGetStringField(TEXT("right"), RightLabel) ||
				!Comparison->TryGetStringField(TEXT("metric"), Metric))
			{
				ExpectationFailures.Add(TEXT("invalid probe_compare entry"));
				continue;
			}
			if (!Comparison->TryGetStringField(TEXT("op"), Operator)) Operator = TEXT("gt");
			double MinDelta = 0.0;
			Comparison->TryGetNumberField(TEXT("min_delta"), MinDelta);
			double LeftValue = 0.0;
			double RightValue = 0.0;
			if (!ReadProbeMaximum(LeftLabel, Metric, LeftValue) || !ReadProbeMaximum(RightLabel, Metric, RightValue))
			{
				ExpectationFailures.Add(FString::Printf(TEXT("probe comparison metric missing: %s.%s or %s.%s"), *LeftLabel, *Metric, *RightLabel, *Metric));
				continue;
			}
			const double Difference = LeftValue - RightValue;
			const bool bPassed =
				(Operator == TEXT("gt") && Difference > MinDelta) ||
				(Operator == TEXT("ge") && Difference >= MinDelta) ||
				(Operator == TEXT("lt") && Difference < -MinDelta) ||
				(Operator == TEXT("le") && Difference <= -MinDelta);
			if (!bPassed)
			{
				ExpectationFailures.Add(FString::Printf(TEXT("probe comparison %s.%s %s %s.%s with delta %.6f failed: %.6f vs %.6f"), *LeftLabel, *Metric, *Operator, *RightLabel, *Metric, MinDelta, LeftValue, RightValue));
			}
		}
	}

	const TSharedPtr<FJsonObject>* PhaseExpectations = nullptr;
	if (Scenario.Expectations->TryGetObjectField(TEXT("phase_pass"), PhaseExpectations) && PhaseExpectations && PhaseExpectations->IsValid())
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& PhasePair : (*PhaseExpectations)->Values)
		{
			const TSharedPtr<FJsonObject> ExpectedPasses = PhasePair.Value->AsObject();
			const TMap<FString, FTimeThiefSmokeTestPhasePassSamples>* ActualPasses = PhasePassSamples.Find(PhasePair.Key);
			if (!ExpectedPasses.IsValid() || !ActualPasses)
			{
				ExpectationFailures.Add(FString::Printf(TEXT("phase result missing: %s"), *PhasePair.Key));
				continue;
			}
			for (const TPair<FString, TSharedPtr<FJsonValue>>& PassPair : ExpectedPasses->Values)
			{
				const TSharedPtr<FJsonObject> Metrics = PassPair.Value->AsObject();
				const FTimeThiefSmokeTestPhasePassSamples* Samples = ActualPasses->Find(PassPair.Key);
				if (!Metrics.IsValid() || !Samples)
				{
					ExpectationFailures.Add(FString::Printf(TEXT("phase pass result missing: %s.%s"), *PhasePair.Key, *PassPair.Key));
					continue;
				}
				for (const TPair<FString, TSharedPtr<FJsonValue>>& MetricPair : Metrics->Values)
				{
					const bool bMin = MetricPair.Key.EndsWith(TEXT("_min"));
					const bool bMax = MetricPair.Key.EndsWith(TEXT("_max"));
					const FString MetricName = MetricPair.Key.LeftChop(bMin || bMax ? 4 : 0);
					double Actual = 0.0;
					if ((!bMin && !bMax) || !PhasePassMetric(*Samples, MetricName, Actual))
					{
						ExpectationFailures.Add(FString::Printf(TEXT("unsupported phase pass expectation: %s.%s.%s"), *PhasePair.Key, *PassPair.Key, *MetricPair.Key));
						continue;
					}
					const double Limit = MetricPair.Value->AsNumber();
					if ((bMin && Actual < Limit) || (bMax && Actual > Limit))
					{
						ExpectationFailures.Add(FString::Printf(TEXT("phase pass %s.%s.%s expected %s %.6f, got %.6f"), *PhasePair.Key, *PassPair.Key, *MetricName, bMin ? TEXT(">=") : TEXT("<="), Limit, Actual));
					}
				}
			}
		}
	}

	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("scenario"), Scenario.Name);
	const TSharedRef<FJsonObject> ExecutionJson = MakeShared<FJsonObject>();
	ExecutionJson->SetBoolField(TEXT("scenario_loaded"), Execution.bScenarioLoaded);
	ExecutionJson->SetNumberField(TEXT("actions_requested"), Execution.ActionsRequested);
	ExecutionJson->SetNumberField(TEXT("actions_completed"), Execution.ActionsCompleted);
	ExecutionJson->SetNumberField(TEXT("actions_failed"), Execution.ActionsFailed);
	ExecutionJson->SetNumberField(TEXT("smokes_requested"), Execution.SmokesRequested);
	ExecutionJson->SetNumberField(TEXT("smokes_spawned"), Execution.SmokesSpawned);
	ExecutionJson->SetNumberField(TEXT("smokes_registered"), Execution.SmokesRegistered);
	ExecutionJson->SetNumberField(TEXT("renderer_frames_received"), Execution.RendererFramesReceived);
	ExecutionJson->SetNumberField(TEXT("gpu_pass_samples"), Execution.GpuPassSamples);
	ExecutionJson->SetNumberField(TEXT("probes_requested"), Execution.ProbesRequested);
	ExecutionJson->SetNumberField(TEXT("probes_completed"), Execution.ProbesCompleted);
	ExecutionJson->SetBoolField(TEXT("test_valid"), ValidityFailures.IsEmpty());
	TArray<TSharedPtr<FJsonValue>> ValidityJson;
	for (const FString& Failure : ValidityFailures) ValidityJson.Add(MakeShared<FJsonValueString>(Failure));
	ExecutionJson->SetArrayField(TEXT("failures"), MoveTemp(ValidityJson));
	Root->SetObjectField(TEXT("execution"), ExecutionJson);

	const TSharedRef<FJsonObject> SituationJson = MakeShared<FJsonObject>();
	SituationJson->SetNumberField(TEXT("smokes_spawned"), Execution.SmokesSpawned);
	SituationJson->SetNumberField(TEXT("bullets_fired"), Situation.BulletsFired);
	SituationJson->SetNumberField(TEXT("bullet_intersections"), Situation.BulletIntersections);
	SituationJson->SetNumberField(TEXT("bullet_events_accepted"), Situation.BulletEventsAccepted);
	SituationJson->SetNumberField(TEXT("bullet_events_rejected"), Situation.BulletEventsRejected);
	SituationJson->SetNumberField(TEXT("explosions"), Situation.Explosions);
	SituationJson->SetNumberField(TEXT("explosion_affected_smokes"), Situation.ExplosionAffectedSmokes);
	SituationJson->SetNumberField(TEXT("actor_push_events"), Situation.ActorPushEvents);
	SituationJson->SetNumberField(TEXT("actor_sphere_push_events"), Situation.ActorSpherePushEvents);
	SituationJson->SetNumberField(TEXT("actor_capsule_push_events"), Situation.ActorCapsulePushEvents);
	SituationJson->SetNumberField(TEXT("actor_box_push_events"), Situation.ActorBoxPushEvents);
	SituationJson->SetNumberField(TEXT("composite_batches"), Situation.CompositeBatches);
	SituationJson->SetNumberField(TEXT("missing_smoke_ids"), Situation.MissingSmokeIds);
	SituationJson->SetNumberField(TEXT("renderer_clear_frames"), Situation.RendererClearFrames);
	SituationJson->SetNumberField(TEXT("bullet_field_activations"), Situation.BulletFieldActivations);
	SituationJson->SetNumberField(TEXT("bullet_field_clears"), Situation.BulletFieldClears);
	SituationJson->SetNumberField(TEXT("rendered_unique_smokes"), RenderedSmokeIds.Num());
	SituationJson->SetNumberField(TEXT("duplicate_rendered_smoke_ids"), DuplicateRenderedSmokeIds);
	SituationJson->SetNumberField(TEXT("unrendered_registered_smokes"), UnrenderedRegisteredSmokeIds);
	Root->SetObjectField(TEXT("situation"), SituationJson);

	const TSharedRef<FJsonObject> PassesJson = MakeShared<FJsonObject>();
	for (const TPair<FString, TArray<double>>& Pair : PassDurations)
	{
		const TSharedRef<FJsonObject> PassJson = MakeShared<FJsonObject>();
		double Total = 0.0;
		for (const double Duration : Pair.Value) Total += Duration;
		PassJson->SetNumberField(TEXT("count"), Pair.Value.Num());
		PassJson->SetNumberField(TEXT("median_ms"), Percentile(Pair.Value, 0.5));
		PassJson->SetNumberField(TEXT("p95_ms"), Percentile(Pair.Value, 0.95));
		PassJson->SetNumberField(TEXT("total_ms"), Total);
		PassesJson->SetObjectField(Pair.Key, PassJson);
	}
	Root->SetObjectField(TEXT("passes"), PassesJson);

	const TSharedRef<FJsonObject> PhasesJson = MakeShared<FJsonObject>();
	for (const TPair<FString, TMap<FString, FTimeThiefSmokeTestPhasePassSamples>>& PhasePair : PhasePassSamples)
	{
		const TSharedRef<FJsonObject> PhaseJson = MakeShared<FJsonObject>();
		for (const TPair<FString, FTimeThiefSmokeTestPhasePassSamples>& PassPair : PhasePair.Value)
		{
			const TSharedRef<FJsonObject> PassJson = MakeShared<FJsonObject>();
			PassJson->SetNumberField(TEXT("count"), PassPair.Value.Durations.Num());
			PassJson->SetNumberField(TEXT("median_ms"), Percentile(PassPair.Value.Durations, 0.5));
			PassJson->SetNumberField(TEXT("p95_ms"), Percentile(PassPair.Value.Durations, 0.95));
			if (!PassPair.Value.DrawFractions.IsEmpty()) PassJson->SetNumberField(TEXT("draw_fraction_median"), Percentile(PassPair.Value.DrawFractions, 0.5));
			if (!PassPair.Value.CameraInsideCounts.IsEmpty()) PassJson->SetNumberField(TEXT("camera_inside_count_median"), Percentile(PassPair.Value.CameraInsideCounts, 0.5));
			if (!PassPair.Value.SurfaceDistances.IsEmpty()) PassJson->SetNumberField(TEXT("surface_distance_median"), Percentile(PassPair.Value.SurfaceDistances, 0.5));
			PhaseJson->SetObjectField(PassPair.Key, PassJson);
		}
		PhasesJson->SetObjectField(PhasePair.Key.IsEmpty() ? TEXT("unlabeled") : PhasePair.Key, PhaseJson);
	}
	Root->SetObjectField(TEXT("phases"), PhasesJson);

	const TSharedRef<FJsonObject> ExpectationsJson = MakeShared<FJsonObject>();
	ExpectationsJson->SetBoolField(TEXT("passed"), ExpectationFailures.IsEmpty());
	TArray<TSharedPtr<FJsonValue>> ExpectationFailureJson;
	for (const FString& Failure : ExpectationFailures) ExpectationFailureJson.Add(MakeShared<FJsonValueString>(Failure));
	ExpectationsJson->SetArrayField(TEXT("failures"), MoveTemp(ExpectationFailureJson));
	Root->SetObjectField(TEXT("expectations"), ExpectationsJson);
	Root->SetBoolField(TEXT("passed"), ValidityFailures.IsEmpty() && ExpectationFailures.IsEmpty());

	const FString ResultPath = FPaths::Combine(OutputDirectory, TEXT("result.json"));
	if (!FFileHelper::SaveStringToFile(JsonString(Root, true), *ResultPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		OutError = FString::Printf(TEXT("Cannot write result file: %s"), *ResultPath);
		return false;
	}
	return true;
}
