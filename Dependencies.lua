
VULKAN_SDK = os.getenv("VULKAN_SDK")

IncludeDir = {}

IncludeDir["GLFW"]                = "%{wks.location}/Iris/dependencies/glfw/include"
IncludeDir["ImGui"]               = "%{wks.location}/Iris/dependencies/ImGui"
IncludeDir["stb"]                 = "%{wks.location}/Iris/dependencies/stb"
IncludeDir["glm"]                 = "%{wks.location}/Iris/dependencies/glm"
IncludeDir["spdlog"]              = "%{wks.location}/Iris/dependencies/spdlog/include"
IncludeDir["EnTT"]                = "%{wks.location}/Iris/dependencies/entt/include"
IncludeDir["VulkanSDK"]           = "%{VULKAN_SDK}/Include"
IncludeDir["Yaml"]                = "%{wks.location}/Iris/dependencies/yaml-cpp/include"
IncludeDir["choc"]                = "%{wks.location}/Iris/dependencies/Choc"
IncludeDir["assimp"]              = "%{wks.location}/Iris/dependencies/assimp/include"
IncludeDir["NFDExtended"]         = "%{wks.location}/Iris/dependencies/NFD-Extended/NFD-Extended/src/include"

LibraryDir = {}

LibraryDir["VulkanSDK"]           = "%{VULKAN_SDK}/Lib"

Library = {}

Library["AssimpDebug"]			  = "%{wks.location}/Iris/dependencies/assimp/bin/Debug/assimp-vc143-mtd.lib"
Library["AssimpRelease"]	      = "%{wks.location}/Iris/dependencies/assimp/bin/Release/assimp-vc143-mt.lib"

Library["Vulkan"]                 = "%{LibraryDir.VulkanSDK}/vulkan-1.lib"
Library["dxc"]                    = "%{LibraryDir.VulkanSDK}/dxcompiler.lib"
								  
Library["ShadercDebug"]           = "%{LibraryDir.VulkanSDK}/shaderc_sharedd.lib"
Library["ShadercRelease"]         = "%{LibraryDir.VulkanSDK}/shaderc_shared.lib"

Library["ShadercUtilsDebug"]      = "%{LibraryDir.VulkanSDK}/shaderc_utild.lib"
Library["ShadercUtilsRelease"]    = "%{LibraryDir.VulkanSDK}/shaderc_util.lib"

Library["SPIRV_CrossDebug"]       = "%{LibraryDir.VulkanSDK}/spirv-cross-cored.lib"
Library["SPIRV_CrossRelease"]     = "%{LibraryDir.VulkanSDK}/spirv-cross-core.lib"

Library["SPIRV_CrossGLSLDebug"]   = "%{LibraryDir.VulkanSDK}/spirv-cross-glsld.lib"
Library["SPIRV_CrossGLSLRelease"] = "%{LibraryDir.VulkanSDK}/spirv-cross-glsl.lib"
								  
Library["SPIRV_ToolsDebug"]       = "%{LibraryDir.VulkanSDK}/SPIRV-Toolsd.lib"
Library["SPIRV_ToolsRelease"]     = "%{LibraryDir.VulkanSDK}/SPIRV-Tools.lib"