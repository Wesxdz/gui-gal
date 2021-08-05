#ifndef SHARED_H
#define SHARED_H

#include <flecs/flecs.h>
#include <GLFW/glfw3.h>

GLFWwindow* window;
ecs_world_t* world;
ecs_entity_t renderer;
ecs_entity_t input;

#endif