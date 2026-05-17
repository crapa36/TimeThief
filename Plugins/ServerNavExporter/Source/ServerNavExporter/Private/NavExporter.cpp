#include "NavExporter.h"
#include "NavExportTypes.h"

#include "Editor.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "NavigationSystem.h"
#include "Navigation/NavLinkProxy.h"
#include "NavLinkCustomComponent.h"
#include "NavMesh/RecastNavMesh.h"
#include "NavMesh/RecastHelpers.h"
#include "AI/Navigation/NavAgentInterface.h"
#include "NavigationPath.h"
#include "NavAreas/NavArea.h"
#include "Detour/DetourNavMesh.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"

#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"

#define LOCTEXT_NAMESPACE "ServerNavExporter"

void FNavExporter::ExportCurrentWorldNavData()
{
	UWorld* World = GetEditorWorld();
	if (World == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ServerNavExporter] Editor World is null."));
		return;
	}
	
	if (!ExportWorld(World))
	{
		UE_LOG(LogTemp, Error, TEXT("[ServerNavExporter] Export failed."));
	}
}

UWorld* FNavExporter::GetEditorWorld()
{
	if (GEditor == nullptr) {
		return nullptr;
	}
	
	return GEditor->GetEditorWorldContext().World();
}

bool FNavExporter::ExportWorld(UWorld* World)
{
	check(World);

	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(World);
	if (NavSys == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("[ServerNavExporter] NavigationSystem is null."));
		return false;
	}

	ANavigationData* NavData = NavSys->GetDefaultNavDataInstance();
	if (NavData == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("[ServerNavExporter] Default NavData is null."));
		return false;
	}

	ARecastNavMesh* RecastNavMesh = Cast<ARecastNavMesh>(NavData);
	if (RecastNavMesh == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("[ServerNavExporter] Default NavData is not ARecastNavMesh. Class=%s"),
			*NavData->GetClass()->GetName());
		return false;
	}

	const dtNavMesh* DetourNavMesh = RecastNavMesh->GetRecastMesh();
	if (DetourNavMesh == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("[ServerNavExporter] DetourNavMesh is null."));
		return false;
	}

	const FString OutputDir = FPaths::ProjectSavedDir() / TEXT("ServerNavExport");

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.DirectoryExists(*OutputDir))
	{
		PlatformFile.CreateDirectoryTree(*OutputDir);
	}

	const FString BinaryPath = OutputDir / FString::Printf(TEXT("%s_NavMesh.bin"), *World->GetName());

	if (!SaveBinary(BinaryPath, DetourNavMesh))
	{
		UE_LOG(LogTemp, Error, TEXT("[ServerNavExporter] Failed to save binary NavMesh."));
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("[ServerNavExporter] Exported Detour NavMesh Binary -> %s"), *BinaryPath);

	return true;
}

bool FNavExporter::SaveBinary(const FString& FilePath, const dtNavMesh* DetourNavMesh)
{
	if (DetourNavMesh == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("[ServerNavExporter] DetourNavMesh is null."));
		return false;
	}

	const dtNavMeshParams* Params = DetourNavMesh->getParams();
	if (Params == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("[ServerNavExporter] dtNavMeshParams is null."));
		return false;
	}

	TArray<uint8> Bytes;

	auto AppendBytes = [&Bytes](const void* Data, int64 Size)
	{
		const int32 OldSize = Bytes.Num();
		Bytes.SetNumUninitialized(OldSize + static_cast<int32>(Size));
		FMemory::Memcpy(Bytes.GetData() + OldSize, Data, Size);
	};

	int32 ValidTileCount = 0;

	for (int32 TileIndex = 0; TileIndex < DetourNavMesh->getMaxTiles(); ++TileIndex)
	{
		const dtMeshTile* Tile = DetourNavMesh->getTile(TileIndex);

		if (Tile != nullptr && Tile->header != nullptr && Tile->data != nullptr && Tile->dataSize > 0)
		{
			++ValidTileCount;
		}
	}

	FServerNavBinaryHeader Header;
	Header.Orig[0] = Params->orig[0];
	Header.Orig[1] = Params->orig[1];
	Header.Orig[2] = Params->orig[2];
	Header.TileWidth = Params->tileWidth;
	Header.TileHeight = Params->tileHeight;
	Header.MaxTiles = Params->maxTiles;
	Header.MaxPolys = Params->maxPolys;
	Header.TileCount = ValidTileCount;

	AppendBytes(&Header, sizeof(Header));

	for (int32 TileIndex = 0; TileIndex < DetourNavMesh->getMaxTiles(); ++TileIndex)
	{
		const dtMeshTile* Tile = DetourNavMesh->getTile(TileIndex);

		if (Tile == nullptr || Tile->header == nullptr || Tile->data == nullptr || Tile->dataSize <= 0)
		{
			continue;
		}

		FServerNavTileHeader TileHeader;
		TileHeader.TileRef = DetourNavMesh->getTileRef(Tile);
		TileHeader.TileDataSize = static_cast<uint32>(Tile->dataSize);

		AppendBytes(&TileHeader, sizeof(TileHeader));
		AppendBytes(Tile->data, Tile->dataSize);
	}

	if (!FFileHelper::SaveArrayToFile(Bytes, *FilePath))
	{
		UE_LOG(LogTemp, Error, TEXT("[ServerNavExporter] Failed to save file: %s"), *FilePath);
		return false;
	}

	UE_LOG(LogTemp, Log,
		TEXT("[ServerNavExporter] Saved Detour NavMesh. MaxTiles=%d, ValidTiles=%d, MaxPolys=%d, Path=%s"),
		Header.MaxTiles,
		Header.TileCount,
		Header.MaxPolys,
		*FilePath);

	return true;
}

#undef LOCTEXT_NAMESPACE
