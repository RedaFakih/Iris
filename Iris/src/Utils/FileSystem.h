#pragma once

#ifdef CreateDirectory
#undef CreateDirectory
#undef DeleteFile
#undef CopyFile
#undef MoveFile
#endif

#include "Core/Buffer.h"

#include <filesystem>

namespace Iris {

	class FileSystem
	{
	public:
		static bool CreateDirectory(const std::filesystem::path& directoy);
		static bool Exists(const std::filesystem::path& path);
		static bool DeleteFile(const std::filesystem::path& path);
		// Provide the full target destPath for Copy and Move
		static bool CopyFile(const std::filesystem::path& srcPath, const std::filesystem::path& destPath);
		static bool MoveFile(const std::filesystem::path& srcPath, const std::filesystem::path& destPath);

		static bool IsDirectory(const std::filesystem::path& path);
		static bool IsNewer(const std::filesystem::path& path1, const std::filesystem::path& path2);

		static bool Copy(const std::filesystem::path& oldPath, const std::filesystem::path& newPath);
		static bool Move(const std::filesystem::path& oldPath, const std::filesystem::path& newPath);
		static bool Rename(const std::filesystem::path& oldPath, const std::filesystem::path& newPath);
		static bool RenameFilename(const std::filesystem::path& oldPath, const std::string& newName);

		static bool ShowFileInExplorer(const std::filesystem::path& path);
		static bool OpenDirectoryInExplorer(const std::filesystem::path& directory);
		static bool OpenExternally(const std::filesystem::path& path);

		static bool WriteBytes(const std::filesystem::path& filepath, const Buffer& buffer);
		static Buffer ReadBytes(const std::filesystem::path& filepath);

		static std::filesystem::path GetUniqueFileName(const std::filesystem::path& path);
		static std::filesystem::path Absolute(const std::filesystem::path& path);

		struct FileDialogFilterItem
		{
			const char* Name;
			const char* Spec;
		};

		static std::filesystem::path OpenFileDialog(const std::initializer_list<FileDialogFilterItem> inFilters = {}) noexcept;
		static std::filesystem::path OpenFolderDialog(const char* initialFolder = "") noexcept;
		static std::filesystem::path SaveFileDialog(const std::initializer_list<FileDialogFilterItem> inFilters = {}) noexcept;

		// Returns a path in a persistant storage folder, e.g. Users/User/AppData/Roaming...
		static std::filesystem::path GetPersistantStoragePath();

	};

}