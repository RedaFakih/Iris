#pragma once

/*
 * GLOBAL IRIS SETTINGS:
 * - 1 Unit = 1 Meter in the scale so setting the grid scale to 1 shows the 1 meter we just need to add suffixes to the values displayed
 *
 * - Always Checkout https://gpuopen.com/learn/rdna-performance-guide/ and https://developer.nvidia.com/blog/vulkan-dos-donts/ for any extra optimisations we could find from the vulkan side
 *
 * TODO: DynamicRendering branch:
 * - NOT HIGH PRIORITY:
 *      - Make the clouds work in the preetham shader
 *      - Maybe Generate the BRDFLut Texture ourselves?
 *		- Add Tracy for profiling
 *		- Implement finally the note described in Device.h (line: 68) that talks about handling one time command buffers submission.
 *
 * TODO: Creative Ideas:
 * - When rendering with more than one font we get weird behaviour we start rendering the atlas of one of the fonts
 * - Maybe we need to change the way the Compute passes in the Renderer are architected... Since they now take a render commandbuffer but that is never used so Look into that please.
 * - Currently we do pass a render command buffer to Renderer::BeginComputePass however we do not use it so its useless for now needs rewriting with the compute pipelie rewrite
 * - Provide an option for the SceneRenderer to show the ACTUAL draw call count since now it only accounts for the color pass draws
 * - Need to fix everything regarding orthographic projection views
 * - For any object other than the primitives we always have an extra material for some reason
 * - Submesh selection when we have a content browser Both static and dynamic meshes will support submesh selection
 * - Viewport camera orthographic views done:
 *		- Gizmo controls and orthographic camera movement collision
 *		- Fix mouse picking in orthographic view
 * - Once we request a Mesh Asset from the AssetManager we should submit a lambda into a queue in the editor layer so that we wait until the asset is ready then we
 *		InstantiateStaticMesh for that mesh from the scene so that we get the whole scene entity hierarchy however for now we have a problem that when we InstantiateMesh
 *		we do get the correct entity hierarchy however every child entity is its own complete model
 *
 * LATEST UPDATES:
 *  ** 25/05/2024 **
 * - Problem for the invalid pipeline layout discovered and temporarily fixed in a bad way (Was that we were using different command buffers for material and pipeline)
 * - ComputePipeline needs a bit of a re-write with regards to the command buffer that it executes on... Since currently we give it the physical VkCommandBuffer object to execute on
 *   which we dont really want that type of architecture. Maybe add an Execute method that just does the whole pass in that function (also debatable) or find a way to pass the
 *   command buffer to properly record the dispatch command with also the Compute pass commands.
 *
 * NEXT THING TO WORK ON:
 * - Write the final two function of TextureCube (CopyToHostBuffer / CopyFromHostBuffer)
 * - The problem with compute passes was with pipeline layouts is that we are using different command buffers inside the ComputePipeline class since when we class RT_Begin we do not pass it a command buffer
 *		So to fix we should either let the whole compute pass use the same commandbuffer or just pass it the current renderCommandBuffer which will use the graphics queue and not the Compute queue
 *		So far I have tried to pass a command buffer that is created on the Compute Queue into the pipeline and work with it which is working for now...
 * - DescriptorSetManager needs to have a semi re-write since it does not correctly handle all the cases were the image is a textureCube and storage image
 *		- There is confusion between cube textures and storage images... A cube texture input.Type is being set as DescriptorResourceType::StorageImage
 *        eventhough it is a cube texture... see we need a way to detect that and handle it accordingly. (Most probably separate those two cases from the
 *		  switch statement and handle them separatly, or leave the normal cases in the switch and if check after to handle accordingly so that we do not 
 *		  break any behaviour for 2D stuff) {This might be fixed for the StorageImages and CubeImages since we now use spv::Dim instead of just random numbers in the switch statement that sets the Fomrat}
 *		- We can know the dimension of the image from shader reflection and using that we can cast back to the original type to know if its a cubeTexture or not
 * - DirectionalLightComponent needs expansion when we have shadows and that means related expansion in: SceneSerializer, Scene, SceneRenderer
 * - Storage Images (Need writing and testing)
 * - Add Mesh panel that prompts the user to create the Iris Mesh file if they load a MeshSource
 * - Selecting what submeshes to load through the mesh importer since in the Iris Mesh file we already know what submesh indices we want so just do not load them with assimp
 * - Add Filled Circles to Renderer2D? (Circle sprites)
 * - For runtime layer switch to using a main camera component and not the editor camera
 *
 * TO BE EXPANDED: (constant expansion)
 * - Shader reflection to get descriptor data
 * - DescriptorSetManager (Constant Expansion with shader reflection)
 * - DecriptorSetManager expansion to other types
 * - Remember to clear stuff that need to be cleared most importantly renderer related stuff
 * - RenderPassInputTypes...
 *
 * NOTE (SceneRenderer):
 *	- Currently the scene renderer renders only opaque meshes in one single geometry pass
 *	- We will want to split that into to separate draw lists that get submitted separatly
 *	- Since we want to add OIT in the future with weighted blended so we need to render opaque first then transparent...
 *
 * NOTE (Materials):
 *	- When you set Metalness or Roughness MAPS in the MaterialEditor or the ones loaded from mesh files:
 *		- The Roughness and Metalness values will be locked to 1 since they are multiplied with the values sampled from the textures
 *      - Which means any adjustments to the roughness and metalness values should be done in the authored texture
 * 
 * NOTE (DescriptorSetManager):
 *	- By calling `vkResetDescriptorPool` we essentially return all allocated descriptor sets to the pool instead of individually calling
 *		`vkFreeDescriptorSets` and then we would have to reallocate all the descriptor sets again in order to use them...
 *		so as long as we do not reset the descriptor pool and do not individually free descriptor sets which does not need to happen really,
 *		we can create the `DescriptorSetManager`'s descriptor pool without the `VK_DESCRIPTOR_POOL_CREATE_FREE_SET_BIT` which slows down performance
 *
 *	- To expand with other types of descriptor you need to:
 *		- Add that type in the ShaderCompiler Reflection and serialization
 *		- Update the Shader code and reflection
 *		- Update the DescriptorManager with all the Utils and types and their handling cases
 *		- Update the RenderPass with the getters and setters and invokes to DescriptorManager
 *
 * NOTE (AssetManager):
 *  - We have two asset managers: Editor and Runtime. They are split since the editor asset manager has to do things differently that what the runtime has to do.
 *  - The asset manager interfaces with the AssetImporter class for loading and Loading data or Serializing data of assets
 *		- The AssetImporter contains a list of AssetSerializer derived class (ex. TertureSerializer, MaterialAssetSerializer, ...)
 *	    - Those classes handle the loading and serializing of their corresponding asset type
 *		- Some assets may have an Iris specific middle file (YAML) that links the asset with its source asset file (ex. StaticMesh links the asset to MeshSource)
 *			- In case of these assets, the corresponding AssetSerializer derived class will handle Loading and Serializing of that YAML file
 *  - AssetThread:
 *		- The asset manager can submit to the AssetThread an asset load request via the GetAssetAsync function, the AssetThread will then go through the
 *		  the asset loading requests queue and start loading the assets one by one. At the start of the next frame we sync the asset manager with the
 *		  AssetThread and retrieve any new loaded assets and queue and dependcy changes...
 *
 * Future Plans:
 *  - Change the way mesh materials are displayed:
 *		- Currently they are displayed in a tree node under the Mesh componenet which is disgusting
 *		- Change that into a button that takes you into a material editor that displays all the list of materials available
 *		- When you click on one of the materials in the list it renders a sphere in a viewport in the MaterialEditor with the selected material on it
 *	    - For that the material editor needs to have its own SceneRenderer to render that sphere
 *		- And the SceneRenderer does NOT have to be a full blown SceneRenderer, it should be a lite version of it:
 *		- - Very stripped down version of the default SceneRenderer (Maybe create a new class called MaterialSceneRenderer since if we want to save on CPU side memory we should remove all the extra state that the default SceneRenderer stores)
 *		- - NO Renderer2D
 *		- - NO Shadows
 *		- - NO Ambient Oclusion
 *		- - NO SSR
 *		- - NO JumpFlood
 *		- - NO Grid
 *		- - NO Wireframe
 *		- - YES Composite just to composite the final image (Composite Shader is not the defualt one, it will be a lite version of the default one)
 *		- - YES Bloom since we might have emissive materials
 *		- Shaders:
 *		- - Depth pass Shader (Could be the same one)
 *		- - Geometry pass PBR Shader (That only considers a directional light and nothing else, no shadows most importantly which strips down alot of the code)
 *		- - Composite pass Shader (That does not have exposure, opacity, time, grid, or any of that stuff. It will basically be just a texture pass shader to tonemap and gamma correct. NOTHING ELSE)
 *		- Scene entities:
 *		- - Skylight
 *		- - Directional light
 *		- - Sphere
 *  - Look into auto exposure because its SO COOL
 * 		- 1: https://bruop.github.io/exposure/
 * 		- 2: https://mynameismjp.wordpress.com/2011/08/10/average-luminance-compute-shader/
 * 		- 3: https://www.google.com/url?sa=t&source=web&rct=j&opi=89978449&url=https://resources.mpi-inf.mpg.de/tmo/logmap/logmap.pdf&ved=2ahUKEwiIrqjRtv2FAxVih_0HHZVMBK84ChAWegQIBRAB&usg=AOvVaw2PQ0E5hUTrfiVHLXDvpYKR
 * 		- 4: https://github.com/SaschaWillems/Vulkan/tree/master?tab=readme-ov-file#Advanced
 *  - Frustum Culling:
 * 		- 1: https://vkguide.dev/docs/new_chapter_5/faster_draw/
 * 		- 2: https://vkguide.dev/docs/gpudriven/compute_culling/
 *  - Tesselation Shaders: (For having low poly meshes with high details using tesselation and LODs)
 *		- 1: https://github.com/SaschaWillems/Vulkan/tree/master?tab=readme-ov-file#tessellation-shader
 *  - Switch to using draw indexed INDIRECT instead of using normal draw indexed since that is faster and facilitates tasks like culling and what not
 *  - OIT? With Weighted Blended technique using info provided from learnopengl.com <https://learnopengl.com/Guest-Articles/2020/OIT/Weighted-Blended> <https://github.com/nvpro-samples/vk_order_independent_transparency>
 *	- Add meshoptimizer? <https://github.com/zeux/meshoptimizer/tree/master>
 *  - Maybe add something in some mini editor window that can edit submesh indices and split them into transparent and non transparent submeshes so that we get OIT when implemented
 *  - Change the way we treat post process effects... Currently they are global to the scene, instead work on introducing the post process volume concept like the one in Unreal Engine
 *
 */