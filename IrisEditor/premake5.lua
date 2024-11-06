project "IrisEditor"
    kind "ConsoleApp"

    targetdir ("%{wks.location}/bin/" .. outputdir .. "/%{prj.name}")
    objdir ("%{wks.location}/bin/Intermediates/" .. outputdir .. "/%{prj.name}")

    files {
        "src/**.h",
        "src/**.cpp"
    }

    includedirs {
        "%{wks.location}/Iris/src",
        "%{wks.location}/Iris/dependencies",
        "%{IncludeDir.Box2D}",
        "%{IncludeDir.JoltPhysics}",
        "%{IncludeDir.spdlog}",
        "%{IncludeDir.ImGui}",
        "%{IncludeDir.glm}",
        "%{IncludeDir.choc}",
        "%{IncludeDir.EnTT}",
        "%{IncludeDir.MagicEnum}",
        "%{IncludeDir.VulkanSDK}"
    }

    links {
        "Iris"
    }

    defines {
        "GLM_FORCE_DEPTH_ZERO_TO_ONE"
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
        optimize "Full"
        -- symbols "off"
        vectorextensions "AVX2"
        isaextensions {
            "BMI", "POPCNT", "LZCNT", "F16C"
        }
        runtime "Release"
        inlining "Auto"

        defines
        {
            "IR_CONFIG_RELEASE",

            "JPH_DEBUG_RENDERER",
            "JPH_FLOATING_POINT_EXCEPTIONS_ENABLED",
            "JPH_EXTERNAL_PROFILE"
        }

    filter { "system:windows", "configurations:Debug" }
        postbuildcommands {
            '{COPY} "%{Library.AssimpDebug}" "%{cfg.targetdir}"'
        }

    filter { "system:windows", "configurations:Release" }
        postbuildcommands {
            '{COPY} "%{Library.AssimpRelease}" "%{cfg.targetdir}"'
        }