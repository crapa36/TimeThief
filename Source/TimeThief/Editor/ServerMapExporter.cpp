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
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/AggregateGeom.h"
#include "Math/Quat.h"
#include "ServerCollisionPresetDataAsset.h"
#include "UObject/Package.h"
#include "Misc/MessageDialog.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Modules/ModuleManager.h"

#if WITH_EDITOR
#include "Editor.h"
#include "AssetToolsModule.h"
#include "Factories/DataAssetFactory.h"
#endif

namespace
{
	static void AddDefaultGeneratedTags(UShapeComponent* ShapeComponent)
	{
		if (ShapeComponent == nullptr)
		{
			return;
		}

		ShapeComponent->ComponentTags.AddUnique(ServerTags::Generated);
		ShapeComponent->ComponentTags.AddUnique(ServerTags::BlockMovement);
		ShapeComponent->ComponentTags.AddUnique(ServerTags::BlockProjectile);
	}

	static void AddGeneratedSourceTag(UShapeComponent* ShapeComponent, const FName& SourceTag)
	{
		if (ShapeComponent == nullptr || SourceTag.IsNone())
		{
			return;
		}

		ShapeComponent->ComponentTags.AddUnique(SourceTag);
	}
}

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
		const int32 AddedCount = BuildColliderDataListFromActorResolved(Actor, Colliders, DebugRecords, Summary);
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

bool ServerMapExporter::ExportPresetToFile(AActor* Actor, UServerCollisionPresetDataAsset* PresetAsset,
	const FString& OutputPath)
{
	if (Actor == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ServerMapExporter] Actor is null"));
		return false;
	}

	if (PresetAsset == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ServerMapExporter] PresetAsset is null"));
		return false;
	}

	UStaticMeshComponent* StaticMeshComponent = FindFirstStaticMeshComponent(Actor);
	if (StaticMeshComponent == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ServerMapExporter] Actor has no UStaticMeshComponent: %s"), *GetNameSafe(Actor));
		return false;
	}

	TArray<se::map::ColliderData> Colliders;
	TArray<FServerMapDebugColliderRecord> DebugRecords;
	FServerMapExportSummary Summary;
	Summary.TaggedActorCount = 1;

	const int32 AddedCount = BuildColliderDataListFromPreset(Actor, StaticMeshComponent, PresetAsset, Colliders, DebugRecords, Summary);
	if (AddedCount <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ServerMapExporter] Failed to build colliders from preset: %s"), *GetNameSafe(PresetAsset));
		return false;
	}

	Summary.ExportedActorCount = 1;

	se::map::MapHeader MapHeader{};
	MapHeader.colliderCount = static_cast<uint32>(Colliders.Num());

	const bool bBinarySaved = WriteServerMapFile(OutputPath, MapHeader, Colliders);
	if (!bBinarySaved)
	{
		return false;
	}

	const FString DebugJsonPath = MakeDebugJsonOutputPath(OutputPath);
	WriteDebugJsonFile(DebugJsonPath, DebugRecords);

	LogExportSummary(TEXT("PresetExport"), OutputPath, Summary);

	UE_LOG(LogTemp, Log, TEXT("[ServerMapExporter] Exported preset to file: Actor=%s Preset=%s"),
		*GetNameSafe(Actor),
		*GetNameSafe(PresetAsset));

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

	AddDefaultGeneratedTags(NewBoxComponent);
	AddGeneratedSourceTag(NewBoxComponent, ServerTags::AutoBoundsFallback);

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

bool ServerMapExporter::GenerateShapesFromSelectedStaticMesh()
{
	AActor* SelectedActor = GetFirstSelectedActor();
	if (SelectedActor == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ServerMapExporter] No selected actor"));
		return false;
	}

	return GenerateShapesFromActorStaticMesh(SelectedActor, true);
}

bool ServerMapExporter::GenerateShapesFromActorStaticMesh(AActor* Actor, bool bClearExistingGeneratedShapes)
{
	if (Actor == nullptr)
	{
		return false;
	}

	UStaticMeshComponent* StaticMeshComponent = FindFirstStaticMeshComponent(Actor);
	if (StaticMeshComponent == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ServerMapExporter] Actor has no UStaticMeshComponent: %s"), *GetNameSafe(Actor));
		return false;
	}

	if (bClearExistingGeneratedShapes)
	{
		RemoveGeneratedShapeComponents(Actor);
	}

	return GenerateShapesFromStaticMeshComponent(StaticMeshComponent, Actor);
}

