project "PlaygroundEditor"
    kind "ConsoleApp"

    targetdir ("%{wks.location}/bin/" .. outputdir .. "/%{prj.name}")
    objdir ("%{wks.location}/bin/Intermediates/" .. outputdir .. "/%{prj.name}")

    files {
        "src/**.h",
        "src/**.cpp"
    }

    includedirs {
        "%{wks.location}/VulkanPlayground/src",
        "%{wks.location}/VulkanPlayground/dependencies",
        "%{IncludeDir.spdlog}",
        "%{IncludeDir.ImGui}",
        "%{IncludeDir.glm}",
        "%{IncludeDir.choc}",
        "%{IncludeDir.EnTT}",
        "%{IncludeDir.VulkanSDK}"
    }

    links {
        "VulkanPlayground"
    }

    defines {
        "GLM_FORCE_DEPTH_ZERO_TO_ONE"
    }

    filter "system:windows"
        systemversion "latest"

    filter "configurations:Debug"
        defines "VKPG_CONFIG_DEBUG"
        runtime "Debug"
        symbols "on"

    filter "configurations:Release"
        optimize "Full"
        vectorextensions "AVX2"
        isaextensions {
            "BMI", "POPCNT", "LZCNT", "F16C"
        }
        runtime "Release"
        inlining "Auto"
        defines "VKPG_CONFIG_RELEASE"

    filter { "system:windows", "configurations:Debug" }
        postbuildcommands {
            '{COPY} "../VulkanPlayground/dependencies/assimp/bin/Debug/assimp-vc143-mtd.dll" "%{cfg.targetdir}"'
        }

    filter { "system:windows", "configurations:Release" }
        postbuildcommands {
            '{COPY} "../VulkanPlayground/dependencies/assimp/bin/Release/assimp-vc143-mt.dll" "%{cfg.targetdir}"'
        }