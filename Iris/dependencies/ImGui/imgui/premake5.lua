project "ImGui"
    kind "StaticLib"

    targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir ("bin/Intermediates/" .. outputdir .. "/%{prj.name}")

    files {
        "imconfig.h",
        "imgui.h",
        "imgui.cpp",
        "imgui_draw.cpp",
        "imgui_internal.h",
        "imgui_tables.cpp",
        "imgui_widgets.cpp",
        "imstb_rectpack.h",
        "imstb_textedit.h",
        "imstb_truetype.h",
        "imgui_impl_vulkan.h",
        "imgui_impl_vulkan.cpp",
        "imgui_impl_glfw.h",
        "imgui_impl_glfw.cpp"
    }

    includedirs {
        "../../../src",
        "%{IncludeDir.GLFW}",
        "%{IncludeDir.VulkanSDK}",
        "%{IncludeDir.glm}",
        "%{IncludeDir.spdlog}"
    }

    filter "system:windows"
        systemversion "latest"

    filter "configurations:Debug"
        runtime "Debug"
        symbols "on"

    filter "configurations:Release"
        runtime "Release"
        optimize "Full"
        vectorextensions "AVX2"
        isaextensions {
            "BMI", "POPCNT", "LZCNT", "F16C"
        }
        inlining "Auto"
