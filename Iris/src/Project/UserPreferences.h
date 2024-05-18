#pragma once

#include "Project.h"

namespace Iris {

	struct RecentProject
	{
		std::string Name;
		std::string Filepath;
		time_t LastOpened;
	};

	struct UserPreferences : public RefCountedObject
	{
		[[nodiscard]] static Ref<UserPreferences> Create()
		{
			return CreateRef<UserPreferences>();
		}

		std::string StartupProject;
		std::map<time_t, RecentProject, std::greater<time_t>> RecentProjects;

		// Not serialized. This is just to keep track of where the user preferences where serialized
		std::string Filepath;
	};


	class UserPreferencesSerializer
	{
	public:
		static void Serialize(Ref<UserPreferences>& userPreferences, const std::filesystem::path& filepath);
		static void Deserialize(Ref<UserPreferences>& userPreferences, const std::filesystem::path& filepath);
	};

}