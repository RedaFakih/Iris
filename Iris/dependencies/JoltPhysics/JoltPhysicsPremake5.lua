project "JoltPhysics"
    kind "StaticLib"
    language "C++"
    cppdialect "C++20"

    targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir ("bin/Intermediates/" .. outputdir .. "/%{prj.name}")
    
    files
    {
        "JoltPhysics/Jolt/**.cpp",
        "JoltPhysics/Jolt/**.h",
        "JoltPhysics/Jolt/**.inl",
        "JoltPhysics/Jolt/**.gliffy"
    }

    includedirs
    {
        "JoltPhysics/Jolt",
        "JoltPhysics"
    }

    filter "system:windows"
        systemversion "latest"

        files { "JoltPhysics/Jolt/Jolt.natvis" }

    filter "configurations:Debug"
        runtime "Debug"
        symbols "on"

        defines
        {
            "_DEBUG",
            "JPH_DEBUG_RENDERER",
            "JPH_FLOATING_POINT_EXCEPTIONS_ENABLED",
            "JPH_EXTERNAL_PROFILE",
			"JPH_ENABLE_ASSERTS"
        }

    filter "configurations:Release"
        runtime "Release"
        symbols "off"
        optimize "Full"
        vectorextensions "AVX2"
        isaextensions {
            "BMI", "POPCNT", "LZCNT", "F16C"
        }
        inlining "Auto"

        defines
        {
            "JPH_DEBUG_RENDERER",
            "JPH_FLOATING_POINT_EXCEPTIONS_ENABLED",
            "JPH_EXTERNAL_PROFILE"
        }