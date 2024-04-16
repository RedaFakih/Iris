#include "IrisPCH.h"
#include "SceneHierarchyPanel.h"

namespace Iris {

	Ref<SceneHierarchyPanel> SceneHierarchyPanel::Create(Ref<Scene>)
	{
		return CreateRef<SceneHierarchyPanel>();
	}

}
