// Copyright Epic Games, Inc. All Rights Reserved.

#include "ServerNavExporter.h"
#include "LevelEditor.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "ToolMenus.h"
#include "NavExporter.h"

static const FName ServerNavExporterTabName("ServerNavExporter");

#define LOCTEXT_NAMESPACE "FServerNavExporterModule"

void FServerNavExporterModule::StartupModule()
{
	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FServerNavExporterModule::RegisterMenus));
}

void FServerNavExporterModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);
}

void FServerNavExporterModule::RegisterMenus()
{
	// Owner will be used for cleanup in call to UToolMenus::UnregisterOwner
	FToolMenuOwnerScoped OwnerScoped(this);

	UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Tools");
	
	FToolMenuSection& Section = Menu->AddSection(
		"ServerNavExporter",
		LOCTEXT("ServerNavExporterSection", "Server Nav"));
	
	Section.AddMenuEntry(
		"ServerNavExport",
		LOCTEXT("ServerNavExport_Label", "Export Server Nav"),
		LOCTEXT("ServerNavExport_Tooltip", "Export Nav Data for Server"),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateStatic(&FNavExporter::ExportCurrentWorldNavData))
	);
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FServerNavExporterModule, ServerNavExporter)