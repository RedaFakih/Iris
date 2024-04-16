#pragma once

/*
 * TODO LATER:
 *  - Fix assimp copying the dll to the exe dir for the runtime if we need to do that... because the runtime should not be using assimp it should be using an assetpack
 * 
 * NEXT STEP ON TUTORIAL: Compute Shaders
 *
 * NEXT THING TO WORK ON:
 * - Change the way the titlebar looks...
 * - Scene and components so that we can finally get some interaction
 * - Add Filled Circles to Renderer2D? (Circle sprites)
 * - Add compute passes to the engine so we can have environment maps and some IBL
 * - SceneRenderer continue building all the passes
 * - Handle renderer descriptor pool situation and come up with some solutions for it... Read note written in Renderer.cpp (line: 101)
 * - For runtime layer switch to using a main camera component and not the editor camera
 * - Add pipeline for double sided materials where you just disable culling... but it has its own drawlist
 * - Mouse picking with read back image OR ray casting OR ID image that we copy each frame to a readback Buffer (Check if this method is slow)
 * - For raycasting grids we need to fix that the grid is not fixed in place and it moves as the camera moves so it is not useful as a reference grid
 * - OIT? With Weighted Blended technique using info provided from learnopengl.com <https://learnopengl.com/Guest-Articles/2020/OIT/Weighted-Blended> <https://github.com/nvpro-samples/vk_order_independent_transparency>
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
 * 
 * Future Plans:
 *	- Add meshoptimizer? <https://github.com/zeux/meshoptimizer/tree/master>
 *  - Maybe add something in some mini editor window that can edit submesh indices and split them into transparent and non transparent submeshes so that we get OIT when implemented
 *
 */