int32 ServerMapExporter::GenerateShapesForActorsWithTag(UWorld* World, const FName& RequiredTag,
	bool bSkipActorsWithExistingShapes, bool bSkipActorsWithExistingPreset,
	bool bClearExistingGeneratedShapesBeforeRegenerate)
{
	if (World == nullptr || RequiredTag.IsNone())
	{
		return 0;
	}

	TArray<AActor*> TaggedActors;
	CollectActorsWithTag(World, RequiredTag, TaggedActors);

	int32 GeneratedActorCount = 0;

	for (AActor* Actor : TaggedActors)
	{
		if (Actor == nullptr)
		{
			continue;
		}

		if (bSkipActorsWithExistingShapes && HasValidShapeComponent(Actor))
		{
			continue;
		}

		UStaticMeshComponent* StaticMeshComponent = FindFirstStaticMeshComponent(Actor);
		if (StaticMeshComponent == nullptr)
		{
			UE_LOG(LogTemp, Warning, TEXT("[ServerMapExporter] Generate skipped. No StaticMeshComponent: %s"), *GetNameSafe(Actor));
			continue;
		}

		UStaticMesh* StaticMesh = StaticMeshComponent->GetStaticMesh();
		if (StaticMesh == nullptr)
		{
			UE_LOG(LogTemp, Warning, TEXT("[ServerMapExporter] Generate skipped. StaticMesh is null: %s"), *GetNameSafe(Actor));
			continue;
		}

		if (bSkipActorsWithExistingPreset)
		{
			if (UServerCollisionPresetDataAsset* Preset = FindPresetForStaticMesh(StaticMesh))
			{
				continue;
			}
		}

		if (bClearExistingGeneratedShapesBeforeRegenerate)
		{
			RemoveGeneratedShapeComponents(Actor);
		}

		if (GenerateShapesFromStaticMeshComponent(StaticMeshComponent, Actor))
		{
			++GeneratedActorCount;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[ServerMapExporter] GenerateShapesForActorsWithTag finished. Tag=%s GeneratedActorCount=%d"),
		*RequiredTag.ToString(),
		GeneratedActorCount);

	return GeneratedActorCount;
}

int32 ServerMapExporter::ClearGeneratedShapesForActorsWithTag(UWorld* World, const FName& RequiredTag)
{
	if (World == nullptr || RequiredTag.IsNone())
	{
		return 0;
	}

	TArray<AActor*> TaggedActors;
	CollectActorsWithTag(World, RequiredTag, TaggedActors);

	int32 ClearedActorCount = 0;

	for (AActor* Actor : TaggedActors)
	{
		if (Actor == nullptr)
		{
			continue;
		}

		const int32 BeforeCount = CountGeneratedShapeComponents(Actor);
		if (BeforeCount <= 0)
		{
			continue;
		}

		RemoveGeneratedShapeComponents(Actor);
		++ClearedActorCount;
	}

	UE_LOG(LogTemp, Log, TEXT("[ServerMapExporter] ClearGeneratedShapesForActorsWithTag finished. Tag=%s ClearedActorCount=%d"),
		*RequiredTag.ToString(),
		ClearedActorCount);

	return ClearedActorCount;
}

int32 ServerMapExporter::SaveGeneratedShapesToPresetsForActorsWithTag(UWorld* World, const FName& RequiredTag,
	const FString& PresetFolderPath, bool bOnlySaveGeneratedShapes)
{
	if (World == nullptr || RequiredTag.IsNone())
	{
		return 0;
	}

	TArray<AActor*> TaggedActors;
	CollectActorsWithTag(World, RequiredTag, TaggedActors);

	int32 SavedCount = 0;

	for (AActor* Actor : TaggedActors)
	{
		if (Actor == nullptr)
		{
			continue;
		}

		UStaticMeshComponent* StaticMeshComponent = FindFirstStaticMeshComponent(Actor);
		if (StaticMeshComponent == nullptr)
		{
			continue;
		}

		UStaticMesh* StaticMesh = StaticMeshComponent->GetStaticMesh();
		if (StaticMesh == nullptr)
		{
			continue;
		}

		UServerCollisionPresetDataAsset* PresetAsset = FindOrCreatePresetForStaticMesh(StaticMesh, PresetFolderPath);
		if (PresetAsset == nullptr)
		{
			UE_LOG(LogTemp, Warning, TEXT("[ServerMapExporter] Failed to find/create preset: %s"), *GetNameSafe(StaticMesh));
			continue;
		}

		if (SaveActorGeneratedShapesToPreset(Actor, PresetAsset, bOnlySaveGeneratedShapes))
		{
			++SavedCount;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[ServerMapExporter] SaveGeneratedShapesToPresetsForActorsWithTag finished. Tag=%s SavedCount=%d"),
		*RequiredTag.ToString(),
		SavedCount);

	return SavedCount;
}

bool ServerMapExporter::ValidateActorsWithTag(UWorld* World, const FName& RequiredTag,
	FServerMapValidationReport& OutReport)
{
	OutReport = {};

	if (World == nullptr || RequiredTag.IsNone())
	{
		return false;
	}

	TArray<AActor*> TaggedActors;
	CollectActorsWithTag(World, RequiredTag, TaggedActors);

	OutReport.TaggedActorCount = TaggedActors.Num();

	for (AActor* Actor : TaggedActors)
	{
		if (Actor == nullptr)
		{
			continue;
		}

		if (HasValidShapeComponent(Actor))
		{
			++OutReport.ActorsWithValidShapes;
			AccumulateGeneratedSourceStats(Actor, OutReport);
			continue;
		}

		UStaticMeshComponent* StaticMeshComponent = FindFirstStaticMeshComponent(Actor);
		if (StaticMeshComponent == nullptr)
		{
			++OutReport.ActorsMissingStaticMesh;
			AppendValidationItem(OutReport, Actor, TEXT("No StaticMeshComponent"));
			continue;
		}

		UStaticMesh* StaticMesh = StaticMeshComponent->GetStaticMesh();
		if (StaticMesh == nullptr)
		{
			++OutReport.ActorsWithNullStaticMesh;
			AppendValidationItem(OutReport, Actor, TEXT("StaticMesh is null"));
			continue;
		}

		UServerCollisionPresetDataAsset* PresetAsset = FindPresetForStaticMesh(StaticMesh);
		if (PresetAsset == nullptr)
		{
			++OutReport.ActorsMissingPreset;
			AppendValidationItem(OutReport, Actor, TEXT("Missing preset"), StaticMesh, nullptr);
			continue;
		}

		++OutReport.ActorsUsingPreset;
	}

	LogValidationReport(RequiredTag, OutReport);
	return true;
}

bool ServerMapExporter::GenerateShapesFromStaticMeshComponent(UStaticMeshComponent* StaticMeshComponent,
                                                              AActor* OwnerActor)
{
	if (StaticMeshComponent == nullptr || OwnerActor == nullptr)
	{
		return false;
	}

	UStaticMesh* StaticMesh = StaticMeshComponent->GetStaticMesh();
	if (StaticMesh == nullptr)
	{
		return false;
	}

	UBodySetup* BodySetup = StaticMesh->GetBodySetup();
	if (BodySetup == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ServerMapExporter] BodySetup is null: %s"), *GetNameSafe(StaticMesh));
		return false;
	}

	const int32 GeneratedCount = GeneratePrimitiveShapesFromBodySetup(StaticMeshComponent, OwnerActor);
	if (GeneratedCount <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ServerMapExporter] No simple collision primitives generated: %s"), *GetNameSafe(StaticMesh));
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("[ServerMapExporter] Generated %d shape components from StaticMesh simple collision: Actor=%s Mesh=%s"),
		GeneratedCount,
		*GetNameSafe(OwnerActor),
		*GetNameSafe(StaticMesh));

	return true;
}

