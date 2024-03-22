#pragma once

/*
 * NEXT STEP ON TUTORIAL: Loading Models.
 *
 * NEXT THING TO WORK ON:
 * - Add `Existing Images` to framebuffers so that Renderer2D could finally work (Look at note in EditorLayer.cpp Line:342)
 * - Add a runtime layer so that we can test without imgui
 * - Work on Scene and Entity and Components so that we can finally work with the playground nicely
 * - Scene And Entity
 * - SceneRenderer
 * - Clean Up EditorLayer and manage the scene and scene renderer correctly
 * - Add a Renderer2D
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
 * Brief Overview:
 *
 * - When we in the future we want to have a scene renderer and all that there will be one main command buffer that has its own abstraction
 * - Descriptor sets should be abstracted into a manager that manages them and manages their allocation via the command pool and all that
 *
 *
 */