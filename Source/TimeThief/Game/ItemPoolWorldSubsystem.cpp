// Fill out your copyright notice in the Description page of Project Settings.


#include "ItemPoolWorldSubsystem.h"

#include "ItemSettings.h"
#include "Actors/Item/ItemBase.h"
#include "Actors/Item/TimePointItemActor.h"
#include "Interface/PoolObject.h"
#include "Kismet/KismetSystemLibrary.h"


void UItemPoolWorldSubsystem::OnWorldBeginPlay(UWorld& InWorld) 
{
	Super::OnWorldBeginPlay(InWorld);
	
	auto ItemData = GetDefault<UItemSettings>()->GetItemData();
	for (const auto& [Key, Value] : ItemData->Items)
	{
		Pools.FindOrAdd(Value.ItemClass);
	}
	
	for (auto& [Key, Value] : Pools)
	{
		for (int i = 0; i < InitPoolSize; ++i)
		{
			if (AActor* NewActor = GetWorld()->SpawnActor(Key))
			{
				Cast<IPoolObject>(NewActor)->Disable();
				Value.Actors.Emplace(NewActor);
			}
		}
	}
}

void UItemPoolWorldSubsystem::OnWorldEndPlay(UWorld& InWorld)
{
	Super::OnWorldEndPlay(InWorld);
	
	for (auto& [Key, Value] : Pools)
	{
		for (int i = Value.Actors.Num() - 1; i >= 0 ; --i)
		{
			if (Value.Actors[i])
			{
				Value.Actors[i]->Destroy();
				Value.Actors.RemoveAt(i);
			}
		}
	}
	
	Pools.Empty();
}

AActor* UItemPoolWorldSubsystem::Get(TSubclassOf<AActor> ObjectClass)
{
	if (ObjectClass == nullptr)
	{
		return nullptr;
	}
	
	for (auto& [Key, Value] : Pools)
	{
		if (Key->IsChildOf(ObjectClass))
		{
			for (const auto& Object : Value.Actors)
			{
				if (Object && Object->IsA(ObjectClass))
				{
					if (IPoolObject* PoolObject = Cast<IPoolObject>(Object))
					{
						if (!PoolObject->bIsEnabled)
						{
							PoolObject->Enable();
							UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("Get Object from Pool %s"), *ObjectClass->GetName()));
							return Object;
						}
					}
				}
			}
			
			if (ObjectClass->ImplementsInterface(UPoolObject::StaticClass()))
			{
				AActor* NewActor = GetWorld()->SpawnActor(ObjectClass);
				Cast<IPoolObject>(NewActor)->Enable();
				Value.Actors.Emplace(NewActor);
				return NewActor;
			}
		}
	}

	return nullptr;
}
