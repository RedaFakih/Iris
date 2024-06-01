#pragma once

#include "Editor/EditorPanel.h"

namespace Iris {

	class AssetManagerPanel : public EditorPanel
	{
	public:
		AssetManagerPanel() = default;
		~AssetManagerPanel() = default;

		[[nodiscard]] static Ref<AssetManagerPanel> Create()
		{
			return CreateRef<AssetManagerPanel>();
		}

		virtual void OnImGuiRender(bool& open) override;

	};

}