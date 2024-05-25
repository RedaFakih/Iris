#pragma once

#include "Editor/AssetEditorPanel.h"
#include "Renderer/Mesh/MaterialAsset.h"

namespace Iris {

	class MaterialEditor : public AssetEditor
	{
	public:
		MaterialEditor();

		virtual void SetAsset(const Ref<Asset>& asset) override;

	private:
		virtual void OnOpen() override;
		virtual void OnClose() override;
		virtual void Render() override;

	private:
		Ref<MaterialAsset> m_MaterialAsset;

	};

}