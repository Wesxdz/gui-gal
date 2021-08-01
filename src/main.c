#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <stdio.h>


void centerWindow(GLFWwindow *window, GLFWmonitor *monitor)
{
    if (!monitor)
        return;

    const GLFWvidmode *mode = glfwGetVideoMode(monitor);
    if (!mode)
        return;

    int monitorX, monitorY;
    glfwGetMonitorPos(monitor, &monitorX, &monitorY);

    int windowWidth, windowHeight;
    glfwGetWindowSize(window, &windowWidth, &windowHeight);
    glfwSetWindowPos(window,
                     monitorX + (mode->width - windowWidth) / 2,
                     monitorY + (mode->height - windowHeight) / 2);
}

void error_callback(int error, const char* description)
{
    fprintf(stderr, "Error: %s\n", description);
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GLFW_TRUE);
}

int main(int argc, char const *argv[])
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);
    // glfwWindowHint(GLFW_DECORATED, GL_FALSE);
    glfwWindowHint(GLFW_VISIBLE, GL_FALSE);
    glfwSetErrorCallback(error_callback);
    GLFWwindow* window = glfwCreateWindow(640, 480, "Gui Gal", NULL, NULL);
    glfwSetKeyCallback(window, key_callback);
    gladLoadGL();
    glfwShowWindow(window);
    while (!glfwWindowShouldClose(window))
    {
    }
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

