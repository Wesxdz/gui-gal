#ifndef INPUT_MODULE_H
#define INPUT_MODULE_H

#include <flecs/flecs.h>
#include <GLFW/glfw3.h>

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);

typedef struct {
    GLFWwindow* window;
    int button, action, mods;
} EventMouseButton;

typedef struct InputModule {
    ECS_DECLARE_COMPONENT(EventMouseButton);
} InputModule;

void InputModuleImport(ecs_world_t* world);

#define InputModuleImportHandles(handles)\
    ECS_IMPORT_COMPONENT(handles, EventMouseButton);

#endif