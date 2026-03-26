#include "ServerMapExporter.h"

#include "EngineUtils.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/Selection.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"

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
	TArray<FServerMapDebugColliderRecord> DebugRecords;
	FServerMapExportSummary Summary;
	Summary.TaggedActorCount = 1;

	const int32 AddedCount = BuildColliderDataListFromActor(SelectedActor, Colliders, DebugRecords, Summary);
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
	TArray<FServerMapDebugColliderRecord> DebugRecords;

	for (AActor* Actor : TaggedActors)
	{
		if (HasValidShapeComponent(Actor))
		{
			UE_LOG(LogTemp, Log, TEXT("[ServerMapExporter] Using ShapeComponents: %s"), *GetNameSafe(Actor));
			
			const int32 AddedCount = BuildColliderDataListFromActor(Actor, Colliders, DebugRecords, Summary);
			if (AddedCount > 0)
			{
				++Summary.ExportedActorCount;
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("[ServerMapExporter] Skipped actor: %s"), *GetNameSafe(Actor));
			}
		}
		else
		{
			// StaticMesh fallback.. (자동 추출)
			UE_LOG(LogTemp, Log, TEXT("[ServerMapExporter] Using StaticMesh fallback: %s"), *GetNameSafe(Actor));
		}
	}
	
	if (Colliders.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[ServerMapExporter] No valid colliders built from tag: %s"), *RequiredTag.ToString());
		return false;
	}
	
	se::map::MapHeader MapHeader{};
	MapHeader.colliderCount = static_cast<uint32>(Colliders.Num());

	const bool bBinarySaved = WriteServerMapFile(OutputPath, MapHeader, Colliders);
	if (!bBinarySaved)
	{
		return false;
	}

	const FString DebugJsonPath = MakeDebugJsonOutputPath(OutputPath);
	WriteDebugJsonFile(DebugJsonPath, DebugRecords);

	LogExportSummary(RequiredTag, OutputPath, Summary);
	return true;
}

bool ServerMapExporter::GenerateBoxFromSelectedStaticMesh()
{
	AActor* SelectedActor = GetFirstSelectedActor();
	if (SelectedActor == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ServerMapExporter] No selected actor"));
		return false;
	}

	return GenerateBoxFromActorStaticMesh(SelectedActor, true);
}

bool ServerMapExporter::GenerateBoxFromActorStaticMesh(AActor* Actor, bool bClearExistingGeneratedShapes)
{
	if (Actor == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ServerMapExporter] Actor is null"));
		return false;
	}

	UStaticMeshComponent* StaticMeshComponent = FindFirstStaticMeshComponent(Actor);
	if (StaticMeshComponent == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ServerMapExporter] Actor has no UStaticMeshComponent: %s"), *GetNameSafe(Actor));
		return false;
	}

	UStaticMesh* StaticMesh = StaticMeshComponent->GetStaticMesh();
	if (StaticMesh == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ServerMapExporter] StaticMesh is null: %s"), *GetNameSafe(Actor));
		return false;
	}

	if (bClearExistingGeneratedShapes)
	{
		RemoveGeneratedShapeComponents(Actor);
	}

	const FBox LocalMeshBox = StaticMesh->GetBoundingBox();
	if (!LocalMeshBox.IsValid)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ServerMapExporter] Invalid mesh bounds: %s"), *GetNameSafe(StaticMesh));
		return false;
	}

	const FVector LocalCenter = LocalMeshBox.GetCenter();
	const FVector LocalExtent = LocalMeshBox.GetExtent();

	if (LocalExtent.X <= KINDA_SMALL_NUMBER ||
		LocalExtent.Y <= KINDA_SMALL_NUMBER ||
		LocalExtent.Z <= KINDA_SMALL_NUMBER)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ServerMapExporter] Mesh bounds extent too small: %s"), *GetNameSafe(StaticMesh));
		return false;
	}

	UBoxComponent* NewBoxComponent = NewObject<UBoxComponent>(Actor);
	if (NewBoxComponent == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ServerMapExporter] Failed to create UBoxComponent"));
		return false;
	}

	NewBoxComponent->SetBoxExtent(LocalExtent);

	NewBoxComponent->ComponentTags.AddUnique(ServerTags::Generated);
	NewBoxComponent->ComponentTags.AddUnique(ServerTags::BlockMovement);
	NewBoxComponent->ComponentTags.AddUnique(ServerTags::BlockProjectile);

	NewBoxComponent->SetupAttachment(StaticMeshComponent);
	NewBoxComponent->SetRelativeLocation(LocalCenter);
	NewBoxComponent->SetRelativeRotation(FRotator::ZeroRotator);

	NewBoxComponent->RegisterComponent();

	Actor->AddInstanceComponent(NewBoxComponent);

