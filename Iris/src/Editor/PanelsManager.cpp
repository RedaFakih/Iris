#include "IrisPCH.h"
#include "PanelsManager.h"

#include "Renderer/Texture.h"
#include "Renderer/UniformBufferSet.h"
#include "Scene/Scene.h"

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

                // TODO:
                //if (closedThisFrame)
                //    Serialize();
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