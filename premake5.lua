include "Dependencies.lua"

workspace "VulkanPlayground"
    architecture "x86_64"

    configurations
    {
        "Debug",
        "Release"
    }

    flags 
    {
        "MultiProcessorCompile"
    }

outputdir = "%{cfg.architecture}-%{cfg.buildcfg}"

group "Dependencies"
    include "VulkanPlayground/dependencies/glfw"
    include "VulkanPlayground/dependencies/ImGui/imgui"
group ""

group "Core"
    include "VulkanPlayground"
group ""