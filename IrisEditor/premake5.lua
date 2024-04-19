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
        defines "IR_CONFIG_DEBUG"
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
        defines "IR_CONFIG_RELEASE"

    filter { "system:windows", "configurations:Debug" }
        postbuildcommands {
            '{COPY} "../Iris/dependencies/assimp/bin/Debug/assimp-vc143-mtd.dll" "%{cfg.targetdir}"'
        }

    filter { "system:windows", "configurations:Release" }
        postbuildcommands {
            '{COPY} "../Iris/dependencies/assimp/bin/Release/assimp-vc143-mt.dll" "%{cfg.targetdir}"'
        }