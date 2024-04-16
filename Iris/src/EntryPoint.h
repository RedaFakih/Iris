#include "Core/Application.h"
#include "Core/Inits.h"

extern Iris::Application* Iris::CreateApplication(int argc, char** argv);

int main(int argc, char** argv)
{
    Iris::Initializers::InitializeCore();
    Iris::Application* app = Iris::CreateApplication(argc, argv);
    IR_VERIFY(app, "App is Null!");
    app->Run();
    delete app;
    Iris::Initializers::ShutdownCore();
}