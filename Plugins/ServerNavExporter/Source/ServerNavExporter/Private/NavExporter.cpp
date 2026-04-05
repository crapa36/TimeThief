#include "NavExporter.h"
#include "NavExportTypes.h"

#include "Editor.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "NavigationSystem.h"
#include "Navigation/NavLinkProxy.h"

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
    if (NavSys)
    {
        if (ANavigationData* NavData = NavSys->GetDefaultNavDataInstance())
        {
            ExportData.Meta.NavDataClassName = NavData->GetClass()->GetName();
        }
    }

    for (TActorIterator<ANavLinkProxy> It(World); It; ++It)
    {
        ANavLinkProxy* LinkProxy = *It;
        if (LinkProxy == nullptr)
        {
            continue;
        }

        // PointLinks의 첫 번째 Simple Link만 우선 export
        if (LinkProxy->PointLinks.Num() <= 0)
        {
            continue;
        }

        const FNavigationLink& Link = LinkProxy->PointLinks[0];

        FExportedNavLink OutLink;
        OutLink.Name = LinkProxy->GetActorNameOrLabel();
        OutLink.Start = LinkProxy->GetActorTransform().TransformPosition(Link.Left);
        OutLink.End = LinkProxy->GetActorTransform().TransformPosition(Link.Right);
        OutLink.bBidirectional = (Link.Direction == ENavLinkDirection::BothWays);
        OutLink.bSmartLinkIsRelevant = LinkProxy->bSmartLinkIsRelevant;
        OutLink.bSmartLinkEnabled = LinkProxy->IsSmartLinkEnabled();

        ExportData.Links.Add(MoveTemp(OutLink));
    }

    TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();

    {
        TSharedRef<FJsonObject> MetaObject = MakeShared<FJsonObject>();
        MetaObject->SetStringField(TEXT("map_name"), ExportData.Meta.MapName);
        MetaObject->SetStringField(TEXT("nav_data_class"), ExportData.Meta.NavDataClassName);
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

            LinkArray.Add(MakeShared<FJsonValueObject>(LinkObject));
        }

        RootObject->SetArrayField(TEXT("links"), LinkArray);
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