int32 ServerMapExporter::GeneratePrimitiveShapesFromBodySetup(UStaticMeshComponent* StaticMeshComponent,
	AActor* OwnerActor)
{
	if (StaticMeshComponent == nullptr || OwnerActor == nullptr)
	{
		return 0;
	}

	UStaticMesh* StaticMesh = StaticMeshComponent->GetStaticMesh();
	if (StaticMesh == nullptr)
	{
		return 0;
	}

	UBodySetup* BodySetup = StaticMesh->GetBodySetup();
	if (BodySetup == nullptr)
	{
		return 0;
	}

	int32 GeneratedCount = 0;

	const FKAggregateGeom& AggGeom = BodySetup->AggGeom;

	UE_LOG(LogTemp, Log, TEXT("[ServerMapExporter] SimpleCollision counts: Box=%d Sphere=%d Capsule=%d Convex=%d"),
		AggGeom.BoxElems.Num(),
		AggGeom.SphereElems.Num(),
		AggGeom.SphylElems.Num(),
		AggGeom.ConvexElems.Num());

	// Box
	for (int32 Index = 0; Index < AggGeom.BoxElems.Num(); ++Index)
	{
		const FKBoxElem& BoxElem = AggGeom.BoxElems[Index];

		const FVector RelativeLocation = FVector(BoxElem.Center);
		const FRotator RelativeRotation = FRotator(BoxElem.Rotation);
		const FVector BoxExtent = FVector(BoxElem.X * 0.5f, BoxElem.Y * 0.5f, BoxElem.Z * 0.5f);

		if (BoxExtent.X <= KINDA_SMALL_NUMBER ||
			BoxExtent.Y <= KINDA_SMALL_NUMBER ||
			BoxExtent.Z <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		if (UBoxComponent* NewBox = CreateGeneratedBoxComponent(
			OwnerActor,
			StaticMeshComponent,
			RelativeLocation,
			RelativeRotation,
			BoxExtent))
		{
			AddGeneratedSourceTag(NewBox, ServerTags::AutoSimple);
			++GeneratedCount;
		}
	}

	// Sphere
	for (int32 Index = 0; Index < AggGeom.SphereElems.Num(); ++Index)
	{
		const FKSphereElem& SphereElem = AggGeom.SphereElems[Index];

		const FVector RelativeLocation = FVector(SphereElem.Center);
		const float Radius = SphereElem.Radius;

		if (Radius <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		if (USphereComponent* NewSphere = CreateGeneratedSphereComponent(
			OwnerActor,
			StaticMeshComponent,
			RelativeLocation,
			Radius))
		{
			AddGeneratedSourceTag(NewSphere, ServerTags::AutoSimple);
			++GeneratedCount;
		}
	}

	// Capsule
	for (int32 Index = 0; Index < AggGeom.SphylElems.Num(); ++Index)
	{
		const FKSphylElem& SphylElem = AggGeom.SphylElems[Index];

		const FVector RelativeLocation = FVector(SphylElem.Center);
		const FRotator RelativeRotation = FRotator(SphylElem.Rotation);
		const float Radius = SphylElem.Radius;
		const float HalfHeight = SphylElem.Length * 0.5f + Radius;

		if (Radius <= KINDA_SMALL_NUMBER || HalfHeight <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		if (UCapsuleComponent* NewCapsule = CreateGeneratedCapsuleComponent(
			OwnerActor,
			StaticMeshComponent,
			RelativeLocation,
			RelativeRotation,
			Radius,
			HalfHeight))
		{
			AddGeneratedSourceTag(NewCapsule, ServerTags::AutoSimple);
			++GeneratedCount;
		}
	}

	if (GeneratedCount == 0 && AggGeom.ConvexElems.Num() > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[ServerMapExporter] Using Convex fallback(per-convex box): Mesh=%s ConvexCount=%d"),
			*GetNameSafe(StaticMesh),
			AggGeom.ConvexElems.Num());

		GeneratedCount += GenerateConvexFallbackShapes(StaticMeshComponent, OwnerActor);
	}

	if (GeneratedCount == 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[ServerMapExporter] Using final mesh bounds fallback: Mesh=%s"),
			*GetNameSafe(StaticMesh));

		if (CreateGeneratedBoundsBoxComponent(StaticMeshComponent, OwnerActor) != nullptr)
		{
			++GeneratedCount;
		}
	}

	return GeneratedCount;
}

int32 ServerMapExporter::GenerateConvexFallbackShapes(UStaticMeshComponent* StaticMeshComponent, AActor* OwnerActor)
{
	if (StaticMeshComponent == nullptr || OwnerActor == nullptr)
	{
		return 0;
	}

	UStaticMesh* StaticMesh = StaticMeshComponent->GetStaticMesh();
	if (StaticMesh == nullptr)
	{
		return 0;
	}

	UBodySetup* BodySetup = StaticMesh->GetBodySetup();
	if (BodySetup == nullptr)
	{
		return 0;
	}

	int32 GeneratedCount = 0;
	const FKAggregateGeom& AggGeom = BodySetup->AggGeom;

	for (int32 ConvexIndex = 0; ConvexIndex < AggGeom.ConvexElems.Num(); ++ConvexIndex)
	{
		const FKConvexElem& ConvexElem = AggGeom.ConvexElems[ConvexIndex];

		if (ConvexElem.VertexData.IsEmpty())
		{
			continue;
		}

		FBox LocalBox(ForceInit);

		for (const FVector& Vertex : ConvexElem.VertexData)
		{
			LocalBox += Vertex;
		}

		if (!LocalBox.IsValid)
		{
			continue;
		}

		const FVector LocalCenter = LocalBox.GetCenter();
		const FVector LocalExtent = LocalBox.GetExtent();

		if (LocalExtent.X <= KINDA_SMALL_NUMBER ||
			LocalExtent.Y <= KINDA_SMALL_NUMBER ||
			LocalExtent.Z <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		// const FVector RelativeLocation = FVector(ConvexElem.ElemBox.GetCenter());
		const FRotator RelativeRotation = FRotator::ZeroRotator;

		UBoxComponent* NewBox = CreateGeneratedBoxComponent(
			OwnerActor,
			StaticMeshComponent,
			LocalCenter,
			RelativeRotation,
			LocalExtent);

		if (NewBox != nullptr)
		{
			AddGeneratedSourceTag(NewBox, ServerTags::AutoConvexFallback);
			++GeneratedCount;

			UE_LOG(LogTemp, Log, TEXT("[ServerMapExporter] Generated convex fallback box %d: Center=%s Extent=%s"),
				ConvexIndex,
				*LocalCenter.ToString(),
				*LocalExtent.ToString());
		}
	}

	return GeneratedCount;
}

UBoxComponent* ServerMapExporter::CreateGeneratedBoxComponent(AActor* OwnerActor, USceneComponent* AttachParent,
                                                              const FVector& RelativeLocation, const FRotator& RelativeRotation, const FVector& BoxExtent)
{
	if (OwnerActor == nullptr || AttachParent == nullptr)
	{
		return nullptr;
	}

	UBoxComponent* NewBoxComponent = NewObject<UBoxComponent>(OwnerActor);
	if (NewBoxComponent == nullptr)
	{
		return nullptr;
	}

	NewBoxComponent->SetBoxExtent(BoxExtent);
	AddDefaultGeneratedTags(NewBoxComponent);

	NewBoxComponent->SetupAttachment(AttachParent);
	NewBoxComponent->SetRelativeLocation(RelativeLocation);
	NewBoxComponent->SetRelativeRotation(RelativeRotation);

	OwnerActor->AddInstanceComponent(NewBoxComponent);
	NewBoxComponent->RegisterComponent();

#if WITH_EDITOR
	OwnerActor->Modify();
	NewBoxComponent->Modify();
#endif

	return NewBoxComponent;
}

USphereComponent* ServerMapExporter::CreateGeneratedSphereComponent(AActor* OwnerActor, USceneComponent* AttachParent,
	const FVector& RelativeLocation, float Radius)
{
	if (OwnerActor == nullptr || AttachParent == nullptr)
	{
		return nullptr;
	}

	USphereComponent* NewSphereComponent = NewObject<USphereComponent>(OwnerActor);
	if (NewSphereComponent == nullptr)
	{
		return nullptr;
	}

	NewSphereComponent->SetSphereRadius(Radius);
	AddDefaultGeneratedTags(NewSphereComponent);

	NewSphereComponent->SetupAttachment(AttachParent);
	NewSphereComponent->SetRelativeLocation(RelativeLocation);
	NewSphereComponent->SetRelativeRotation(FRotator::ZeroRotator);

	OwnerActor->AddInstanceComponent(NewSphereComponent);
	NewSphereComponent->RegisterComponent();

#if WITH_EDITOR
	OwnerActor->Modify();
	NewSphereComponent->Modify();
#endif

	return NewSphereComponent;
}

UCapsuleComponent* ServerMapExporter::CreateGeneratedCapsuleComponent(AActor* OwnerActor, USceneComponent* AttachParent,
	const FVector& RelativeLocation, const FRotator& RelativeRotation, float Radius, float HalfHeight)
{
	if (OwnerActor == nullptr || AttachParent == nullptr)
	{
		return nullptr;
	}

	UCapsuleComponent* NewCapsuleComponent = NewObject<UCapsuleComponent>(OwnerActor);
	if (NewCapsuleComponent == nullptr)
	{
		return nullptr;
	}

	NewCapsuleComponent->SetCapsuleRadius(Radius);
	NewCapsuleComponent->SetCapsuleHalfHeight(HalfHeight);

	AddDefaultGeneratedTags(NewCapsuleComponent);

	NewCapsuleComponent->SetupAttachment(AttachParent);
	NewCapsuleComponent->SetRelativeLocation(RelativeLocation);
	NewCapsuleComponent->SetRelativeRotation(RelativeRotation);

	OwnerActor->AddInstanceComponent(NewCapsuleComponent);
	NewCapsuleComponent->RegisterComponent();

#if WITH_EDITOR
	OwnerActor->Modify();
	NewCapsuleComponent->Modify();
#endif

	return NewCapsuleComponent;
}

UBoxComponent* ServerMapExporter::CreateGeneratedBoundsBoxComponent(UStaticMeshComponent* StaticMeshComponent,
	AActor* OwnerActor)
{
	if (StaticMeshComponent == nullptr || OwnerActor == nullptr)
	{
		return nullptr;
	}

	UStaticMesh* StaticMesh = StaticMeshComponent->GetStaticMesh();
	if (StaticMesh == nullptr)
	{
		return nullptr;
	}

	const FBox LocalMeshBox = StaticMesh->GetBoundingBox();
	if (!LocalMeshBox.IsValid)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ServerMapExporter] Invalid mesh bounds: %s"), *GetNameSafe(StaticMesh));
		return nullptr;
	}

	const FVector LocalCenter = LocalMeshBox.GetCenter();
	const FVector LocalExtent = LocalMeshBox.GetExtent();

	if (LocalExtent.X <= KINDA_SMALL_NUMBER ||
		LocalExtent.Y <= KINDA_SMALL_NUMBER ||
		LocalExtent.Z <= KINDA_SMALL_NUMBER)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ServerMapExporter] Mesh bounds extent too small: %s"), *GetNameSafe(StaticMesh));
		return nullptr;
	}

	UBoxComponent* NewBoxComponent = CreateGeneratedBoxComponent(
		OwnerActor,
		StaticMeshComponent,
		LocalCenter,
		FRotator::ZeroRotator,
		LocalExtent);

	if (NewBoxComponent != nullptr)
	{
		AddGeneratedSourceTag(NewBoxComponent, ServerTags::AutoBoundsFallback);

		UE_LOG(LogTemp, Log, TEXT("[ServerMapExporter] Generated bounds fallback box: Actor=%s Mesh=%s Center=%s Extent=%s"),
			*GetNameSafe(OwnerActor),
			*GetNameSafe(StaticMesh),
			*LocalCenter.ToString(),
			*LocalExtent.ToString());
	}

	return NewBoxComponent;
}

