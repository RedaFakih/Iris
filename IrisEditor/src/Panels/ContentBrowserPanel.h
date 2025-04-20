#pragma once

#include "Core/Events/KeyEvents.h"
#include "Core/Events/MouseEvents.h"
#include "Editor/EditorPanel.h"
#include "Project/Project.h"
#include "Scene/Scene.h"
#include "Thumbnails/ThumbnailCache.h"
#include "Thumbnails/ThumbnailGenerator.h"
#include "Utils/FileSystem.h"

namespace Iris {

#define CB_MAX_INPUT_SEARCH_BUFFER_LENGTH 128

	enum class ContentBrowserAction
	{
		None = 0,
		ClearSelections = BIT(0),
		Selected = BIT(1),
		Deselected = BIT(2),
		Hovered = BIT(3),
		StartRenaming = BIT(4),
		Renamed = BIT(5),
		OpenDeleteDialogue = BIT(6),
		SelectToHere = BIT(7),
		Moved = BIT(8),
		ShowInExplorer = BIT(9),
		OpenExternal = BIT(10),
		Reload = BIT(11),
		Copy = BIT(12),
		Activated = BIT(13),
		Refresh = BIT(14)
	};

	struct CBItemActionResult
	{
		uint16_t Field = 0;

		void Set(ContentBrowserAction flag, bool value)
		{
			if (value)
				Field |= static_cast<uint16_t>(flag);
			else
				Field &= ~static_cast<uint16_t>(flag);
		}

		bool IsSet(ContentBrowserAction flag) const { return static_cast<uint16_t>(flag) & Field; }
	};

	class ContentBrowserPanel;

	struct DirectoryInfo : public RefCountedObject
	{
		// We need a handle for directories since that makes it easier to deal with rather than using filenames/filepaths
		AssetHandle Handle;
		Ref<DirectoryInfo> Parent;
		std::filesystem::path FilePath;

		std::vector<AssetHandle> Assets;
		std::map<AssetHandle, Ref<DirectoryInfo>> SubDirectories;

		[[nodiscard]] inline static Ref<DirectoryInfo> Create()
		{
			return CreateRef<DirectoryInfo>();
		}

	};

	class ContentBrowserItem : public RefCountedObject
	{
	public:
		enum class ItemType : uint8_t
		{
			Directory, Asset
		};
	public:
		ContentBrowserItem(ItemType type, AssetHandle id, const std::string_view name, const Ref<Texture2D>& icon);
		virtual ~ContentBrowserItem() = default;

		[[nodiscard]] inline static Ref<ContentBrowserItem> Create(ItemType type, AssetHandle id, const std::string_view name, const Ref<Texture2D>& texture)
		{
			return CreateRef<ContentBrowserItem>(type, id, name, texture);
		}

		void OnRenderBegin();
		CBItemActionResult OnRender(Ref<ContentBrowserPanel> contentBrowser);
		void OnRenderEnd();

		virtual void Delete() {}
		virtual bool Move(const std::filesystem::path& destination) { (void)destination; return false; }

		void StartRenaming();
		void StopRenaming();
		bool IsRenaming() const { return m_IsRenaming; }

		inline ItemType GetItemType() const { return m_Type; }
		inline AssetHandle GetID() const { return m_ID; }
		inline const std::string& GetFileName() const { return m_FileName; }
		inline Ref<Texture2D> GetIcon() const { return m_Icon; }

	protected:
		// To be overriden if needed
		virtual void OnRenamed(const std::string& newName) { m_FileName = newName; }
		virtual void UpdateDrop(CBItemActionResult& result) {}

	private:
		void Rename(const std::string& newName);
		void GetDisplayNameFromFileName();

		// NOTE: This is needed for the input stuff that uses a combination of Mouse + Key states so we can not separate them into OnKeyPressed/OnMouseButtonPressed
		void UpdateInput(CBItemActionResult& result);

	private:
		AssetHandle m_ID;
		ItemType m_Type;
		std::string m_DisplayName;
		std::string m_FileName;
		Ref<Texture2D> m_Icon;

		bool m_IsRenaming = false;
		bool m_IsDragging = false;

