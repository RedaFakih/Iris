project "Iris"
    kind "StaticLib"

    targetdir ("%{wks.location}/bin/" .. outputdir .. "/%{prj.name}")
    objdir ("%{wks.location}/bin/Intermediates/" .. outputdir .. "/%{prj.name}")

    pchheader "IrisPCH.h"
    pchsource "src/IrisPCH.cpp"

    files {
        "src/**.h",
        "src/**.cpp",
        "src/**.inl",

        "dependencies/stb/**.h",
        "dependencies/stb/**.cpp",

        "dependencies/Vma/**.h",
        "dependencies/Vma/**.cpp",

        "dependencies/yaml-cpp/include/**.h",
        "dependencies/yaml-cpp/src/**.h",
        "dependencies/yaml-cpp/src/**.cpp"
    }

    defines {
        "GLM_FORCE_DEPTH_ZERO_TO_ONE"
    }

    includedirs {
        "src",
        "%{IncludeDir.assimp}",
        "%{IncludeDir.GLFW}",
        "%{IncludeDir.ImGui}",
        "%{IncludeDir.glm}",
        "%{IncludeDir.VulkanSDK}",
        "%{IncludeDir.choc}",
        "%{IncludeDir.stb}",
        "%{IncludeDir.spdlog}",
        "%{IncludeDir.EnTT}",
        "%{IncludeDir.msdfAtlasGen}",
        "%{IncludeDir.msdfGen}",
        "%{IncludeDir.NFDExtended}",
        "%{IncludeDir.Yaml}",
        "%{IncludeDir.MagicEnum}"
    }

    links {
        "GLFW",
        "ImGui",
        "msdf-atlas-gen",
        "NFD-Extended",
        "%{Library.dxc}",
        "%{Library.Vulkan}"
    }

    filter "system:windows"
        systemversion "latest"

    filter "configurations:Debug"
        runtime "Debug"
        symbols "on"
        defines { "IR_CONFIG_DEBUG" }

        links
        {
            "%{Library.AssimpDebug}",
            "%{Library.ShadercDebug}",
            "%{Library.ShadercUtilsDebug}",
            "%{Library.SPIRV_CrossDebug}",
            "%{Library.SPIRV_CrossGLSLDebug}",
            "%{Library.SPIRV_ToolsDebug}"
        }

    filter "configurations:Release"
        runtime "Release"
        optimize "Full"
        vectorextensions "AVX2"
        isaextensions {
            "BMI", "POPCNT", "LZCNT", "F16C"
        }
        inlining "Auto"
        defines { "IR_CONFIG_RELEASE" }

        links {
            "%{Library.AssimpRelease}",
            "%{Library.ShadercRelease}",
            "%{Library.ShadercUtilsRelease}",
            "%{Library.SPIRV_CrossRelease}",
            "%{Library.SPIRV_CrossGLSLRelease}",
            "%{Library.SPIRV_ToolsRelease}"
        }

    filter "files:dependencies/stb/**.cpp"
        flags { "NoPCH" }

    filter "files:dependencies/yaml-cpp/src/**.cpp"
        flags { "NoPCH" }

    filter "files:dependencies/Vma/**.cpp"
        flags { "NoPCH" }

    filter "files:src/ImGui/imgui_demo.cpp"
        flags { "NoPCH" }
