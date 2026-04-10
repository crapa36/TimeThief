#include "ClientConfigLoader.h"

#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"

bool FClientConfigLoader::LoadClientConfigFromFile(const FString& FilePath, FClientConfig& OutConfig)
{
	FString JsonText;
	if (!FFileHelper::LoadFileToString(JsonText, *FilePath))
	{
		UE_LOG(LogTemp, Error, TEXT("[Config] Failed to load client config file: %s"), *FilePath);
		return false;
	}
	
	return ParseClientConfig(JsonText, OutConfig);
}

bool FClientConfigLoader::ParseClientConfig(const FString& JsonText, FClientConfig& OutConfig)
{
	TSharedPtr<FJsonObject> RootObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("[Config] Failed to parse client config JSON"));
		return false;
	}
	
	const TSharedPtr<FJsonObject>* NetworkObject = nullptr;
	if (RootObject->TryGetObjectField(TEXT("network"), NetworkObject) && NetworkObject && NetworkObject->IsValid())
	{
		FString ServerDNS;
		if ((*NetworkObject)->TryGetStringField(TEXT("server_dns"), ServerDNS))
		{
			OutConfig.ServerDNS = ServerDNS;
		}
		
		FString ServerIp;
		if ((*NetworkObject)->TryGetStringField(TEXT("fallback_ip"), ServerIp))
		{
			OutConfig.FallbackIp = ServerIp;
		}
		
		int32 ServerPort;
		if ((*NetworkObject)->TryGetNumberField(TEXT("server_port"), ServerPort))
		{
			OutConfig.ServerPort = ServerPort;
		}
	}
	
	return true;
}