#if WITH_EDITOR
	Actor->Modify();
	NewBoxComponent->Modify();
#endif

	UE_LOG(LogTemp, Log, TEXT("[ServerMapExporter] Generated BoxComponent from StaticMesh: Actor=%s Mesh=%s Center=%s Extent=%s"),
		*GetNameSafe(Actor),
		*GetNameSafe(StaticMesh),
		*LocalCenter.ToString(),
		*LocalExtent.ToString());

	return true;
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

UStaticMeshComponent* ServerMapExporter::FindFirstStaticMeshComponent(AActor* Actor)
{
	if (Actor == nullptr)
	{
		return nullptr;
	}

	return Actor->FindComponentByClass<UStaticMeshComponent>();
}

void ServerMapExporter::RemoveGeneratedShapeComponents(AActor* Actor)
{
	if (Actor == nullptr)
	{
		return;
	}

	TArray<UShapeComponent*> ShapeComponents;
	CollectShapeComponents(Actor, ShapeComponents);

	for (UShapeComponent* ShapeComponent : ShapeComponents)
	{
		if (ShapeComponent == nullptr)
		{
			continue;
		}

		if (ShapeComponent->ComponentHasTag(ServerTags::Generated))
		{
			ShapeComponent->DestroyComponent();
		}
	}
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
	
	if (ShapeComponent->ComponentHasTag(ServerTags::Ignore))
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

bool ServerMapExporter::HasValidShapeComponent(AActor* Actor)
{
	if (Actor == nullptr)
	{
		return false;
	}

	TArray<UShapeComponent*> Shapes;
	CollectShapeComponents(Actor, Shapes);

	for (UShapeComponent* Shape : Shapes)
	{
		if (ShouldExportShapeComponent(Shape) &&
			IsValidShapeComponentForExport(Shape))
		{
			return true;
		}
	}

	return false;
}

uint32 ServerMapExporter::BuildColliderFlagsFromShapeComponent(const UShapeComponent* ShapeComponent)
{
	if (ShapeComponent == nullptr)
	{
		return se::map::Collider_None;
	}

	const bool bHasMovementTag = ShapeComponent->ComponentHasTag(ServerTags::BlockMovement);
	const bool bHasProjectileTag = ShapeComponent->ComponentHasTag(ServerTags::BlockProjectile);

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

int32 ServerMapExporter::BuildColliderDataListFromActor(AActor* Actor, TArray<se::map::ColliderData>& OutColliders,
	TArray<FServerMapDebugColliderRecord>& OutDebugRecords, FServerMapExportSummary& Summary)
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
			AppendDebugRecord(Actor, ShapeComponent, ColliderData, OutDebugRecords);
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

bool ServerMapExporter::WriteDebugJsonFile(const FString& OutputPath,
	const TArray<FServerMapDebugColliderRecord>& DebugRecords)
{
	TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();

	RootObject->SetNumberField(TEXT("colliderCount"), DebugRecords.Num());

	TArray<TSharedPtr<FJsonValue>> ColliderArray;
	ColliderArray.Reserve(DebugRecords.Num());

	for (int32 Index = 0; Index < DebugRecords.Num(); ++Index)
	{
		const FServerMapDebugColliderRecord& Record = DebugRecords[Index];

		TSharedRef<FJsonObject> ColliderObject = MakeShared<FJsonObject>();
		ColliderObject->SetNumberField(TEXT("index"), Index);
		ColliderObject->SetStringField(TEXT("actorName"), Record.ActorName);
		ColliderObject->SetStringField(TEXT("componentName"), Record.ComponentName);
		ColliderObject->SetNumberField(TEXT("type"), static_cast<int32>(Record.ColliderData.type));
		ColliderObject->SetNumberField(TEXT("flags"), static_cast<int32>(Record.ColliderData.flags));

		TSharedRef<FJsonObject> PositionObject = MakeShared<FJsonObject>();
		PositionObject->SetNumberField(TEXT("x"), Record.ColliderData.position.x);
		PositionObject->SetNumberField(TEXT("y"), Record.ColliderData.position.y);
		PositionObject->SetNumberField(TEXT("z"), Record.ColliderData.position.z);
		ColliderObject->SetObjectField(TEXT("position"), PositionObject);

		TSharedRef<FJsonObject> RotationObject = MakeShared<FJsonObject>();
		RotationObject->SetNumberField(TEXT("x"), Record.ColliderData.rotationDeg.x);
		RotationObject->SetNumberField(TEXT("y"), Record.ColliderData.rotationDeg.y);
		RotationObject->SetNumberField(TEXT("z"), Record.ColliderData.rotationDeg.z);
		ColliderObject->SetObjectField(TEXT("rotationDeg"), RotationObject);

		TSharedRef<FJsonObject> ExtentsObject = MakeShared<FJsonObject>();
		ExtentsObject->SetNumberField(TEXT("x"), Record.ColliderData.extents.x);
		ExtentsObject->SetNumberField(TEXT("y"), Record.ColliderData.extents.y);
		ExtentsObject->SetNumberField(TEXT("z"), Record.ColliderData.extents.z);
		ColliderObject->SetObjectField(TEXT("extents"), ExtentsObject);

		ColliderObject->SetNumberField(TEXT("radius"), Record.ColliderData.radius);
		ColliderObject->SetNumberField(TEXT("halfHeight"), Record.ColliderData.halfHeight);

		ColliderArray.Add(MakeShared<FJsonValueObject>(ColliderObject));
	}

	RootObject->SetArrayField(TEXT("colliders"), ColliderArray);

	FString JsonString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);

	if (!FJsonSerializer::Serialize(RootObject, Writer))
	{
		UE_LOG(LogTemp, Warning, TEXT("[ServerMapExporter] Failed to serialize debug json"));
		return false;
	}

	if (!FFileHelper::SaveStringToFile(JsonString, *OutputPath))
	{
		UE_LOG(LogTemp, Warning, TEXT("[ServerMapExporter] Failed to save debug json: %s"), *OutputPath);
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("[ServerMapExporter] Debug json saved: %s"), *OutputPath);
	return true;
}

FString ServerMapExporter::MakeDebugJsonOutputPath(const FString& BinaryOutputPath)
{
	const FString Directory = FPaths::GetPath(BinaryOutputPath);
	const FString BaseName = FPaths::GetBaseFilename(BinaryOutputPath);
	return Directory / (BaseName + TEXT(".debug.json"));
}

void ServerMapExporter::AppendDebugRecord(const AActor* Actor, const UActorComponent* Component,
	const se::map::ColliderData& ColliderData, TArray<FServerMapDebugColliderRecord>& OutDebugRecords)
{
	FServerMapDebugColliderRecord Record;
	Record.ActorName = GetNameSafe(Actor);
	Record.ComponentName = GetNameSafe(Component);
	Record.ColliderData = ColliderData;

	OutDebugRecords.Add(MoveTemp(Record));
}

