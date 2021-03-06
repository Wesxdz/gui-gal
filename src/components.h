#include <cglm/cglm.h>
#include <GLFW/glfw3.h>
#include <SDL2/SDL_image.h>
#include <flecs.h>
#include "nanovg.h"

typedef struct
{
    float interpTime;
    float scale;
    float targetScale;
    vec2 pos;
    vec2 targetPos;
    mat4 view;
    mat4 targetView;
} Camera;
ECS_COMPONENT_DECLARE(Camera);

typedef struct
{
    float zoomRate, minZoom, maxZoom;
    bool dragging;
} CameraController;
ECS_COMPONENT_DECLARE(CameraController);

typedef struct
{
    vec2 pos;
} Local2D;
ECS_DECLARE_COMPONENT(Local2D);

typedef struct
{
    vec2 pos;
    float angle;
    float scale;
    int layer;
} Transform2D;
ECS_COMPONENT_DECLARE(Transform2D);

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
    vec2 scale;
    SDL_Surface* img;
} Texture2D;
ECS_COMPONENT_DECLARE(Texture2D);

typedef struct
{
    const char* path;
} LocalFile;
ECS_COMPONENT_DECLARE(LocalFile);

typedef struct
{
    GLuint* ids;
    int textureCount;
} MultiTexture2D; // For animated images
ECS_COMPONENT_DECLARE(MultiTexture2D);

#include <gif_lib.h>

typedef struct
{
    GifFileType* gif;
    int fps;
    float progress;
    size_t frame;
} GifAnimator;
ECS_COMPONENT_DECLARE(GifAnimator);

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
ECS_COMPONENT_DECLARE(BatchSpriteRenderer);

typedef struct {
    float x, y, w, h;
} DragSelector;
ECS_COMPONENT_DECLARE(DragSelector);

typedef struct
{
    float width, height;
} NineSlice;
ECS_COMPONENT_DECLARE(NineSlice);

typedef struct
{
    ecs_entity_t actionIndicators[4];
} Selected;
ECS_COMPONENT_DECLARE(Selected);

ECS_TAG_DECLARE(Grabbed);
ECS_TAG_DECLARE(DragHover);
ECS_TAG_DECLARE(TakeSnapshot);
ECS_TAG_DECLARE(SnapshotTarget);

// Input
typedef struct {
} ConsumeEvent;
ECS_COMPONENT_DECLARE(ConsumeEvent);

// TODO: Refactor into module
// Should GLFWwindow be included in events?
typedef struct {
    GLFWwindow* window;
    int button, action, mods;
} EventMouseButton;
ECS_COMPONENT_DECLARE(EventMouseButton);

typedef struct {
    GLFWwindow* window;
    vec2 pos, delta;
} EventMouseMotion;
ECS_COMPONENT_DECLARE(EventMouseMotion);

typedef struct {
    GLFWwindow* window;
    double xoffset;
    double yoffset;
} EventScroll;
ECS_COMPONENT_DECLARE(EventScroll);

typedef struct {
    GLFWwindow* window;
    int count;
    const char** paths;
} EventDropFiles;
ECS_COMPONENT_DECLARE(EventDropFiles);

typedef struct {
  GLFWwindow* window;
  int key, scancode, action, mods;
} EventKey;
ECS_COMPONENT_DECLARE(EventKey);

typedef struct {
    NVGcontext* vg;
} NanoVG;
ECS_COMPONENT_DECLARE(NanoVG);

typedef struct {
    c2Circle bounds;
    int cursorType;
    int op;
    vec2 screenOffset;
    ecs_entity_t symbol;
    NVGcolor color;
} CircleActionIndicator;
ECS_COMPONENT_DECLARE(CircleActionIndicator);

typedef struct {
    bool active;
    ecs_entity_t symbol;
    int op;
    vec2 origin;
    vec2 startScale;
} ActionOnMouseInput;
ECS_COMPONENT_DECLARE(ActionOnMouseInput);

enum Operations
{
    SCALE_UPPER_LEFT,
    SCALE_UPPER_RIGHT,
    SCALE_LOWER_RIGHT,
    SCALE_LOWER_LEFT,
};

typedef struct
{
    float horizontal, vertical;
} Anchor;
ECS_COMPONENT_DECLARE(Anchor);

typedef struct
{
    int time_step;
    size_t index;
    size_t capacity;
    size_t count;
    ecs_snapshot_t** snapshots;
} CommandBuffer;

typedef struct
{
    char* path;
    // TODO: Format?
} SavedData;
ECS_COMPONENT_DECLARE(SavedData);

ECS_TAG_DECLARE(BrowseDirectory);