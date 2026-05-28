#pragma once

#include "CoreMinimal.h"
#include "Subsystems/EngineSubsystem.h"
#include "TimeThiefSmokeRendererTypes.h"
#include "TimeThiefSmokeRendererSubsystem.generated.h"

class FTimeThiefSmokeViewExtension;

UCLASS()
class TIMETHIEFSMOKERENDERER_API UTimeThiefSmokeRendererSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	void SubmitFrame(const FTimeThiefSmokeRendererFrame& Frame);

private:
	TSharedPtr<FTimeThiefSmokeViewExtension, ESPMode::ThreadSafe> ViewExtension;
};