bool ServerMapExporter::SaveSelectedActorGeneratedShapesToPreset(UServerCollisionPresetDataAsset* PresetAsset,
	bool bOnlyGeneratedShapes)
{
	AActor* SelectedActor = GetFirstSelectedActor();
	if (SelectedActor == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ServerMapExporter] No selected actor"));
		return false;
	}
	
	return SaveActorGeneratedShapesToPreset(SelectedActor, PresetAsset, bOnlyGeneratedShapes);
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

UServerCollisionPresetDataAsset* ServerMapExporter::FindPresetForStaticMesh(UStaticMesh* StaticMesh)
{
	if (StaticMesh == nullptr)
	{
		return nullptr;
	}

	FAssetRegistryModule& AssetRegistryModule =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

	TArray<FAssetData> AssetDataList;
	AssetRegistryModule.Get().GetAssetsByClass(
		UServerCollisionPresetDataAsset::StaticClass()->GetClassPathName(),
		AssetDataList);

	for (const FAssetData& AssetData : AssetDataList)
	{
		UServerCollisionPresetDataAsset* PresetAsset =
			Cast<UServerCollisionPresetDataAsset>(AssetData.GetAsset());

		if (PresetAsset == nullptr)
		{
			continue;
		}

		if (PresetAsset->SourceStaticMesh == StaticMesh)
		{
			return PresetAsset;
		}
	}

	return nullptr;
}

