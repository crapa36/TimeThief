#pragma once

struct FExportedNavData;
class dtNavMesh;
class UWorld;

class FNavExporter
{
public:
	static void ExportCurrentWorldNavData();
	
private:
	static UWorld* GetEditorWorld();
	static bool ExportWorld(UWorld* World);
	static bool SaveBinary(const FString& FilePath, const dtNavMesh* DetourNavMesh, const FExportedNavData& ExportData);
	
};
