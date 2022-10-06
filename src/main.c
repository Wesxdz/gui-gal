#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <stdio.h>
#include "flecs.h"
#include "flecs/addons/meta.h"
#include "flecs/addons/json.h"
#include <SDL2/SDL_image.h>
#include <string.h>
#include <gif_lib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>

#define CUTE_C2_IMPLEMENTATION
#include "cute_c2.h"

#include "components.h"

#define NANOVG_GL3_IMPLEMENTATION
#include "nanovg.h"
#include "nanovg_gl.h"
#include "nvgutil.h"
#include <pthread.h>

#include "curl/curl.h"

#define PY_SSIZE_T_CLEAN
#include <Python.h>

// flecs modules
// #include "input.h"

// Multithreading for image gen
pthread_mutex_t mutex1 = PTHREAD_MUTEX_INITIALIZER;
bool thread_started = false;

// Mulithreading for speech to text
pthread_mutex_t mutex_stt = PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_t mutex_stable_diffusion = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_image_loader = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_clipseg_mask = PTHREAD_MUTEX_INITIALIZER;

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

void SetupCamera(ecs_iter_t* it)
{
    printf("Setup camera\n");
    Camera* camera = ecs_field(it, Camera, 1);
    mat4 view;
    vec3 eye = {0.0f, 0.0f, 0.0};
    vec3 center = {0.0, 0.0, -1.0};
    vec3 up = {0.0, 1.0, 0.0};
    glm_lookat(eye, center, up, camera->view);
}

void SetupNanoVG(ecs_iter_t* it)
{
    NanoVG* nano = ecs_field(it, NanoVG, 1);
    nvgCreateFont(nano->vg, "sans", "../res/Roboto-Regular.ttf");
    nvgCreateFont(nano->vg, "sans_bold", "../res/Roboto-Bold.ttf");
}