UServerCollisionPresetDataAsset* ServerMapExporter::FindOrCreatePresetForStaticMesh(UStaticMesh* StaticMesh,
	const FString& PresetFolderPath)
{
	if (StaticMesh == nullptr)
	{
		return nullptr;
	}

	if (UServerCollisionPresetDataAsset* ExistingPreset = FindPresetForStaticMesh(StaticMesh))
	{
		return ExistingPreset;
	}

#if WITH_EDITOR
	FString PackagePath = PresetFolderPath;
	if (PackagePath.IsEmpty())
	{
		PackagePath = TEXT("/Game/ServerMap/Presets");
	}

	const FString AssetName = FString::Printf(TEXT("DA_%s_ServerCollision"), *StaticMesh->GetName());

	FAssetToolsModule& AssetToolsModule =
		FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");

	UDataAssetFactory* Factory = NewObject<UDataAssetFactory>();
	Factory->DataAssetClass = UServerCollisionPresetDataAsset::StaticClass();

	UObject* NewAssetObject = AssetToolsModule.Get().CreateAsset(
		AssetName,
		PackagePath,
		UServerCollisionPresetDataAsset::StaticClass(),
		Factory);

	UServerCollisionPresetDataAsset* NewPreset =
		Cast<UServerCollisionPresetDataAsset>(NewAssetObject);

	if (NewPreset != nullptr)
	{
		NewPreset->SourceStaticMesh = StaticMesh;
		NewPreset->MarkPackageDirty();

		UE_LOG(LogTemp, Log, TEXT("[ServerMapExporter] Created preset: %s for mesh %s"),
			*GetNameSafe(NewPreset),
			*GetNameSafe(StaticMesh));
	}

	return NewPreset;
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

int32 ServerMapExporter::CountGeneratedShapeComponents(AActor* Actor)
{
	if (Actor == nullptr)
	{
		return 0;
	}

	TArray<UShapeComponent*> Shapes;
	CollectShapeComponents(Actor, Shapes);

	int32 Count = 0;
	for (UShapeComponent* Shape : Shapes)
	{
		if (Shape && Shape->ComponentHasTag(ServerTags::Generated))
		{
			++Count;
		}
	}
	return Count;
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

void ServerMapExporter::CollectAuthoringShapeComponents(AActor* Actor, bool bOnlyGeneratedShapes,
	TArray<UShapeComponent*>& OutShapeComponents)
{
	OutShapeComponents.Reset();

	if (Actor == nullptr)
	{
		return;
	}

	TArray<UShapeComponent*> AllShapes;
	CollectShapeComponents(Actor, AllShapes);

	for (UShapeComponent* ShapeComponent : AllShapes)
	{
		if (ShapeComponent == nullptr)
		{
			continue;
		}

		if (ShapeComponent->ComponentHasTag(ServerTags::Ignore))
		{
			continue;
		}

		if (bOnlyGeneratedShapes)
		{
			if (!ShapeComponent->ComponentHasTag(ServerTags::Generated))
			{
				continue;
			}
		}
		else
		{
			const bool bIsGenerated = ShapeComponent->ComponentHasTag(ServerTags::Generated);
			const bool bIsManualApproved = ShapeComponent->ComponentHasTag(ServerTags::ManualApproved);

			if (!bIsGenerated && !bIsManualApproved)
			{
				continue;
			}
		}

		OutShapeComponents.Add(ShapeComponent);
	}
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

void ServerMapExporter::AppendValidationItem(FServerMapValidationReport& Report, AActor* Actor, const FString& Reason,
	UStaticMesh* StaticMesh, UServerCollisionPresetDataAsset* PresetAsset)
{
	FServerMapValidationItem Item;
	Item.ActorName = GetNameSafe(Actor);
	Item.Reason = Reason;
	Item.StaticMeshName = GetNameSafe(StaticMesh);
	Item.PresetName = GetNameSafe(PresetAsset);
	Report.Items.Add(MoveTemp(Item));
}

void ServerMapExporter::LogValidationReport(const FName& RequiredTag, const FServerMapValidationReport& Report)
{
	UE_LOG(LogTemp, Log, TEXT("========== ServerMap Validation Report =========="));
	UE_LOG(LogTemp, Log, TEXT("[ServerMapExporter] Tag                    : %s"), *RequiredTag.ToString());
	UE_LOG(LogTemp, Log, TEXT("[ServerMapExporter] Tagged Actors          : %d"), Report.TaggedActorCount);
	UE_LOG(LogTemp, Log, TEXT("[ServerMapExporter] Actors With Shapes     : %d"), Report.ActorsWithValidShapes);
	UE_LOG(LogTemp, Log, TEXT("[ServerMapExporter] Actors Using Preset    : %d"), Report.ActorsUsingPreset);
	UE_LOG(LogTemp, Log, TEXT("[ServerMapExporter] Missing StaticMeshComp : %d"), Report.ActorsMissingStaticMesh);
	UE_LOG(LogTemp, Log, TEXT("[ServerMapExporter] Null StaticMesh        : %d"), Report.ActorsWithNullStaticMesh);
	UE_LOG(LogTemp, Log, TEXT("[ServerMapExporter] Missing Preset         : %d"), Report.ActorsMissingPreset);
	UE_LOG(LogTemp, Log, TEXT("[ServerMapExporter] Generated Simple      : %d"), Report.ActorsGeneratedFromSimple);
	UE_LOG(LogTemp, Log, TEXT("[ServerMapExporter] Generated Convex FB   : %d"), Report.ActorsGeneratedFromConvexFallback);
	UE_LOG(LogTemp, Log, TEXT("[ServerMapExporter] Generated Bounds FB   : %d"), Report.ActorsGeneratedFromBoundsFallback);
	for (const FServerMapValidationItem& Item : Report.Items)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ServerMapExporter] ValidationItem Actor=%s Reason=%s StaticMesh=%s Preset=%s"),
			*Item.ActorName,
			*Item.Reason,
			*Item.StaticMeshName,
			*Item.PresetName);
	}

	UE_LOG(LogTemp, Log, TEXT("================================================="));
}

