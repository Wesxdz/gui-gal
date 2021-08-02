#include <cglm/cglm.h>
#include <GLFW/glfw3.h>
#include <SDL2/SDL_image.h>

typedef struct
{
    mat4 view;
} Camera;

typedef struct
{
    float zoomRate, minZoom, maxZoom;
    bool dragging;
} CameraController;

typedef struct
{
    vec2 pos;
    float angle;
    float scale;
    int layer;
} Transform2D;

typedef struct
{
    GLuint id;
    unsigned width, height;
} Texture2D;

typedef struct
{
    unsigned int programId;
} Shader;

typedef struct
{
    Shader shader;
    GLuint quadVAO;
    GLuint indexBuffer;
} BatchSpriteRenderer;