include "Dependencies.lua"

workspace "Iris"
    startproject "IrisEditor"
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
    include "Iris/dependencies/glfw"
    include "Iris/dependencies/Box2D"
    include "Iris/dependencies/ImGui/imgui"
    include "Iris/dependencies/NFD-Extended"
    group "Dependencies/msdf"
    include "Iris/dependencies/msdf-atlas-gen"
group ""

group "Core"
    include "Iris"
group ""

group "Tools"
    include "IrisEditor"
    include "IrisRuntime"
group ""