int32 ServerMapExporter::BuildColliderDataListFromActorResolved(AActor* Actor,
                                                                TArray<se::map::ColliderData>& OutColliders, TArray<FServerMapDebugColliderRecord>& OutDebugRecords,
                                                                FServerMapExportSummary& Summary)
{
	if (Actor == nullptr)
	{
		return 0;
	}

	if (HasValidShapeComponent(Actor))
	{
		UE_LOG(LogTemp, Log, TEXT("[ServerMapExporter] Using ShapeComponents: %s"), *GetNameSafe(Actor));
		++Summary.ShapeSourceActorCount;

		return BuildColliderDataListFromActor(Actor, OutColliders, OutDebugRecords, Summary);
	}

	UStaticMeshComponent* StaticMeshComponent = FindFirstStaticMeshComponent(Actor);
	if (StaticMeshComponent == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ServerMapExporter] No UStaticMeshComponent and no valid shapes: %s"), *GetNameSafe(Actor));
		++Summary.MissingPresetActorCount;
		return 0;
	}

	UStaticMesh* StaticMesh = StaticMeshComponent->GetStaticMesh();
	if (StaticMesh == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ServerMapExporter] StaticMesh is null: %s"), *GetNameSafe(Actor));
		++Summary.MissingPresetActorCount;
		return 0;
	}

	UServerCollisionPresetDataAsset* PresetAsset = FindPresetForStaticMesh(StaticMesh);
	if (PresetAsset == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ServerMapExporter] No preset found for StaticMesh: %s"), *GetNameSafe(StaticMesh));
		++Summary.MissingPresetActorCount;
		return 0;
	}

	UE_LOG(LogTemp, Log, TEXT("[ServerMapExporter] Using Preset: Actor=%s Preset=%s"),
		*GetNameSafe(Actor),
		*GetNameSafe(PresetAsset));

	++Summary.PresetSourceActorCount;

	return BuildColliderDataListFromPreset(Actor, StaticMeshComponent, PresetAsset, OutColliders, OutDebugRecords, Summary);
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
	UE_LOG(LogTemp, Log, TEXT("[ServerMapExporter] Shape Source Actors : %d"), Summary.ShapeSourceActorCount);
	UE_LOG(LogTemp, Log, TEXT("[ServerMapExporter] Preset Source Actors: %d"), Summary.PresetSourceActorCount);
	UE_LOG(LogTemp, Log, TEXT("[ServerMapExporter] Missing Preset Actors: %d"), Summary.MissingPresetActorCount);
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

bool ServerMapExporter::SaveActorGeneratedShapesToPreset(AActor* Actor, UServerCollisionPresetDataAsset* PresetAsset,
	bool bOnlyGeneratedShapes)
{
	if (Actor == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ServerMapExporter] Actor is null"));
		return false;
	}

	if (PresetAsset == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ServerMapExporter] PresetAsset is null"));
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

	TArray<UShapeComponent*> AuthoringShapes;
	CollectAuthoringShapeComponents(Actor, bOnlyGeneratedShapes, AuthoringShapes);

	if (AuthoringShapes.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[ServerMapExporter] No authoring shape components found: %s"), *GetNameSafe(Actor));
		return false;
	}

	TArray<FServerCollisionPresetCollider> NewColliders;
	NewColliders.Reserve(AuthoringShapes.Num());

	for (const UShapeComponent* ShapeComponent : AuthoringShapes)
	{
		if (ShapeComponent == nullptr)
		{
			continue;
		}

		if (!ShouldExportShapeComponent(ShapeComponent) || !IsValidShapeComponentForExport(ShapeComponent))
		{
			continue;
		}

		FServerCollisionPresetCollider PresetCollider;
		if (BuildPresetColliderFromShapeComponent(ShapeComponent, PresetCollider))
		{
			NewColliders.Add(MoveTemp(PresetCollider));
		}
	}

	if (NewColliders.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[ServerMapExporter] Failed to build preset colliders from generated shapes: %s"), *GetNameSafe(Actor));
		return false;
	}

