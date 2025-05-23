// AnimatedTexture.Build.cs
using UnrealBuildTool;
using System.IO;

public class AnimatedTexture : ModuleRules
{
    public AnimatedTexture(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        // Expose our Public folder so other modules (like your chat plugin) can #include "AnimatedTexture2D.h"
        PublicIncludePaths.AddRange(new string[] {
            Path.Combine(ModuleDirectory, "Public")
        });

        // Private includes for internal plugin code (including libwebp)
        PrivateIncludePaths.AddRange(new string[] {
            Path.Combine(ModuleDirectory, "Private"),
            Path.Combine(ModuleDirectory, "Private", "libwebp")
        });

        // Engine modules this plugin depends on
        PublicDependencyModuleNames.AddRange(new string[] {
            "Core",
            "CoreUObject",
            "Engine",
            "RenderCore",
            "RHI",
            "Slate",
            "SlateCore",
            "UMG",
            "MediaAssets"
        });

        PrivateDependencyModuleNames.AddRange(new string[] {
            "Projects"
        });
    }
}
