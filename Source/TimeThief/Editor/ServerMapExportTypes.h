#pragma once

#include <cstdint>

namespace se::map
{
	constexpr uint32_t kServerMapMagic = 0x534D4150; // "SMAP"
	constexpr uint32_t kServerMapVersion = 1;

	enum class ColliderType : uint8_t
	{
		None    = 0,
		AABB    = 1,
		OBB     = 2,
		Sphere  = 3,
		Capsule = 4,
	};

	enum ColliderFlags : uint32_t
	{
		Collider_None            = 0,
		Collider_BlockMovement   = 1 << 0,
		Collider_BlockProjectile = 1 << 1,
	};

	struct Float3
	{
		float x = 0.0f;
		float y = 0.0f;
		float z = 0.0f;
	};

	struct MapHeader
	{
		uint32_t magic = kServerMapMagic;
		uint32_t version = kServerMapVersion;
		uint32_t colliderCount = 0;
		uint32_t reserved0 = 0;
		uint32_t reserved1 = 0;
	};

	struct ColliderData
	{
		ColliderType type = ColliderType::None;
		uint8_t reserved0 = 0;
		uint16_t reserved1 = 0;
		uint32_t flags = Collider_None;

		Float3 position;
		Float3 rotationDeg;
		Float3 extents;

		float radius = 0.0f;
		float halfHeight = 0.0f;
	};
	
}

static_assert(sizeof(se::map::Float3) == 12);
static_assert(sizeof(se::map::MapHeader) == 20);
static_assert(sizeof(se::map::ColliderData) == 52);

struct FServerMapExportStats
{
	int32 TaggedActorCount = 0;
	int32 ExportedActorCount = 0;
	int32 ExportedColliderCount = 0;

	int32 BoxCount = 0;
	int32 SphereCount = 0;
	int32 CapsuleCount = 0;

	int32 IgnoredComponentCount = 0;
	int32 InvalidComponentCount = 0;
	
	int32 ShapeSourceActorCount = 0;
	int32 PresetSourceActorCount = 0;
	int32 MissingPresetActorCount = 0;
};

struct FServerMapColliderDebugRecord
{
	FString ActorName;
	FString ComponentName;

	se::map::ColliderData ColliderData;
};

