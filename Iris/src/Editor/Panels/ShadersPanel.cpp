#include "IrisPCH.h"
#include "ShadersPanel.h"

#include "Core/Input/Input.h"
#include "ImGui/ImGuiUtils.h"
#include "Project/Project.h"
#include "Renderer/Renderer.h"
#include "Renderer/Shaders/Shader.h"
#include "Scene/Scene.h"

#include <imgui/imgui.h>

namespace Iris {

    static bool s_ActivateSearchWidget = false;

    Ref<ShadersPanel> ShadersPanel::Create()
    {
        return CreateRef<ShadersPanel>();
    }

    void ShadersPanel::OnImGuiRender(bool& isOpen)
    {
        ImGui::Begin("Shaders", &isOpen, ImGuiWindowFlags_NoCollapse);

        constexpr float edgeOffset = 4.0f;
        UI::ShiftCursorX(edgeOffset * 3.0f);
        UI::ShiftCursorY(edgeOffset * 2.0f);

        if (s_ActivateSearchWidget)
        {
            ImGui::SetKeyboardFocusHere();
            s_ActivateSearchWidget = false;
        }

        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - edgeOffset * 3.0f);
        static std::string searchString;
        UI::SearchWidget(searchString);

        ImGui::Spacing();
        ImGui::Spacing();

        UI::ShiftCursorX(10.0f);
        ImGui::TextUnformatted(fmt::format("Total Shaders: {}", Renderer::GetShadersLibrary()->GetSize()).c_str());
        UI::UnderLine();

        UI::BeginPropertyGrid();

        uint32_t counter = 0;
        Ref<ShadersLibrary> library = Renderer::GetShadersLibrary();
        for (auto& [name, shader] : library->GetShaders())
        {
            if (!UI::IsMatchingSearch(name, searchString))
                continue;

            ImGui::PushID(fmt::format("shader{0}", name).c_str());

            UI::PropertyStringReadOnly(" - Name", name.c_str());
            UI::PropertyStringReadOnly("   Path", shader->GetFilePath().c_str());

            UI::ShiftCursor(15.0f, 9.0f);

            if (ImGui::Button("Reload"))
                shader->Reload();

            bool reloadingSucceeded = shader->GetCompilationStatus();
            ImGui::NextColumn();
            UI::ShiftCursorY(4.0f);
            
            ImGui::PushItemWidth(-1);
            std::string message = reloadingSucceeded ? "Shader Ready!" : "Compilation Error! Check console for more info. Reverting to previous shader version.";
            if (!reloadingSucceeded)
                ImGui::PushStyleColor(ImGuiCol_Text, Colors::Theme::TextError);
            else
                ImGui::PushStyleColor(ImGuiCol_Text, Colors::Theme::TextSuccess);

            ImGui::InputText(fmt::format("##shader{0}compilationFailed", name).c_str(), (char*)message.c_str(), message.size(), ImGuiInputTextFlags_ReadOnly);
            
            ImGui::PopStyleColor();
            ImGui::PopItemWidth();

            ImGui::NextColumn();
            if (counter++ < library->GetSize() - 1)
                ImGui::Separator();

            ImGui::PopID();
        }

        UI::EndPropertyGrid();

        ImGui::End();
    }

    void ShadersPanel::OnEvent(Events::Event& e)
    {
        Events::EventDispatcher dispatcher(e);

        dispatcher.Dispatch<Events::KeyPressedEvent>([this](Events::KeyPressedEvent& e) { return OnKeyPressed(e); });
    }

    bool ShadersPanel::OnKeyPressed(Events::KeyPressedEvent& e)
    {
        if (Input::IsKeyDown(KeyCode::LeftControl))
        {
            switch (e.GetKeyCode())
            {
                case KeyCode::F:
                {
                    s_ActivateSearchWidget = true;
                    break;
                }
            }
        }

        return false;
    }

}