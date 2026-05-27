#include "AN_CatOffset.h"

void UAN_CatOffset::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation)
{
	if (!MeshComp)
		return;

	AActor* Owner = MeshComp->GetOwner();
	if (!Owner)
		return;

	const FTransform SocketWorld =
		MeshComp->GetSocketTransform(SocketName, RTS_World);

	const FVector MuzzleWorld =
		SocketWorld.TransformPosition(LocationOffset);

	const FVector ActorLocalOffset =
		Owner->GetActorTransform().InverseTransformPosition(MuzzleWorld);

	UE_LOG(LogTemp, Warning,
		TEXT("[MuzzleOffset] Anim=%s Socket=%s World=%s ActorLocal=%s"),
		*GetNameSafe(Animation),
		*SocketName.ToString(),
		*MuzzleWorld.ToString(),
		*ActorLocalOffset.ToString()
	);
}
