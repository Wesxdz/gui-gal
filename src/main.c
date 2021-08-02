#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <flecs/flecs.h>
#include <SDL2/SDL_image.h>
#include <string.h>
#include <gif_lib.h>
#include <sys/types.h>
#include <sys/stat.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "components.h"

void print_log(GLuint object)
{
	GLint log_length = 0;
	if (glIsShader(object))
		glGetShaderiv(object, GL_INFO_LOG_LENGTH, &log_length);
	else if (glIsProgram(object))
		glGetProgramiv(object, GL_INFO_LOG_LENGTH, &log_length);
	else
	{
		fprintf(stderr, "printlog: Not a shader or a program\n");
		return;
	}

	char* log = (char*)malloc(log_length);

	if (glIsShader(object))
		glGetShaderInfoLog(object, log_length, NULL, log);
	else if (glIsProgram(object))
		glGetProgramInfoLog(object, log_length, NULL, log);

	fprintf(stderr, "%s", log);
	free(log);
}

GLuint create_shader_from_string(const char* source, GLenum type)
{
	GLuint res = glCreateShader(type);
	const char* sources[] = { source };
	glShaderSource(res, 1, sources, NULL);
	glCompileShader(res);
	GLint compile_ok = GL_FALSE;
	glGetShaderiv(res, GL_COMPILE_STATUS, &compile_ok);
	if (compile_ok == GL_FALSE)
	{
		fprintf(stderr, "Error compiling shader of type %d!\n", type);
		print_log(res);
		glDeleteShader(res);
		return 0;
	}

	return res;
}

char* get_shader_source(const char* file)
{
    FILE* fp;
    long lSize;
    char* buffer;

    fp = fopen (file, "rb");
    if(!fp)
        perror("blah.txt"),exit(1);

    fseek(fp, 0L, SEEK_END);
    lSize = ftell( fp );
    rewind( fp );

    /* allocate memory for entire content */
    buffer = calloc(1, lSize+1);
    if(!buffer)
        fclose(fp),fputs("memory alloc fails",stderr),exit(1);

    /* copy the file into the buffer */
    if( 1!=fread( buffer , lSize, 1 , fp) )
        fclose(fp),free(buffer),fputs("entire read fails",stderr),exit(1);

    /* do your work here, buffer is a string contains the whole text */


    fclose(fp);
    return buffer;
}

Shader create_shader(const char* name)
{
    printf("Shader started!\n");
    Shader shader;
    shader.programId = glCreateProgram();
    GLuint vertex, fragment;

    GLchar file[50];
    sprintf(file, "../res/shader/%s.vert", name);
    char* source = get_shader_source(file);
    vertex = create_shader_from_string(source, GL_VERTEX_SHADER);
    free(source);

    memset(file, 0, sizeof file);
    sprintf(file, "../res/shader/%s.frag", name);
    source = get_shader_source(file);
    fragment = create_shader_from_string(source, GL_FRAGMENT_SHADER);
    free(source);

    glAttachShader(shader.programId, vertex);
    glAttachShader(shader.programId, fragment);

    GLint success;
    glLinkProgram(shader.programId);
    glGetProgramiv(shader.programId, GL_LINK_STATUS, &success);
    if(!success) {
        print_log(shader.programId);
    }

    glDeleteShader(vertex);
    glDeleteShader(fragment);
    printf("Shader created!\n");
    return shader;
}

void setup_camera(ecs_iter_t* it)
{
    printf("Setup camera\n");
    Camera* camera = ecs_term(it, Camera, 1);
    mat4 view;
    vec3 eye = {0.0f, 0.0f, 0.0};
    vec3 center = {0.0, 0.0, -1.0};
    vec3 up = {0.0, 1.0, 0.0};
    glm_lookat(eye, center, up, camera->view);
}

