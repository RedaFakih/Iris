#include "IrisPCH.h"
#include "Project.h"

#include "Physics/Physics.h"

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

			PhysicsSystem::Shutdown();
		}

		s_ActiveProject = project;
		if (s_ActiveProject)
		{
			s_AssetManager = EditorAssetManager::Create();
			
			PhysicsSystem::Init();
		}
	}

}