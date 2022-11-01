#include "Application.h"

vkPlayground::Application* CreateApplication(/* TODO: ApplicationSpecification */)
{
    return new vkPlayground::Application();
}

int main() 
{
    vkPlayground::Application* app = CreateApplication();
    PG_ASSERT(app, "App is Null!");
    app->Run();
    delete app;
}