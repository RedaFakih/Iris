#include "vkPch.h"
#include "SceneRenderer.h"

namespace vkPlayground {

	Ref<SceneRenderer> SceneRenderer::Create(Ref<Scene> scene)
	{
		return CreateRef<SceneRenderer>(scene);
	}

	SceneRenderer::SceneRenderer(Ref<Scene> scene)
	{
	}

	SceneRenderer::~SceneRenderer()
	{
	}

}