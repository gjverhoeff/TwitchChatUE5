// --- Source/TwitchChat/TwitchChat.Build.cs ---
using UnrealBuildTool;
using System.Collections.Generic;

public class TwitchChat : ModuleRules
{
    public TwitchChat(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        // Runtime dependencies
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core", "CoreUObject", "Engine", "InputCore",
            "Slate", "SlateCore", "UMG", "AnimatedTexture",
            "HTTP", "RenderCore", "RHI", "ImageWrapper",
            "WebSockets", "Json", "JsonUtilities", "Text3D"
        });

        // Private dependencies
        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Sockets", "Networking", "Projects", "Slate",      
                "SlateCore"
        });

        // Editor-only dependencies
        if (Target.bBuildEditor)
        {
            PublicDependencyModuleNames.AddRange(new string[] {
                "Kismet","Settings","DeveloperSettings","AnimatedTextureEditor",
                "Slate","SlateCore"      // ← ensure these are listed
            });
            PrivateDependencyModuleNames.AddRange(new string[] {
                "UnrealEd","EditorStyle","ToolMenus","PropertyEditor",
                "Slate","SlateCore"      // ← …and here
            });
        }
    }
}