		bool m_IsSelected = false;
		bool m_JustSelected = false;
		bool m_IsFocused = false;

		// static since only one item can be renamed at one time
		inline static char s_RenameBuffer[CB_MAX_INPUT_SEARCH_BUFFER_LENGTH]{};

	};

	class ContentBrowserDirectory : public ContentBrowserItem
	{
	public:
		ContentBrowserDirectory(const Ref<DirectoryInfo>& directoryInfo);
		virtual ~ContentBrowserDirectory() = default;

		[[nodiscard]] inline static Ref<ContentBrowserDirectory> Create(const Ref<DirectoryInfo>& directoryInfo)
		{
			return CreateRef<ContentBrowserDirectory>(directoryInfo);
		}

		virtual void Delete() override;
		virtual bool Move(const std::filesystem::path& destination) override;

		const Ref<DirectoryInfo>& GetDirectoryInfo() const { return m_DirectoryInfo; }

	private:
		virtual void OnRenamed(const std::string& newName) override;
		virtual void UpdateDrop(CBItemActionResult& result) override;

	private:
		Ref<DirectoryInfo> m_DirectoryInfo;

	};

	class ContentBrowserAsset : public ContentBrowserItem
	{
	public:
		ContentBrowserAsset(const AssetMetaData& metaData, const Ref<Texture2D>& icon);
		virtual ~ContentBrowserAsset() = default;

		[[nodiscard]] inline static Ref<ContentBrowserAsset> Create(const AssetMetaData& metaData, const Ref<Texture2D>& icon)
		{
			return CreateRef<ContentBrowserAsset>(metaData, icon);
		}

		virtual void Delete() override;
		virtual bool Move(const std::filesystem::path& destination) override;

		const AssetMetaData& GetAssetInfo() const { return m_AssetInfo; }

	private:
		virtual void OnRenamed(const std::string& newName) override;

	private:
		AssetMetaData m_AssetInfo;

	};

	// ContentBrowser utils
	namespace Utils {

		class ItemList;

		class SelectionStack
		{
		public:
			// Uses the SelectionManager for currently selected handles and items to get the item
			void CopyFrom(const Utils::ItemList& items);

			inline void Select(const Ref<ContentBrowserItem>& item)
			{
				if (IsSelected(item->GetID()))
					return;

				m_SelectedItems.push_back(item);
			}

			inline void Deselect(AssetHandle handle)
			{
				if (!IsSelected(handle))
					return;

				for (auto it = m_SelectedItems.begin(); it != m_SelectedItems.end(); it++)
				{
					if (handle == (*it)->GetID())
					{
						m_SelectedItems.erase(it);
						break;
					}
				}
			}

			inline bool IsSelected(AssetHandle handle) const
			{
				for (const Ref<ContentBrowserItem>& item : m_SelectedItems)
				{
					if (item->GetID() == handle)
						return true;
				}

				return false;
			}

			inline std::size_t Find(AssetHandle handle) const
			{
				if (m_SelectedItems.empty())
					return s_InvalidItem;

				for (std::size_t i = 0; i < m_SelectedItems.size(); i++)
				{
					if (handle == m_SelectedItems[i]->GetID())
						return i;
				}

				return s_InvalidItem;
			}

			inline void Clear()
			{
				m_SelectedItems.clear();
			}

			inline std::size_t SelectionCount() const { return m_SelectedItems.size(); }
			inline std::size_t Size() const { return m_SelectedItems.size(); }

			inline auto begin() { return m_SelectedItems.begin(); }
			inline auto end() { return m_SelectedItems.end(); }
			inline auto begin() const { return m_SelectedItems.begin(); }
			inline auto end() const { return m_SelectedItems.end(); }

		private:
			// We have to keep track of the Item we initiated a copy from since if we change directory, the m_CurrentItems member in ContentBrowserPanel will be cleared!
			std::vector<Ref<ContentBrowserItem>> m_SelectedItems;

		public:
			// Invalid Item index
			static constexpr std::size_t s_InvalidItem = std::numeric_limits<std::size_t>::max();

		};

		class ItemList
		{
		public:
			ItemList() = default;

			ItemList(const ItemList& other)
				: m_Items(other.m_Items)
			{
			}