void SetupBatchRenderer(ecs_iter_t* it)
{
    BatchSpriteRenderer* renderer = ecs_field(it, BatchSpriteRenderer, 1);

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
CommandBuffer buffer;
ecs_entity_t renderer;
ecs_entity_t input;
ecs_entity_t AI_painter;

void AnimateGif(ecs_iter_t* it)
{
    GifAnimator* animator = ecs_field(it, GifAnimator, 1);
    Texture2D* texture = ecs_field(it, Texture2D, 2);
    MultiTexture2D* multitexture = ecs_field(it, MultiTexture2D, 3);
    for (int i = 0; i < it->count; i++)
    {
        animator[i].progress += it->delta_time;
        SavedImage* saved = &animator[i].gif->SavedImages[animator[i].frame];
        float frameTime = 1.0/animator[i].fps;
        ExtensionBlock* block = saved->ExtensionBlocks;
        if (block->ByteCount == 4)
        {
            frameTime = (block->Bytes[1] | (block->Bytes[2] << 8))/100.0;
            // printf("%d frame time\n", frameTime);
        }
        while (frameTime > 0.0 && animator[i].progress > frameTime)
        {
            animator[i].progress -= frameTime;
            animator[i].frame = (animator[i].frame + 1) % multitexture[i].textureCount;
            texture[i].id = multitexture[i].ids[animator[i].frame];
        }
    }
}

void RenderSprites(ecs_iter_t* it)
{
    // printf("%d sprite(s)\n", it->count);
    Camera* camera = ecs_field(it, Camera, 1);
    BatchSpriteRenderer* renderer = ecs_field(it, BatchSpriteRenderer, 2);
    Transform2D* transform = ecs_field(it, Transform2D, 3);
    Texture2D* texture = ecs_field(it, Texture2D, 4);
    TextureView* tv = ecs_field(it, TextureView, 5);
    // printf("Use shader %d\n", renderer->shader.programId);
    glUseProgram(renderer->shader.programId);
    glUniformMatrix4fv(glGetUniformLocation(renderer->shader.programId, "view"), 1, false, camera->view[0]);
    mat4 proj;
    int wwidth, wheight;
    vec4 box = {0.0, 0.0, 1.0, 1.0};
    glUniform4f(glGetUniformLocation(renderer->shader.programId, "box"), box[0], box[1], box[2], box[3]);
    glfwGetWindowSize(window, &wwidth, &wheight);
    glm_ortho(0.0, wwidth, wheight, 0.0, -1.0, 10.0, proj); // TODO: Window component
    glUniformMatrix4fv(glGetUniformLocation(renderer->shader.programId, "projection"), 1, false, proj[0]);
    for (int i = 0; i < it->count; i++)
    {
        mat4 model;
        glm_mat4_identity(model);
        vec3 offset = {transform[i].pos[0], transform[i].pos[1], 0.0f};
        glm_translate(model, offset);
        vec3 scale = {texture[i].pow_w / texture[i].scale[0], texture[i].pow_h / texture[i].scale[1], 1.0};
        glm_scale(model, scale);
        glUniformMatrix4fv(glGetUniformLocation(renderer->shader.programId, "model"), 1, false, model[0]);
        if (tv)
        {
            float x_ratio = (float)texture[i].width/texture[i].pow_w;
            float y_ratio = (float)texture[i].height/texture[i].pow_h;
            // printf("Y bounds are %f, %f\n", tv[i].bounds.min.y, tv[i].bounds.max.y);
            // printf("Y is %f to %f\n", (tv[i].bounds.min.y/texture[i].height)*y_ratio, (tv[i].bounds.max.y/texture[i].height)*y_ratio);
            glUniform4f(glGetUniformLocation(renderer->shader.programId, "subimage"), (tv[i].bounds.min.x/texture[i].width)*x_ratio, (tv[i].bounds.min.y/texture[i].height)*y_ratio, (tv[i].bounds.max.x/texture[i].width)*x_ratio, (tv[i].bounds.max.y/texture[i].height)*y_ratio);
            // glUniform4f(glGetUniformLocation(renderer->shader.programId, "subimage"), 0.0, 0.0, 0.5 * x_ratio, 0.5 * y_ratio);
        } else
        {
            glUniform4f(glGetUniformLocation(renderer->shader.programId, "subimage"), 0.0, 0.0, 1.0, 1.0);
        }
        glUniform2f(glGetUniformLocation(renderer->shader.programId, "scale"), 1.0, 1.0);
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
    Texture2D* texture = ecs_field(it, Texture2D, 1);
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

void char_callback(GLFWwindow* window, unsigned int codepoint)
{
    printf("Character callback\n");
    ecs_set(world, input, EventCharEntry, {window, codepoint});
    ecs_set_pair(world, input, ConsumeEvent, ecs_id(EventCharEntry), {});
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GLFW_TRUE);

    printf("Key callback\n");
    ecs_set(world, input, EventKey, {window, key, scancode, action, mods});
    ecs_set_pair(world, input, ConsumeEvent, ecs_id(EventKey), {});
}

unsigned nearest_pow2(int length)
{
    return pow(2,ceil(log(length)/log(2)));
}

bool create_texture(ecs_world_t* s_world, const char* file, ecs_entity_t entity, unsigned int* twidth, unsigned int* theight)
{
    GLuint id;
    SDL_Surface* img = IMG_Load(file);
    if (img == NULL) return false;
    int bpp;
    Uint32 Rmask, Gmask, Bmask, Amask;
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
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, img->w, img->h, GL_RGBA, GL_UNSIGNED_BYTE, img_rgba8888->pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexParameteri ( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri ( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    // SDL_FreeSurface(img);
    SDL_FreeSurface(img_rgba8888);
    ecs_set(s_world, entity, Texture2D, {id, pow_w, pow_h, img->w, img->h, {1.0, 1.0}, img});
    *twidth = img->w; *theight = img->h;
    return true;
}

void create_multitexture_from_gif(ecs_world_t* s_world, GifFileType* gif, ecs_entity_t entity, SDL_Surface** firstFrame)
{
    GLuint* ids = malloc(sizeof(GLuint) * gif->ImageCount);
    SavedImage* prevSaved = &gif->SavedImages[gif->ImageCount - 2];
    GifImageDesc* prevDesc = &prevSaved->ImageDesc;
    ColorMapObject* prevColorMap = prevDesc->ColorMap ? prevDesc->ColorMap : gif->SColorMap;
    int bpp;
    Uint32 Rmask, Gmask, Bmask, Amask; //SDL_PIXELFORMAT_ABGR8888
    SDL_PixelFormatEnumToMasks(SDL_PIXELFORMAT_ABGR8888, &bpp, &Rmask,
    &Gmask, &Bmask, &Amask);
    unsigned pow_w = nearest_pow2(gif->SWidth);
    unsigned pow_h = nearest_pow2(gif->SHeight);
    size_t pixelCount = sizeof(Uint32) * pow_w * pow_h;
    void* clearSurface = malloc(pixelCount);
    memset(clearSurface, 0x00, pixelCount);
    glGenTextures(gif->ImageCount, ids);
    // http://giflib.sourceforge.net/whatsinagif/bits_and_bytes.html
    size_t bufferSize = gif->SWidth * gif->SHeight * 4; // 4 bytes per pixel on SDL format surface blit
    GifByteType* pixelBuffer = malloc(bufferSize); // double buffer to support Restore to previous
    GifByteType* pixelBuffer1 = malloc(bufferSize);
    GifByteType* pixelBuffer2 = malloc(bufferSize);
    bool isPrevPixelBufferFilled = false;
    memset(pixelBuffer, 0xFF, bufferSize);
    memset(pixelBuffer2, 0x00, bufferSize);
    printf("Gif canvas is (%d, %d)\n", gif->SWidth, gif->SHeight);
    int shiftBufferCount = 0;
    for (int i = 0; i < gif->ImageCount; i++)
    {
        SavedImage* saved = &gif->SavedImages[i];
        GifImageDesc* desc = &saved->ImageDesc;
        ColorMapObject* colorMap = desc->ColorMap ? desc->ColorMap : gif->SColorMap;
        printf("(%d, %d) are images coords\n", desc->Top, desc->Left);
        bool isTransparent = false;
        unsigned char transparentIndex;
        GifByteType disposalMode = 0;
        if (saved->ExtensionBlockCount > 0)
        {
            /*
            gif89
            Values :    0 -   No disposal specified. The decoder is
                                not required to take any action.
                        1 -   Do not dispose. The graphic is to be left
                                in place.
                        2 -   Restore to background color. The area used by the
                                graphic must be restored to the background color.
                        3 -   Restore to previous. The decoder is required to
                                restore the area overwritten by the graphic with
                                what was there prior to rendering the graphic.
                        4-7 -    To be defined.
            */
            disposalMode = (saved->ExtensionBlocks->Bytes[0] & 0b00011100) >> 2;
            printf("Disposal method: %u\n", disposalMode);
            isTransparent = saved->ExtensionBlocks->Bytes[0] & 0b00000001;
            transparentIndex = saved->ExtensionBlocks->Bytes[3];
            if (disposalMode == 3)
            {
                isTransparent = prevSaved->ExtensionBlocks->Bytes[0] & 0b00000001;
                transparentIndex = prevSaved->ExtensionBlocks->Bytes[3];
            }
        }
        if (disposalMode == 0)
        {
            for (int row = 0; row < desc->Height; row++)
            {
                for (int col = 0; col < desc->Width; col++)
                {
                    size_t globalIndex = (desc->Left + col) + (desc->Top + row) * gif->SWidth;
                    size_t index = col + row * desc->Width;
                    int c = saved->RasterBits[index];
                    GifColorType rgb = colorMap->Colors[c];
                    if (isTransparent && transparentIndex == c)
                    {
                        pixelBuffer[4 * globalIndex + 3] = 0x00;
                    }
                    else
                    {
                        pixelBuffer[4 * globalIndex] = rgb.Red;
                        pixelBuffer[4 * globalIndex + 1] = rgb.Green;
                        pixelBuffer[4 * globalIndex + 2] = rgb.Blue;
                        pixelBuffer[4 * globalIndex + 3] = 0xFF;
                    }
                }
            }
        }
        if (disposalMode == 1)
        {
            for (int row = 0; row < desc->Height; row++)
            {
                for (int col = 0; col < desc->Width; col++)
                {
		    size_t globalIndex = (desc->Left + col) + (desc->Top + row) * gif->SWidth;
                    size_t index = col + row * desc->Width;
                    int c = saved->RasterBits[index];
                    GifColorType rgb = colorMap->Colors[c];
                    if (isTransparent && transparentIndex == c)
                    {

                    }
                    else
                    {
                        pixelBuffer[4 * globalIndex] = rgb.Red;
                        pixelBuffer[4 * globalIndex + 1] = rgb.Green;
                        pixelBuffer[4 * globalIndex + 2] = rgb.Blue;
                    }
                }
            }
        } 
        if (disposalMode == 2)
        {
            for (int row = 0; row < desc->Height; row++)
            {
                for (int col = 0; col < desc->Width; col++)
                {
                    size_t globalIndex = (desc->Left + col) + (desc->Top + row) * gif->SWidth;
                    size_t index = col + row * desc->Width;
                    int c = saved->RasterBits[index];
                    if (isTransparent && transparentIndex == c)
                    {
                        GifColorType background = colorMap->Colors[gif->SBackGroundColor];
                        pixelBuffer[4 * globalIndex] = background.Red;
                        pixelBuffer[4 * globalIndex + 1] = background.Green;
                        pixelBuffer[4 * globalIndex + 2] = background.Blue;
                        if (gif->SBackGroundColor == transparentIndex)
                        {
                            pixelBuffer[4 * globalIndex + 3] = 0x00;
                        }
                        else
                        {
                            pixelBuffer[4 * globalIndex + 3] = 0xFF;
                        }
                    }
                    else
                    {
                        pixelBuffer[4 * globalIndex + 3] = 0xFF;
                    }
                    GifColorType rgb = colorMap->Colors[c];
                    pixelBuffer[4 * globalIndex] = rgb.Red;
                    pixelBuffer[4 * globalIndex + 1] = rgb.Green;
                    pixelBuffer[4 * globalIndex + 2] = rgb.Blue;
                }
            }
        }
        else if (disposalMode == 3)
        {
            if (!isPrevPixelBufferFilled)
            {
                if (shiftBufferCount < 2)
                {
                    for (int row = 0; row < prevDesc->Height; row++)
                    {
                        for (int col = 0; col < prevDesc->Width; col++)
                        {
                            size_t globalIndex = (prevDesc->Left + col) + (prevDesc->Top + row) * gif->SWidth;
                            size_t index = col + row * prevDesc->Width;
                            int c = prevSaved->RasterBits[index];
                            GifColorType rgb = prevColorMap->Colors[c];
                            pixelBuffer2[4 * globalIndex] = rgb.Red;
                            pixelBuffer2[4 * globalIndex + 1] = rgb.Green;
                            pixelBuffer2[4 * globalIndex + 2] = rgb.Blue;
                            if (isTransparent && c == transparentIndex)
                            {
                                pixelBuffer2[4 * globalIndex + 3] = 0x00;
                            } else
                            {
                                pixelBuffer2[4 * globalIndex + 3] = 0xFF;
                            }
                        }
                    }
                }
                isPrevPixelBufferFilled = true;
            }
            for (int row = 0; row < desc->Height; row++)
            {
                for (int col = 0; col < desc->Width; col++)
                {
                    size_t globalIndex = (desc->Left + col) + (desc->Top + row) * gif->SWidth;
                    size_t index = col + row * desc->Width;
                    int c = saved->RasterBits[index];
                    GifColorType rgb = colorMap->Colors[c];
                    if (isTransparent && c == transparentIndex)
                    {
                        pixelBuffer[4 * globalIndex] =      pixelBuffer2[4 * globalIndex];
                        pixelBuffer[4 * globalIndex + 1] =  pixelBuffer2[4 * globalIndex + 1];
                        pixelBuffer[4 * globalIndex + 2] =  pixelBuffer2[4 * globalIndex + 2];
                        pixelBuffer[4 * globalIndex + 3] =  pixelBuffer2[4 * globalIndex + 3];
                    } else
                    {
                        pixelBuffer[4 * globalIndex] = rgb.Red;
                        pixelBuffer[4 * globalIndex + 1] = rgb.Green;
                        pixelBuffer[4 * globalIndex + 2] = rgb.Blue;
                        pixelBuffer[4 * globalIndex + 3] = 0xFF;
                    }
                }
            }
        }


        printf("%d bits per pixel\n", colorMap->BitsPerPixel);
        printf("Gif image desc is (%d, %d)\n", desc->Width, desc->Height);
        // SDL_Surface *img_rgba8888
        *firstFrame = SDL_CreateRGBSurfaceFrom(pixelBuffer, gif->SWidth, gif->SHeight,
        bpp, 4, Rmask, Gmask, Bmask, Amask);
        SDL_SetSurfaceAlphaMod(*firstFrame, 0xFF);
        SDL_SetSurfaceBlendMode(*firstFrame, SDL_BLENDMODE_NONE);
        glBindTexture(GL_TEXTURE_2D, ids[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, pow_w, pow_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, clearSurface);
        // (pow_w - gif->SWidth)/2.0 (pow_h - gif->SHeight)/2.0
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0.0, 0.0, gif->SWidth, gif->SHeight, GL_RGBA, GL_UNSIGNED_BYTE, (*firstFrame)->pixels);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
        glTexParameteri ( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR); // GL_NEAREST for pixel art
        glTexParameteri ( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        memmove(pixelBuffer2, pixelBuffer1, bufferSize);
        memmove(pixelBuffer1, pixelBuffer, bufferSize);
        shiftBufferCount += 2;
        prevSaved = &gif->SavedImages[i - 1];
        prevDesc = &prevSaved->ImageDesc;
        prevColorMap = prevDesc->ColorMap ? prevDesc->ColorMap : gif->SColorMap;
        // SDL_FreeSurface(img_rgba8888);
    }
    free(pixelBuffer);
    free(pixelBuffer1);
    free(pixelBuffer2);
    free(clearSurface);
    ecs_set(s_world, entity, MultiTexture2D, {ids, gif->ImageCount});
    printf("Created multitexture!\n");
}

void SetInitialMultitexture(ecs_iter_t* it)
{
    printf("Set initial multitexture!\n");
    Texture2D* texture = ecs_field(it, Texture2D, 1);
    MultiTexture2D* multitexture = ecs_field(it, MultiTexture2D, 2);
    texture->id = multitexture->ids[0];
}


void window_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}

void LoadClipboardFiles(ecs_iter_t* it)
{
    EventMouseButton* event = ecs_field(it, EventMouseButton, 1);
    const char* clipboard = glfwGetClipboardString(window);
}
typedef struct
{
    ecs_entity_t node;
    vec2 size;
} VisualSymbolCreated;

VisualSymbolCreated load_visual_symbol(ecs_world_t* s_world, char* path, float x, float y, Anchor anchor)
{
    ecs_entity_t node;
    VisualSymbolCreated vsc = {node, {0,0}};
    struct stat info;
    bool validPath = stat(path, &info) == 0;
    bool isDirectory = (S_ISDIR(info.st_mode));
    printf("%s\n", path);
    if (!validPath)
    {
        printf("Not valid path\n");
        // Can something be loaded from a URL?
        curl_global_init(CURL_GLOBAL_ALL);
        CURL* curl = curl_easy_init();
        if (curl)
        {
            CURLcode res;
            FILE* fp;
            char* filename;
            char* c_path [FILENAME_MAX];
            strcpy(c_path, path);
            char* pch = strtok(c_path, "/");
            while (pch != NULL)
            {
                filename = pch;
                pch = strtok(NULL, "/");
            }
            fp = fopen(filename, "w");

            printf("Creating file %s\n", filename);
            curl_easy_setopt(curl, CURLOPT_URL, path);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, NULL);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
            res = curl_easy_perform(curl);
            printf("Curl code %d\n", res);
            
            // TODO: Curl get first image from non-image html element?
            curl_easy_cleanup(curl);
            fclose(fp);
            strcpy(path, filename);
            printf("%s\n", path);
            curl_global_cleanup();
        }
    }
    if (isDirectory) return vsc;
    int error;
    GifFileType* gif = DGifOpenFileName(path, &error);
    bool isGif = gif != NULL;
    printf("%p:%d\n", (void*)gif, error);

    if (!isDirectory)
    {
        node = ecs_new_id(s_world);
        unsigned int twidth, theight;
        if (isGif)
        {
            DGifSlurp(gif); // TODO: Move logic to GifAnimator system
            printf("Gif has %d images!\n" ,gif->ImageCount);
            SDL_Surface* firstFrame;
            create_multitexture_from_gif(s_world, gif, node, &firstFrame);
            ecs_set(s_world, node, Texture2D, {NULL, nearest_pow2(gif->SWidth), nearest_pow2(gif->SHeight), gif->SWidth, gif->SHeight, {1.0, 1.0}, firstFrame});
            // printf("%d stbi frame count!\n", z);
            twidth = gif->SWidth; theight = gif->SHeight;
            if (gif->ImageCount > 1)
            {
                ecs_set(s_world, node, GifAnimator, {gif, 24, 0.0, 0}); // data, delays, y, comp
            }
        } else
        {
            printf("Loading %s\n", path);
            bool created = create_texture(s_world, path, node, &twidth, &theight);
            vsc.size[0] = twidth;
            vsc.size[1] = theight;
            if (!created) return vsc;
        }
        ecs_set(s_world, node, Transform2D, {{x - twidth * anchor.horizontal, y - theight * anchor.vertical}, 0.0f, 1.0f, 0});
        ecs_set(s_world, node, Local2D, {0, 0});
        ecs_set(s_world, node, LocalFile, {path});
    }
    vsc.node = node;
    return vsc;
}

void LoadDroppedFiles(ecs_iter_t* it)
{
    EventDropFiles* drop = ecs_field(it, EventDropFiles, 1);
    Camera* camera = ecs_field(it, Camera, 2);
    double xpos, ypos;
    glfwGetCursorPos(window, &xpos, &ypos);
    vec2 cursor_screen_pos = {xpos, ypos};
    vec2 cursor_world_pos;
    screen_to_world(camera->view, cursor_screen_pos, cursor_world_pos);
    printf("Load dropped files\n");
    float x = 0.0f;
    float y = 0.0f;
    Anchor place = {0, 0};
    if (drop->count == 1)
    {
        place.horizontal = 0.5;
        place.vertical = 0.5;
    }
    float maxCols = 10;
    float maxHeight = 0;
    float colIter = 0;
    for (int i = 0; i < drop->count; i++)
    {
        VisualSymbolCreated vsc = load_visual_symbol(it->world, drop->paths[i], x + cursor_world_pos[0], y + cursor_world_pos[1], place);
        printf("Create visual symbol\n");
        // ecs_get is not available right away, I would like to understand why this is more
        // const Texture2D* texture = ecs_get(it->world, entity, Texture2D);
        
        x += vsc.size[0];
        maxHeight = c2Max(vsc.size[1], maxHeight);
        colIter++;
        if (colIter > maxCols)
        {
            colIter = 0;
            x = 0;
            y += maxHeight;
            maxHeight = 0;
        }
    }
    free(drop->paths); // TODO: Move to another system and free unused paths
}

void drop_callback(GLFWwindow* window, int count, const char** paths)
{
    ecs_remove(world, input, DragHover);
    char** savedPaths = malloc(sizeof(char*) * count);
    for (int i = 0; i < count; i++)
    {
        size_t pathSize = sizeof(char*) * strlen(paths[i]);
        savedPaths[i] = malloc(pathSize);
        memcpy(savedPaths[i], paths[i], pathSize);
    }
    ecs_set(world, input, EventDropFiles, {window, count, savedPaths});
    printf("Create EventDropFiles\n");
    ecs_set_pair(world, input, ConsumeEvent, ecs_id(EventDropFiles), {});
}

void drag_callback(GLFWwindow* window, int entered)
{
    if (entered)
    {
        ecs_add(world, input, DragHover);
    } else
    {
        ecs_remove(world, input, DragHover);
    }
}

void RenderDragHover(ecs_iter_t* it)
{
    NanoVG* nano = ecs_field(it, NanoVG, 1);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);

    int width, height;
    glfwGetWindowSize(window, &width, &height);
    nvgBeginPath(nano->vg);
    int border = 2;
    int lineWidth = 1;
    nvgRoundedRect(nano->vg, border, border, width - border*2, height - border*2, 16);
    nvgPathWinding(nano->vg, NVG_HOLE);
    nvgRoundedRect(nano->vg, border + lineWidth, border + lineWidth, width - border*2 - lineWidth*2, height - border*2 - lineWidth*2, 16);
    nvgFillColor(nano->vg, nvgRGBA(221, 135, 56, 255));
    // nvgFillColor(nano->vg, nvgRGBA(48, 170, 208, 255));
    nvgFill(nano->vg);

    nvgClosePath(nano->vg);
    // glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
}

void ConsumeEvents(ecs_iter_t* it)
{
    ecs_entity_t pair = ecs_field_id(it, 1);
    ecs_entity_t comp = ecs_pair_object(it->world, pair);
    for (int32_t i = 0; i < it->count; i++)
    {
        ecs_remove_id(it->world, it->entities[i], comp);
        ecs_remove_id(it->world, it->entities[i], pair);
    }
}

void DeleteSelected(ecs_iter_t* it)
{
  EventKey* event = ecs_field(it, EventKey, 1);
  ecs_defer_begin(it->world);
  if (event->key == GLFW_KEY_DELETE && event->action == GLFW_PRESS) {
    for (int32_t i = 0; i < it->count; i++)
      {
          ecs_delete(it->world, it->entities[i]);
      }
  }
  ecs_defer_end(it->world);
}


void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
    ecs_set(world, input, EventMouseButton, {window, button, action, mods});
    ecs_set_pair(world, input, ConsumeEvent, ecs_id(EventMouseButton), {});
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    ecs_set(world, input, EventScroll, {window, xoffset, yoffset});
    ecs_set_pair(world, input, ConsumeEvent, ecs_id(EventScroll), {});
}

void calc_view(Camera* camera)
{
    vec3 eye = {0.0f, 0.0f, 0.0};
    vec3 center = {0.0, 0.0, -1.0};
    vec3 up = {0.0, 1.0, 0.0};

    glm_lookat(eye, center, up, camera->view);
    vec3 pos = {camera->pos[0], camera->pos[1], 0.0};
    glm_translate(camera->view, pos);
    vec3 scale = {camera->scale, camera->scale, 1.0};
    glm_scale(camera->view, scale);

    glm_lookat(eye, center, up, camera->targetView);
    vec3 pos2 = {camera->targetPos[0], camera->targetPos[1], 0.0};
    glm_translate(camera->targetView, pos2);
    vec3 scale2 = {camera->targetScale, camera->targetScale, 1.0};
    glm_scale(camera->targetView, scale2);
}

void CameraCalculateView(ecs_iter_t* it)
{
    Camera* camera = ecs_field(it, Camera, 1);
    for (int32_t i = 0; i < it->count; i++)
    {
        calc_view(&camera[i]);
    }
}

void push_command(CommandBuffer* buffer, ecs_snapshot_t* snapshot)
{
    buffer->snapshots[buffer->index] = snapshot;
    buffer->index++;
    buffer->index %= buffer->capacity;
    buffer->count++;
    printf("Pushed command! %d\n", buffer->index);
}

void undo_command(ecs_world_t* world, CommandBuffer* buffer)
{
    // Delay snapshot restore to outside world.progress()
    if (buffer->count > 0)
    {
        buffer->index--;
        if (buffer->index < 0) buffer->index = 0;
        ecs_snapshot_restore(world, buffer->snapshots[buffer->index]);
        buffer->count--;
    }
    buffer->time_step++;
}

void UndoCommand(ecs_iter_t* it)
{
    printf("Undo command!\n");
    EventKey* event = ecs_field(it, EventKey, 1);
    if (event->key == GLFW_KEY_Z && event->mods & GLFW_MOD_CONTROL && event->action == GLFW_PRESS)
    {
        buffer.time_step--;
    }
}

void MoveGrabbedTransforms(ecs_iter_t* it)
{
    Camera* camera = ecs_field(it, Camera, 1);
    EventMouseMotion* motion = ecs_field(it, EventMouseMotion, 2);
    Transform2D* transform = ecs_field(it, Transform2D, 3);

    for (int32_t i = 0; i < it->count; i++)
    {
        vec2 worldPosLast;
        vec2 worldPosNow;
        vec2 motionPosLast;
        vec2 worldDelta;
        glm_vec2_sub(motion->pos, motion->delta, motionPosLast);
        screen_to_world(camera->view, motion->pos, worldPosNow);
        screen_to_world(camera->view, motionPosLast, worldPosLast);
        glm_vec2_sub(worldPosNow, worldPosLast, worldDelta);
        glm_vec2_add(transform[i].pos, worldDelta, transform[i].pos);
    }
}

void GrabMoveCamera(ecs_iter_t* it)
{
    Camera* camera = ecs_field(it, Camera, 1);
    EventMouseMotion* motion = ecs_field(it, EventMouseMotion, 2);
    int state = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE);
    if (state == GLFW_PRESS)
    {
        glm_vec2_add(camera->targetPos, motion->delta, camera->targetPos);
    }
}

