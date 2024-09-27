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
        defines "IR_CONFIG_DEBUG"
        runtime "Debug"
        symbols "on"

    filter "configurations:Release"
        defines "IR_CONFIG_RELEASE"
        runtime "Release"
        optimize "Speed"
        inlining "Auto"

    -- TODO: For now these are here but when we have asset packing they should be removed!
    filter { "system:windows", "configurations:Debug" }
        postbuildcommands {
            '{COPY} "%{Library.AssimpDebug}" "%{cfg.targetdir}"'
        }

    filter { "system:windows", "configurations:Release" }
        postbuildcommands {
            '{COPY} "%{Library.AssimpRelease}" "%{cfg.targetdir}"'
        }