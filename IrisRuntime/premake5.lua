project "IrisRuntime"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++20"
    staticruntime "off"

    targetdir ("%{wks.location}/bin/" .. outputdir .. "/%{prj.name}")
    objdir ("%{wks.location}/bin/Intermediates/" .. outputdir .. "/%{prj.name}")

    files
    {
        "src/**.h",
        "src/**.cpp"
    }

    includedirs
    {
        "%{wks.location}/Iris/src",
        "%{wks.location}/Iris/dependencies",
        "%{IncludeDir.ImGui}",
        "%{IncludeDir.spdlog}",
        "%{IncludeDir.Box2D}",
        "%{IncludeDir.JoltPhysics}",
        "%{IncludeDir.glm}",
        "%{IncludeDir.choc}",
        "%{IncludeDir.EnTT}",
        "%{IncludeDir.VulkanSDK}"
    }

    links
    {
        "Iris"
    }

    defines
    {
        "GLM_FORCE_DEPTH_ZERO_TO_ONE",
        "_CRT_SECURE_NO_WARNINGS"
    }

    filter "system:windows"
        systemversion "latest"

    filter "configurations:Debug"
        runtime "Debug"
        symbols "on"

        defines
        {
            "IR_CONFIG_DEBUG",

            "JPH_DEBUG_RENDERER",
            "JPH_FLOATING_POINT_EXCEPTIONS_ENABLED",
            "JPH_EXTERNAL_PROFILE"
        }

    filter "configurations:Release"
        runtime "Release"
        -- symbols "off"
        optimize "Speed"
        inlining "Auto"

        defines
        {
            "IR_CONFIG_RELEASE",

            "JPH_DEBUG_RENDERER",
            "JPH_FLOATING_POINT_EXCEPTIONS_ENABLED",
            "JPH_EXTERNAL_PROFILE"
        }

    -- TODO: For now these are here but when we have asset packing they should be removed!
    filter { "system:windows", "configurations:Debug" }
        postbuildcommands {
            '{COPY} "%{Library.AssimpDebug}" "%{cfg.targetdir}"'
        }

    filter { "system:windows", "configurations:Release" }
        postbuildcommands {
            '{COPY} "%{Library.AssimpRelease}" "%{cfg.targetdir}"'
        }