void UnGrab(ecs_iter_t* it)
{
    EventMouseButton* mouse = ecs_field(it, EventMouseButton, 1);

    if (mouse->button == GLFW_MOUSE_BUTTON_LEFT && mouse->action == GLFW_RELEASE)
    {
        ecs_defer_begin(it->world);
        for (int32_t i = 0; i < it->count; i++)
        {
            ecs_remove(it->world, it->entities[i], Grabbed);
        }
        ecs_defer_end(it->world);
    }
}

void RenderDragSelector(ecs_iter_t* it)
{
    NanoVG* nano = ecs_field(it, NanoVG, 1);
    DragSelector* selector = ecs_field(it, DragSelector, 2);
    InteractionState* interaction = ecs_field(it, InteractionState, 3);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);

    nvgBeginPath(nano->vg);
    nvgRect(nano->vg, selector->x, selector->y, selector->w, 1);
    nvgRect(nano->vg, selector->x, selector->y, 1, selector->h);
    nvgRect(nano->vg, selector->x, selector->y + selector->h, selector->w, 1);
    nvgRect(nano->vg, selector->x + selector->w, selector->y, 1, selector->h);
    if (interaction->mode == STANDARD)
    {
        nvgFillColor(nano->vg, nvgRGBA(34, 224, 107, 255));
    } 
    else if (interaction->mode == CROP)
    {
        nvgFillColor(nano->vg, nvgRGBA(255, 100, 107, 255));
    }
    nvgFill(nano->vg);
    nvgClosePath(nano->vg);

    nvgBeginPath(nano->vg);
    nvgRect(nano->vg, selector->x, selector->y, selector->w, selector->h);
    if (interaction->mode == STANDARD)
    {
        nvgFillColor(nano->vg, nvgRGBA(34, 224, 107, 8));
    } 
    else if (interaction->mode == CROP)
    {
        nvgFillColor(nano->vg, nvgRGBA(255, 100, 107, 8));
    }
    nvgFill(nano->vg);
    nvgClosePath(nano->vg);
    // glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
}

void RenderImageName(ecs_iter_t* it)
{
    NanoVG* nano = ecs_field(it, NanoVG, 1);
    // nvgFontSize(nano->vg, 32.0f);
    // nvgFontFace(nano->vg, "sans");
    // nvgBeginPath(nano->vg);
    // nvgFillColor(nano->vg, nvgRGBA(255,255,255,255));
    // nvgTextAlign(nano->vg, NVG_ALIGN_LEFT|NVG_ALIGN_TOP);
    // const char* test = "Hello!";
    // nvgText(nano->vg, 30, 30, test, NULL);
    // nvgClosePath(nano->vg);
    // // drawSlider(nano->vg, 0, 100, 100, 400, 64);
    // drawEditBoxNum(nano->vg, "20", "m", 100, 100, 128, 54);


}

void RenderPaintFrame(ecs_iter_t* it)
{
    NanoVG* nano = ecs_field(it, NanoVG, 1);
    Camera* camera = ecs_field(it, Camera, 2);
    Transform2D* transform = ecs_field(it, Transform2D, 3);
    PaintFrame* frame = ecs_field(it, PaintFrame, 4);

    NVGcolor modeColors[3] = {nvgRGBA(0, 255, 195,255), nvgRGBA(172,40,201,255), nvgRGBA(255,68,0,255)};

    // TODO: Function to world to screen multiple verts
    vec2 upperLeft = {transform->pos[0], transform->pos[1]};
    world_to_screen(camera->view, upperLeft, upperLeft);
    vec2 upperRight = {transform->pos[0] + frame->w, transform->pos[1]};
    world_to_screen(camera->view, upperRight, upperRight);
    vec2 lowerLeft = {transform->pos[0], transform->pos[1] + frame->h};
    world_to_screen(camera->view, lowerLeft, lowerLeft);
    vec2 lowerRight = {transform->pos[0] + frame->w, transform->pos[1] + frame->h};
    world_to_screen(camera->view, lowerRight, lowerRight);

    float w = upperRight[0] - upperLeft[0];
    float h = upperRight[1] - lowerRight[1];

    nvgBeginPath(nano->vg);
    nvgMoveTo(nano->vg, 0.0f, 0.0f);
    nvgRect(nano->vg, upperLeft[0], upperLeft[1]-h, w, h);
    nvgClosePath(nano->vg);
    // nvgStrokeColor(nano->vg, nvgRGBA(0, 0, 255, 255));
    nvgStrokeColor(nano->vg, modeColors[frame->interaction_mode]);
    nvgStrokeWidth(nano->vg, 3.0f);
    nvgStroke(nano->vg);

    nvgFontSize(nano->vg, 20.0f);
    nvgFontFace(nano->vg, "sans");
    nvgBeginPath(nano->vg);
    nvgFillColor(nano->vg, nvgRGBA(255,255,255,255));
    nvgTextAlign(nano->vg, NVG_ALIGN_LEFT|NVG_ALIGN_TOP);
    nvgText(nano->vg, lowerLeft[0], lowerLeft[1] + 12, frame->prompt, NULL);
    nvgTextAlign(nano->vg, NVG_ALIGN_LEFT|NVG_ALIGN_BOTTOM);
    nvgFillColor(nano->vg, modeColors[frame->interaction_mode]);
    if (frame->interaction_mode == 0)
    {
        nvgText(nano->vg, upperLeft[0], upperLeft[1] - 12, "txt2img", NULL);
    }
    else if (frame->interaction_mode == 1)
    {
        nvgText(nano->vg, upperLeft[0], upperLeft[1] - 12, "img2img", NULL);
    }
    else if (frame->interaction_mode == 2)
    {
        nvgText(nano->vg, upperLeft[0], upperLeft[1] - 12, "inpainting", NULL);
    }
}

void MoveDragSelector(ecs_iter_t* it)
{
    DragSelector* selector = ecs_field(it, DragSelector, 1);
    EventMouseMotion* event = ecs_field(it, EventMouseMotion, 2);
    Camera* camera = ecs_field(it, Camera, 3);

    selector->w = event->pos[0] - selector->x;
    selector->h = event->pos[1] - selector->y;
    printf("Selector %f, %f\n", selector->w, selector->h);
    ecs_defer_begin(it->world);
    select_dragged(it, selector, camera);
    ecs_defer_end(it->world);
    // TODO: Select new overlaps!
}

void ScaleVisualSymbol(ecs_iter_t* it)
{
    EventMouseButton* event = ecs_field(it, EventMouseButton, 1);
    Transform2D* transform = ecs_field(it, Transform2D, 2);
    Texture2D* texture = ecs_field(it, Texture2D, 3);
}

void MouseStartAction(ecs_iter_t* it)
{
    EventMouseButton* event = ecs_field(it, EventMouseButton, 1);
    ActionOnMouseInput* action = ecs_field(it, ActionOnMouseInput, 2);

    if (event->action == GLFW_PRESS && event->button == GLFW_MOUSE_BUTTON_LEFT)
    {
        // printf("Mouse start\n");
        action->active = true;
    }
}

