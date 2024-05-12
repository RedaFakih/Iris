#include "IrisPCH.h"
#include "PanelsManager.h"

#include "Renderer/Texture.h"
#include "Renderer/UniformBufferSet.h"
#include "Scene/Scene.h"
#include "Utils/FileSystem.h"

#include <yaml-cpp/yaml.h>

namespace Iris {

    Scope<PanelsManager> PanelsManager::Create()
    {
        return CreateScope<PanelsManager>();
    }

    PanelsManager::~PanelsManager()
    {
        for (auto& panelMap : m_Panels)
            panelMap.clear();
    }

    void PanelsManager::OnImGuiRender()
    {
        for (auto& panelMap : m_Panels)
        {
            for (auto& [id, panelSpec] : panelMap)
            {
                bool closedThisFrame = false;

                if (panelSpec.IsOpen)
                {
                    panelSpec.Panel->OnImGuiRender(panelSpec.IsOpen);
                    closedThisFrame = !panelSpec.IsOpen;
                }

                if (closedThisFrame)
                    Serialize();
            }
        }
    }

    void PanelsManager::OnEvent(Events::Event& e)
    {
        for (auto& panelMap : m_Panels)
        {
            for (auto& [id, panelSpec] : panelMap)
                panelSpec.Panel->OnEvent(e);
        }
    }

    void PanelsManager::SetSceneContext(const Ref<Scene> scene)
    {
        for (auto& panelMap : m_Panels)
        {
            for (auto& [id, panelSpec] : panelMap)
                panelSpec.Panel->SetSceneContext(scene);
        }
    }

    void PanelsManager::OnProjectChanged(Ref<Project> project)
    {
        for (auto& panelMap : m_Panels)
        {
            for (auto& [id, panelData] : panelMap)
                panelData.Panel->OnProjectChanged(project);
        }

        Deserialize();
    }

    void PanelsManager::Serialize()
    {
        YAML::Emitter out;
        out << YAML::BeginMap;

        out << YAML::Key << "Panels" << YAML::Value << YAML::BeginSeq;

        for (std::size_t category = 0; category < m_Panels.size(); category++)
        {
            for (const auto& [panelID, spec] : m_Panels[category])
            {
                out << YAML::BeginMap;

                out << YAML::Key << "ID" << YAML::Value << panelID;
                out << YAML::Key << "Name" << YAML::Value << spec.Name;
                out << YAML::Key << "IsOpen" << YAML::Value << spec.IsOpen;

                out << YAML::EndMap;
            }
        }

        out << YAML::EndSeq;

        out << YAML::EndMap;

        std::ofstream fout(FileSystem::GetPersistantStoragePath() / "EditorLayout.yaml");
        fout << out.c_str();
        fout.close();
    }

    void PanelsManager::Deserialize()
    {
        std::filesystem::path layoutPath = FileSystem::GetPersistantStoragePath() / "EditorLayout.yaml";
        if (!FileSystem::Exists(layoutPath))
            return;

        std::ifstream fin(layoutPath);
        IR_VERIFY(fin);

        std::stringstream ss;
        ss << fin.rdbuf();

        YAML::Node data = YAML::Load(ss.str());
        YAML::Node panels = data["Panels"];
        if (!panels)
        {
            IR_CORE_ERROR_TAG("PanelsManager", "Failed to load EditorLayout.yaml! Path: {}", layoutPath.string());
            return;
        }

        for (auto panelNode : panels)
        {
            PanelSpecification* panelSpec = GetPanelSpec(panelNode["ID"].as<uint32_t>(0));

            if (panelSpec == nullptr)
                continue;

            panelSpec->IsOpen = panelNode["IsOpen"].as<bool>(panelSpec->IsOpen);
        }
    }

    PanelSpecification* PanelsManager::GetPanelSpec(uint32_t id)
    {
        for (auto& panelMap : m_Panels)
        {
            if (!panelMap.contains(id))
                continue;

            return &panelMap.at(id);
        }

        return nullptr;
    }

    const PanelSpecification* PanelsManager::GetPanelSpec(uint32_t id) const
    {
        for (const auto& panelMap : m_Panels)
        {
            if (!panelMap.contains(id))
                continue;

            return &panelMap.at(id);
        }

        return nullptr;
    }

    void PanelsManager::RemovePanel(const char* strID)
    {
        uint32_t panelID = Hash::GenerateFNVHash(strID);

        for (auto& panelMap : m_Panels)
        {
            if (!panelMap.contains(panelID))
                continue;

            panelMap.erase(panelID);
            return;
        }

        IR_CORE_ERROR_TAG("PanelsManager", "Could not find panel with ID: {}", strID);
    }

}