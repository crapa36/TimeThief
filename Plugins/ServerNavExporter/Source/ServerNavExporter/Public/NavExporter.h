#pragma once

class UWorld;

class FNavExporter
{
public:
	static void ExportCurrentWorldNavData();
	
private:
	static UWorld* GetEditorWorld();
	static bool ExportWorld(UWorld* World);
	
};