void MouseAffectAction(ecs_iter_t* it)
{
    EventMouseMotion* event = ecs_field(it, EventMouseMotion, 1);
    ActionOnMouseInput* action = ecs_field(it, ActionOnMouseInput, 2);
    Camera* camera = ecs_field(it, Camera, 3);
    SelectionBounds* selection = ecs_field(it, SelectionBounds, 4);

    // vec2 eventDiff = {event->pos[0] - action->origin[0], event->pos[1] - action->origin[1]};
    // float percentScale = 1.0f - c2Min(eventDiff[0]/action->startSize[0], eventDiff[1]/action->startSize[1]); /// TODO: Relative to opposite
    // printf("Scale percent is %f\n", percentScale);
    
    vec2 cursorWorldPos;
    screen_to_world(camera->view, event->pos, cursorWorldPos);
    vec2 actionOriginWorldPos;
    screen_to_world(camera->view, action->origin, actionOriginWorldPos);
    vec2 diff;
    glm_vec2_sub(cursorWorldPos, actionOriginWorldPos, diff);
    printf("Diff is (%f, %f)\n", diff[0], diff[1]);
    vec2 ratio = {1.0f + diff[0]/action->startSize[0], 1.0f + diff[1]/action->startSize[1]};
    printf("Action start size is %f, %f\n", action->startSize[0], action->startSize[1]);

    float greater = c2Max(1.0/ratio[0], 1.0/ratio[1]);

    // TODO: Get from mouse to action origin
    // Use that to determine a scale, assuming constant ratio
    // Reposition and scale all selected visual symbols relative to the diff change

    ecs_query_t* query = ecs_query_new(world, "Transform2D, Texture2D, ?TextureView, Selected");
    ecs_iter_t qIt = ecs_query_iter(it->world, query);
    while (ecs_query_next(&qIt))
    {
        Transform2D* transform = ecs_field(&qIt, Transform2D, 1);
        Texture2D* texture = ecs_field(&qIt, Texture2D, 2);
        TextureView* tv = ecs_field(&qIt, TextureView, 3);
        for (int i = 0; i < qIt.count; i++)
        {
            if (action->op == SCALE_LOWER_RIGHT)
            {
                texture[i].scale[0] = greater;
                texture[i].scale[1] = greater;
                // transform->pos[0] += frameDiff[0];
                // transform->pos[1] += frameDiff[1];
                // transform->scale = percentScale;
            }
        }
    }

    // for (int32_t i = 0; i < it->count; i++)
    
        
        // printf("Action origin is (%f, %f)\n", action->origin[0], action->origin[1]);

        // if (ecs_field_is_set(it, 6))
        // {
        //     float cropped_width = (tv[i].bounds.max.x - tv[i].bounds.min.x);
        //     float cropped_height = (tv[i].bounds.max.y - tv[i].bounds.min.y);
        //     float start_x = transform[i].pos[0] + tv[i].bounds.min.x/action->startScale[0];
        //     float start_y = transform[i].pos[1] + tv[i].bounds.min.y/action->startScale[1];
        //     vec2 startPos = {start_x, start_y};
        //     if (action->op == SCALE_LOWER_RIGHT)
        //     {
        //         vec2 upperLeft;
        //         upperLeft[0] = action->origin[0] - cropped_width/action->startScale[0];
        //         upperLeft[1] = action->origin[1] - cropped_height/action->startScale[1];
        //         glm_vec2_sub(cursorWorldPos, upperLeft, diff);
        //         printf("Diff is (%f, %f)\n", diff[0], diff[1]);
        //         vec2 ratio = {diff[0]/cropped_width, diff[1]/cropped_height};
        //         float greater = c2Max(1.0/ratio[0], 1.0/ratio[1]);
        //         texture[i].scale[0] = greater;
        //         texture[i].scale[1] = greater;
        //         transform[i].pos[0] = upperLeft[0] - tv[i].bounds.min.x/greater;
        //         transform[i].pos[1] = upperLeft[1] - tv[i].bounds.min.y/greater;
        //     }
        //     else if (action->op == SCALE_LOWER_LEFT)
        //     {
        //         vec2 upperRight;
        //         upperRight[0] = action->origin[0] + cropped_width/action->startScale[0];
        //         upperRight[1] = action->origin[1] - cropped_height/action->startScale[1];
        //         glm_vec2_sub(cursorWorldPos, upperRight, diff);
        //         vec2 ratio = {diff[0]/-cropped_width, diff[1]/cropped_height};
        //         float greater = c2Max(1.0/ratio[0], 1.0/ratio[1]);
        //         texture[i].scale[0] = greater;
        //         texture[i].scale[1] = greater;
        //         transform[i].pos[0] = upperRight[0] - tv[i].bounds.max.x/greater;
        //         transform[i].pos[1] = upperRight[1] - tv[i].bounds.min.y/greater;
        //     }
        //     else if (action->op == SCALE_UPPER_RIGHT)
        //     {
        //         vec2 lowerLeft;
        //         lowerLeft[0] = action->origin[0] - cropped_width/action->startScale[0];
        //         lowerLeft[1] = action->origin[1] + cropped_height/action->startScale[1];
        //         glm_vec2_sub(cursorWorldPos, lowerLeft, diff);
        //         vec2 ratio = {diff[0]/-cropped_width, diff[1]/-cropped_height};
        //         float greater = c2Max(1.0/ratio[0], 1.0/ratio[1]);
        //         texture[i].scale[0] = greater;
        //         texture[i].scale[1] = greater;
        //         transform[i].pos[0] = lowerLeft[0] - tv[i].bounds.min.x/greater;
        //         transform[i].pos[1] = lowerLeft[1] - tv[i].bounds.max.y/greater;
        //     }
        //     else if (action->op == SCALE_UPPER_LEFT)
        //     {
        //         vec2 lowerRight;
        //         lowerRight[0] = action->origin[0] + cropped_width/action->startScale[0];
        //         lowerRight[1] = action->origin[1] + cropped_height/action->startScale[1];
        //         glm_vec2_sub(cursorWorldPos, lowerRight, diff);
        //         vec2 ratio = {diff[0]/cropped_width, diff[1]/-cropped_height};
        //         float greater = c2Max(1.0/ratio[0], 1.0/ratio[1]);
        //         texture[i].scale[0] = greater;
        //         texture[i].scale[1] = greater;
        //         transform[i].pos[0] = lowerRight[0] - tv[i].bounds.max.x/greater;
        //         transform[i].pos[1] = lowerRight[1] - tv[i].bounds.max.y/greater;
        //     }
        // } else
        // {
        //     if (action->op == SCALE_LOWER_RIGHT)
        //     {
        //         glm_vec2_sub(cursorWorldPos, transform[i].pos, diff);
        //         vec2 ratio = {diff[0]/texture[i].width, diff[1]/texture[i].height};

        //         float greater = c2Max(1.0/ratio[0], 1.0/ratio[1]);
        //         texture[i].scale[0] = greater;
        //         texture[i].scale[1] = greater;
        //     }
        //     else if (action->op == SCALE_LOWER_LEFT)
        //     {
        //         vec2 opposite = {action->origin[0] + texture[i].width/action->startScale[0], action->origin[1]};
        //         diff[0] = cursorWorldPos[0] - (opposite[0]);
        //         diff[1] = cursorWorldPos[1] - transform[i].pos[1];
        //         vec2 ratio = {diff[0]/texture[i].width, diff[1]/texture[i].height};

        //         float greater = c2Max(-1.0/ratio[0], 1.0/ratio[1]);
        //         texture[i].scale[0] = greater;
        //         texture[i].scale[1] = greater;
        //         transform[i].pos[0] = opposite[0] - texture[i].width/greater;
        //     }
        //     else if (action->op == SCALE_UPPER_RIGHT)
        //     {
        //         vec2 opposite = {action->origin[0], action->origin[1] + texture[i].height/action->startScale[1]};
        //         diff[0] = cursorWorldPos[0] - transform[i].pos[0];
        //         diff[1] = cursorWorldPos[1] - opposite[1];
        //         vec2 ratio = {diff[0]/texture[i].width, diff[1]/texture[i].height};

        //         float greater = c2Max(1.0/ratio[0], 1.0/ratio[1]);
        //         texture[i].scale[0] = greater;
        //         texture[i].scale[1] = greater;
        //         transform[i].pos[1] = opposite[1] - texture[i].height/greater;
        //     }
        //     else if (action->op == SCALE_UPPER_LEFT)
        //     {
        //         vec2 opposite = {action->origin[0] + texture[i].width/action->startScale[0], action->origin[1] + texture[i].height/action->startScale[1]};
        //         diff[0] = cursorWorldPos[0] - opposite[0];
        //         diff[1] = cursorWorldPos[1] - opposite[1];
        //         vec2 ratio = {-diff[0]/texture[i].width, -diff[1]/texture[i].height};

        //         float greater = c2Max(1.0/ratio[0], 1.0/ratio[1]);
        //         texture[i].scale[0] = greater;
        //         texture[i].scale[1] = greater;
        //         transform[i].pos[0] = opposite[0] - texture[i].width/greater;
        //         transform[i].pos[1] = opposite[1] - texture[i].height/greater;
        //     }
        // }
    // }
}

void TypePaintPrompt(ecs_iter_t* it)
{
    PaintFrame* paint = ecs_field(it, PaintFrame, 1);
    EventCharEntry* charEntry = ecs_field(it, EventCharEntry, 2);
    
    // printf("Type paint prompt {%c}\n", charEntry->codepoint);
    printf("%c\n",charEntry->codepoint);
    strcat(paint->prompt, &charEntry->codepoint);
}

static char* speech = "";
static PyObject*
flit_voice_command(PyObject* self, PyObject* args) // PyObject* params
{
    pthread_mutex_lock(&mutex_stt);
    PyArg_ParseTuple(args, "s", &speech);
    // printf("Speech is %s\n", speech);
    pthread_mutex_unlock(&mutex_stt);
    return PyLong_FromLong(0);
}

// Start with something simple that works, then add queue
static bool should_create_image = false;
static bool is_creating_image = false;
static char* prompt = "";

static PyObject*
flit_query_stable_diffusion_requests(PyObject* self, PyObject* args) // PyObject* params
{
    int locked = pthread_mutex_trylock(&mutex_stable_diffusion);
    if (locked)
    {
        if (should_create_image)
        {
            printf("Create image\n");
            PyObject* prompt_to_gen = PyUnicode_DecodeFSDefault(prompt);
            prompt = "";
            should_create_image = false;
            pthread_mutex_unlock(&mutex_stable_diffusion);
            return prompt_to_gen;
        }
        // Check if there are any requests to generate an image with stable diffusion
        // If there are, return the highest priority one :)
        pthread_mutex_unlock(&mutex_stable_diffusion);
    }
    return PyLong_FromLong(0);
}

// Start with something simple that works, then add queue
// static bool should_mask_image = false;
// static char* mask_prompt = "";

// static PyObject*
// flit_query_clipseg_mask(PyObject* self, PyObject* args) // PyObject* params
// {
//     int locked = pthread_mutex_trylock(&mutex_clipseg_mask);
//     if (locked)
//     {
//         if (should_mask_image)
//         {
//             printf("Create image\n");
//             PyObject* prompt_to_gen = PyUnicode_DecodeFSDefault(mask_prompt);
//             mask_prompt = "";
//             // TODO: Need to get the current image context (last image generated or image selected)
//             should_create_image = false;
//             pthread_mutex_unlock(&mutex_clipseg_mask);
//             return prompt_to_gen;
//         }
//         // Check if there are any requests to generate an image with stable diffusion
//         // If there are, return the highest priority one :)
//         pthread_mutex_unlock(&mutex_stable_diffusion);
//     }
//     return PyLong_FromLong(0);
// }


static bool should_load_image = false;
static char* image_to_load_filepath = "";

static PyObject*
flit_load_generated_image(PyObject* self, PyObject* args)
{
    char* img_path;
    PyArg_ParseTuple(args, "s", &img_path);
    printf("Load image %s\n", img_path);
    pthread_mutex_lock(&mutex_image_loader);
    should_load_image = true;
    image_to_load_filepath = img_path;
    pthread_mutex_unlock(&mutex_image_loader);
    return PyLong_FromLong(0);
}

static PyMethodDef FlitMethods[] = {
    {"voice_command", flit_voice_command, METH_VARARGS,
    "Update Flit with voice context"},
    {"query_stable_diffusion_requests", flit_query_stable_diffusion_requests, METH_VARARGS,
    "Request Stable Diffusion generation requests from Flit"},
    {"load_generated_image", flit_load_generated_image, METH_VARARGS,
    "Request Flit load image at path"},
    {NULL, NULL, 0, NULL}
};

static PyModuleDef FlitModule = {
    PyModuleDef_HEAD_INIT, "flit", NULL, -1, FlitMethods,
    NULL, NULL, NULL, NULL
};

static PyObject* PyInit_flit(void)
{
    return PyModule_Create(&FlitModule);
}

void embed_python(void* ptr)
{
    PyObject *pName, *pModule, *pFunc;
    PyObject *pArgs, *pValue;
    // printf("Loading speech recognition systems...\n");
    wchar_t *program = Py_DecodeLocale("rtsr", NULL);
    Py_SetProgramName(program);
    wchar_t *home = Py_DecodeLocale("/home/aeri/miniconda3/envs/ldm", NULL);
    Py_SetPythonHome(home);
    PyImport_AppendInittab("flit", &PyInit_flit);
    Py_Initialize();
    PyRun_SimpleString("import sys\nsys.path.append('../python/')");
    pName = PyUnicode_DecodeFSDefault("rtsr"); // argv[1]
    /* Error checking of pName left out */
    pModule = PyImport_Import(pName);
    Py_DECREF(pName);
    Py_DECREF(pModule);
    if (Py_FinalizeEx() < 0) {
        return 120;
    }
}

void image_gen_completed(ecs_iter_t* it, void* data)
{
    // TODO: Load image and place it at PaintFrame :)
    PromptCallbackData* pcd = (PromptCallbackData*)data;
    printf("Image with name example_%d finished generating!\n", pcd->seed);
    int len = snprintf(NULL, 0, "example_%d.png", pcd->seed);
    char* filepath = malloc(len);
    snprintf(filepath, len+1, "example_%d.png", pcd->seed);
    printf("Filepath is %s\n", filepath);
    ecs_entity_t load_image_task = ecs_new_id(it->world);
    ecs_set(it->world, load_image_task, EventPaintLoad, {filepath});
    // This isn't working for some reason...
    // ecs_defer_begin(it->world);
    // load_visual_symbol(it->world, filepath);
    // ecs_defer_end(it->world);
}

void UpdatePromptFromSpeech(ecs_iter_t* it)
{
    PaintFrame* paint = ecs_field(it, PaintFrame, 1);
    // int locked = pthread_mutex_trylock(&mutex_stt);
    // if (!locked)
    // {
    //     // TODO: Only append new speech to allow text editing
    //     strcpy(paint[0].prompt, speech);
    //     pthread_mutex_unlock(&mutex_stt);
    // }
}

void CheckThreadComplete(ecs_iter_t* it)
{
    WaitThreadComplete* wait = ecs_field(it, WaitThreadComplete, 1);
    int locked = pthread_mutex_trylock(wait->mutex);
    // printf("Is thread locked? %i\n", locked);
    if (!locked)
    {
        // printf("Has thread started? %i\n", wait->has_thread_started);
        if (wait->has_thread_started)
        {
            // printf("Thread complete!\n");
            wait->callback(it, wait->data); // Thread is finished!
            ecs_delete(it->world, it->entities[0]);
        }
        pthread_mutex_unlock(wait->mutex);
        // printf("Thread unlocked by checker!\n");
    }
}

void BackspacePaintPrompt(ecs_iter_t* it)
{
    PaintFrame* paint = ecs_field(it, PaintFrame, 1);
    EventKey* key = ecs_field(it, EventKey, 2);
    size_t prompt_len = strlen(paint->prompt);
    if (key->action == GLFW_PRESS && key->key == GLFW_KEY_V && key->mods & GLFW_MOD_CONTROL)
    {
        // Paste text here
        const char* clipboard = glfwGetClipboardString(window);
        strcat(paint->prompt, clipboard);
    }
    if  (key->key == GLFW_KEY_BACKSPACE && prompt_len > 0 && prompt_len < 255 && (key->action == GLFW_PRESS || key->action == GLFW_REPEAT))
    {
        // paint->prompt[prompt_len] = NULL;
        paint->prompt[prompt_len-1] = NULL;
    }
    printf("Key is %i. Action is %i\n",key->scancode, key->action);
    if (key->key == GLFW_KEY_ENTER && key->action == GLFW_PRESS)
    {
        paint->run_endless = !paint->run_endless;
    }
}

void GenerateImages(ecs_iter_t* it)
{
    PaintFrame* paint = ecs_field(it, PaintFrame, 1);
    if (paint->run_endless)
    {
        if (!is_creating_image)
        {
            pthread_mutex_lock(&mutex_stable_diffusion);
            printf("Start create image!");
            prompt = paint->prompt;
            should_create_image = true;
            is_creating_image = true;
            pthread_mutex_unlock(&mutex_stable_diffusion); 
        }
    }
}

