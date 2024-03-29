#pragma once

#include "Scene/Scene.h"

namespace vkPlayground {

	/*
	 * So what do we want to do here?
	 *	- Build draw lists separated by the type of submitted object (static mesh, dynamic mesh, etc...)
	 *	- Flush draw list at the end with all the different passes that we will undergo
	 *  - Handle uniform buffer updating
	 *  - Handle Pipeline/Framebuffer/Images invalidation in case of screen resize
	 *  - Keep track of viewport size...
	 */

	class SceneRenderer : public RefCountedObject
	{
	public:
		SceneRenderer(Ref<Scene> scene);
		~SceneRenderer();

		[[nodiscard]] static Ref<SceneRenderer> Create(Ref<Scene> scene);

	private:

	};

}