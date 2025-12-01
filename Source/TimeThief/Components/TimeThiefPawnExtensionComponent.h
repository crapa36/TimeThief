#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TimeThiefPawnExtensionComponent.generated.h"

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class TIMETHIEF_API UTimeThiefPawnExtensionComponent : public UActorComponent {
	GENERATED_BODY()

protected:
	template <class T>
	T* GetOwningPawn() const {
		static_assert(TPointerIsConvertibleFromTo<T, APawn>::Value, "T must be derived from APawn");
		return CastChecked<T>(GetOwner());
	}

	APawn* GetOwningPawn() const {
		return GetOwningPawn<APawn>();
	}

	template <class T>
	T* GetOwningController() const {
		static_assert(TPointerIsConvertibleFromTo<T, AController>::Value, "T must be derived from AController");
		return GetOwningPawn<APawn>()->GetController<T>();
	}
};