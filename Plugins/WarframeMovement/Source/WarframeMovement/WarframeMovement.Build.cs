// Copyright OPT-MUS. All Rights Reserved.

using UnrealBuildTool;

public class WarframeMovement : ModuleRules
{
	public WarframeMovement(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		// UE5 standardi: engine tarafindaki deprecated API'leri derlemeye sokma.
		IWYUSupport = IWYUSupport.Full;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput"
		});
	}
}
