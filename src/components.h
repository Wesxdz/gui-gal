#include <cglm/cglm.h>
#include <GLFW/glfw3.h>
#include <SDL2/SDL_image.h>

typedef struct
{
    float scale;
    vec2 pos;
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
    float width, height;
} BoundingBox;
// TODO: Bounding volumes

typedef struct
{
    GLuint id;
    unsigned pow_w, pow_h;
    unsigned width, height;
} Texture2D;

typedef struct
{
    const char* path;
} LocalFile;

typedef struct
{
    GLuint* ids;
    int textureCount;
} MultiTexture2D; // For animated images

#include <gif_lib.h>

typedef struct
{
    GifFileType* gif;
    int fps;
    float progress;
    size_t frame;
} GifAnimator;

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

// Input
typedef struct {
} ConsumeEvent;

// TODO: Refactor into module
// Should GLFWwindow be included in events?
typedef struct {
    GLFWwindow* window;
    int button, action, mods;
} EventMouseButton;

typedef struct {
    GLFWwindow* window;
    vec2 pos, delta;
} EventMouseMotion;

typedef struct {
    GLFWwindow* window;
    double xoffset;
    double yoffset;
} EventScroll;

typedef struct {
    GLFWwindow* window;
    int count;
    const char** paths;
} EventDropFiles;