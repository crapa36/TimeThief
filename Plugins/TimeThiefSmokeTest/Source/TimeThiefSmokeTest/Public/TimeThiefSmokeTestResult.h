#pragma once

#include "CoreMinimal.h"

struct FTimeThiefSmokeTestExecutionSummary
{
	bool bScenarioLoaded = false;
	int32 ActionsRequested = 0;
	int32 ActionsCompleted = 0;
	int32 ActionsFailed = 0;
	int32 SmokesRequested = 0;
	int32 SmokesSpawned = 0;
	int32 SmokesRegistered = 0;
	int32 RendererFramesReceived = 0;
	int32 GpuPassSamples = 0;
	int32 ProbesRequested = 0;
	int32 ProbesCompleted = 0;
};

struct FTimeThiefSmokeTestSituationSummary
{
	int32 BulletsFired = 0;
	int32 BulletIntersections = 0;
	int32 BulletEventsAccepted = 0;
	int32 BulletEventsRejected = 0;
	int32 Explosions = 0;
	int32 ExplosionAffectedSmokes = 0;
	int32 ActorPushEvents = 0;
	int32 ActorSpherePushEvents = 0;
	int32 ActorCapsulePushEvents = 0;
	int32 ActorBoxPushEvents = 0;
	int32 CompositeBatches = 0;
	int32 MissingSmokeIds = 0;
	int32 RendererClearFrames = 0;
	int32 BulletFieldActivations = 0;
	int32 BulletFieldClears = 0;
};
