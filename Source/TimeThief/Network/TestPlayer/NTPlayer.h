#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "NTPlayer.generated.h"

constexpr  float PositionTolerance = 5.0f;
constexpr  float RotationTolerance = 2.0f;

UCLASS()
class TIMETHIEF_API ANTPlayer : public ACharacter
// Network Test Player
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	ANTPlayer();
	virtual ~ANTPlayer();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;
	
public:
	bool IsLocalPlayer() const;
	
public:
	void SetNowPosition(const FVector& NewPosition) { NowPosition = NewPosition; }
	void SetNowRotation(const FRotator& NewRotation) { NowRotation = NewRotation; }
	
	void SetDestPosition(const FVector& NewPosition) { DestPosition = NewPosition; }
	void SetDestRotation(const FRotator& NewRotation) { DestRotation = NewRotation; }
	
public:
	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	
protected:
	uint32 EntityId = 0;
	
	FVector NowPosition;
	FRotator NowRotation;
	
	FVector DestPosition;
	FRotator DestRotation;
	
};