void MouseEndAction(ecs_iter_t* it)
{
    EventMouseButton* event = ecs_field(it, EventMouseButton, 1);
    ActionOnMouseInput* action = ecs_field(it, ActionOnMouseInput, 2);

    printf("Action %d", event->action);
    if (event->action == GLFW_RELEASE)
    {
        printf("Mouse end\n");
        ecs_remove(it->world, input, ActionOnMouseInput);
    }
}

void ChangeInteractionOnKeyPress(ecs_iter_t* it)
{
    EventKey* event = ecs_field(it, EventKey, 1);
    InteractionState* interaction = ecs_field(it, InteractionState, 2);

    if (event->key == GLFW_KEY_C)
    {
        if (event->action == GLFW_PRESS)
        {
            interaction->mode = CROP;
        } 
        else if (event->action == GLFW_RELEASE)
        {
            interaction->mode = STANDARD;
        }
    }
}

void UpdateCursorAction(ecs_iter_t* it)
{
    Camera* camera = ecs_field(it, Camera, 1);
    EventMouseMotion* event = ecs_field(it, EventMouseMotion, 2);
    CircleActionIndicator* indicator = ecs_field(it, CircleActionIndicator, 3);
    Transform2D* transform = ecs_field(it, Transform2D, 4);
    SelectionBounds* selection = ecs_field(it, SelectionBounds, 5);

    vec2 worldPos;
    screen_to_world(camera->view, event->pos, worldPos);
    c2v cursorWorldPos = {worldPos[0], worldPos[1]};
    for (int32_t i = 0; i < it->count; i++)
    {
        vec2 transformScreenPos;
        // world_to_screen(camera->view, transform[i].pos, transformScreenPos);
        // glm_vec2_add(transformScreenPos, indicator[i].screenOffset, transform[i].pos);
        vec2 transformWorldPos;
        screen_to_world(camera->view, transform[i].pos, transformWorldPos);
        c2Circle c = indicator[i].bounds;
        c.p.x = transformWorldPos[0];
        c.p.y = transformWorldPos[1];
        // printf("Transform pos (%f, %f)\n", c.p.x, c.p.y);
        c.r /= camera->scale;
        bool inBounds = c2CircleToPoint(c, cursorWorldPos);
        if (inBounds)
        {
            GLFWcursor* cursor = glfwCreateStandardCursor(indicator[i].cursorType);
            glfwSetCursor(window, cursor);
            ecs_set(it->world, input, ActionOnMouseInput, {false, indicator->symbol, indicator[i].op, {transform[i].pos[0], transform[i].pos[1]}, {selection->bounds[2] - selection->bounds[0], selection->bounds[3] - selection->bounds[1]}});
        }
    }
}

void LoadPaintedImage(ecs_iter_t* it)
{
    // EventPaintLoad* paintLoad = ecs_field(it, EventPaintLoad, 1);
    int locked = pthread_mutex_trylock(&mutex_image_loader);
    if (locked)
    {
        if (should_load_image)
        {
            Anchor place = {0, 0};
            load_visual_symbol(it->world, image_to_load_filepath, 0.0f, 0.0f, place);
            // free(paintLoad->filepath);
            // ecs_delete(it->world, it->entities[0]);
            should_load_image = false;
            is_creating_image = false;
        }
        pthread_mutex_unlock(&mutex_image_loader);
    }
}

void SetupSelectionBoundsActions(ecs_iter_t* it)
{
    SelectionBounds* selection = ecs_field(it, SelectionBounds, 1);

    ecs_entity_t indicators[4];
    const int cursorTypes[4] = {GLFW_RESIZE_NWSE_CURSOR, GLFW_RESIZE_NESW_CURSOR, GLFW_RESIZE_NWSE_CURSOR, GLFW_RESIZE_NESW_CURSOR};
    const c2v coords[4] = {{0, 0}, {0, 0}, {0, 0}, {0, 0}};
    // vec2 screenOffsets[4] = {{-6, -6}, {6, -6}, {6, 6}, {-6, 6}};
    vec2 screenOffsets[4] = {{0, 0}, {0, 0}, {0, 0}, {0, 0}};
    vec2 anchors[4] = {{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}};
    for (size_t i = 0; i < 4; i++)
    {
        indicators[i] = ecs_new_id(world);
        c2Circle bounds = {coords[i], 6};
        ecs_set(world, indicators[i], CircleActionIndicator, {bounds, cursorTypes[i], i, {screenOffsets[i][0], screenOffsets[i][1]}, it->entities[0]});
        ecs_add(world, indicators[i], Transform2D);
        ecs_set(world, indicators[i], Anchor, {anchors[i][0], anchors[i][1]});
        ecs_add_pair(world, indicators[i], EcsChildOf, it->entities[0]);
    }
}

void select_visual_symbol(ecs_world_t* world, ecs_entity_t* entity)
{
    ecs_add(world, entity, Selected);
}

void SaveProjectShortcut(ecs_iter_t* it)
{
    EventKey* event = ecs_field(it, EventKey, 1);
    if (event->key == GLFW_KEY_S && event->mods & GLFW_MOD_CONTROL)
    {
        printf("Save!\n");
        // ecs_entity_t t = ecs_struct_init(it->world, &(ecs_struct_desc_t) {
        //     .entity.name = "T",
        //     .members = {
        //         "path", {ecs_id(ecs_string_t)}
        //     }
        // });
        ecs_query_t* query = ecs_query_new(world, "Transform2D, LocalFile");
        ecs_iter_t qIt = ecs_query_iter(world, query);

        size_t visualSymbolCount = 0;
        while (ecs_query_next(&qIt))
        {
            Transform2D* transform = ecs_field(&qIt, Transform2D, 1);
            LocalFile* file = ecs_field(&qIt, LocalFile, 2);
            for (int32_t i = 0; i < qIt.count; i++)
            {
                printf("%s", file[i].path);
                // LocalFile value = {"test"};
                // char *expr = ecs_ptr_to_json(qIt.world, t, &file[i]);
                // char* expr = ecs_ptr_to_json(qIt.world, ecs_id(LocalFile), &value);
                // printf("%s\n", expr);
            }
        }
    }
}

void save_project_to_file(ecs_world_t* save_world, const char* save_file)
{
    ecs_snapshot_t* save = ecs_snapshot_take(save_world);
    void* save_file_target;
    ecs_os_fopen(&save_file_target, save_file, "w");
    // ecs_snapshot_take_w_iter()
    // ecs_ptr_to_expr()
    // save->serialize()

}

void AnchorPropagate(ecs_iter_t* it)
{
    SelectionBounds* selection = ecs_field(it, SelectionBounds, 1);
    Transform2D* transform = ecs_field(it, Transform2D, 2);
    Anchor* anchor = ecs_field(it, Anchor, 3);

    float width = selection->bounds[2] - selection->bounds[0];
    float height = selection->bounds[3] - selection->bounds[1];
    if (selection)
    {
        // printf("Let's go %d!\n", it->count);
        for (int32_t i = 0; i < it->count; i++)
        {
            transform[i].pos[0] = selection->bounds[0] + anchor[i].horizontal * width;
            transform[i].pos[1] = selection->bounds[1] + anchor[i].vertical * height;
        }
    }
}

void AnchorPropagatePartial(ecs_iter_t* it)
{
    Transform2D* parent = ecs_field(it, Transform2D, 1);
    Texture2D* texture = ecs_field(it, Texture2D, 2);
    TextureView* tv = ecs_field(it, TextureView, 3);
    Transform2D* transform = ecs_field(it, Transform2D, 4);
    Anchor* anchor = ecs_field(it, Anchor, 5);

    if (parent)
    {
        // printf("Let's go %d!\n", it->count);
        for (int32_t i = 0; i < it->count; i++)
        {
            transform[i].pos[0] = parent->pos[0] + tv->bounds.min.x/texture->scale[0] + anchor[i].horizontal * (tv->bounds.max.x - tv->bounds.min.x) / texture->scale[0];
            transform[i].pos[1] = parent->pos[1] + tv->bounds.min.y/texture->scale[1] + anchor[i].vertical * (tv->bounds.max.y - tv->bounds.min.y) / texture->scale[1];
        }
    }   
}

void TransformCascadeHierarchy(ecs_iter_t* it)
{
    Transform2D* parent = ecs_field(it, Transform2D, 1);
    Transform2D* transform = ecs_field(it, Transform2D, 2);
    Local2D* local = ecs_field(it, Local2D, 3);

    if (!parent)
    {
        for (int32_t i = 0; i < it->count; i++)
        {
            // TODO: Should Local2D affect root transform?
        }
    } else
    {
        for (int32_t i = 0; i < it->count; i++)
        {
            transform[i].pos[0] = parent->pos[0] + local[i].pos[0];
            transform[i].pos[1] = parent->pos[1] + local[i].pos[1];
        }
    }
}

void add_command_on_grab(ecs_iter_t* it, CommandBuffer* buffer)
{
    // ecs_filter_t filter;
    // ecs_filter_init(world, &filter, &(ecs_filter_desc_t) {
    //     .terms = {{ ecs_id(Selected) } }
    // });
    // ecs_iter_t change = ecs_filter_iter(world, &filter);
    // ecs_snapshot_t *s = ecs_snapshot_take_w_iter(&change, ecs_filter_next);
    // // ecs_snapshot_t *s = ecs_snapshot_take(world);
    // push_command(&buffer, s);
    // ecs_add(it->world, input, TakeSnapshot);
}

void OpenSymbolPath(ecs_iter_t* it)
{
    Camera* camera = ecs_field(it, Camera, 1);
    EventMouseButton* event = ecs_field(it, EventMouseButton, 2);
    if (event->action == GLFW_PRESS && event->button == GLFW_MOUSE_BUTTON_RIGHT)
    {
        return; // TODO: Pass query in ctx...?
        printf("Open symbolPath\n");
        ecs_query_t* query = ecs_query_new(world, "Transform2D, Texture2D, LocalFile");
        ecs_iter_t qIt = ecs_query_iter(it->world, query);

        size_t visualSymbolCount = 0;
        while (ecs_query_next(&qIt))
        {
            visualSymbolCount += qIt.count;
        }
        ecs_entity_t indication;
        qIt = ecs_query_iter(it->world, query);
        double xpos, ypos;
        glfwGetCursorPos(event->window, &xpos, &ypos);
        vec2 screenPos = {xpos, ypos};
        vec2 worldPos;
        screen_to_world(camera->view, screenPos, worldPos);
        while (ecs_query_next(&qIt))
        {
            Transform2D* transform = ecs_field(&qIt, Transform2D, 1);
            Texture2D* texture = ecs_field(&qIt, Texture2D, 2);
            LocalFile* file = ecs_field(&qIt, LocalFile, 3);

            for (int32_t i = 0; i < qIt.count; i++)
            {
                vec2 dist = {0.0f, 0.0f};
                glm_vec2_sub(worldPos, transform[i].pos, dist);
                printf("Check within\n");
                if (dist[0] > 0.0 && dist[0] <= texture[i].width / texture[i].scale[0] &&
                dist[1] > 0.0 && dist[1] <= texture[i].height / texture[i].scale[1])
                {
                    indication = qIt.entities[i];
                    char command[PATH_MAX + 16];
                    sprintf(command, "xdg-open \"%s\"\0", file[i].path);
                    system(command);
                    break;
                }
            }
            if (!ecs_is_valid(qIt.world, indication))
            {
                break;
            }
        }


    }
}

void IndicateSavePaint(ecs_iter_t* it)
{
    PaintFrame* paint = ecs_field(it, PaintFrame, 1);
    EventKey* event = ecs_field(it, EventKey, 2);

    if (event->key == GLFW_KEY_F && event->mods & GLFW_MOD_CONTROL)
    {
        printf("Save paint\n");
        ecs_set(it->world, AI_painter, EventSavePaintIntersection, {0});
    }
    if (event->action == GLFW_PRESS)
    {
        if (event->key == GLFW_KEY_F1)
        {
            paint->interaction_mode = 0;
        } else if (event->key == GLFW_KEY_F2)
        {
            paint->interaction_mode = 1;
        } else if (event->key == GLFW_KEY_F3)
        {
            paint->interaction_mode = 2;
        }
    }
}

