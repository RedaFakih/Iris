#include "IrisPCH.h"
#include "RendererConfiguration.h"

#include "Renderer.h"
#include "Utils/FileSystem.h"

#include <yaml-cpp/yaml.h>

namespace Iris {

	static std::filesystem::path s_RendererConfigurationPath;

	void RendererConfigurationSerialzier::Init()
	{
		s_RendererConfigurationPath = std::filesystem::absolute("Config");

		if (!FileSystem::Exists(s_RendererConfigurationPath))
			FileSystem::CreateDirectory(s_RendererConfigurationPath);

		s_RendererConfigurationPath /= "RendererConfiguration.yaml";

		Deserialize(Renderer::GetConfig());
	}

	void RendererConfigurationSerialzier::Serialize(const RendererConfiguration& config)
	{
		YAML::Emitter out;

		out << YAML::BeginMap;

		out << YAML::Key << "RendererConfiguration" << YAML::Value;
		{
			out << YAML::BeginMap;

			out << YAML::Key << "ComputeEnvironmentMaps" << YAML::Value << config.ComputeEnvironmentMaps;
			out << YAML::Key << "EnvironmentMapResolution" << YAML::Value << config.EnvironmentMapResolution;
			out << YAML::Key << "IrradianceMapComputeSamples" << YAML::Value << config.IrradianceMapComputeSamples;
			out << YAML::Key << "MaxNumberOfPointLights" << YAML::Value << config.MaxNumberOfPointLights;
			out << YAML::Key << "MaxNumberOfSpotLights" << YAML::Value << config.MaxNumberOfSpotLights;

			out << YAML::EndMap;
		}

		out << YAML::EndMap;

		std::ofstream ofStream(s_RendererConfigurationPath);
		ofStream << out.c_str();
	}

	void RendererConfigurationSerialzier::Deserialize(RendererConfiguration& config)
	{
		// Generate default settings file if one doesn't exist
		if (!FileSystem::Exists(s_RendererConfigurationPath))
		{
			Serialize(Renderer::GetConfig());
			return;
		}

		std::ifstream ifStream(s_RendererConfigurationPath);
		IR_ASSERT(ifStream);
		std::stringstream ss;
		ss << ifStream.rdbuf();

		YAML::Node data = YAML::Load(ss.str());
		if (!data["RendererConfiguration"])
			return;

		YAML::Node rootNode = data["RendererConfiguration"];

		config.ComputeEnvironmentMaps = rootNode["ComputeEnvironmentMaps"].as<bool>(true);
		config.EnvironmentMapResolution = rootNode["EnvironmentMapResolution"].as<uint32_t>(1024);
		config.IrradianceMapComputeSamples = rootNode["IrradianceMapComputeSamples"].as<uint32_t>(512);
		config.MaxNumberOfPointLights = rootNode["MaxNumberOfPointLights"].as<uint32_t>(100);
		config.MaxNumberOfSpotLights = rootNode["MaxNumberOfSpotLights"].as<uint32_t>(100);
	}

}