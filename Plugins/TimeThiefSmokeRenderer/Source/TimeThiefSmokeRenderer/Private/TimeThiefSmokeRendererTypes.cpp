#include "TimeThiefSmokeRendererTypes.h"

namespace
{
	FVector3f ToVector3f(const FVector& Value)
	{
		return FVector3f(static_cast<float>(Value.X), static_cast<float>(Value.Y), static_cast<float>(Value.Z));
	}

	FVector3f GetDefaultRenderBoundsExtent()
	{
		return ToVector3f(TimeThiefSmokeParameterDefaults::GetSmokeBoundsExtent() + TimeThiefSmokeParameterDefaults::GetRenderBoundsPadding());
	}

	FVector3f GetDefaultSimulationBoundsExtent()
	{
		return GetDefaultRenderBoundsExtent();
	}
}

FTimeThiefSmokeRendererVolume::FTimeThiefSmokeRendererVolume()
	: BoundsExtent(GetDefaultSimulationBoundsExtent())
	, SimulationBoundsExtent(GetDefaultSimulationBoundsExtent())
	, NaturalBoundsExtent(ToVector3f(TimeThiefSmokeParameterDefaults::GetSmokeBoundsExtent()))
	, RenderBoundsExtent(GetDefaultRenderBoundsExtent())
	, DurationSeconds(TimeThiefSmokeParameterDefaults::SmokeDuration)
{
}
