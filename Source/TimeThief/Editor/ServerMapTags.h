#pragma once

#include "CoreMinimal.h"

namespace ServerTags
{
	inline const FName Collision(TEXT("ServerCollision"));
	inline const FName CollisionPawn(TEXT("ServerCollisionPawn"));
	inline const FName Store(TEXT("ServerStore"));
	inline const FName Chest(TEXT("ServerChest"));
	
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

namespace ServerPawnTags
{
	inline const FName Player(TEXT("Pawn.Player"));
	inline const FName Cat(TEXT("Pawn.Cat"));
	inline const FName Minion(TEXT("Pawn.Minion"));
	inline const FName Kong(TEXT("Pawn.Kong"));

	inline const TArray<FName>& GetAll()
	{
		static const TArray<FName> Tags =
		{
			Player,
			Cat,
			Minion,
			Kong
		};

		return Tags;
	}

	inline FString GetTypeName(const FName& PawnTag)
	{
		FString TagString = PawnTag.ToString();
		TagString.RemoveFromStart(TEXT("Pawn."));
		return TagString.ToLower();
	}
}

namespace ServerPawnColliderTags
{
	inline const FName Collider(TEXT("ServerPawnCollider"));
	inline const FName DamageReceiver(TEXT("DamageReceiver"));
}

namespace ServerPawnPartTags
{
	inline const FName Head(TEXT("Part.Head"));
	inline const FName Body(TEXT("Part.Body"));
	inline const FName LeftArm(TEXT("Part.LeftArm"));
	inline const FName RightArm(TEXT("Part.RightArm"));
	inline const FName LeftLeg(TEXT("Part.LeftLeg"));
	inline const FName RightLeg(TEXT("Part.RightLeg"));

	inline const TArray<FName>& GetAll()
	{
		static const TArray<FName> Tags =
		{
			Head,
			Body,
			LeftArm,
			RightArm,
			LeftLeg,
			RightLeg
		};

		return Tags;
	}

	inline FString GetPartName(const FName& PartTag)
	{
		FString TagString = PartTag.ToString();
		TagString.RemoveFromStart(TEXT("Part."));
		return TagString.ToLower();
	}
}
