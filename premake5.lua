include "Dependencies.lua"

workspace "VulkanPlayground"
    startproject "PlaygroundEditor"
    conformancemode "On"

    language "C++"
    cppdialect "C++20"
    staticruntime "Off"

    configurations {
        "Debug",
        "Release"
    }

    flags {
        "MultiProcessorCompile"
    }

    defines {
        "_CRT_SECURE_NO_WARNINGS"
    }

    filter "action:vs*"
        linkoptions { "/ignore:4099" } -- Disable no PDB found warning

    filter "language:C++ or language:C"
        architecture "x86_64"

outputdir = "%{cfg.architecture}-%{cfg.buildcfg}"

group "Dependencies"
    include "VulkanPlayground/dependencies/glfw"
    include "VulkanPlayground/dependencies/ImGui/imgui"
group ""

group "Core"
    include "VulkanPlayground"
group ""

group "Tools"
    include "PlaygroundEditor"
    include "PlaygroundRuntime"
group ""