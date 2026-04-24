#pragma once

#include "CoreMinimal.h"

namespace ServerTags
{
	inline const FName Collision(TEXT("ServerCollision"));
	
	inline const FName Generated(TEXT("Generated"));
	inline const FName Ignore(TEXT("Ignore"));
	inline const FName BlockMovement(TEXT("BlockMovement"));
	inline const FName BlockProjectile(TEXT("BlockProjectile"));

	inline const FName AutoSimple(TEXT("AutoSimple"));
	inline const FName AutoConvexFallback(TEXT("AutoConvexFallback"));
	inline const FName AutoBoundsFallback(TEXT("AutoBoundsFallback"));

	inline const FName FromPreset(TEXT("FromPreset"));
	inline const FName ManualApproved(TEXT("ManualApproved"));
}