void SavePaintFrameInput(ecs_iter_t* it)
{
    // Could render window to texture and crop rect...
    Transform2D* transform = ecs_field(it, Transform2D, 1);
    Texture2D* texture = ecs_field(it, Texture2D, 2);
    Camera* camera = ecs_field(it, Camera, 3);
    PaintFrame* paint = ecs_field(it, PaintFrame, 4);
    EventSavePaintIntersection* intersection = ecs_field(it, EventSavePaintIntersection, 5);
    Transform2D* paintTransform = ecs_field(it, Transform2D, 6);
    printf("Save paint frame input!");
    // Save an image file with the visual symbols contained within a paint frame in a transparent image
    // void* pixels = malloc(paint->w * paint->h * 4);
    c2AABB frameBounds = {{paintTransform->pos[0], paintTransform->pos[1]}, {paintTransform->pos[0] + paint->w, paintTransform->pos[1] + paint->h}};
    SDL_Surface* output = SDL_CreateRGBSurface(0, paint->w, paint->h, 32, 0, 0, 0, 0);
    for (int32_t i = 0; i < it->count; i++)
    {
        c2AABB texBounds = {{transform[i].pos[0], transform[i].pos[1]}, {transform[i].pos[0] + texture[i].width, transform[i].pos[1] + texture[i].height}};
        // Determine if the texture  s with the paint frame (don't consider scale or rotation yet)
        if (c2AABBtoAABB(frameBounds, texBounds))
        {
            // What rect of frame is the texture in
            float paint_min_x = c2Max(0, texBounds.min.x);
            float paint_min_y = c2Max(0, texBounds.min.y);
            float paint_max_y = c2Min(frameBounds.max.y, texBounds.max.y);
            c2AABB paintRect = {{paint_min_x, paint_min_y}, 
            {c2Min(frameBounds.max.x, texBounds.max.x), paint_max_y}};
            // printf("\nPaint overlap section is min(%f, %f), max(%f, %f)\n", paintRect.min.x, paintRect.min.y, paintRect.max.x, paintRect.max.y);

            // What rect of the texture overlaps the frame
            float min_x = c2Max(0, frameBounds.min.x - texBounds.min.x);
            float min_y = c2Max(0, frameBounds.min.y - texBounds.min.y);
            printf("Max y %d\n", texBounds.max.y);
            c2AABB textureRect = {{min_x, min_y}, 
            {c2Min(min_x + frameBounds.max.x - paint_min_x, min_x + texBounds.max.x), c2Min(min_y + frameBounds.max.y - paint_min_y, min_y + texBounds.max.y)}};
            // printf("Texture overlap section is min(%f, %f), max(%f, %f)\n", textureRect.min.x, textureRect.min.y, textureRect.max.x, textureRect.max.y);

            const SDL_Rect src_rect = {textureRect.min.x, textureRect.min.y, textureRect.max.x, textureRect.max.y};
            const SDL_Rect dst_rect = {paintRect.min.x, paintRect.min.y, paintRect.max.x, paintRect.max.y};
            SDL_BlitSurface(texture[i].img, &src_rect, output, &dst_rect);
            IMG_SaveJPG(output, "test_save.jpg", 100);

        }
        printf("Check texture with %d width", texture[i].width);
    }
    ecs_remove(it->world, AI_painter, EventSavePaintIntersection);
}

void select_dragged(ecs_iter_t* it, DragSelector* selector, Camera* camera)
{
    // ecs_query_t* query = ecs_query_new(world, "Transform2D, Texture2D, ?TextureView"); // TODO: Store?
    ecs_iter_t qIt = ecs_query_iter(it->world, (ecs_query_t*)it->ctx);
    while (ecs_query_next(&qIt))
    {
        Transform2D* transform = ecs_field(&qIt, Transform2D, 1);
        Texture2D* texture = ecs_field(&qIt, Texture2D, 2);
        TextureView* tv = ecs_field(&qIt, TextureView, 3);

        for (int32_t i = 0; i < qIt.count; i++)
        {
            vec2 sStart = {selector->x, selector->y};
            vec2 selectorStart;
            screen_to_world(camera->view, sStart, selectorStart);
            vec2 sEnd = {selector->x + selector->w, selector->y + selector->h};
            vec2 selectorEnd;
            screen_to_world(camera->view, sEnd, selectorEnd);
            vec2 minPos = {c2Min(selectorStart[0], selectorEnd[0]), c2Min(selectorStart[1], selectorEnd[1])};
            vec2 maxPos = {c2Max(selectorStart[0], selectorEnd[0]), c2Max(selectorStart[1], selectorEnd[1])};
            c2AABB selectorBox = {{minPos[0], minPos[1]}, {maxPos[0], maxPos[1]}};
            printf("Selector box is %f %f, %f, %f\n", selectorBox.min.x, selectorBox.min.y, selectorBox.max.x, selectorBox.max.y);
            c2AABB visualSymbolBounds = {{transform[i].pos[0], transform[i].pos[1]}, {transform[i].pos[0] + texture[i].width / texture[i].scale[0], transform[i].pos[1] + texture[i].height / texture[i].scale[0]}};
            c2AABB croppedVSB = visualSymbolBounds;
            if (ecs_field_is_set(&qIt, 3))
            {
                printf("Query for view is set\n");
                croppedVSB.min.x += tv[i].bounds.min.x/texture[i].scale[0];
                croppedVSB.min.y += tv[i].bounds.min.y/texture[i].scale[1];
                croppedVSB.max.x = transform[i].pos[0] + tv[i].bounds.max.x/texture[i].scale[0];
                croppedVSB.max.y = transform[i].pos[1] + tv[i].bounds.max.y/texture[i].scale[1];
            }
            printf("Symbol bounds are %f %f, %f, %f\n", croppedVSB.min.x, croppedVSB.min.y, croppedVSB.max.x, croppedVSB.max.y);
            bool selectorOverlaps = c2AABBtoAABB(selectorBox, croppedVSB);

            bool isSelected = ecs_has(it->world, qIt.entities[i], Selected);
            if (selectorOverlaps)
            {
                if (!isSelected)
                {
                    select_visual_symbol(it->world, qIt.entities[i]);
                }
            } else if (isSelected) // deselect
            {
                ecs_remove(it->world, qIt.entities[i], Selected);
            }
        }

    }
}

void SelectVisualSymbolQuery(ecs_iter_t* it)
{
    // printf("Select visual symbol\n");
    Camera* camera = ecs_field(it, Camera, 1);
    EventMouseButton* event = ecs_field(it, EventMouseButton, 2);
    InteractionState* interaction = ecs_field(it, InteractionState, 3);
    SelectionBounds* selection = ecs_field(it, SelectionBounds, 4);
    // ActionOnMouseInput* mouseInputAction = ecs_field(it, ActionOnMouseInput, 4);

    printf("Select visual symbol query!\n");

    DragSelector* selector = NULL;
    if (ecs_has(it->world, input, DragSelector))
    {
        selector = ecs_get_mut(it->world, input, DragSelector);
    };
    bool isDragSelected = selector != NULL;

    ecs_defer_begin(it->world);
    if (event->button == GLFW_MOUSE_BUTTON_LEFT)
    {
        ecs_iter_t qIt = ecs_query_iter(it->world, (ecs_query_t*)it->ctx);

        size_t visualSymbolCount = 0;
        while (ecs_query_next(&qIt))
        {
            visualSymbolCount += qIt.count;
        }
        ecs_entity_t curSelectedNodes[visualSymbolCount];
        size_t curSelectedCount = 0;
        ecs_entity_t toSelectNodes[visualSymbolCount];
        size_t toSelectCount = 0;
        qIt = ecs_query_iter(it->world, (ecs_query_t*)it->ctx);

        if (isDragSelected && event->action == GLFW_RELEASE)
        {
            while (ecs_query_next(&qIt))
            {
                Transform2D* transform = ecs_field(&qIt, Transform2D, 1);
                Texture2D* texture = ecs_field(&qIt, Texture2D, 2);
                TextureView* tv = ecs_field(&qIt, TextureView, 3);

                for (int32_t i = 0; i < qIt.count; i++)
                {
                    vec2 sStart = {selector->x, selector->y};
                    vec2 selectorStart;
                    screen_to_world(camera->view, sStart, selectorStart);
                    vec2 sEnd = {selector->x + selector->w, selector->y + selector->h};
                    vec2 selectorEnd;
                    screen_to_world(camera->view, sEnd, selectorEnd);
                    vec2 minPos = {c2Min(selectorStart[0], selectorEnd[0]), c2Min(selectorStart[1], selectorEnd[1])};
                    vec2 maxPos = {c2Max(selectorStart[0], selectorEnd[0]), c2Max(selectorStart[1], selectorEnd[1])};
                    c2AABB selectorBox = {{minPos[0], minPos[1]}, {maxPos[0], maxPos[1]}};
                    printf("Selector box is %f %f, %f, %f\n", selectorBox.min.x, selectorBox.min.y, selectorBox.max.x, selectorBox.max.y);
                    c2AABB visualSymbolBounds = {{transform[i].pos[0], transform[i].pos[1]}, {transform[i].pos[0] + texture[i].width / texture[i].scale[0], transform[i].pos[1] + texture[i].height / texture[i].scale[0]}};
                    c2AABB croppedVSB = visualSymbolBounds;
                    if (ecs_field_is_set(&qIt, 3))
                    {
                        printf("Query for view is set\n");
                        croppedVSB.min.x += tv[i].bounds.min.x/texture[i].scale[0];
                        croppedVSB.min.y += tv[i].bounds.min.y/texture[i].scale[1];
                        croppedVSB.max.x = transform[i].pos[0] + tv[i].bounds.max.x/texture[i].scale[0];
                        croppedVSB.max.y = transform[i].pos[1] + tv[i].bounds.max.y/texture[i].scale[1];
                    }
                    printf("Symbol bounds are %f %f, %f, %f\n", croppedVSB.min.x, croppedVSB.min.y, croppedVSB.max.x, croppedVSB.max.y);
                    bool selectorOverlaps = c2AABBtoAABB(selectorBox, croppedVSB);

                    if (selectorOverlaps)
                    {
                        select_visual_symbol(it->world, qIt.entities[i]);
                        if (interaction->mode == CROP)
                        {
                            // What rect of texture is drag selector overlapping
                            float min_x = c2Max(0, selectorBox.min.x - visualSymbolBounds.min.x);
                            float min_y = c2Max(0, selectorBox.min.y - visualSymbolBounds.min.y);
                            float vs_width = visualSymbolBounds.max.x - visualSymbolBounds.min.x;
                            float vs_height = visualSymbolBounds.max.y - visualSymbolBounds.min.y;
                            c2AABB bounds = {{min_x*texture[i].scale[0], min_y*texture[i].scale[1]},
                            {c2Min(vs_width, selectorBox.max.x - visualSymbolBounds.min.x)*texture[i].scale[0], c2Min(vs_height, selectorBox.max.y - visualSymbolBounds.min.y)*texture[i].scale[1]}};
                            printf("Texture segment cropped is %f %f, %f, %f\n", bounds.min.x, bounds.min.y, bounds.max.x, bounds.max.y);
                            ecs_set(it->world, qIt.entities[i], TextureView, {bounds});
                        }
                    }
                }

            }
            ecs_remove(it->world, input, DragSelector);
        }
        if (event->action == GLFW_PRESS)
        {
            double xpos, ypos;
            glfwGetCursorPos(event->window, &xpos, &ypos);
            vec2 screenPos = {xpos, ypos};
            vec2 worldPos;
            screen_to_world(camera->view, screenPos, worldPos);
            int right = glfwGetKey(event->window, GLFW_KEY_RIGHT_SHIFT);
            int left = glfwGetKey(event->window, GLFW_KEY_LEFT_SHIFT);

            bool selectAdd = false;
            bool grabSelected = false;
            bool cropSelected = false;
            while (ecs_query_next(&qIt))
            {
                Transform2D* transform = ecs_field(&qIt, Transform2D, 1);
                Texture2D* texture = ecs_field(&qIt, Texture2D, 2);
                TextureView* tv = ecs_field(&qIt, TextureView, 3);

                if (right == GLFW_PRESS || left == GLFW_PRESS)
                {
                    selectAdd = true;
                }
                for (int32_t i = 0; i < qIt.count; i++)
                {
                    bool isSelected = ecs_has(qIt.world, qIt.entities[i], Selected);
                    if (isSelected)
                    {
                        curSelectedNodes[curSelectedCount] = qIt.entities[i];
                        curSelectedCount++;
                    }
                    c2AABB visualSymbolBounds = {{transform[i].pos[0], transform[i].pos[1]}, {transform[i].pos[0] + texture[i].width / texture[i].scale[0], transform[i].pos[1] + texture[i].height / texture[i].scale[0]}};
                    c2AABB croppedVSB = visualSymbolBounds;
                    if (ecs_field_is_set(&qIt, 3))
                    {
                        printf("Query for view is set\n");
                        croppedVSB.min.x += tv[i].bounds.min.x/texture[i].scale[0];
                        croppedVSB.min.y += tv[i].bounds.min.y/texture[i].scale[1];
                        croppedVSB.max.x = transform[i].pos[0] + tv[i].bounds.max.x/texture[i].scale[0];
                        croppedVSB.max.y = transform[i].pos[1] + tv[i].bounds.max.y/texture[i].scale[1];
                    }
                    c2v queryPoint = {worldPos[0], worldPos[1]};
                    if (c2AABBtoPoint(croppedVSB, queryPoint))
                    {
                        if (isSelected)
                        {
                            if (interaction->mode == STANDARD)
                            {
                                grabSelected = true;
                            } 
                            else if (interaction->mode == CROP)
                            {
                                cropSelected = true;
                            }
                        }
                        if (interaction->mode == STANDARD)
                        {
                            toSelectNodes[toSelectCount] = qIt.entities[i];
                            toSelectCount++;
                        }
                    }
                }
            }

            if (selectAdd)
            {
                select_visual_symbol(it->world, toSelectNodes[0]);
                ecs_set(it->world, input, DragSelector, {xpos, ypos, 0, 0});
            } else
            {
                if (grabSelected)
                {
                    add_command_on_grab(it, &buffer);
                    for (int32_t i = 0; i < curSelectedCount; i++)
                    {
                        ecs_add(it->world, curSelectedNodes[i], Grabbed);
                    }
                } else
                {
                    bool shouldDeselect = !cropSelected;
                    if (shouldDeselect)
                    {
                        for (int32_t i = 0; i < curSelectedCount; i++)
                        {
                            ecs_remove(it->world, curSelectedNodes[i], Selected);
                        }
                    }
                    if (toSelectCount > 0)
                    {
                        add_command_on_grab(it, &buffer);
                        select_visual_symbol(it->world, toSelectNodes[0]);
                        ecs_add(it->world, toSelectNodes[0], Grabbed);
                    }
                    if (toSelectCount == 0 || !shouldDeselect) 
                    {
                        ecs_set(it->world, input, DragSelector, {xpos, ypos, 0, 0});
                    }
                }
            }
        }

    }
    ecs_defer_end(it->world);
}