#if WITH_EDITOR
	PresetAsset->Modify();
#endif

	PresetAsset->SourceStaticMesh = StaticMesh;
	PresetAsset->Colliders = MoveTemp(NewColliders);

#if WITH_EDITOR
	PresetAsset->MarkPackageDirty();
#endif

	UE_LOG(LogTemp, Log, TEXT("[ServerMapExporter] Saved %d colliders to preset: %s (Mesh=%s)"),
		PresetAsset->Colliders.Num(),
		*GetNameSafe(PresetAsset),
		*GetNameSafe(StaticMesh));

	return true;
}

void ServerMapExporter::CollectGeneratedShapeComponents(AActor* Actor, TArray<UShapeComponent*>& OutShapeComponents)
{
	OutShapeComponents.Reset();

	if (Actor == nullptr)
	{
		return;
	}

	TArray<UShapeComponent*> AllShapes;
	CollectShapeComponents(Actor, AllShapes);

	for (UShapeComponent* ShapeComponent : AllShapes)
	{
		if (ShapeComponent == nullptr)
		{
			continue;
		}

		if (!ShapeComponent->ComponentHasTag(ServerTags::Generated))
		{
			continue;
		}

		OutShapeComponents.Add(ShapeComponent);
	}
}

bool ServerMapExporter::BuildPresetColliderFromShapeComponent(const UShapeComponent* ShapeComponent,
	FServerCollisionPresetCollider& OutPresetCollider)
{
	if (ShapeComponent == nullptr)
	{
		return false;
	}

	if (const UBoxComponent* BoxComponent = Cast<UBoxComponent>(ShapeComponent))
	{
		return BuildPresetColliderFromBoxComponent(BoxComponent, OutPresetCollider);
	}

	if (const USphereComponent* SphereComponent = Cast<USphereComponent>(ShapeComponent))
	{
		return BuildPresetColliderFromSphereComponent(SphereComponent, OutPresetCollider);
	}

	if (const UCapsuleComponent* CapsuleComponent = Cast<UCapsuleComponent>(ShapeComponent))
	{
		return BuildPresetColliderFromCapsuleComponent(CapsuleComponent, OutPresetCollider);
	}

	return false;
}

bool ServerMapExporter::BuildPresetColliderFromBoxComponent(const UBoxComponent* BoxComponent,
	FServerCollisionPresetCollider& OutPresetCollider)
{
	if (BoxComponent == nullptr)
	{
		return false;
	}

	OutPresetCollider = {};
	OutPresetCollider.ShapeType = EServerColliderShapeType::Box;
	OutPresetCollider.LocalPosition = BoxComponent->GetRelativeLocation();
	OutPresetCollider.LocalRotation = BoxComponent->GetRelativeRotation();
	OutPresetCollider.BoxExtent = BoxComponent->GetUnscaledBoxExtent();
	OutPresetCollider.Radius = 0.0f;
	OutPresetCollider.HalfHeight = 0.0f;
	OutPresetCollider.bBlockMovement = BoxComponent->ComponentHasTag(ServerTags::BlockMovement);
	OutPresetCollider.bBlockProjectile = BoxComponent->ComponentHasTag(ServerTags::BlockProjectile);
	OutPresetCollider.DebugName = FName(GetNameSafe(BoxComponent));

	return true;
}

bool ServerMapExporter::BuildPresetColliderFromSphereComponent(const USphereComponent* SphereComponent,
	FServerCollisionPresetCollider& OutPresetCollider)
{
	if (SphereComponent == nullptr)
	{
		return false;
	}

	OutPresetCollider = {};
	OutPresetCollider.ShapeType = EServerColliderShapeType::Sphere;
	OutPresetCollider.LocalPosition = SphereComponent->GetRelativeLocation();
	OutPresetCollider.LocalRotation = FRotator::ZeroRotator;
	OutPresetCollider.BoxExtent = FVector::ZeroVector;
	OutPresetCollider.Radius = SphereComponent->GetUnscaledSphereRadius();
	OutPresetCollider.HalfHeight = 0.0f;
	OutPresetCollider.bBlockMovement = SphereComponent->ComponentHasTag(ServerTags::BlockMovement);
	OutPresetCollider.bBlockProjectile = SphereComponent->ComponentHasTag(ServerTags::BlockProjectile);
	OutPresetCollider.DebugName = FName(GetNameSafe(SphereComponent));

	return true;
}

bool ServerMapExporter::BuildPresetColliderFromCapsuleComponent(const UCapsuleComponent* CapsuleComponent,
	FServerCollisionPresetCollider& OutPresetCollider)
{
	if (CapsuleComponent == nullptr)
	{
		return false;
	}

	OutPresetCollider = {};
	OutPresetCollider.ShapeType = EServerColliderShapeType::Capsule;
	OutPresetCollider.LocalPosition = CapsuleComponent->GetRelativeLocation();
	OutPresetCollider.LocalRotation = CapsuleComponent->GetRelativeRotation();
	OutPresetCollider.BoxExtent = FVector::ZeroVector;
	OutPresetCollider.Radius = CapsuleComponent->GetUnscaledCapsuleRadius();
	OutPresetCollider.HalfHeight = CapsuleComponent->GetUnscaledCapsuleHalfHeight();
	OutPresetCollider.bBlockMovement = CapsuleComponent->ComponentHasTag(ServerTags::BlockMovement);
	OutPresetCollider.bBlockProjectile = CapsuleComponent->ComponentHasTag(ServerTags::BlockProjectile);
	OutPresetCollider.DebugName = FName(GetNameSafe(CapsuleComponent));

	return true;
}