void setup_batch_renderer(ecs_iter_t* it)
{
    BatchSpriteRenderer* renderer = ecs_term(it, BatchSpriteRenderer, 1);

    renderer->shader = create_shader("quad");
    // configure VAO/VBO
    GLuint VBO;
    GLfloat vertices[] = {
        // pos      // tex
        0.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 1.0f,
        1.0f, 0.0f, 1.0f, 0.0f,
        1.0f, 1.0f, 1.0f, 1.0f,
    };

    GLubyte indices[] = {1, 2, 0, 1, 3, 2};

    glGenVertexArrays(1, &renderer->quadVAO);
    glBindVertexArray(renderer->quadVAO);

    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (void*)0);
    glEnableVertexAttribArray(0);

    glGenBuffers(1, &renderer->indexBuffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, renderer->indexBuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_DYNAMIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

GLFWwindow* window;
ecs_world_t* world;
ecs_entity_t renderer;

void animate_gif(ecs_iter_t* it)
{
    GifAnimator* animator = ecs_column(it, GifAnimator, 1);
    Texture2D* texture = ecs_column(it, Texture2D, 2);
    for (int i = 0; i < it->count; i++)
    {
        // How to get FPS/frame delays???
        // animator[i].gif->
        // texture->id
    }
}

void render_sprites(ecs_iter_t* it)
{
    Camera* camera = ecs_term(it, Camera, 1);
    BatchSpriteRenderer* renderer = ecs_term(it, BatchSpriteRenderer, 2);
    Transform2D* transform = ecs_column(it, Transform2D, 3);
    Texture2D* texture = ecs_column(it, Texture2D, 4);
    // printf("Use shader %d\n", renderer->shader.programId);
    glUseProgram(renderer->shader.programId);
    glUniformMatrix4fv(glGetUniformLocation(renderer->shader.programId, "view"), 1, false, camera->view[0]);
    mat4 proj;
    int wwidth, wheight;
    glfwGetWindowSize(window, &wwidth, &wheight);
    glm_ortho(0.0, wwidth, wheight, 0.0, -1.0, 10.0, proj); // TODO: Window component
    glUniformMatrix4fv(glGetUniformLocation(renderer->shader.programId, "projection"), 1, false, proj[0]);
    // printf("%d sprite(s)\n", it->count);
    for (int i = 0; i < it->count; i++)
    {
        mat4 model;
        glm_mat4_identity(model);
        vec3 offset = {transform[i].pos[0], transform[i].pos[1], 0.0f};
        glm_translate(model, offset);
        vec3 scale = {texture[i].width, texture[i].height, 1.0};
        glm_scale(model, scale);
        glUniformMatrix4fv(glGetUniformLocation(renderer->shader.programId, "model"), 1, false, model[0]);
        // vec4 box = {10.0, 0.0, texture->surface->w, texture->surface->h};
        // glUniform4f(glGetUniformLocation(renderer->shader.programId, "box"), 100, 40, 1, 1);
        // glUniform2f(glGetUniformLocation(renderer->shader.programId, "scale"), 1, 1);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture[i].id);
        glBindVertexArray(renderer->quadVAO);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, renderer->indexBuffer);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, (void*) 0);
        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
}

void deallocate_texture(ecs_iter_t* it)
{
    Texture2D* texture = ecs_term(it, Texture2D, 1);
    glDeleteTextures(1, &texture->id);
}

void center_window(GLFWwindow *window, GLFWmonitor *monitor)
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

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GLFW_TRUE);
}

unsigned nearest_pow2(int length)
{
    return pow(2,ceil(log(length)/log(2)));
}