void screen_to_world(mat4* view, vec2 screenCoords, vec2 world)
{
    mat4 proj;
    int wwidth, wheight;
    glfwGetWindowSize(window, &wwidth, &wheight);
    glm_ortho(0.0, wwidth, wheight, 0.0, -1.0, 10.0, proj);
    // TODO: consider model for rotated textures
    vec4 coords = {screenCoords[0], screenCoords[1], 0.0, 1.0};
    vec4 screenNormalized;
    glm_mat4_mulv(proj, coords, screenNormalized);
    mat4 vp;
    glm_mat4_mul(proj, view, vp);
    glm_mat4_inv(vp, vp);
    vec4 worldPos;
    glm_mat4_mulv(vp, screenNormalized, worldPos);
    world[0] = worldPos[0];
    world[1] = worldPos[1];
}

void world_to_screen(mat4* view, vec2 worldCoords, vec2 screen)
{
    mat4 proj;
    int wwidth, wheight;
    glfwGetWindowSize(window, &wwidth, &wheight);
    glm_ortho(0.0, wwidth, wheight, 0.0, -1.0, 10.0, proj);
    // TODO: consider model for rotated textures
    vec4 coords = {worldCoords[0], worldCoords[1], 0.0, 1.0};
    vec4 screenNormalized;
    glm_mat4_inv(proj, proj);
    mat4 invView;
    glm_mat4_inv(view, invView);
    glm_mat4_mulv(proj, coords, screenNormalized);
    mat4 vp;
    glm_mat4_mul(proj, invView, vp);
    glm_mat4_inv(vp, vp);
    vec4 worldPos;
    glm_mat4_mulv(vp, screenNormalized, worldPos);
    screen[0] = worldPos[0];
    screen[1] = worldPos[1];
}

void register_components()
{

}

void ScrollZoomCamera(ecs_iter_t* it)
{
    Camera* camera = ecs_field(it, Camera, 1);
    EventScroll* scroll = ecs_field(it, EventScroll, 2);

    // Keep cursor positioned on the same image pixel it was above
    double xpos, ypos;
    glfwGetCursorPos(scroll->window, &xpos, &ypos);

    int wwidth, wheight;
    glfwGetWindowSize(scroll->window, &wwidth, &wheight);
 
    // vec2 cursorScreenPos = {wwidth/2.0 - xpos, wheight/2.0 - ypos};
    vec2 cursorScreenPos = {xpos, ypos};
    vec2 cursorWorldPos;
    screen_to_world(camera->targetView, cursorScreenPos, cursorWorldPos);
    camera->targetScale = c2Clamp(camera->targetScale + scroll->yoffset * 0.10 * camera->targetScale, 0.1, 10.0);
    calc_view(camera);

    vec2 cursorNewWorldPos;
    screen_to_world(camera->targetView, cursorScreenPos, cursorNewWorldPos);

    vec2 diff = {camera->targetScale * (cursorWorldPos[0] - cursorNewWorldPos[0]), camera->targetScale * (cursorWorldPos[1] - cursorNewWorldPos[1])};
    glm_vec2_sub(camera->targetPos, diff, camera->targetPos);
    calc_view(camera);
}

void StartNanoVGFrame(ecs_iter_t* it)
{
    NanoVG* nano = ecs_field(it, NanoVG, 1);
    int winWidth, winHeight;
    int fbWidth, fbHeight;
    float pxRatio;
    glfwGetWindowSize(window, &winWidth, &winHeight);
    glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
    // Calculate pixel ration for hi-dpi devices.
    pxRatio = (float)fbWidth / (float)winWidth;
    nvgBeginFrame(nano->vg, winWidth, winHeight, pxRatio);
}

void EndNanoVGFrame(ecs_iter_t* it)
{
    NanoVG* nano = ecs_field(it, NanoVG, 1);
    nvgEndFrame(nano->vg);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
}

// https://www.iquilezles.org/www/articles/functions/functions.htm
float gain(float x, float k) 
{
    const float a = 0.5*pow(2.0*((x<0.5)?x:1.0-x), k);
    return (x<0.5)?a:1.0-a;
}

float interp(float a, float b, float p)
{
    return (a * (1.0 - p) + b * p);
}

void InterpolateCamera(ecs_iter_t* it)
{
    Camera* camera = ecs_field(it, Camera, 1);
    float p = fmin(1.0, it->delta_time/camera->interpTime);
    camera->scale = interp(camera->scale, camera->targetScale, p);
    camera->pos[0] = interp(camera->pos[0], camera->targetPos[0], p);
    camera->pos[1] = interp(camera->pos[1], camera->targetPos[1], p);
}

void RenderSelectionIndicators(ecs_iter_t* it)
{
    NanoVG* nano = ecs_field(it, NanoVG, 1);
    Camera* camera = ecs_field(it, Camera, 2);
    SelectionBounds* selection = ecs_field(it, SelectionBounds, 3);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);

    nvgBeginPath(nano->vg);
    float radius = 6;
    
    ecs_iter_t qIt = ecs_query_iter(it->world, (ecs_query_t*)it->ctx);
    int s = 0;
    while (ecs_query_next(&qIt))
    {
        Transform2D* transform = ecs_field(&qIt, Transform2D, 1);
        Texture2D* texture = ecs_field(&qIt, Texture2D, 2);
        TextureView* tv = ecs_field(&qIt, TextureView, 3);
        for (int32_t i = 0; i < qIt.count; i++)
        {
            vec4 offsets = {0, 0, texture[i].width, texture[i].height};
            if (ecs_field_is_set(it, 4))
            {
                offsets[0] = tv[i].bounds.min.x / texture[i].scale[0];
                offsets[1] = tv[i].bounds.min.y / texture[i].scale[1];
                offsets[2] = tv[i].bounds.max.x;
                offsets[3] = tv[i].bounds.max.y;
            }
            vec2 upperLeft = {transform[i].pos[0] + offsets[0], transform[i].pos[1] + offsets[1]};
            world_to_screen(camera->view, upperLeft, upperLeft);
            vec2 upperRight = {transform[i].pos[0] + c2Min(offsets[2], texture[i].width) / texture[i].scale[0], transform[i].pos[1] + offsets[1]};
            world_to_screen(camera->view, upperRight, upperRight);
            vec2 lowerLeft = {transform[i].pos[0] + offsets[0], transform[i].pos[1] + c2Min(texture[i].height, offsets[3]) / texture[i].scale[1]};
            world_to_screen(camera->view, lowerLeft, lowerLeft);
            vec2 lowerRight = {transform[i].pos[0] + c2Min(offsets[2], texture[i].width) / texture[i].scale[0], transform[i].pos[1] + c2Min(texture[i].height, offsets[3]) / texture[i].scale[1]};
            world_to_screen(camera->view, lowerRight, lowerRight);

            float w = upperRight[0] - upperLeft[0];
            float h = lowerLeft[1] - upperLeft[1];

            nvgRect(nano->vg, upperLeft[0], upperLeft[1], upperRight[0] - upperLeft[0], lowerRight[1] - upperRight[1]);
            if (s == 0)
            {
                selection->bounds[0] = upperLeft[0];
                selection->bounds[1] = upperLeft[1];
                selection->bounds[2] = lowerRight[0];
                selection->bounds[3] = lowerRight[1];
            } else
            {
                selection->bounds[0] = c2Min(selection->bounds[0], upperLeft[0]);
                selection->bounds[1] = c2Min(selection->bounds[1], upperLeft[1]);
                selection->bounds[2] = c2Max(selection->bounds[2], lowerRight[0]);
                selection->bounds[3] = c2Max(selection->bounds[3], lowerRight[1]);
            }
            s++;
        }
    }
    if (qIt.count > 0)
    {
        nvgRect(nano->vg, selection->bounds[0], selection->bounds[1], selection->bounds[2] - selection->bounds[0], selection->bounds[3] - selection->bounds[1]);
    }
    nvgStrokeColor(nano->vg, nvgRGBA(157, 3, 252,255));
    nvgStroke(nano->vg);
    // nvgFillColor(nano->vg, nvgRGBA(157, 3, 252,255));
    // nvgFill(nano->vg);
    nvgClosePath(nano->vg);
    // glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
}

void ResetCursor(ecs_iter_t* it)
{
    ActionOnMouseInput* action = ecs_get(it->world, input, ActionOnMouseInput);
    if (!action || !action->active)
    {
        ecs_remove(it->world, input, ActionOnMouseInput);
        // ecs_query_t* query = ecs_query_new(world, "Grabbed");
        // ecs_iter_t qIt = ecs_query_iter(query);
        // size_t totalGrabbed = 0;
        // while (ecs_query_next(&qIt))
        // {
        //     totalGrabbed += qIt.count;
        // }
        // if (totalGrabbed > 0)
        // {
        //     GLFWcursor* cursor = glfwCreateStandardCursor(GLFW_HAND_CURSOR);
        //     glfwSetCursor(window, cursor);
        // } else
        // {
        // }
        glfwSetCursor(window, NULL);
    }
}

#include <dirent.h>
// #include <limits.h>

void ApparateVisualSymbols(ecs_iter_t* it)
{
    SavedData* data = ecs_field(it, SavedData, 2);
    DIR* dir = opendir(data->path);
    struct dirent* next;
    char buf[FILENAME_MAX];
    strcat(buf, data->path);
    size_t index = strlen(data->path);
    Transform2D* lastTrans = NULL;
    Texture2D* lastTexture = NULL;
    size_t symbol_count = 0;
    ecs_entity_t test[100];
    while ((next = readdir(dir)) != NULL)
    {
        strcat(buf, next->d_name);
        printf("Path is %s\n", buf);
        Anchor place = {0, 0};
        VisualSymbolCreated vsc = load_visual_symbol(it->world, buf, 0, 0, place);
        if (ecs_is_valid(it->world, vsc.node))
        {
            test[symbol_count] = vsc.node;
            symbol_count++;
        }
        memset(&buf[index], 0, sizeof(next->d_name));
    }
    closedir(dir);
    
    ecs_delete(it->world, it->entities[0]);
}

void RenderActionIndicators(ecs_iter_t* it)
{
    NanoVG* nano = ecs_field(it, NanoVG, 1);
    Camera* camera = ecs_field(it, Camera, 2);
    SelectionBounds* selection = ecs_field(it, SelectionBounds, 3);
    Transform2D* transform = ecs_field(it, Transform2D, 4);
    CircleActionIndicator* indicator = ecs_field(it, CircleActionIndicator, 5);

    int selectedSymbolCount = 0;
    ecs_filter_t* filter = ecs_filter(it->world, {.terms={ecs_id(Selected)}});
    ecs_iter_t qIt = ecs_filter_iter(it->world, filter);
    while (ecs_filter_next(&qIt))
    {
        selectedSymbolCount += qIt.count;
    }

    if (selectedSymbolCount > 0)
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_CULL_FACE);
        glDisable(GL_DEPTH_TEST);

        float radius = 6;

        for (int32_t i = 0; i < it->count; i++)
        {
            nvgBeginPath(nano->vg);
            nvgCircle(nano->vg, transform[i].pos[0] + indicator[i].screenOffset[0], transform[i].pos[1] + indicator[i].screenOffset[1], radius);
            nvgFillColor(nano->vg, nvgRGBA(157, 3, 252,255));
            nvgFill(nano->vg);
            nvgClosePath(nano->vg);
        }
        glDisable(GL_CULL_FACE);
    }

}

double lastXPos, lastYPos;
void cursor_position_callback(GLFWwindow* window, double xpos, double ypos)
{
    bool isAdded = false;
    bool exists = ecs_has(world, input, EventMouseMotion);
    if (exists)
    {
        bool isAdded;
        EventMouseMotion* motion = ecs_get_mut(world, input, EventMouseMotion);
        vec2 delta = {xpos - lastXPos, ypos - lastYPos};
        glm_vec2_add(motion->delta, delta, motion->delta);
    } else
    {
        ecs_set(world, input, EventMouseMotion, {window, {xpos, ypos}, {xpos - lastXPos, ypos - lastYPos}});
        ecs_set_pair(world, input, ConsumeEvent, ecs_id(EventMouseMotion), {});
    }
    lastXPos = xpos;
    lastYPos = ypos;
}