int32 ServerMapExporter::BuildColliderDataListFromPreset(AActor* Actor, UStaticMeshComponent* StaticMeshComponent,
	const UServerCollisionPresetDataAsset* PresetAsset, TArray<se::map::ColliderData>& OutColliders,
	TArray<FServerMapDebugColliderRecord>& OutDebugRecords, FServerMapExportSummary& Summary)
{
	if (Actor == nullptr || StaticMeshComponent == nullptr || PresetAsset == nullptr)
	{
		return 0;
	}

	const int32 PrevCount = OutColliders.Num();
	const FTransform MeshWorldTransform = StaticMeshComponent->GetComponentTransform();

	for (const FServerCollisionPresetCollider& PresetCollider : PresetAsset->Colliders)
	{
		se::map::ColliderData ColliderData;
		if (!BuildWorldColliderDataFromPresetCollider(PresetCollider, MeshWorldTransform, ColliderData))
		{
			continue;
		}

		OutColliders.Add(ColliderData);
		AccumulateSummary(ColliderData, Summary);

		FServerMapDebugColliderRecord Record;
		Record.ActorName = GetNameSafe(Actor);
		Record.ComponentName = PresetCollider.DebugName.IsNone()
			? TEXT("PresetCollider")
			: PresetCollider.DebugName.ToString();
		Record.ColliderData = ColliderData;
		OutDebugRecords.Add(MoveTemp(Record));
	}

	return OutColliders.Num() - PrevCount;
}

bool ServerMapExporter::BuildWorldColliderDataFromPresetCollider(const FServerCollisionPresetCollider& PresetCollider,
	const FTransform& MeshComponentWorldTransform, se::map::ColliderData& OutColliderData)
{
	OutColliderData = {};

	const FTransform LocalTransform(PresetCollider.LocalRotation, PresetCollider.LocalPosition, FVector::OneVector);
	const FTransform WorldTransform = LocalTransform * MeshComponentWorldTransform;

	const FVector WorldLocation = WorldTransform.GetLocation();
	const FRotator WorldRotation = WorldTransform.Rotator();
	const FVector WorldScale = MeshComponentWorldTransform.GetScale3D().GetAbs();

	OutColliderData.position = {
		static_cast<float>(WorldLocation.X),
		static_cast<float>(WorldLocation.Y),
		static_cast<float>(WorldLocation.Z)
	};

	OutColliderData.rotationDeg = {
		static_cast<float>(WorldRotation.Pitch),
		static_cast<float>(WorldRotation.Yaw),
		static_cast<float>(WorldRotation.Roll)
	};

	uint32 Flags = se::map::Collider_None;
	if (PresetCollider.bBlockMovement)
	{
		Flags |= se::map::Collider_BlockMovement;
	}
	if (PresetCollider.bBlockProjectile)
	{
		Flags |= se::map::Collider_BlockProjectile;
	}
	OutColliderData.flags = Flags;

	switch (PresetCollider.ShapeType)
	{
	case EServerColliderShapeType::Box:
	{
		OutColliderData.type = se::map::ColliderType::OBB;

		const FVector ScaledExtent = FVector(
			PresetCollider.BoxExtent.X * WorldScale.X,
			PresetCollider.BoxExtent.Y * WorldScale.Y,
			PresetCollider.BoxExtent.Z * WorldScale.Z);

		OutColliderData.extents = {
			static_cast<float>(ScaledExtent.X),
			static_cast<float>(ScaledExtent.Y),
			static_cast<float>(ScaledExtent.Z)
		};

		OutColliderData.radius = 0.0f;
		OutColliderData.halfHeight = 0.0f;
		return true;
	}

	case EServerColliderShapeType::Sphere:
	{
		OutColliderData.type = se::map::ColliderType::Sphere;

		const float UniformScale = FMath::Max3(WorldScale.X, WorldScale.Y, WorldScale.Z);
		const float ScaledRadius = PresetCollider.Radius * UniformScale;

		OutColliderData.extents = { 0.0f, 0.0f, 0.0f };
		OutColliderData.radius = ScaledRadius;
		OutColliderData.halfHeight = 0.0f;
		return true;
	}

	case EServerColliderShapeType::Capsule:
	{
		OutColliderData.type = se::map::ColliderType::Capsule;

		const float RadiusScale = FMath::Max(WorldScale.X, WorldScale.Y);
		const float HeightScale = WorldScale.Z;

		OutColliderData.extents = { 0.0f, 0.0f, 0.0f };
		OutColliderData.radius = PresetCollider.Radius * RadiusScale;
		OutColliderData.halfHeight = PresetCollider.HalfHeight * HeightScale;
		return true;
	}

	default:
		return false;
	}
}

void ServerMapExporter::AccumulateGeneratedSourceStats(AActor* Actor, FServerMapValidationReport& Report)
{
	if (Actor == nullptr)
	{
		return;
	}

	TArray<UShapeComponent*> Shapes;
	CollectShapeComponents(Actor, Shapes);

	bool bHasSimple = false;
	bool bHasConvex = false;
	bool bHasBounds = false;

	for (UShapeComponent* Shape : Shapes)
	{
		if (Shape == nullptr)
		{
			continue;
		}

		if (!Shape->ComponentHasTag(ServerTags::Generated))
		{
			continue;
		}

		bHasSimple |= Shape->ComponentHasTag(ServerTags::AutoSimple);
		bHasConvex |= Shape->ComponentHasTag(ServerTags::AutoConvexFallback);
		bHasBounds |= Shape->ComponentHasTag(ServerTags::AutoBoundsFallback);
	}

	if (bHasSimple)
	{
		++Report.ActorsGeneratedFromSimple;
	}
	if (bHasConvex)
	{
		++Report.ActorsGeneratedFromConvexFallback;
	}
	if (bHasBounds)
	{
		++Report.ActorsGeneratedFromBoundsFallback;
	}
}