			ItemList& operator=(const ItemList& other)
			{
				m_Items = other.m_Items;
				return *this;
			}

			inline void Clear()
			{
				std::scoped_lock<std::mutex> lock(m_Mutex);
				m_Items.clear();
			}

			inline void Erase(AssetHandle handle)
			{
				std::size_t index = Find(handle);
				if (index == s_InvalidItem)
					return;

				std::scoped_lock<std::mutex> lock(m_Mutex);
				m_Items.erase(m_Items.begin() + index);
			}

			inline std::size_t Find(AssetHandle handle) const
			{
				if (m_Items.empty())
					return s_InvalidItem;

				std::scoped_lock<std::mutex> lock(m_Mutex);
				for (std::size_t i = 0; i < m_Items.size(); i++)
				{
					if (handle == m_Items[i]->GetID())
						return i;
				}

				return s_InvalidItem;
			}

			inline std::vector<Ref<ContentBrowserItem>>& GetItems() { return m_Items; }
			inline const std::vector<Ref<ContentBrowserItem>>& GetItems() const { return m_Items; }

			inline auto begin() { return m_Items.begin(); }
			inline auto end() { return m_Items.end(); }
			inline auto begin() const { return m_Items.begin(); }
			inline auto end() const { return m_Items.end(); }

			Ref<ContentBrowserItem>& operator[](std::size_t index) { return m_Items[index]; }
			const Ref<ContentBrowserItem>& operator[](std::size_t index) const { return m_Items[index]; }

		private:
			std::vector<Ref<ContentBrowserItem>> m_Items;
			mutable std::mutex m_Mutex;

		public:
			static constexpr std::size_t s_InvalidItem = std::numeric_limits<std::size_t>::max();
		};

		inline constexpr const char* ContentBrowserItemTypeToString(ContentBrowserItem::ItemType type)
		{
			switch (type)
			{
				case ContentBrowserItem::ItemType::Directory:	return "Directory";
				case ContentBrowserItem::ItemType::Asset:		return "Asset";
			}

			IR_VERIFY(false);
			return "UNKNOWN ITEMTYPE";
		}

	}

	/*
	 * We have chosen the approach of recreating the whole hierarchy structure in memory since that is a better approach for UI, and generally will not use that much memory
	 * in the order of megabytes unless we are working with a huge project.
	 * It has a benefit for better UI performance for browsing back and forward instead of lazily loading entries.
	 * Each directory is addressed by a handle (AssetHandle internally) since it makes it easier to work with and address in code
	 */

	class ContentBrowserPanel : public EditorPanel	
	{
	public:
		ContentBrowserPanel();
		~ContentBrowserPanel() = default;

		[[nodiscard]] inline static Ref<ContentBrowserPanel> Create()
		{
			return CreateRef<ContentBrowserPanel>();
		}

		virtual void OnImGuiRender(bool& isOpen) override;

		virtual void OnEvent(Events::Event& e) override;
		virtual void OnProjectChanged(const Ref<Project>& project) override;
		virtual void SetSceneContext(const Ref<Scene>& context) override { m_SceneContext = context; }

		inline Utils::ItemList& GetCurrentItems() { return m_CurrentItems; }
		inline const Utils::ItemList& GetCurrentItems() const { return m_CurrentItems; }
		inline Utils::SelectionStack& GetCurrentCopiedItems() { return m_CopiedAssets; }
		inline const Utils::SelectionStack& GetCurrentCopiedItems() const { return m_CopiedAssets; }

		Ref<DirectoryInfo> GetDirectory(const std::filesystem::path& filepath) const;
		const Ref<DirectoryInfo> GetCurrentOpenDirectory() const { return m_CurrentDirectory; }

		inline void RegisterItemActivateCallback(AssetType type, const std::function<void(const AssetMetaData&)>& callback) { m_ItemActivationCallbacks[type] = callback; }

		inline bool IsCopyingItems() const { return m_CopiedAssets.Size() != 0; }

		Ref<ThumbnailCache> GetThumbnailCache() const { return m_ThumbnailCache; }

		inline static ContentBrowserPanel& Get() { IR_VERIFY(s_Instance); return *s_Instance; }
		
