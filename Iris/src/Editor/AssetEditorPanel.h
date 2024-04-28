#pragma once

#include "AssetManager/Asset/Asset.h"
#include "Core/Base.h"
#include "Core/Events/Events.h"
#include "Core/TimeStep.h"
#include "Scene/Scene.h"

namespace Iris {

	typedef int ImGuiWindowFlags;

	class AssetEditor
	{
	protected:
		AssetEditor(const char* id);

	public:
		virtual ~AssetEditor() = default;

		virtual void OnUpdate(TimeStep ts) {}
		virtual void OnEvent(Events::Event& e) {}
		virtual void OnImGuiRender();
		virtual void SetSceneContext(const Ref<Scene>& scene) {}
		virtual void SetAsset(const Ref<Asset>& asset) = 0;

		void SetOpen(bool isOpen);
		bool IsOpen() const { return m_IsOpen; }

		const std::string& GetTitle() const { return m_Title; }
		void SetTitle(const std::string& newTitle)
		{
			m_Title = newTitle;
			m_TitleAndID = newTitle + "###" + m_ID;
		}

	protected:
		void SetMinSize(uint32_t width, uint32_t height);
		void SetMaxSize(uint32_t width, uint32_t height);

		virtual ImGuiWindowFlags GetWindowFlags() { return 0; }

		// Subclass can optionally override this to customize window style
		virtual void OnWindowStylePush() {}
		virtual void OnWindowStylePop() {}

	private:
		virtual void OnOpen() = 0;
		virtual void OnClose() = 0;
		virtual void Render() = 0;

	protected:
		std::string m_ID;
		std::string m_Title;
		std::string m_TitleAndID;

		glm::vec2 m_MinSize;
		glm::vec2 m_MaxSize;

		bool m_IsOpen = false;

	};

	class AssetEditorPanel
	{
	public:
		static void RegisterDefaultEditors();
		static void UnregisterAllEditors();
		static void OnUpdate(TimeStep ts);
		static void OnEvent(Events::Event& e);
		static void SetSceneContext(const Ref<Scene>& context);
		static void OnImGuiRender();
		static void OpenEditor(const Ref<Asset>& asset);

		template<typename T>
		static void RegisterEditor(AssetType type)
		{
			static_assert(std::is_base_of<AssetEditor, T>::value, "AssetEditorPanel::RegisterEditor requires template type to inherit from AssetEditor");
			IR_ASSERT(!s_Editors.contains(type), "There's already an editor for that asset!");
			s_Editors[type] = CreateScope<T>();
		}

	private:
		inline static std::unordered_map<AssetType, Scope<AssetEditor>> s_Editors;
		inline static Ref<Scene> s_SceneContext;

	};

}