int main(int argc, char const *argv[])
{
    srand(time(0));

    // Embed Python
    // pthread_t python_thread;
    // pthread_create(&python_thread, NULL, embed_python, (void*)NULL);

    buffer.index = 0;
    buffer.count = 0;
    buffer.capacity = 100;
    buffer.snapshots = malloc(sizeof(ecs_snapshot_t*) * buffer.capacity);
    world = ecs_init();
    ECS_COMPONENT_DEFINE(world, Camera);
    ECS_COMPONENT_DEFINE(world, CameraController);
    ECS_COMPONENT_DEFINE(world, Transform2D);
    ECS_COMPONENT_DEFINE(world, BatchSpriteRenderer);
    ECS_COMPONENT_DEFINE(world, Texture2D);
    ECS_COMPONENT_DEFINE(world, LocalFile);
    ECS_COMPONENT_DEFINE(world, MultiTexture2D);
    ECS_COMPONENT_DEFINE(world, GifAnimator);
    ECS_COMPONENT_DEFINE(world, ConsumeEvent);
    ECS_COMPONENT_DEFINE(world, EventMouseButton);
    ECS_COMPONENT_DEFINE(world, EventMouseMotion);
    ECS_COMPONENT_DEFINE(world, EventScroll);
    ECS_COMPONENT_DEFINE(world, EventCharEntry);
    ECS_COMPONENT_DEFINE(world, EventDropFiles);
    ECS_COMPONENT_DEFINE(world, DragSelector);
    ECS_COMPONENT_DEFINE(world, NineSlice);
    ECS_COMPONENT_DEFINE(world, EventKey);
    ECS_COMPONENT_DEFINE(world, NanoVG);
    ECS_COMPONENT_DEFINE(world, CircleActionIndicator);
    ECS_COMPONENT_DEFINE(world, Local2D);
    ECS_COMPONENT_DEFINE(world, ActionOnMouseInput);
    ECS_COMPONENT_DEFINE(world, Anchor);
    ECS_COMPONENT_DEFINE(world, SavedData);
    ECS_COMPONENT_DEFINE(world, PaintFrame);
    ECS_COMPONENT_DEFINE(world, WaitThreadComplete);
    ECS_COMPONENT_DEFINE(world, EventPaintLoad);
    ECS_COMPONENT_DEFINE(world, EventSavePaintIntersection);
    ECS_COMPONENT_DEFINE(world, InteractionState);
    ECS_COMPONENT_DEFINE(world, TextureView);
    ECS_COMPONENT_DEFINE(world, SelectionBounds);
    typedef enum SelectMode SelectMode;
    ECS_TAG_DEFINE(world, Selected);
    ECS_TAG_DEFINE(world, Grabbed);
    ECS_TAG_DEFINE(world, DragHover);
    ECS_TAG_DEFINE(world, TakeSnapshot);
    ECS_TAG_DEFINE(world, BrowseDirectory);

    // ECS_IMPORT(world, InputModule);
    input = ecs_set_name(world, 0, "input");
    ecs_set(world, input, InteractionState, {STANDARD});

    AI_painter = ecs_set_name(world, 0, "AI_painter");
    ecs_set(world, AI_painter, Transform2D, {0.0f, 0.0f});
    ecs_set(world, AI_painter, PaintFrame, {512.0f, 512.0f, false, 0, ""});

    ECS_OBSERVER(world, SetupBatchRenderer, EcsOnSet, BatchSpriteRenderer);
    ECS_OBSERVER(world, SetupCamera, EcsOnSet, Camera);
    ECS_OBSERVER(world, SetInitialMultitexture, EcsOnSet, Texture2D, MultiTexture2D);
    ECS_OBSERVER(world, deallocate_texture, EcsOnRemove, Texture2D);
    ECS_OBSERVER(world, ApparateVisualSymbols, EcsOnSet, BrowseDirectory, SavedData, [inout] *());
    ECS_OBSERVER(world, SetupNanoVG, EcsOnSet, NanoVG);
    
    ECS_OBSERVER(world, SetupSelectionBoundsActions, EcsOnAdd, SelectionBounds);
    ecs_add(world, input, SelectionBounds);

    IMG_Init(IMG_INIT_JPG | IMG_INIT_PNG | IMG_INIT_WEBP);
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_SAMPLES, 4);
    // glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    // glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GL_TRUE);
    glfwWindowHint(GLFW_DECORATED, GL_FALSE);
    glfwWindowHint(GLFW_VISIBLE, GL_FALSE);
    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, 1);
    glfwSetErrorCallback(error_callback);
    window = glfwCreateWindow(640, 480, "Gui Gal 👩‍💻", NULL, NULL);
    glfwMakeContextCurrent(window);
    gladLoadGL();
    glfwSwapInterval(1);
    glfwSetKeyCallback(window, key_callback);
    glfwSetCharCallback(window, char_callback);
    glfwSetDropCallback(window, drop_callback);
    glfwSetDragCallback(window, drag_callback);
    glfwSetCursorPosCallback(window, cursor_position_callback);
    glfwSetWindowSizeCallback(window, window_size_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glClearColor(32/255.0, 32/255.0, 32/255.0, 0.9);
    center_window(window, glfwGetPrimaryMonitor());
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    glViewport(0, 0, width, height);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glEnable(GL_STENCIL_TEST);
    glEnable(GL_MULTISAMPLE);
    // glEnable(GL_DEPTH_TEST);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    renderer = ecs_set_name(world, 0, "renderer");
    ecs_set(world, renderer, Camera, {0.05f, 1.0, 1.0, {0.0f, 0.0f}, {0.0f, 0.0f}});
    ecs_set(world, renderer, BatchSpriteRenderer, {});
    ecs_set(world, renderer, NanoVG, {nvgCreateGL3(NVG_ANTIALIAS | NVG_STENCIL_STROKES | NVG_DEBUG)});

    ECS_OBSERVER(world, LoadDroppedFiles, EcsOnSet, EventDropFiles, Camera(renderer));

    // if (argc > 1)
    // {
    //     ecs_entity_t nav_command = ecs_new_id(world);
    //     ecs_add(world, nav_command, Transform2D);
    //     ecs_add(world, nav_command, BrowseDirectory);
    //     ecs_set(world, nav_command, SavedData, {argv[1]});
    // }

    ECS_SYSTEM(world, ChangeInteractionOnKeyPress, EcsPreFrame, EventKey(input), InteractionState(input));
    ECS_SYSTEM(world, SavePaintFrameInput, EcsPreFrame, Transform2D, Texture2D, Camera(renderer), PaintFrame(AI_painter), EventSavePaintIntersection(AI_painter), Transform2D(AI_painter), [inout] *());
    ECS_SYSTEM(world, LoadPaintedImage, EcsPreFrame, [inout]*());
    ECS_SYSTEM(world, IndicateSavePaint, EcsPreFrame, PaintFrame, EventKey(input));
    ECS_SYSTEM(world, CheckThreadComplete, EcsPreFrame, WaitThreadComplete, [inout] *());
    ECS_SYSTEM(world, ResetCursor, EcsPreFrame, EventMouseMotion);
    ECS_SYSTEM(world, AnimateGif, EcsPreUpdate, GifAnimator, Texture2D, MultiTexture2D);
    ECS_SYSTEM(world, RenderSprites, EcsOnUpdate, Camera(renderer), BatchSpriteRenderer(renderer), Transform2D, Texture2D, ?TextureView);
    //ECS_SYSTEM(world, RenderNineSlices, EcsOnUpdate, renderer:Camera, renderer:BatchSpriteRenderer, Transform2D, Texture2D, NineSlice);
    ECS_SYSTEM(world, GrabMoveCamera, EcsPreUpdate, Camera(renderer), EventMouseMotion(input));
    ECS_SYSTEM(world, ScrollZoomCamera, EcsPreUpdate, Camera(renderer), EventScroll(input));
    ECS_SYSTEM(world, CameraCalculateView, EcsOnUpdate, Camera);
    ecs_query_t* vsq = ecs_query_new(world, "Transform2D, Texture2D, ?TextureView");
    ecs_system(world, { 
        .entity = ecs_entity(world, { .name = "SelectVisualSymbolQuery", .add = {EcsPreUpdate} }), 
        .query.filter.expr = "Camera(renderer), EventMouseButton(input), InteractionState(input), SelectionBounds(input), !ActionOnMouseInput(input), [inout] *()", 
        .callback = SelectVisualSymbolQuery,
        .ctx = vsq
    });
    ECS_SYSTEM(world, SelectVisualSymbolQuery, EcsPreUpdate, Camera(renderer), EventMouseButton(input), InteractionState(input), SelectionBounds(input), !ActionOnMouseInput(input), [inout] *());
    ECS_SYSTEM(world, OpenSymbolPath, EcsPreUpdate, Camera(renderer), EventMouseButton(input), !ActionOnMouseInput(input), [inout] *());
    ECS_SYSTEM(world, MoveGrabbedTransforms, EcsPreFrame, Camera(renderer), EventMouseMotion(input), Transform2D, Grabbed);
    ECS_SYSTEM(world, ConsumeEvents, EcsPostFrame, (ConsumeEvent, *));
    ECS_SYSTEM(world, UnGrab, EcsOnUpdate, EventMouseButton(input), Grabbed);
    ECS_SYSTEM(world, DeleteSelected, EcsOnUpdate, EventKey(input), Selected)
    ECS_SYSTEM(world, StartNanoVGFrame, EcsPreFrame, NanoVG(renderer));
    ECS_SYSTEM(world, EndNanoVGFrame, EcsPostUpdate, NanoVG(renderer));
    ECS_SYSTEM(world, RenderDragHover, EcsPostUpdate, NanoVG(renderer), DragHover(input));
    ECS_SYSTEM(world, RenderPaintFrame, EcsPostUpdate, NanoVG(renderer), Camera(renderer), Transform2D(AI_painter), PaintFrame(AI_painter));
    ecs_system(world, { 
        .entity = ecs_entity(world, { .name = "MoveDragSelector", .add = { ecs_dependson(EcsPreFrame) } }), 
        .query.filter.expr = "DragSelector(input), EventMouseMotion(input), Camera(renderer)", 
        .callback = MoveDragSelector,
        .ctx = vsq
    });
    ECS_SYSTEM(world, UndoCommand, EcsOnUpdate, EventKey(input));
    ECS_SYSTEM(world, SaveProjectShortcut, EcsPostUpdate, EventKey(input), [inout] *());
    ECS_SYSTEM(world, InterpolateCamera, EcsPostUpdate, Camera);
    ECS_SYSTEM(world, RenderImageName, EcsPostUpdate, NanoVG(renderer));
    ECS_SYSTEM(world, TypePaintPrompt, EcsPostUpdate, PaintFrame(AI_painter), EventCharEntry(input), [inout] *());
    ECS_SYSTEM(world, BackspacePaintPrompt, EcsPostUpdate, PaintFrame(AI_painter), EventKey(input), [inout] *());
    ECS_SYSTEM(world, GenerateImages, EcsPostUpdate, PaintFrame(AI_painter));
    ECS_SYSTEM(world, AnchorPropagate, EcsPreUpdate, SelectionBounds(parent), Transform2D, Anchor);
    ECS_SYSTEM(world, RenderActionIndicators, EcsPostUpdate, NanoVG(renderer), Camera(renderer), SelectionBounds(input), Transform2D, CircleActionIndicator);
    ecs_query_t* svsq = ecs_query_new(world, "Transform2D, Texture2D, ?TextureView, Selected");
    ecs_system(world, {
        .entity = ecs_entity(world, { .name = "RenderSelectionIndicators", .add = { ecs_dependson(EcsPreFrame) } }), 
        .query.filter.expr = "NanoVG(renderer), Camera(renderer), SelectionBounds(input), [inout] *()", 
        .callback = RenderSelectionIndicators,
        .ctx = svsq
    });
    // ECS_SYSTEM(world, RenderSelectionIndicators, EcsPostUpdate, NanoVG(renderer), Camera(renderer), SelectionBounds(input));
    ECS_SYSTEM(world, RenderDragSelector, EcsPostUpdate, NanoVG(renderer), DragSelector(input), InteractionState(input));
    ECS_SYSTEM(world, LoadClipboardFiles, EcsOnUpdate, EventMouseButton(input));
    ECS_SYSTEM(world, UpdateCursorAction, EcsOnUpdate, Camera(renderer), EventMouseMotion(input), CircleActionIndicator, Transform2D, SelectionBounds(parent), !ActionOnMouseInput(input));
    ECS_SYSTEM(world, TransformCascadeHierarchy, EcsPreFrame, ?Transform2D(parent|cascade), Transform2D, Local2D);
    ECS_SYSTEM(world, MouseStartAction, EcsOnUpdate, EventMouseButton, ActionOnMouseInput);
    ECS_SYSTEM(world, MouseEndAction, EcsPreFrame, EventMouseButton, ActionOnMouseInput);
    ECS_SYSTEM(world, MouseAffectAction, EcsOnUpdate, EventMouseMotion(input), ActionOnMouseInput(input), Camera(renderer), SelectionBounds);

    ECS_SYSTEM(world, UpdatePromptFromSpeech, EcsOnUpdate, PaintFrame(AI_painter));

    glfwShowWindow(window);

    while (!glfwWindowShouldClose(window))
    {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        glfwPollEvents();
        ecs_progress(world, 0);
        // if (ecs_has(world, input, TakeSnapshot))
        // {
        //     ecs_remove(world, input, TakeSnapshot);
        //     // Create filter from just the grabbed transforms!

        //     // ecs_filter_t filter;
        //     // ecs_filter_init(world, &filter, &(ecs_filter_desc_t) {
        //     //     .terms = {{ ecs_id(Selected) } }
        //     // });
        //     // ecs_iter_t change = ecs_filter_iter(world, &filter);
        //     // ecs_snapshot_t *s = ecs_snapshot_take_w_iter(&change, ecs_filter_next);
        //     // ecs_snapshot_t *s = ecs_snapshot_take(world);
        //     // push_command(&buffer, s);
        // }
        // while (buffer.time_step < 0)
        // {
        //     undo_command(world, &buffer);
        // }
        glfwSwapBuffers(window);
    }
    glfwDestroyWindow(window);
    glfwTerminate();
    free(buffer.snapshots);
    ecs_fini(world);
    // nvgDeleteGL3(vg);
    exit(0);
    return 0;
}
