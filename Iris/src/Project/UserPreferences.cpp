#include "IrisPCH.h"
#include "UserPreferences.h"

#include <yaml-cpp/yaml.h>

namespace Iris {

	void UserPreferencesSerializer::Serialize(Ref<UserPreferences>& userPreferences, const std::filesystem::path& filepath)
	{
		YAML::Emitter out;

		out << YAML::BeginMap;

		out << YAML::Key << "UserPreferences" << YAML::Value;
		{
			out << YAML::BeginMap;

			if (!userPreferences->StartupProject.empty())
				out << YAML::Key << "StartupProject" << YAML::Value << userPreferences->StartupProject;

			out << YAML::Key << "RecentProjects" << YAML::Value << YAML::BeginSeq;
			for (const auto& [timeOpened, project] : userPreferences->RecentProjects)
			{
				out << YAML::BeginMap;

				out << YAML::Key << "Name" << YAML::Value << project.Name;
				out << YAML::Key << "ProjectPath" << YAML::Value << project.Filepath;
				out << YAML::Key << "LastOpened" << YAML::Value << project.LastOpened;

				out << YAML::EndMap;
			}
			out << YAML::EndSeq;

			out << YAML::EndMap;
		}

		out << YAML::EndMap;

		std::ofstream ofStream(filepath);
		ofStream << out.c_str();

		userPreferences->Filepath = filepath.string();
	}

	void UserPreferencesSerializer::Deserialize(Ref<UserPreferences>& userPreferences, const std::filesystem::path& filepath)
	{
		std::ifstream ifStream(filepath);
		IR_ASSERT(ifStream);

		std::stringstream strStream;
		strStream << ifStream.rdbuf();

		YAML::Node data = YAML::Load(strStream.str());
		if (!data["UserPreferences"])
			return;

		YAML::Node rootNode = data["UserPreferences"];
		userPreferences->StartupProject = rootNode["StartupProject"].as<std::string>("");

		for (auto recentProject : rootNode["RecentProjects"])
		{
			RecentProject entry = {
				.Name = recentProject["Name"].as<std::string>(),
				.Filepath = recentProject["ProjectPath"].as<std::string>(),
				.LastOpened = recentProject["LastOpened"].as<time_t>(time(NULL))
			};
			userPreferences->RecentProjects[entry.LastOpened] = entry;
		}

		userPreferences->Filepath = filepath.string();
	}

}