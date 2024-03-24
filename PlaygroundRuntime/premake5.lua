project "PlaygroundRuntime"
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
        "%{wks.location}/VulkanPlayground/src",
        "%{wks.location}/VulkanPlayground/dependencies/spdlog/include",
        "%{wks.location}/VulkanPlayground/dependencies",
        "%{IncludeDir.ImGui}",
        "%{IncludeDir.glm}",
        "%{IncludeDir.choc}",
        "%{IncludeDir.EnTT}",
        "%{IncludeDir.VulkanSDK}"
    }

    links
    {
        "VulkanPlayground"
    }

    defines
    {
        "GLM_FORCE_DEPTH_ZERO_TO_ONE",
        "_CRT_SECURE_NO_WARNINGS"
    }

    filter "system:windows"
        systemversion "latest"

    filter "configurations:Debug"
        defines "VKPG_CONFIG_DEBUG"
        runtime "Debug"
        symbols "on"

    filter "configurations:Release"
        defines "VKPG_CONFIG_RELEASE"
        runtime "Release"
        optimize "Speed"
        inlining "Auto"
