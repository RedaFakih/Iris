#include "IrisPCH.h"
#include "FileSystem.h"

#include "StringUtils.h"

#include <nfd.hpp>

#include <shellapi.h>
#include <ShlObj.h>

namespace Iris {

	static std::filesystem::path s_PersistantStoragePath;

	bool FileSystem::CreateDirectory(const std::filesystem::path& directoy)
	{
		return std::filesystem::create_directories(directoy);
	}

	bool FileSystem::Exists(const std::filesystem::path& path)
	{
		return std::filesystem::exists(path);
	}

	bool FileSystem::DeleteFile(const std::filesystem::path& path)
	{
		if (!Exists(path))
			return false;

		if(IsDirectory(path))
			return std::filesystem::remove_all(path) > 0;
		return std::filesystem::remove(path);
	}

	bool FileSystem::CopyFile(const std::filesystem::path& srcPath, const std::filesystem::path& destPath)
	{
		return Copy(srcPath, destPath);
	}

	bool FileSystem::MoveFile(const std::filesystem::path& srcPath, const std::filesystem::path& destPath)
	{
		return Move(srcPath, destPath);
	}

	bool FileSystem::IsDirectory(const std::filesystem::path& path)
	{
		return std::filesystem::is_directory(path);
	}

	bool FileSystem::IsNewer(const std::filesystem::path& path1, const std::filesystem::path& path2)
	{
		return std::filesystem::last_write_time(path1) > std::filesystem::last_write_time(path2);
	}

	bool FileSystem::Copy(const std::filesystem::path& oldPath, const std::filesystem::path& newPath)
	{
		if (!Exists(newPath))
			return false;

		std::filesystem::copy(oldPath, newPath);
		return true;
	}

	bool FileSystem::Move(const std::filesystem::path& oldPath, const std::filesystem::path& newPath)
	{
		if (!Exists(newPath))
			return false;

		std::filesystem::rename(oldPath, newPath);
		return true;
	}

	bool FileSystem::Rename(const std::filesystem::path& oldPath, const std::filesystem::path& newPath)
	{
		return Move(oldPath, newPath);
	}

	bool FileSystem::RenameFilename(const std::filesystem::path& oldPath, const std::string& newName)
	{
		std::filesystem::path newPath = oldPath.parent_path() / std::filesystem::path(newName + oldPath.extension().string());
		return Rename(oldPath, newPath);
	}

	bool FileSystem::ShowFileInExplorer(const std::filesystem::path& path)
	{
		auto absolutePath = std::filesystem::canonical(path);
		if (!Exists(absolutePath))
			return false;

		std::string cmd = fmt::format("explorer.exe /select,\"{0}\"", absolutePath.string());

		system(cmd.c_str());
		return true;
	}

	bool FileSystem::OpenDirectoryInExplorer(const std::filesystem::path& directory)
	{
		auto absolutePath = std::filesystem::canonical(directory);
		if (!Exists(absolutePath))
			return false;

		ShellExecute(NULL, L"explore", absolutePath.c_str(), NULL, NULL, SW_SHOWNORMAL);
		return true;
	}

	bool FileSystem::OpenExternally(const std::filesystem::path& path)
	{
		auto absolutePath = std::filesystem::canonical(path);
		if (!Exists(absolutePath))
			return false;

		ShellExecute(NULL, L"open", absolutePath.c_str(), NULL, NULL, SW_SHOWNORMAL);
		return true;
	}

	std::filesystem::path FileSystem::GetUniqueFilename(const std::filesystem::path& path)
	{
		if (!Exists(path))
			return path;

		int counter = 0;
		auto checkID = [&counter, path](auto checkID) -> std::filesystem::path
		{
			++counter;
			const std::string counterStr = counter < 10 ? "0" + std::to_string(counter) : std::to_string(counter);

			std::string newFileName = fmt::format("{} ({})", Utils::RemoveExtension(path.filename().string()), counter);

			if (path.has_extension())
				newFileName = fmt::format("{}{}", newFileName, path.extension().string());

			if (Exists(path.parent_path() / newFileName))
				return checkID(checkID);
			else
				return path.parent_path() / newFileName;
		};

		return checkID(checkID);
	}

	std::filesystem::path FileSystem::Absolute(const std::filesystem::path& path)
	{
		return std::filesystem::absolute(path);
	}

	std::filesystem::path FileSystem::OpenFileDialog(const std::initializer_list<FileDialogFilterItem> inFilters)
	{
		NFD::UniquePath filePath;
		nfdresult_t result = NFD::OpenDialog(filePath, reinterpret_cast<const nfdfilteritem_t*>(inFilters.begin()), static_cast<nfdfiltersize_t>(inFilters.size()));

		switch (result)
		{
			case NFD_OKAY:
				return filePath.get();
			case NFD_CANCEL:
				return "";
			case NFD_ERROR:
			{
				IR_VERIFY(false, "NFD-Extended error: {}", NFD::GetError());
				return "";
			}
		}

		IR_VERIFY(false);
		return "";
	}

	std::filesystem::path FileSystem::OpenFolderDialog(const char* initialFolder)
	{
		NFD::UniquePath filePath;
		nfdresult_t result = NFD::PickFolder(filePath, initialFolder);

		switch (result)
		{
			case NFD_OKAY:
				return filePath.get();
			case NFD_CANCEL:
				return "";
			case NFD_ERROR:
			{
				IR_VERIFY(false, "NFD-Extended error: {}", NFD::GetError());
				return "";
			}
		}

		IR_VERIFY(false);
		return "";
	}

	std::filesystem::path FileSystem::SaveFileDialog(const std::initializer_list<FileDialogFilterItem> inFilters)
	{
		NFD::UniquePath filePath;
		nfdresult_t result = NFD::SaveDialog(filePath, reinterpret_cast<const nfdfilteritem_t*>(inFilters.begin()), static_cast<nfdfiltersize_t>(inFilters.size()));

		switch (result)
		{
			case NFD_OKAY:
				return filePath.get();
			case NFD_CANCEL:
				return "";
			case NFD_ERROR:
			{
				IR_VERIFY(false, "NFD-Extended error: {}", NFD::GetError());
				return "";
			}
		}

		IR_VERIFY(false);
		return "";
	}

	std::filesystem::path FileSystem::GetPersistantStoragePath()
	{
		if (!s_PersistantStoragePath.empty())
			return s_PersistantStoragePath;

		PWSTR roamingFilePath;
		HRESULT result = SHGetKnownFolderPath(FOLDERID_RoamingAppData, KF_FLAG_DEFAULT, NULL, &roamingFilePath);
		IR_VERIFY(result == S_OK);

		s_PersistantStoragePath = roamingFilePath;
		s_PersistantStoragePath /= "Iris";

		if (!Exists(s_PersistantStoragePath))
			CreateDirectory(s_PersistantStoragePath);

		return s_PersistantStoragePath;
	}

}