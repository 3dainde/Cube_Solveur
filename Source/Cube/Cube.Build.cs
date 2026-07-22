using UnrealBuildTool;

public class Cube : ModuleRules
{
    public Cube(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
    "Core",
    "CoreUObject",
    "Engine",
    "InputCore",
    "HeadMountedDisplay",
    "EnhancedInput",
    "PhysicsCore",
    "ProceduralMeshComponent",
    "UMG",
    "Niagara",
    "OnlineSubsystem",
    "OnlineSubsystemUtils",
    "HTTP",
    "Networking",
    "Sockets"
        });

        PrivateDependencyModuleNames.AddRange(new string[] { });

        // Steam will be loaded dynamically (via plugin and config .ini)
        DynamicallyLoadedModuleNames.Add("OnlineSubsystemSteam");

        // Uncomment if you are using Slate UI
        // PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
    }
}
