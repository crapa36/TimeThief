#include "ServerMapExporter.h"

#include "EngineUtils.h"
#include "Components/BoxComponent.h"
#include "Engine/Selection.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"

#if WITH_EDITOR
#include "Editor.h"
#endif


bool ServerMapExporter::ExportActorsWithTagToFile(UWorld* World, const FName& RequiredTag, const FString& OutputPath)
{
	if (World == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ServerMapExporter] World is null"));
		return false;
	}
	
	if (RequiredTag.IsNone())
	{
		UE_LOG(LogTemp, Warning, TEXT("[ServerMapExporter] RequiredTag is None"));
		return false;
	}
	
	TArray<AActor*> TaggedActors;
	CollectActorsWithTag(World, RequiredTag, TaggedActors);
	
	if (TaggedActors.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[ServerMapExporter] No actors found with tag: %s"), *RequiredTag.ToString());
		return false;
	}
	
	TArray<se::map::ColliderData> Colliders;
	Colliders.Reserve(TaggedActors.Num());
	
	for (AActor* Actor : TaggedActors)
	{
		se::map::ColliderData ColliderData;
		if (BuildColliderDataFromActor(Actor, ColliderData))
		{
			Colliders.Add(ColliderData);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[ServerMapExporter] Skipped actor: %s"), *GetNameSafe(Actor));
		}
	}
	
	if (Colliders.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[ServerMapExporter] No valid colliders built from tag: %s"), *RequiredTag.ToString());
		return false;
	}
	
	se::map::MapHeader MapHeader{};
	MapHeader.colliderCount = static_cast<uint32>(Colliders.Num());
	
	UE_LOG(LogTemp, Log, TEXT("[ServerMapExporter] Found %d tagged actors, exported %d colliders"),
		TaggedActors.Num(), Colliders.Num());

	return WriteServerMapFile(OutputPath, MapHeader, Colliders);
}

bool ServerMapExporter::ExportSelectedActorBoxToFile(const FString& OutputPath)
{
	AActor* SelectedActor = GetFirstSelectedActor();
	if (SelectedActor == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ServerMapExporter] No selected actor"));
		return false;
	}
	
	UBoxComponent* BoxComponent = FindBoxComponent(SelectedActor);
	if (BoxComponent == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ServerMapExporter] Selected actor has no UBoxComponent: %s"), *GetNameSafe(SelectedActor));
		return false;
	}
	
	se::map::ColliderData ColliderData;
	if (!BuildColliderDataFromBoxComponent(BoxComponent, ColliderData))
	{
		UE_LOG(LogTemp, Warning, TEXT("[ServerMapExporter] Failed to build collider data"));
		return false;
	}
	
	se::map::MapHeader MapHeader{};
	MapHeader.colliderCount = 1;
	
	TArray<se::map::ColliderData> Colliders;
	Colliders.Add(ColliderData);
	
	return WriteServerMapFile(OutputPath, MapHeader, Colliders);
}

AActor* ServerMapExporter::GetFirstSelectedActor()
{
#if WITH_EDITOR
	if (GEditor == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ServerMapExporter] GEditor is null"));
		return nullptr;
	}
	
	USelection* SelectedActors = GEditor->GetSelectedActors();
	if (SelectedActors == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ServerMapExporter] SelectedActors is null"));
		return nullptr;
	}

	if (SelectedActors->Num() <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ServerMapExporter] No actor selected"));
		return nullptr;
	}
	
	return Cast<AActor>(SelectedActors->GetSelectedObject(0));
#else
	return nullptr;
#endif
	
}

UBoxComponent* ServerMapExporter::FindBoxComponent(AActor* Actor)
{
	if (Actor == nullptr)
	{
		return nullptr;
	}
	
	return Actor->FindComponentByClass<UBoxComponent>();
}

bool ServerMapExporter::BuildColliderDataFromBoxComponent(const UBoxComponent* BoxComponent,
	se::map::ColliderData& ColliderData)
{
	if (BoxComponent == nullptr)
	{
		return false;
	}
	
	const FVector Location = BoxComponent->GetComponentLocation();
	const FRotator Rotation = BoxComponent->GetComponentRotation();
	const FVector Extent = BoxComponent->GetScaledBoxExtent();
	
	ColliderData = {};
	ColliderData.type = se::map::ColliderType::OBB;
	ColliderData.flags = se::map::Collider_BlockMovement | se::map::Collider_BlockProjectile;
	
	ColliderData.position = { 
		static_cast<float>(Location.X), 
		static_cast<float>(Location.Y), 
		static_cast<float>(Location.Z) };
	
	ColliderData.rotationDeg = {
		static_cast<float>(Rotation.Pitch),
		static_cast<float>(Rotation.Yaw),
		static_cast<float>(Rotation.Roll) };
	
	ColliderData.extents = {
		static_cast<float>(Extent.X),
		static_cast<float>(Extent.Y),
		static_cast<float>(Extent.Z) };
	
	ColliderData.radius = 0.0f;
	ColliderData.halfHeight = 0.0f;
	
	return true;
}

bool ServerMapExporter::WriteServerMapFile(const FString& OutputPath, const se::map::MapHeader& MapHeader,
	const TArray<se::map::ColliderData>& Colliders)
{
	if (OutputPath.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[ServerMapExporter] OutputPath is empty"));
		return false;
	}
	
	const FString Directory = FPaths::GetPath(OutputPath);
	if (!Directory.IsEmpty())
	{
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		if (!PlatformFile.DirectoryExists(*Directory))
		{
			PlatformFile.CreateDirectoryTree(*Directory);
		}
	}
	
	TArray<uint8> Buffer;
	Buffer.Reserve(sizeof(se::map::MapHeader) + sizeof(se::map::ColliderData) * Colliders.Num());
	
	Buffer.Append(reinterpret_cast<const uint8*>(&MapHeader), sizeof(se::map::MapHeader));
	
	if (!Colliders.IsEmpty())
	{
		Buffer.Append(reinterpret_cast<const uint8*>(Colliders.GetData()), sizeof(se::map::ColliderData) * Colliders.Num());
	}
	
	const bool bSaved = FFileHelper::SaveArrayToFile(Buffer, *OutputPath);
	if (!bSaved)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ServerMapExporter] Failed to save file: %s"), *OutputPath);
		return false;
	}
	
	UE_LOG(LogTemp, Log, TEXT("[ServerMapExporter] Export success: %s"), *OutputPath);
	return true;
}

void ServerMapExporter::CollectActorsWithTag(UWorld* World, const FName& RequiredTag, TArray<AActor*>& OutActors)
{
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (Actor && Actor->ActorHasTag(RequiredTag))
		{
			OutActors.Add(Actor);
		}
	}
}

bool ServerMapExporter::BuildColliderDataFromActor(AActor* Actor, se::map::ColliderData& OutColliderData)
{
	if (Actor == nullptr)
	{
		return false;
	}
	
	UBoxComponent* BoxComponent = FindBoxComponent(Actor);
	if (BoxComponent == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ServerMapExporter] Actor has no UBoxComponent: %s"), *GetNameSafe(Actor));
		return false;
	}
	
	return BuildColliderDataFromBoxComponent(BoxComponent, OutColliderData);
}
