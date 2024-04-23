#include "IrisPCH.h"
#include "Project.h"

namespace Iris {

	Ref<Project> Project::Create()
	{
		return CreateRef<Project>();
	}

	void Project::SetActive(Ref<Project> project)
	{
		if (s_ActiveProject)
		{
			s_AssetManager->Shutdown();
			s_AssetManager = nullptr;
		}

		s_ActiveProject = project;
		if (s_ActiveProject)
		{
			s_AssetManager = EditorAssetManager::Create();
		}
	}

}