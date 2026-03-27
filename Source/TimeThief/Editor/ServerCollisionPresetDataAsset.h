

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/StaticMesh.h"

#include "ServerCollisionPresetDataAsset.generated.h"


UENUM(BlueprintType)
enum class EServerColliderShapeType : uint8
{
	Box			UMETA(DisplayName = "Box"),
	Sphere		UMETA(DisplayName = "Sphere"),
	Capsule		UMETA(DisplayName = "Capsule"),
};

USTRUCT(BlueprintType)
struct FServerCollisionPresetCollider
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Server Collision")
	EServerColliderShapeType ShapeType = EServerColliderShapeType::Box;

	// Local space transform relative to the source static mesh/component.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Server Collision")
	FVector LocalPosition = FVector::ZeroVector;

	// Local rotation. Sphere usually ignores this.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Server Collision")
	FRotator LocalRotation = FRotator::ZeroRotator;

	// Used when ShapeType == Box
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Server Collision", meta = (EditCondition = "ShapeType == EServerColliderShapeType::Box", EditConditionHides))
	FVector BoxExtent = FVector::ZeroVector;

	// Used when ShapeType == Sphere or Capsule
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Server Collision", meta = (EditCondition = "ShapeType != EServerColliderShapeType::Box", EditConditionHides, ClampMin = "0.0"))
	float Radius = 0.0f;

	// Used when ShapeType == Capsule
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Server Collision", meta = (EditCondition = "ShapeType == EServerColliderShapeType::Capsule", EditConditionHides, ClampMin = "0.0"))
	float HalfHeight = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Server Collision")
	bool bBlockMovement = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Server Collision")
	bool bBlockProjectile = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Server Collision")
	FName DebugName = NAME_None;
};

/**
 * 
 */
UCLASS(BlueprintType)
class TIMETHIEF_API UServerCollisionPresetDataAsset : public UDataAsset
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Server Collision")
	TObjectPtr<UStaticMesh> SourceStaticMesh = nullptr;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Server Collision")
	TArray<FServerCollisionPresetCollider> Colliders;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Server Collision")
	FString Notes;
	
};