void create_texture(const char* file, ecs_entity_t entity)
{
    // printf("%d\n", glGetError());
    ECS_COMPONENT(world, Texture2D);
    GLuint id;
    // glGenTextures(1, &id);
    // glBindTexture(GL_TEXTURE_2D, id);
    // int width, height, nrChannels;
    // unsigned char* data = stbi_load(file, &width, &height, &nrChannels, 0);
    SDL_Surface* img = IMG_Load(file);
    // glBindTexture(GL_TEXTURE_2D, id);
    // glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, img->pixels);

    // stbi_image_free(data);
    int bpp;
    Uint32 Rmask, Gmask, Bmask, Amask; //SDL_PIXELFORMAT_ABGR8888
    SDL_PixelFormatEnumToMasks(SDL_PIXELFORMAT_ABGR8888, &bpp, &Rmask,
    &Gmask, &Bmask, &Amask);
    unsigned pow_w = nearest_pow2(img->w);
    unsigned pow_h = nearest_pow2(img->h);
    size_t pixelCount = sizeof(Uint32) * pow_w * pow_h;
    void* clearSurface = malloc(pixelCount);
    memset(clearSurface, 0, pixelCount);
    SDL_Surface *img_rgba8888 = SDL_CreateRGBSurface(0, img->w, img->h, bpp,
                                                     Rmask, Gmask, Bmask, Amask);
    SDL_SetSurfaceAlphaMod(img, 0xFF);
    SDL_SetSurfaceBlendMode(img, SDL_BLENDMODE_NONE);
    SDL_BlitSurface(img, NULL, img_rgba8888, NULL);
    printf("(%d, %d) -> (%d, %d)\n",img->w, img->h, pow_w, pow_h);
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, pow_w, pow_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, clearSurface);
    free(clearSurface);
    glTexSubImage2D(GL_TEXTURE_2D, 0, (pow_w - img->w)/2.0, (pow_h - img->h)/2.0, img->w, img->h, GL_RGBA, GL_UNSIGNED_BYTE, img_rgba8888->pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexParameteri ( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri ( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    SDL_FreeSurface(img);
    SDL_FreeSurface(img_rgba8888);
    ecs_set(world, entity, Texture2D, {id, pow_w, pow_h});
    // printf("%d\n", glGetError());
}

void window_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}

void drop_callback(GLFWwindow* window, int count, const char** paths)
{
    for (int i = 0; i < count; i++)
    {
        // struct stat* buf = malloc(sizeof stat);
        // stat(paths[i], buf);
        //  = S_ISDIR(buf->st_mode);
         bool isDirectory = false;
        // free(buf);
        int error;
        GifFileType* gif = DGifOpenFileName(paths[i], &error);
        bool isGif = gif != NULL;
        printf("%p:%d\n", (void*)gif, error);
        // printf("Is Gif? %d\n", isGif);
        printf("%p\n", paths[i]);
        if (isGif)
        {
            DGifSlurp(gif); // TODO: Move logic to GifAnimator system
            printf("Gif has %d images!\n" ,gif->ImageCount);
        }
        else if (!isDirectory)
        {
            ecs_entity_t node = ecs_new_id(world);
            ECS_COMPONENT(world, Transform2D);
            ECS_COMPONENT(world, Texture2D);
            double xpos, ypos;
            glfwGetCursorPos(window, &xpos, &ypos);
            // printf("(%f, %f)\n", xpos, ypos);
            create_texture(paths[i], node);
            Texture2D* texture = ecs_get(world, node, Texture2D);
            ECS_COMPONENT(world, Camera);
            Camera* camera = ecs_get(world, renderer, Camera);
            vec4 t;
            mat4 r;
            vec3 s;
            glm_decompose(camera->view, t, r, s);
            vec3 inverse_s = {1.0/s[0], 1.0/s[1], 1.0/s[2]};
            xpos *= inverse_s[0];
            ypos *= inverse_s[1];
            vec2 translate;
            glm_vec2_copy(camera->view[3], translate);
            glm_vec2_mul(translate, inverse_s, translate);
            vec2 screen = {xpos - texture->width/2.0, ypos - texture->height/2};
            vec2 position;
            glm_vec2_sub(screen, translate, position);
            ecs_set(world, node, Transform2D, {{position[0], position[1]}, 0.0f, 1.0f, 0});
        }
    }
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    ECS_COMPONENT(world, Camera);
    Camera* camera = ecs_get_mut(world, renderer, Camera, NULL);
    vec4 t;
    mat4 r;
    vec3 s;
    glm_decompose(camera->view, t, r, s);
    vec3 zoom = {yoffset * 0.05, yoffset * 0.05, 0.0};
    vec3 updateScale;
    vec3 i;
    glm_vec3_one(i);
    glm_vec3_add(i, zoom, updateScale);
    printf("%f\n", s[0]);
    glm_scale(camera->view, updateScale);
}


double lastXPos, lastYPos;
void cursor_position_callback(GLFWwindow* window, double xpos, double ypos)
{
    int state = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE);
    if (state == GLFW_PRESS)
    {
        double deltaX = xpos - lastXPos;
        double deltaY = ypos - lastYPos;
        printf("(%f, %f)\n", deltaX, deltaY);
        ECS_COMPONENT(world, Camera);
        vec3 delta = {deltaX, deltaY, 0.0};
        Camera* camera = ecs_get_mut(world, renderer, Camera, NULL);
        vec4 t;
        mat4 r;
        vec3 s;
        glm_decompose(camera->view, t, r, s);
        vec3 inverse_s = {1.0/s[0], 1.0/s[1], 1.0/s[2]};
        printf("%d\n", s[0]);
        glm_vec3_mul(inverse_s, delta, delta);
        glm_translate(camera->view, delta);
        vec2 translate;
        glm_vec2_copy(camera->view[3], translate);
        printf("%f, %f\n", translate[0], translate[1]);
    }
    lastXPos = xpos;
    lastYPos = ypos;
}

