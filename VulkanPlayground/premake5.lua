project "VulkanPlayground"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++20"
    staticruntime "off"

    targetdir ("%{wks.location}/bin/" .. outputdir .. "/%{prj.name}")
    objdir ("%{wks.location}/bin/Intermediates/" .. outputdir .. "/%{prj.name}")

    files
    {
        "src/**.h",
        "src/**.cpp",

        "dependencies/stb/**.h",
        "dependencies/stb/**.cpp",

        "dependencies/Vma/**.h",
        "dependencies/Vma/**.cpp"
    }

    defines
    {
        "_CRT_SECURE_NO_WARNINGS",
        "GLFW_INCLUDE_NONE",
        "GLM_FORCE_DEPTH_ZERO_TO_ONE"
    }

    includedirs
    {
        "src",
        "%{IncludeDir.GLFW}",
        "%{IncludeDir.ImGui}",
        "%{IncludeDir.glm}",
        "%{IncludeDir.VulkanSDK}",
        "%{IncludeDir.choc}",
        "%{IncludeDir.stb}",
        "%{IncludeDir.spdlog}"
    }

    links
    {
        "GLFW",
        "ImGui",
        "%{Library.Vulkan}"
    }

    filter "system:windows"
        systemversion "latest"

    filter "configurations:Debug"
        defines "PLAYGROUND_DEBUG"
        runtime "Debug"
        symbols "on"

        links
        {
            "%{Library.dxc}",
            "%{Library.ShadercDebug}",
            "%{Library.ShadercUtilsDebug}",
            "%{Library.SPIRV_CrossDebug}",
            "%{Library.SPIRV_CrossGLSLDebug}",
            "%{Library.SPIRV_ToolsDebug}"
        }

    filter "configurations:Release"
        defines "PLAYGROUND_RELEASE"
        runtime "Release"
        optimize "Speed"
        inlining "Auto"

        links
        {
            "%{Library.dxc}",
            "%{Library.ShadercRelease}",
            "%{Library.ShadercUtilsRelease}",
            "%{Library.SPIRV_CrossRelease}",
            "%{Library.SPIRV_CrossGLSLRelease}",
            "%{Library.SPIRV_ToolsRelease}"
        }

    filter "files:dependencies/stb/**.cpp"
        flags { "NoPCH" }