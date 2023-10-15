#include "Core/Application.h"
#include "Core/Inits.h"

vkPlayground::Application* vkPlayground::CreateApplication(const vkPlayground::ApplicationSpecification& spec)
{
    return new vkPlayground::Application(spec);
}

int main() 
{
    vkPlayground::ApplicationSpecification appSpec = {
        .Name = "Vulkan Playground",
        .WindowWidth = 1280,
        .WindowHeight = 720,
        .FullScreen = false,
        .VSync = false,
        .StartMaximized = false,
        .Resizable = true,
        .RendererConfig = { .FramesInFlight = 3 }
    };

    vkPlayground::Initializers::InitializeCore();
    vkPlayground::Application* app = CreateApplication(appSpec);
    PG_ASSERT(app, "App is Null!");
    app->Run();
    delete app;
    vkPlayground::Initializers::ShutdownCore();
}