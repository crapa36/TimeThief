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

    FExportedNavData ExportData;
    ExportData.Meta.MapName = World->GetMapName();

    UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(World);
    ANavigationData* NavData = nullptr;
    ARecastNavMesh* RecastNavMesh = nullptr;

    if (NavSys)
    {
        NavData = NavSys->GetDefaultNavDataInstance();
        if (NavData)
        {
            ExportData.Meta.NavDataClassName = NavData->GetClass()->GetName();
            RecastNavMesh = Cast<ARecastNavMesh>(NavData);
        }
    }

    if (RecastNavMesh)
    {
        const int32 TileCount = RecastNavMesh->GetNavMeshTilesCount();
        ExportData.Meta.TileCount = TileCount;
        
        const dtNavMesh* DetourNavMesh = RecastNavMesh->GetRecastMesh();
        if (!DetourNavMesh)
        {
            UE_LOG(LogTemp, Error, TEXT("[ServerNavExporter] DetourNavMesh is null."));
            return false;
        }
        
        for (int32 TileIndex = 0; TileIndex < TileCount; ++TileIndex)
        {
            const dtMeshTile* Tile = DetourNavMesh->getTile(TileIndex);
            if (!Tile || !Tile->header)
            {
                continue;
            }
            
            FBox TileBounds = Recast2UnrealBox(Tile->header->bmin, Tile->header->bmax);
            if (!TileBounds.IsValid)
            {
                continue;
            }
            
            FExportedNavTile OutTile;
            OutTile.TileIndex = TileIndex;
            OutTile.MinBound = TileBounds.Min;
            OutTile.MaxBound = TileBounds.Max;
            
            for (int32 PolyIndex = 0; PolyIndex < Tile->header->polyCount; ++PolyIndex)
            {
                const dtPoly* Poly = &Tile->polys[PolyIndex];

                if (!Poly)
                    continue;

                FExportedNavPoly OutPoly;
                OutPoly.PolyId = PolyIndex;

                for (int32 v = 0; v < Poly->vertCount; ++v)
                {
                    int32 VertIndex = Poly->verts[v];
                    const dtReal* V = &Tile->verts[VertIndex * 3];

                    FVector UnrealPos = Recast2UnrealPoint(V);

                    OutPoly.Vertices.Add(UnrealPos);
                }

                OutTile.Polys.Add(MoveTemp(OutPoly));
            }
            
            ExportData.Tiles.Add(MoveTemp(OutTile));
        }
        
        const FNavDataConfig& Config = RecastNavMesh->GetConfig();

        ExportData.Meta.AgentRadius = Config.AgentRadius;
        ExportData.Meta.AgentHeight = Config.AgentHeight;
        ExportData.Meta.AgentStepHeight = Config.AgentStepHeight;
        // ExportData.Meta.AgentMaxSlope = Config.AgentSlope;
        ExportData.Meta.AgentMaxSlope = 0.0f;

        ExportData.Meta.CellSize = RecastNavMesh->GetCellSize(ENavigationDataResolution::Default);
        ExportData.Meta.CellHeight = RecastNavMesh->GetCellHeight(ENavigationDataResolution::Default);
        ExportData.Meta.TileSizeUU = RecastNavMesh->GetTileSizeUU();
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[ServerNavExporter] Default NavData is not ARecastNavMesh."));
    }

    for (TActorIterator<ANavLinkProxy> It(World); It; ++It)
    {
        ANavLinkProxy* LinkProxy = *It;
        if (LinkProxy == nullptr)
        {
            continue;
        }

        for (const FNavigationLink& Link : LinkProxy->PointLinks)
        {
            FExportedNavLink OutLink;

            OutLink.Name = LinkProxy->GetActorNameOrLabel();
            OutLink.Start = LinkProxy->GetActorTransform().TransformPosition(Link.Left);
            OutLink.End = LinkProxy->GetActorTransform().TransformPosition(Link.Right);
            OutLink.bBidirectional = (Link.Direction == ENavLinkDirection::BothWays);
            OutLink.bSmartLinkIsRelevant = LinkProxy->bSmartLinkIsRelevant;
            OutLink.bSmartLinkEnabled = LinkProxy->IsSmartLinkEnabled();

            if (NavSys)
            {
                FNavLocation ProjectedStart;
                FNavLocation ProjectedEnd;

                OutLink.bStartProjected = NavSys->ProjectPointToNavigation(
                    OutLink.Start, ProjectedStart, FVector(50.f, 50.f, 200.f), NavData);

                OutLink.bEndProjected = NavSys->ProjectPointToNavigation(
                    OutLink.End, ProjectedEnd, FVector(50.f, 50.f, 200.f), NavData);

                if (OutLink.bStartProjected)
                {
                    OutLink.ProjectedStart = ProjectedStart.Location;
                }

                if (OutLink.bEndProjected)
                {
                    OutLink.ProjectedEnd = ProjectedEnd.Location;
                }
            }

            ExportData.Links.Add(MoveTemp(OutLink));
        }
        
        if (LinkProxy->bSmartLinkIsRelevant)
        {
            if (UNavLinkCustomComponent* SmartComp = LinkProxy->GetSmartLinkComp())
            {
                if (SmartComp->IsEnabled())
                {
                    FVector Start, End;
                    ENavLinkDirection::Type Direction;
                    
                    SmartComp->GetLinkData(Start, End, Direction);

                    FExportedNavLink OutLink;
                    OutLink.Name = LinkProxy->GetActorNameOrLabel() + TEXT("_Smart");
                    OutLink.Start = Start;
                    OutLink.End = End;
                    OutLink.bBidirectional = (Direction == ENavLinkDirection::BothWays);
                    OutLink.bSmartLinkIsRelevant = true;
                    OutLink.bSmartLinkEnabled = true;

                    if (NavSys)
                    {
                        FNavLocation ProjectedStart;
                        FNavLocation ProjectedEnd;

                        OutLink.bStartProjected = NavSys->ProjectPointToNavigation(
                            OutLink.Start, ProjectedStart, FVector(50.f, 50.f, 200.f), NavData);

                        OutLink.bEndProjected = NavSys->ProjectPointToNavigation(
                            OutLink.End, ProjectedEnd, FVector(50.f, 50.f, 200.f), NavData);

                        if (OutLink.bStartProjected)
                        {
                            OutLink.ProjectedStart = ProjectedStart.Location;
                        }

                        if (OutLink.bEndProjected)
                        {
                            OutLink.ProjectedEnd = ProjectedEnd.Location;
                        }
                    }

                    ExportData.Links.Add(MoveTemp(OutLink));
                }
            }
        }
    }

    TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();

    {
	    TSharedRef<FJsonObject> MetaObject = MakeShared<FJsonObject>();
	    MetaObject->SetStringField(TEXT("map_name"), ExportData.Meta.MapName);
	    MetaObject->SetStringField(TEXT("nav_data_class"), ExportData.Meta.NavDataClassName);
	    MetaObject->SetStringField(TEXT("coordinate_system"), ExportData.Meta.CoordinateSystem);

	    MetaObject->SetNumberField(TEXT("agent_radius"), ExportData.Meta.AgentRadius);
	    MetaObject->SetNumberField(TEXT("agent_height"), ExportData.Meta.AgentHeight);
	    MetaObject->SetNumberField(TEXT("agent_step_height"), ExportData.Meta.AgentStepHeight);
	    MetaObject->SetNumberField(TEXT("agent_max_slope"), ExportData.Meta.AgentMaxSlope);

	    MetaObject->SetNumberField(TEXT("cell_size"), ExportData.Meta.CellSize);
	    MetaObject->SetNumberField(TEXT("cell_height"), ExportData.Meta.CellHeight);
	    MetaObject->SetNumberField(TEXT("tile_size_uu"), ExportData.Meta.TileSizeUU);
	    
	    MetaObject->SetNumberField(TEXT("tile_count"), ExportData.Meta.TileCount);

	    RootObject->SetObjectField(TEXT("meta"), MetaObject);
    }

    {
        TArray<TSharedPtr<FJsonValue>> LinkArray;

        for (const FExportedNavLink& Link : ExportData.Links)
        {
            TSharedRef<FJsonObject> LinkObject = MakeShared<FJsonObject>();

            LinkObject->SetStringField(TEXT("name"), Link.Name);

            {
                TArray<TSharedPtr<FJsonValue>> StartArray;
                StartArray.Add(MakeShared<FJsonValueNumber>(Link.Start.X));
                StartArray.Add(MakeShared<FJsonValueNumber>(Link.Start.Y));
                StartArray.Add(MakeShared<FJsonValueNumber>(Link.Start.Z));
                LinkObject->SetArrayField(TEXT("start"), StartArray);
            }

            {
                TArray<TSharedPtr<FJsonValue>> EndArray;
                EndArray.Add(MakeShared<FJsonValueNumber>(Link.End.X));
                EndArray.Add(MakeShared<FJsonValueNumber>(Link.End.Y));
                EndArray.Add(MakeShared<FJsonValueNumber>(Link.End.Z));
                LinkObject->SetArrayField(TEXT("end"), EndArray);
            }

            LinkObject->SetBoolField(TEXT("bidirectional"), Link.bBidirectional);
            LinkObject->SetBoolField(TEXT("smart_link_relevant"), Link.bSmartLinkIsRelevant);
            LinkObject->SetBoolField(TEXT("smart_link_enabled"), Link.bSmartLinkEnabled);
            
            LinkObject->SetBoolField(TEXT("start_projected"), Link.bStartProjected);
            LinkObject->SetBoolField(TEXT("end_projected"), Link.bEndProjected);

            {
                TArray<TSharedPtr<FJsonValue>> ProjectedStartArray;
                ProjectedStartArray.Add(MakeShared<FJsonValueNumber>(Link.ProjectedStart.X));
                ProjectedStartArray.Add(MakeShared<FJsonValueNumber>(Link.ProjectedStart.Y));
                ProjectedStartArray.Add(MakeShared<FJsonValueNumber>(Link.ProjectedStart.Z));
                LinkObject->SetArrayField(TEXT("projected_start"), ProjectedStartArray);
            }

            {
                TArray<TSharedPtr<FJsonValue>> ProjectedEndArray;
                ProjectedEndArray.Add(MakeShared<FJsonValueNumber>(Link.ProjectedEnd.X));
                ProjectedEndArray.Add(MakeShared<FJsonValueNumber>(Link.ProjectedEnd.Y));
                ProjectedEndArray.Add(MakeShared<FJsonValueNumber>(Link.ProjectedEnd.Z));
                LinkObject->SetArrayField(TEXT("projected_end"), ProjectedEndArray);
            }

            LinkArray.Add(MakeShared<FJsonValueObject>(LinkObject));
        }

        RootObject->SetArrayField(TEXT("links"), LinkArray);
    }
    
	{
        TArray<TSharedPtr<FJsonValue>> TileArray;

        for (const FExportedNavTile& Tile : ExportData.Tiles)
        {
            TSharedRef<FJsonObject> TileObject = MakeShared<FJsonObject>();

            TileObject->SetNumberField(TEXT("tile_index"), Tile.TileIndex);

            // Min Bound
            {
                TArray<TSharedPtr<FJsonValue>> MinArray;
                MinArray.Add(MakeShared<FJsonValueNumber>(Tile.MinBound.X));
                MinArray.Add(MakeShared<FJsonValueNumber>(Tile.MinBound.Y));
                MinArray.Add(MakeShared<FJsonValueNumber>(Tile.MinBound.Z));
                TileObject->SetArrayField(TEXT("min"), MinArray);
            }

            // Max Bound
            {
                TArray<TSharedPtr<FJsonValue>> MaxArray;
                MaxArray.Add(MakeShared<FJsonValueNumber>(Tile.MaxBound.X));
                MaxArray.Add(MakeShared<FJsonValueNumber>(Tile.MaxBound.Y));
                MaxArray.Add(MakeShared<FJsonValueNumber>(Tile.MaxBound.Z));
                TileObject->SetArrayField(TEXT("max"), MaxArray);
            }

            // Polys
            TArray<TSharedPtr<FJsonValue>> PolyArray;

            for (const FExportedNavPoly& Poly : Tile.Polys)
            {
                TSharedRef<FJsonObject> PolyObject = MakeShared<FJsonObject>();

                PolyObject->SetNumberField(TEXT("poly_id"), Poly.PolyId);

                // Vertices
                TArray<TSharedPtr<FJsonValue>> VertArray;

                for (const FVector& V : Poly.Vertices)
                {
                    TArray<TSharedPtr<FJsonValue>> Vec;
                    Vec.Add(MakeShared<FJsonValueNumber>(V.X));
                    Vec.Add(MakeShared<FJsonValueNumber>(V.Y));
                    Vec.Add(MakeShared<FJsonValueNumber>(V.Z));

                    VertArray.Add(MakeShared<FJsonValueArray>(Vec));
                }

                PolyObject->SetArrayField(TEXT("vertices"), VertArray);

                PolyArray.Add(MakeShared<FJsonValueObject>(PolyObject));
            }

            TileObject->SetArrayField(TEXT("polys"), PolyArray);

            TileArray.Add(MakeShared<FJsonValueObject>(TileObject));
        }

        RootObject->SetArrayField(TEXT("tiles"), TileArray);
    }

    FString OutputString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
    if (!FJsonSerializer::Serialize(RootObject, Writer))
    {
        return false;
    }

    const FString OutputDir = FPaths::ProjectSavedDir() / TEXT("ServerNavExport");
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

    if (!PlatformFile.DirectoryExists(*OutputDir))
    {
        PlatformFile.CreateDirectoryTree(*OutputDir);
    }

    const FString FilePath = OutputDir / FString::Printf(TEXT("%s_NavLinks.json"), *World->GetName());

    if (!FFileHelper::SaveStringToFile(OutputString, *FilePath))
    {
        return false;
    }

    UE_LOG(LogTemp, Log, TEXT("[ServerNavExporter] Exported NavLinks: %d -> %s"),
        ExportData.Links.Num(), *FilePath);

    return true;
}

#undef LOCTEXT_NAMESPACE
