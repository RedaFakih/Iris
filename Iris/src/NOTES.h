#pragma once

/*
 * GLOBAL IRIS SETTINGS:
 * - 1 Unit = 1 Meter in the scale so setting the grid scale to 1 shows the 1 meter we just need to add suffixes to the values displayed
 *
 * - Always Checkout https://gpuopen.com/learn/rdna-performance-guide/ and https://developer.nvidia.com/blog/vulkan-dos-donts/ for any extra optimisations we could find from the vulkan side
 *
 * TODO: Renderer Re-write
 * - - Re-write the renderer using NVRHI which will help make it alot more stable and portable
 *
 * TODO: ContentBrowser
 * - We need to make the content browser be able to be rendered in different contexts
 * - We need to make the material editor have its own context in a way since it needs to handle inputs alone
 * - Fix the main viewport widgets rendering since they no longer work for input
 * - Fix Image Layouts for SceneRendererLite
 * - Since we can detect lost focus on tabs now maybe bring back shadows to the SceneRendererLite?
 *		- If we bring back shadows, It will not be full CSM, rather just maybe one cascade and not Medium quality shadows
 * - Add stuff like environment maps used by the engine for thumbnails or other stuff to the Resources folder of the editor and not in the Assets folder!
 *		- Move default meshes from the Assets directory to the Resources directory of the Editor
 * - For now we only display thumbnails for Textures and Materials, maybe add support for meshes and EnvironmentMaps?
 *		- Add memory limit to the ThumbnailCache
 *
 * TODO: Jolt Physics
 * - Serializing MeshColliders in MeshCookingFactory is actually useless because each time we load the engine we will regenerate the same colliders with different IDs so the previous ones are just stacked and waste memory
 * - Optimizing broad phase:
 *		- Include a button in the Physics settings in the Project panel to optimize the broad phase on demand.
 *		- Maybe we should optimize the broad phase after a certain amount of bodies has been added before we PhysicsScene::PreSimulate?
 * - MeshColliderCache currently serializes to the disk but the that happens everytime the engine is restarted since the colliders will have different UUIDs so the filenames will not match
 * - Go through all todos in physics system since some of them need to be handled
 * - Project Serializer maybe also serialize 2D physics settings?
 * - Project Panel Physics Settings
 * - Started work on PhysicsScene
 *		- Need to add alot more methods and start implementing, Bulk storage stuff is not really needed since it is not gonna be used for now.
 * - Need to work on PhysicsBody that will be where the jolt physics body and collider shape created
 *		- (NOTE: For the PhysicsShape, we do not need to have an array for each ShapeType since a body should be linked to only one collider shape but apparently C# needed to have the api that Hazel has)
 * - JoltContactListener: Two methods need the scripting engine to be implemented
 * - In Jolt: Collider shapes are joined in the same entity with the rigid body, so if we add a shape collider component to an entity, in code we have to add a
 *			  default rigid body as well because thats how it works in Jolt unlike Box2D where the rigid body is separate from the collider shape
 *
 * TODO: DynamicRendering branch:
 * - Importing specific sub meshes from a mesh file should only be a feature for dynamic meshes and not static meshes, since for static meshes the hierarchy is flattended and doesnt make sense to load specific submeshes from it
 *		- Thats why it will only be available for dynamic meshes since the hierarchy is not flattened
 * - LightCulling2_5.glsl is a rewrite of the LightCulling shader that uses 2.5D culling but it doesnt fully work yet and needs some fixing
 *		- 1: For the actual shader itself <https://github.com/turanszkij/WickedEngine/blob/master/WickedEngine/shaders/lightCullingCS.hlsl>
 *		- 2: For all the helper stuff <https://github.com/turanszkij/WickedEngine/blob/master/WickedEngine/shaders/cullingShaderHF.hlsli>
 *		- 3: Also look into the flat bit arrays <https://wickedengine.net/2019/02/thoughts-on-light-culling-stream-compaction-vs-flat-bit-arrays/>
 * - Add the application settings panel that for now is only gonna be used to set or unset the colored meshes highlighting in SceneHierarchyPanel
 * - Embed as much as possible the images that the engine uses such as icons and other stuff that way we can minize engien startup time since we won't have to decode so many images
 * - When we have the ConsolePanel, we should cleanup the logging settings in the project panel and make it comply with only stuff that logs to the console panel
 * - Add Thumbnails...?
 * - Add Area Lights <https://learnopengl.com/Guest-Articles/2022/Area-Lights>
 * - Duplicated Meshes all reference the same material and have the same material table
 * - Add the Depth Of Field Picker
 * - Need to clean up image layouts for the depth of field
 * - When creating a logging console panel we need to enable some sort of flag in compile time that reroutes all logging for both application console and also console panel 
 * - Make the window title be {Iris - SceneName (*)} the * is whether it is saved or not
 * - Make the SceneName in the engine contain a (*) if it is saved or not
 *      - To detect changes we can serialize the scene every couple frames to an intermediate string/file and then hash that with the seriliazed file on disk:
 *      -	- Equal Hashes => Nothing changed, dont add * to scene name
 *		-	- Not equal Hashes => Something in scene changed, add * to scene name
 *		- Or we can use another `actions` based method. Where we detect if any action has occured in the editor and we know that the scene state has changed and needs serializing
 *		-	- This is the better method and should be used however it will need us to redo the whole way we treat actions in the editor!
 * - We are getting best practices warning that for the pass where no resources are created we have unused bound vertex buffers...
 * - NOT HIGH PRIORITY:
 *      - Maybe Generate the BRDFLut Texture ourselves?
 *		- Add Tracy for profiling
 *
 * TODO: Creative Ideas:
 * - Should bloom resources be double/triple buffered?
 * - We need to serialize the SceneRenderer BloomDirtTexture
 * - Should we serialize the scene renderer settings for example the postFx stuff?
 * - Put an error or highlight in the viewport when there is no primary camera in the scene
 * - Provide an option for the SceneRenderer to show the ACTUAL draw call count since now it only accounts for the color pass draws
 * - Submesh selection when we have dynamic meshes
 * - Need to fix everything regarding orthographic projection views
 * - Gizmo controls and orthographic camera movement collision
 * - Fix mouse picking in orthographic view
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
 * NOTE (Physics):
 *	- TODO: Describe how the physics system works
 *
 * NOTE (SceneHierarchyPanel):
 *  - For the lights exceeding limits highlighting, we always highlight all the entities after the first RendererConfiguration::MaxNumberOf***Lights entities in the hierarchy
 * 
 * NOTE (SceneRenderer):
 *	- Currently the scene renderer renders only opaque meshes in one single geometry pass
 *	- We will want to split that into to separate draw lists that get submitted separatly
 *	- Since we want to add OIT in the future with weighted blended so we need to render opaque first then transparent...
 *  - Currently we have compute passes running on the graphics queue and not on the separate compute queue in order to avoid resource queue ownership changes
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
 *  - LightCulling Shader:
 *		- Should we add AABB culling with the normal frustum culling? <https://wickedengine.net/2018/01/optimizing-tile-based-light-culling/>
 *		- Look into 2.5D culling
 *  - Change the way mesh materials are displayed:
 *		- Currently they are displayed in a tree node under the Mesh componenet which is disgusting
 *		- Change that into a button that takes you into a material editor that displays all the list of materials available for that mesh
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
 *		- - Could use a PBR shader That writes depth and also alpha that way we can avoid using a predepth and also we can display transparent material
 *		- - Depth pass Shader (Could be the same one)
 *		- - Geometry pass PBR Shader (That only considers a directional light and nothing else, no shadows most importantly which strips down alot of the code)
 *		- - Composite pass Shader (That does not have exposure, opacity, time, grid, or any of that stuff. It will basically be just a texture pass shader to tonemap and gamma correct. NOTHING ELSE)
 *		- Scene entities:
 *		- - Skylight
 *		- - Sphere
 *  - Look into auto exposure because its SO COOL
 * 		- 1: https://bruop.github.io/exposure/
 * 		- 2: https://mynameismjp.wordpress.com/2011/08/10/average-luminance-compute-shader/
 * 		- 3: https://www.google.com/url?sa=t&source=web&rct=j&opi=89978449&url=https://resources.mpi-inf.mpg.de/tmo/logmap/logmap.pdf&ved=2ahUKEwiIrqjRtv2FAxVih_0HHZVMBK84ChAWegQIBRAB&usg=AOvVaw2PQ0E5hUTrfiVHLXDvpYKR
 * 		- 4: https://github.com/SaschaWillems/Vulkan/tree/master?tab=readme-ov-file#Advanced
 *  - Frustum Culling:
 * 		- 1: https://vkguide.dev/docs/new_chapter_5/faster_draw/
 * 		- 2: https://vkguide.dev/docs/gpudriven/compute_culling/
 *		- 3: https://github.com/SaschaWillems/Vulkan/blob/master/examples/computecullandlod/computecullandlod.cpp
 *  - Tesselation Shaders: (For having low poly meshes with high details using tesselation and LODs)
 *		- 1: https://github.com/SaschaWillems/Vulkan/tree/master?tab=readme-ov-file#tessellation-shader
 *  - Add some Anti-Aliasing pass technique:
 *      - 1: https://github.com/iryoku/smaa (For SMAA which is the one preferred since it is pretty lightweight if configured correctly)
 *      - 2: https://github.com/dmnsgn/glsl-smaa
 *  - Look into this: <https://www.youtube.com/watch?v=SiCNFMhDZ1o>
 *  - Switch to using draw indexed INDIRECT instead of using normal draw indexed since that is faster and facilitates tasks like culling and what not
 *  - OIT? With Weighted Blended technique using info provided from learnopengl.com <https://learnopengl.com/Guest-Articles/2020/OIT/Weighted-Blended> <https://github.com/nvpro-samples/vk_order_independent_transparency>
 *	- Add meshoptimizer? <https://github.com/zeux/meshoptimizer/tree/master>
 *  - Maybe add something in some mini editor window that can edit submesh indices and split them into transparent and non transparent submeshes so that we get OIT when implemented
 *  - Change the way we treat post process effects... Currently they are global to the scene, instead work on introducing the post process volume concept like the one in Unreal Engine
 *	- For shadow casting point lights look at: Clustered/Tile-Based Deferred Shadow Mapping
 *		- 1: http://www.hd-prg.com/tileBasedShadows.html
 *		- 2: https://www.aortiz.me/2018/12/21/CG.html
 *
 */