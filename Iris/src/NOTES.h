#pragma once

/*
 * TODO LATER:
 *  - Fix assimp copying the dll to the exe dir for the runtime if we need to do that... because the runtime should not be using assimp it should be using an assetpack
 * 
 * NEXT STEP ON TUTORIAL: Compute Shaders
 *
 * NEXT THING TO WORK ON:
 * - Runtime crashes on exit in Multithreaded mode
 * - Renderer2D quads are not rendering/flickering for some reason
 * - Bug in adding a mesh to the static mesh component popup (It is added for one frame and then deleted)
 * - Add Mesh panel that prompts the user to create the Iris Mesh file if they load a MeshSource
 * - If Mouse is pressed and moved in any other panel, the move mouse event is dispatched and then when we click in the viewport it is used which makes the flicker thing
 * - Swith the asset thread to sleep and wake based on a condition variable
 * - Add compute passes to the engine so we can have environment maps and some IBL
 * - Add Filled Circles to Renderer2D? (Circle sprites)
 * - SceneRenderer continue building all the passes
 * - Maybe make the outline color user set?
 * - Handle renderer descriptor pool situation and come up with some solutions for it... Read note written in Renderer.cpp (line: 101)
 * - For runtime layer switch to using a main camera component and not the editor camera
 * - Add pipeline for double sided materials where you just disable culling... but it has its own drawlist
 * - Make the place where the scene names are rendered in the titlebar act as scene `TABS` that way you could have multiple scenes open at the same time and you switch between them by just pressing the scene name in the titlebar
 * - For raycasting grids we need to fix that the grid is not fixed in place and it moves as the camera moves so it is not useful as a reference grid
 * - OIT? With Weighted Blended technique using info provided from learnopengl.com <https://learnopengl.com/Guest-Articles/2020/OIT/Weighted-Blended> <https://github.com/nvpro-samples/vk_order_independent_transparency>
 * 
 * - When we have an AssetManager:
 *   - Switch all the stuff that references assets in the Properties panel (Textures, meshes, materials...)
 * 
 * TO BE EXPANDED: (constant expansion)
 * - DescriptorSetManager (Constant Expansion with shader reflection)
 * - Shader reflection to get descriptor data
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
 *		- How it works with the asset manager?
 *      - How dependecy between assets work and dependency changing (OnDependencyChanged)?
 *		- Stuff it uses for waiting and all that
 * 
 * Future Plans:
 *	- Add meshoptimizer? <https://github.com/zeux/meshoptimizer/tree/master>
 *  - Maybe add something in some mini editor window that can edit submesh indices and split them into transparent and non transparent submeshes so that we get OIT when implemented
 *
 */