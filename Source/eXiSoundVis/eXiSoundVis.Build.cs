using UnrealBuildTool;
using System.IO;
 
public class eXiSoundVis : ModuleRules
{
    public eXiSoundVis(TargetInfo Target)
    {
        PrivateIncludePaths.AddRange(new string[] { "eXiSoundVis/Private" });
        PublicIncludePaths.AddRange(new string[] { "eXiSoundVis/Public" });
 
        PublicDependencyModuleNames.AddRange(new string[] { "Engine", "Core", "CoreUObject", "InputCore", "RHI", "Kiss_FFT" });

        if (Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.Win32)
        {
            // VS2015 updated some of the CRT definitions but not all of the Windows SDK has been updated to match.
            // Microsoft provides this shim library to enable building with VS2015 until they fix everything up.
            //@todo: remove when no longer neeeded (no other code changes should be necessary).
            if (WindowsPlatform.Compiler == WindowsCompiler.VisualStudio2015)
            {
                PublicAdditionalLibraries.Add("legacy_stdio_definitions.lib");
            }
        }

        AddThirdPartyPrivateStaticDependencies(Target, "Kiss_FFT");
    }
}