int main(int argc, char const *argv[])
{
    world = ecs_init();
    ECS_COMPONENT(world, Transform2D);
    ECS_COMPONENT(world, Camera);
    ECS_COMPONENT(world, BatchSpriteRenderer);
    ECS_COMPONENT(world, Texture2D);

    ECS_SYSTEM(world, setup_batch_renderer, EcsOnSet, BatchSpriteRenderer);
    ECS_SYSTEM(world, setup_camera, EcsOnSet, Camera);
    // ECS_SYSTEM(world, deallocate_texture, EcsOnRemove, Texture2D);

    IMG_Init(IMG_INIT_JPG | IMG_INIT_PNG | IMG_INIT_WEBP);
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GL_TRUE);
    glfwWindowHint(GLFW_DECORATED, GL_FALSE);
    glfwWindowHint(GLFW_VISIBLE, GL_FALSE);
    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, 1);
    glfwSetErrorCallback(error_callback);
    window = glfwCreateWindow(640, 480, "Gui Gal", NULL, NULL);
    glfwMakeContextCurrent(window);
    gladLoadGL();
    glfwSwapInterval(1);
    glfwSetKeyCallback(window, key_callback);
    glfwSetDropCallback(window, drop_callback);
    glfwSetCursorPosCallback(window, cursor_position_callback);
    glfwSetWindowSizeCallback(window, window_size_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glClearColor(32/255.0, 32/255.0, 32/255.0, 0.97);
    center_window(window, glfwGetPrimaryMonitor());
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    glViewport(0, 0, width, height);
    glEnable(GL_TEXTURE_2D);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    renderer = ecs_new_id(world);
    ecs_set_name(world, renderer, "renderer");
    ecs_set(world, renderer, Camera, {});
    ecs_set(world, renderer, BatchSpriteRenderer, {});

    ECS_SYSTEM(world, render_sprites, EcsOnUpdate, renderer:Camera, renderer:BatchSpriteRenderer, Transform2D, Texture2D);

    glfwShowWindow(window);

    while (!glfwWindowShouldClose(window))
    {
        double xpos, ypos;
        glfwGetCursorPos(window, &xpos, &ypos);
        // printf("%f, %f\n", xpos, ypos);
        glClear(GL_COLOR_BUFFER_BIT);
        ecs_progress(world, 0);
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    glfwDestroyWindow(window);
    glfwTerminate();
    ecs_fini(world);
    return 0;
}