	private:
		AssetHandle ProcessDirectory(const std::filesystem::path& directoryPath, const Ref<DirectoryInfo>& parent);

		void ChangeDirectory(const Ref<DirectoryInfo>& directory);
		void OnBrowseBack();
		void OnBrowseForward();

		void Refresh();

		void PasteCopiedAssets();
		void ClearSelections();
		void SortItemList();

		// Query string is in m_SearchBuffer
		Utils::ItemList Search(const Ref<DirectoryInfo>& directoryInfo);

		// NOTE: This is needed for the input stuff that uses a combination of Mouse + Key states so we can not separate them into OnKeyPressed/OnMouseButtonPressed
		void UpdateInput();
		void UpdateBreadCrumbDragDropArea(const Ref<DirectoryInfo>& targetDirectory);

		bool OnKeyPressed(Events::KeyPressedEvent& e);
		bool OnMouseButtonPressed(Events::MouseButtonPressedEvent& e);
		bool OnAssetCreatedNotification(Events::AssetCreatedNotificationEvent& e);

		// TODO: Do we want to have a side outliner for the main files of the hierarchy?
		void RenderTopBar(float height);
		void RenderItems();
		void RenderBottomBar(float height);

		void RenderDeleteDialogue();
		void RemoveDirectory(Ref<DirectoryInfo>& directory, bool removeFromParent = true);
		
		void GenerateThumbnails();

	private:
		// NOTE: Should only used insdie ContentBrowserPanel, everywhere else use the AssetManager!
		template<typename T, typename... Args>
		Ref<T> CreateAsset(const std::string& fileName, Args&&... args)
		{
			return CreateAssetInDirectory<T>(fileName, m_CurrentDirectory, std::forward<Args>(args)...);
		}

		// NOTE: Should only used insdie ContentBrowserPanel, everywhere else use the AssetManager!
		template<typename T, typename... Args>
		Ref<T> CreateAssetInDirectory(const std::string& fileName, Ref<DirectoryInfo>& directory, Args&&... args)
		{
			std::filesystem::path filePath = FileSystem::GetUniqueFileName(Project::GetAssetDirectory() / directory->FilePath / fileName);

			Ref<T> asset = Project::GetEditorAssetManager()->CreateNewAsset<T>(filePath.filename().string(), directory->FilePath, std::forward<Args>(args)...);
			if (!asset)
			{
				IR_CORE_ERROR_TAG("Editor", "Failed to create asset in ContentBrowser with filename: {}", fileName);

				return nullptr;
			}

			directory->Assets.push_back(asset->Handle);

			const AssetMetaData& assetMetaData = Project::GetEditorAssetManager()->GetMetaData(asset);

			Refresh();

			return asset;
		}

	private:
		Ref<Project> m_Project;
		Ref<Scene> m_SceneContext;

		Utils::ItemList m_CurrentItems;

		Ref<ThumbnailCache> m_ThumbnailCache;
		Ref<ThumbnailGenerator> m_ThumbnailGenerator;

		Ref<DirectoryInfo> m_BaseDirectory;
		Ref<DirectoryInfo> m_CurrentDirectory;
		// These are for the undo/redo buttons in the top bar of the content browser panel
		Ref<DirectoryInfo> m_PreviousDirectory;
		Ref<DirectoryInfo> m_NextDirectory;

		Utils::SelectionStack m_CopiedAssets;

		// Maps AssetHandles that map to directy item (NOT ASSETS)
		std::unordered_map<AssetHandle, Ref<DirectoryInfo>> m_Directories;

		std::unordered_map<AssetType, std::function<void(const AssetMetaData&)>> m_ItemActivationCallbacks;

		char m_SearchBuffer[CB_MAX_INPUT_SEARCH_BUFFER_LENGTH]{};

		std::vector<Ref<DirectoryInfo>> m_BreadCrumbData;
		bool m_UpdateNavigationPath = false;

		bool m_IsAnyItemHovered = false;

		bool m_IsContentBrowserHovered = false;
		bool m_IsContentBrowserFocused = false;

		inline static ContentBrowserPanel* s_Instance;
	};

}