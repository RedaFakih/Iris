#include "Core/Application.h"
#include "Core/Inits.h"

extern vkPlayground::Application* vkPlayground::CreateApplication(int argc, char** argv);

int main(int argc, char** argv)
{
    vkPlayground::Initializers::InitializeCore();
    vkPlayground::Application* app = vkPlayground::CreateApplication(argc, argv);
    PG_ASSERT(app, "App is Null!");
    app->Run();
    delete app;
    vkPlayground::Initializers::ShutdownCore();
}