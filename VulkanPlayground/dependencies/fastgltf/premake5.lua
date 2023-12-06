project "FastGLTF"
    kind "StaticLib"
    language "C++"
    cppdialect "C++20"
    staticruntime "off"

    targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir ("bin/Intermediates/" .. outputdir .. "/%{prj.name}")

    includedirs
    {
        "include",
        "deps/simdjson"
    }

    files 
    {
        "include/fastgltf/**.hpp",
        "src/**.cpp",
        "deps/**.h",
        "deps/**.cpp"
    }

    filter "system:windows"
        systemversion "latest"

    filter "configurations:Debug"
        runtime "Debug"
        symbols "on"

    filter "configurations:Release"
        runtime "Release"
        optimize "Speed"
        inlining "Auto"