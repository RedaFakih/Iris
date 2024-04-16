project "NFD-Extended"
    kind "StaticLib"

    targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("bin/Intermediates/" .. outputdir .. "/%{prj.name}")

    files {
        "NFD-Extended/src/include/nfd.h",
        "NFD-Extended/src/include/nfd.hpp"
    }

    includedirs {
        "NFD-Extended/src/include/"
    }

    filter "system:windows"
		systemversion "latest"

        files { 
            "NFD-Extended/src/nfd_win.cpp"
        }

    filter "configurations:Debug"
        runtime "Debug"
        symbols "On"

    filter "configurations:Release"
        runtime "Release"
        optimize "Full"
        vectorextensions "AVX2"
        isaextensions {
            "BMI", "POPCNT", "LZCNT", "F16C"
        }
        inlining "Auto"
        defines { "NDEBUG" }