#include "ServerMapExporter.h"

#include "EngineUtils.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/Selection.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"

#if WITH_EDITOR
#include "Editor.h"
#endif


bool ServerMapExporter::ExportSelectedActorBoxesToFile(const FString& OutputPath)
{
	AActor* SelectedActor = GetFirstSelectedActor();
	if (SelectedActor == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ServerMapExporter] No selected actor"));
		return false;
	}

	TArray<se::map::ColliderData> Colliders;
	FServerMapExportSummary Summary;
	Summary.TaggedActorCount = 1;

	const int32 AddedCount = BuildColliderDataListFromActor(SelectedActor, Colliders, Summary);
	if (AddedCount <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ServerMapExporter] Failed to build collider data from selected actor: %s"), *GetNameSafe(SelectedActor));
		return false;
	}

	Summary.ExportedActorCount = 1;

	se::map::MapHeader MapHeader{};
	MapHeader.colliderCount = static_cast<uint32>(Colliders.Num());

	const bool bResult = WriteServerMapFile(OutputPath, MapHeader, Colliders);
	if (bResult)
	{
		LogExportSummary(TEXT("SelectedActor"), OutputPath, Summary);
	}

	return bResult;
}

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
	
	FServerMapExportSummary Summary;
	Summary.TaggedActorCount = TaggedActors.Num();

	if (TaggedActors.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[ServerMapExporter] No actors found with tag: %s"), *RequiredTag.ToString());
		return false;
	}
	
	TArray<se::map::ColliderData> Colliders;
	
	for (AActor* Actor : TaggedActors)
	{
		const int32 AddedCount = BuildColliderDataListFromActor(Actor, Colliders, Summary);
		if (AddedCount > 0)
		{
			++Summary.ExportedActorCount;
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

	const bool bResult = WriteServerMapFile(OutputPath, MapHeader, Colliders);
	if (bResult)
	{
		LogExportSummary(RequiredTag, OutputPath, Summary);
	}

	return bResult;
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

void ServerMapExporter::CollectShapeComponents(AActor* Actor, TArray<UShapeComponent*>& OutShapeComponents)
{
	OutShapeComponents.Reset();
	
	if (Actor == nullptr)
	{
		return;
	}
	
	Actor->GetComponents<UShapeComponent>(OutShapeComponents);
}

bool ServerMapExporter::ShouldExportShapeComponent(const UShapeComponent* ShapeComponent)
{
	if (ShapeComponent == nullptr)
	{
		return false;
	}
	
	if (ShapeComponent->ComponentHasTag(TEXT("ServerIgnore")))
	{
		return false;
	}
	
	return true;
}

bool ServerMapExporter::IsValidShapeComponentForExport(const UShapeComponent* ShapeComponent)
{
	if (ShapeComponent == nullptr)
	{
		return false;
	}
	
	if (const UBoxComponent* BoxComponent = Cast<UBoxComponent>(ShapeComponent))
	{
		const FVector Extent = BoxComponent->GetScaledBoxExtent();
		return Extent.X > KINDA_SMALL_NUMBER 
			&& Extent.Y > KINDA_SMALL_NUMBER 
			&& Extent.Z > KINDA_SMALL_NUMBER;
	}
	
	if (const USphereComponent* SphereComponent = Cast<USphereComponent>(ShapeComponent))
	{
		const float Radius = SphereComponent->GetScaledSphereRadius();
		return Radius > KINDA_SMALL_NUMBER;
	}
	
	if (const UCapsuleComponent* CapsuleComponent = Cast<UCapsuleComponent>(ShapeComponent))
	{
		const float Radius = CapsuleComponent->GetScaledCapsuleRadius();
		const float HalfHeight = CapsuleComponent->GetScaledCapsuleHalfHeight();
		return Radius > KINDA_SMALL_NUMBER && HalfHeight > KINDA_SMALL_NUMBER;
	}
	
	return false;
}

uint32 ServerMapExporter::BuildColliderFlagsFromShapeComponent(const UShapeComponent* ShapeComponent)
{
	if (ShapeComponent == nullptr)
	{
		return se::map::Collider_None;
	}

	const bool bHasMovementTag = ShapeComponent->ComponentHasTag(TEXT("ServerBlockMovement"));
	const bool bHasProjectileTag = ShapeComponent->ComponentHasTag(TEXT("ServerBlockProjectile"));

	if (!bHasMovementTag && !bHasProjectileTag)
	{
		return se::map::Collider_BlockMovement | se::map::Collider_BlockProjectile;
	}

	uint32 Flags = se::map::Collider_None;

	if (bHasMovementTag)
	{
		Flags |= se::map::Collider_BlockMovement;
	}

	if (bHasProjectileTag)
	{
		Flags |= se::map::Collider_BlockProjectile;
	}

	return Flags;
}

bool ServerMapExporter::BuildColliderDataFromShapeComponent(const UShapeComponent* ShapeComponent,
	se::map::ColliderData& OutColliderData)
{
	if (ShapeComponent == nullptr)
	{
		return false;
	}

	if (const UBoxComponent* BoxComponent = Cast<UBoxComponent>(ShapeComponent))
	{
		return BuildColliderDataFromBoxComponent(BoxComponent, OutColliderData);
	}

	if (const USphereComponent* SphereComponent = Cast<USphereComponent>(ShapeComponent))
	{
		return BuildColliderDataFromSphereComponent(SphereComponent, OutColliderData);
	}

	if (const UCapsuleComponent* CapsuleComponent = Cast<UCapsuleComponent>(ShapeComponent))
	{
		return BuildColliderDataFromCapsuleComponent(CapsuleComponent, OutColliderData);
	}

	return false;
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
	ColliderData.flags = BuildColliderFlagsFromShapeComponent(BoxComponent);
	
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

bool ServerMapExporter::BuildColliderDataFromSphereComponent(const USphereComponent* SphereComponent,
	se::map::ColliderData& OutColliderData)
{
	if (SphereComponent == nullptr)
	{
		return false;
	}

	const FVector Location = SphereComponent->GetComponentLocation();
	const float Radius = SphereComponent->GetScaledSphereRadius();

	OutColliderData = {};
	OutColliderData.type = se::map::ColliderType::Sphere;
	OutColliderData.flags = BuildColliderFlagsFromShapeComponent(SphereComponent);

	OutColliderData.position = {
		static_cast<float>(Location.X),
		static_cast<float>(Location.Y),
		static_cast<float>(Location.Z)
	};

	OutColliderData.rotationDeg = { 0.0f, 0.0f, 0.0f };
	OutColliderData.extents = { 0.0f, 0.0f, 0.0f };
	OutColliderData.radius = Radius;
	OutColliderData.halfHeight = 0.0f;

	return true;
}

bool ServerMapExporter::BuildColliderDataFromCapsuleComponent(const UCapsuleComponent* CapsuleComponent,
	se::map::ColliderData& OutColliderData)
{
	if (CapsuleComponent == nullptr)
	{
		return false;
	}

	const FVector Location = CapsuleComponent->GetComponentLocation();
	const FRotator Rotation = CapsuleComponent->GetComponentRotation();
	const float Radius = CapsuleComponent->GetScaledCapsuleRadius();
	const float HalfHeight = CapsuleComponent->GetScaledCapsuleHalfHeight();

	OutColliderData = {};
	OutColliderData.type = se::map::ColliderType::Capsule;
	OutColliderData.flags = BuildColliderFlagsFromShapeComponent(CapsuleComponent);

	OutColliderData.position = {
		static_cast<float>(Location.X),
		static_cast<float>(Location.Y),
		static_cast<float>(Location.Z)
	};

	OutColliderData.rotationDeg = {
		static_cast<float>(Rotation.Pitch),
		static_cast<float>(Rotation.Yaw),
		static_cast<float>(Rotation.Roll)
	};

	OutColliderData.extents = { 0.0f, 0.0f, 0.0f };
	OutColliderData.radius = Radius;
	OutColliderData.halfHeight = HalfHeight;

	return true;
}

int32 ServerMapExporter::BuildColliderDataListFromActor(AActor* Actor, TArray<se::map::ColliderData>& OutColliders, FServerMapExportSummary& Summary)
{
	if (Actor == nullptr)
	{
		return 0;
	}

	TArray<UShapeComponent*> ShapeComponents;
	CollectShapeComponents(Actor, ShapeComponents);

	if (ShapeComponents.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[ServerMapExporter] Actor has no supported shape component: %s"), *GetNameSafe(Actor));
		return 0;
	}

	const int32 PrevCount = OutColliders.Num();

	for (const UShapeComponent* ShapeComponent : ShapeComponents)
	{
		if (ShapeComponent == nullptr)
		{
			continue;
		}

		if (!ShouldExportShapeComponent(ShapeComponent))
		{
			++Summary.IgnoredComponentCount;
			continue;
		}

		if (!IsValidShapeComponentForExport(ShapeComponent))
		{
			++Summary.InvalidComponentCount;
			UE_LOG(LogTemp, Warning, TEXT("[ServerMapExporter] Invalid shape component skipped: %s (%s)"),
				*GetNameSafe(ShapeComponent),
				*GetNameSafe(Actor));
			continue;
		}

		se::map::ColliderData ColliderData;
		if (BuildColliderDataFromShapeComponent(ShapeComponent, ColliderData))
		{
			OutColliders.Add(ColliderData);
			AccumulateSummary(ColliderData, Summary);
		}
	}

	return OutColliders.Num() - PrevCount;
}

void ServerMapExporter::AccumulateSummary(const se::map::ColliderData& ColliderData, FServerMapExportSummary& Summary)
{
	++Summary.ExportedColliderCount;

	switch (ColliderData.type)
	{
	case se::map::ColliderType::AABB:
	case se::map::ColliderType::OBB:
		++Summary.BoxCount;
		break;
		
	case se::map::ColliderType::Sphere:
		++Summary.SphereCount;
		break;
		
	case se::map::ColliderType::Capsule:
		++Summary.CapsuleCount;
		break;
		
	default:
		break;
	}
}

void ServerMapExporter::LogExportSummary(const FName& RequiredTag, const FString& OutputPath,
	const FServerMapExportSummary& Summary)
{
	UE_LOG(LogTemp, Log, TEXT("========== ServerMap Export Summary =========="));
	UE_LOG(LogTemp, Log, TEXT("[ServerMapExporter] Tag                : %s"), *RequiredTag.ToString());
	UE_LOG(LogTemp, Log, TEXT("[ServerMapExporter] Output             : %s"), *OutputPath);
	UE_LOG(LogTemp, Log, TEXT("[ServerMapExporter] Tagged Actors      : %d"), Summary.TaggedActorCount);
	UE_LOG(LogTemp, Log, TEXT("[ServerMapExporter] Exported Actors    : %d"), Summary.ExportedActorCount);
	UE_LOG(LogTemp, Log, TEXT("[ServerMapExporter] Exported Colliders : %d"), Summary.ExportedColliderCount);
	UE_LOG(LogTemp, Log, TEXT("[ServerMapExporter] Box Count          : %d"), Summary.BoxCount);
	UE_LOG(LogTemp, Log, TEXT("[ServerMapExporter] Sphere Count       : %d"), Summary.SphereCount);
	UE_LOG(LogTemp, Log, TEXT("[ServerMapExporter] Capsule Count      : %d"), Summary.CapsuleCount);
	UE_LOG(LogTemp, Log, TEXT("[ServerMapExporter] Ignored Components : %d"), Summary.IgnoredComponentCount);
	UE_LOG(LogTemp, Log, TEXT("[ServerMapExporter] Invalid Components : %d"), Summary.InvalidComponentCount);
	UE_LOG(LogTemp, Log, TEXT("=============================================="));
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
