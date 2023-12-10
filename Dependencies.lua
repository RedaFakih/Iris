
VULKAN_SDK = os.getenv("VULKAN_SDK")

IncludeDir = {}

IncludeDir["GLFW"]                = "%{wks.location}/VulkanPlayground/dependencies/glfw/include"
IncludeDir["ImGui"]               = "%{wks.location}/VulkanPlayground/dependencies/ImGui"
IncludeDir["stb"]                 = "%{wks.location}/VulkanPlayground/dependencies/stb"
IncludeDir["glm"]                 = "%{wks.location}/VulkanPlayground/dependencies/glm"
IncludeDir["spdlog"]              = "%{wks.location}/VulkanPlayground/dependencies/spdlog/include"
IncludeDir["fastgltf"]            = "%{wks.location}/VulkanPlayground/dependencies/fastgltf/include"
IncludeDir["EnTT"]                = "%{wks.location}/VulkanPlayground/dependencies/entt/include"
IncludeDir["VulkanSDK"]           = "%{VULKAN_SDK}/Include"
IncludeDir["Yaml"]                = "%{wks.location}/VulkanPlayground/dependencies/yaml-cpp/include"
IncludeDir["choc"]                = "%{wks.location}/VulkanPlayground/dependencies/Choc"

LibraryDir = {}

LibraryDir["VulkanSDK"]           = "%{VULKAN_SDK}/Lib"

Library = {}

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