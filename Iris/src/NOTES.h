#pragma once

/*
 * GLOBAL IRIS SETTINGS:
 * - 1 Unit = 1 Meter in the scale so setting the grid scale to 1 shows the 1 meter we just need to add suffixes to the values displayed
 * 
 * TODO LATER:
 *  - Fix assimp copying the dll to the exe dir for the runtime if we need to do that... because the runtime should not be using assimp it should be using an assetpack
 * 
 * TODO Creative Ideas:
 * - Add mesh visibility column
 * - Bug when duplicating meshes, they will share the same material and not have their own material instance
 * - Submesh selection when we have a content browser Both static and dynamic meshes will support submesh selection
 * - Viewport camera orthographic views done:
 *		- Gizmo controls and orthographic camera movement collision
 *		- Fix bugs in orthogrpahic view
 *		- Fix mouse picking in orthographic view
 * - Fix the grid highlighting for Z and X axes
 * - Do the idea for the scene tabs...
 * - Look into auto exposure because its SO COOL
 *		- 1: https://bruop.github.io/exposure/
 *		- 2: https://mynameismjp.wordpress.com/2011/08/10/average-luminance-compute-shader/
 *		- 3: https://www.google.com/url?sa=t&source=web&rct=j&opi=89978449&url=https://resources.mpi-inf.mpg.de/tmo/logmap/logmap.pdf&ved=2ahUKEwiIrqjRtv2FAxVih_0HHZVMBK84ChAWegQIBRAB&usg=AOvVaw2PQ0E5hUTrfiVHLXDvpYKR
 * 
 * LATEST UPDATES:
 *  ** 05/05/2024 **
 * - DescriptorSetManager problem is MAYBE fixed but we still need to check for that when we write TextureCube properly
 * - We still get pipeline layout not matching validation errors (Maybe has to do something with DescriptorSetManager)
 * - We need to write TextureCube properly
 *	 - For that we can make the following assumptions:
 *         - Cubemaps in Iris are always going to be hdr maps so, the cube images will always be filled from a compute shader so we will never have data
 *			 when we are creating the resource so we can remove all the code that takes care of "If we had data to fill the image with"
 * 
 *  ** 01/05/2024 **
 * - Added TextureCube but needs ALOT of debugging for image layouts
 * - Added StorageBuffers and they should be okay...
 * - Adding StorageImages...
 * - Added SceneEnvirontment
 * - Currently working on to continue:
 *	- Writing textureCube
 *	- Fixing DescriptorSetManager
 *  - Writing StorageImages
 *  - Creating the compute pipeline and passes and validating them in Renderer.cpp
 *	- Computing environment maps and transforming them
 * 
 * NEXT THING TO WORK ON:
 * - DescriptorSetManager needs to have a semi re-write since it does not correctly handle all the cases were the image is a textureCube and storage image
 *		- There is confusion between cube textures and storage images... A cube texture input.Type is being set as DescriptorResourceType::StorageImage
 *        eventhough it is a cube texture... see we need a way to detect that and handle it accordingly. (Most probably separate those two cases from the
 *		  switch statement and handle them separatly, or leave the normal cases in the switch and if check after to handle accordingly so that we do not 
 *		  break any behaviour for 2D stuff)
 *		- We can know the dimension of the image from shader reflection and using that we can cast back to the original type to know if its a cubeTexture or not
 * - Finish writing the TextureCube class
 * - Cube textures (Need testing, change the layout in the descriptor image info)
 * - Storage Images (Need testing and writing)
 * - Add compute passes to the engine so we can have environment maps and some IBL
 * - Selecting what submeshes to load through the mesh importer since in the Iris Mesh file we already know what submesh indices we want so just do not load them with assimp
 * - Add Mesh panel that prompts the user to create the Iris Mesh file if they load a MeshSource
 * - Renderer2D problem with rendering ALOT of lines... (Vertexbuffer not resizing / no extra buffer is being createad)
 * - Add Filled Circles to Renderer2D? (Circle sprites)
 * - Handle renderer descriptor pool situation and come up with some solutions for it... Read note written in Renderer.cpp (line: 101)
 * - For runtime layer switch to using a main camera component and not the editor camera
 * - For raycasting grids we need to fix that the grid is not fixed in place and it moves as the camera moves so it is not useful as a reference grid
 * - OIT? With Weighted Blended technique using info provided from learnopengl.com <https://learnopengl.com/Guest-Articles/2020/OIT/Weighted-Blended> <https://github.com/nvpro-samples/vk_order_independent_transparency>
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
 *	- Add meshoptimizer? <https://github.com/zeux/meshoptimizer/tree/master>
 *  - Maybe add something in some mini editor window that can edit submesh indices and split them into transparent and non transparent submeshes so that we get OIT when implemented
 *
 */