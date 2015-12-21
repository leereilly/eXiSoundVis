#pragma once
 
#include "ModuleManager.h"
 
class IeXiSoundVisPlugin : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	void StartupModule();
	void ShutdownModule();

	static inline IeXiSoundVisPlugin& Get()
	{
		return FModuleManager::LoadModuleChecked < IeXiSoundVisPlugin >( "eXiSoundVisPlugin" );
	}

	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("eXiSoundVisPlugin");
	}
};