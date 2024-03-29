#pragma once

/*
 * TODO LATER:
 *  - Fix assimp copying the dll to the exe dir for the runtime if we need to do that... because the runtime should not be using assimp it should be using an assetpack
 * 
 * NEXT STEP ON TUTORIAL: Loading Models.
 *
 * NEXT THING TO WORK ON:
 * - Handle renderer descriptor pool situation and come up with some solutions for it... Read note written in Renderer.cpp (line: 101)
 * - start working on gltf importing
 * - Add a runtime layer so that we can test without imgui (Needs fixing)
 * - Work on Scene and Entity and Components so that we can finally work with the playground nicely
 * - Scene And Entity
 * - SceneRenderer
 * - Clean Up EditorLayer and manage the scene and scene renderer correctly
 * - DescriptorSetManager (Constant Expansion with shader reflection)
 * 
 * TO BE EXPANDED: (constant expansion)
 * - Shader reflection to get descriptor data
 * - DecriptorSetManager expansion to other types
 * - Remember to clear stuff that need to be cleared most importantly renderer related stuff
 * - RenderPassInputTypes...
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
 *	- Add meshoptimizer? (https://github.com/zeux/meshoptimizer/tree/master)
 *
 */