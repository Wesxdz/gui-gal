#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <stdio.h>
#include "flecs.h"
#include <SDL2/SDL_image.h>
#include <string.h>
#include <gif_lib.h>
#include <sys/types.h>
#include <sys/stat.h>

#define CUTE_C2_IMPLEMENTATION
#include "cute_c2.h"

#include "components.h"

#define NANOVG_GL3_IMPLEMENTATION
#include "nanovg.h"
#include "nanovg_gl.h"

#include "curl/curl.h"

// flecs modules
// #include "input.h"

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
    Camera* camera = ecs_term(it, Camera, 1);
    mat4 view;
    vec3 eye = {0.0f, 0.0f, 0.0};
    vec3 center = {0.0, 0.0, -1.0};
    vec3 up = {0.0, 1.0, 0.0};
    glm_lookat(eye, center, up, camera->view);
}

void SetupBatchRenderer(ecs_iter_t* it)
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
CommandBuffer buffer;
ecs_entity_t renderer;
ecs_entity_t input;

void AnimateGif(ecs_iter_t* it)
{
    GifAnimator* animator = ecs_term(it, GifAnimator, 1);
    Texture2D* texture = ecs_term(it, Texture2D, 2);
    MultiTexture2D* multitexture = ecs_term(it, MultiTexture2D, 3);
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
    Camera* camera = ecs_term(it, Camera, 1);
    BatchSpriteRenderer* renderer = ecs_term(it, BatchSpriteRenderer, 2);
    Transform2D* transform = ecs_term(it, Transform2D, 3);
    Texture2D* texture = ecs_term(it, Texture2D, 4);
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
        // printf("Rendering sprite %f\n", it->world_time);
        mat4 model;
        glm_mat4_identity(model);
        vec3 offset = {transform[i].pos[0], transform[i].pos[1], 0.0f};
        glm_translate(model, offset);
        vec3 scale = {texture[i].pow_w / texture[i].scale[0], texture[i].pow_h / texture[i].scale[1], 1.0};
        glm_scale(model, scale);
        glUniformMatrix4fv(glGetUniformLocation(renderer->shader.programId, "model"), 1, false, model[0]);
        // vec4 box = {10.0, 0.0, texture->surface->w, texture->surface->h};
        // glUniform4f(glGetUniformLocation(renderer->shader.programId, "box"), 100, 40, 1, 1);
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

    printf("Key callback\n");
    ecs_set(world, input, EventKey, {window, key, scancode, action, mods});
    ecs_set_pair(world, input, ConsumeEvent, ecs_id(EventKey), {});
}

unsigned nearest_pow2(int length)
{
    return pow(2,ceil(log(length)/log(2)));
}

bool create_texture(const char* file, ecs_entity_t entity, unsigned int* twidth, unsigned int* theight)
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
    ecs_set(world, entity, Texture2D, {id, pow_w, pow_h, img->w, img->h, {1.0, 1.0}, img});
    *twidth = img->w; *theight = img->h;
    return true;
}

void create_multitexture_from_gif(GifFileType* gif, ecs_entity_t entity, SDL_Surface** firstFrame)
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
        glTexParameteri ( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri ( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
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
    ecs_set(world, entity, MultiTexture2D, {ids, gif->ImageCount});
    printf("Created multitexture!\n");
}

void SetInitialMultitexture(ecs_iter_t* it)
{
    printf("Set initial multitexture!\n");
    Texture2D* texture = ecs_term(it, Texture2D, 1);
    MultiTexture2D* multitexture = ecs_term(it, MultiTexture2D, 2);
    texture->id = multitexture->ids[0];
}


void window_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}

void LoadClipboardFiles(ecs_iter_t* it)
{
    EventMouseButton* event = ecs_term(it, EventMouseButton, 1);
    const char* clipboard = glfwGetClipboardString(window);
}

ecs_entity_t load_visual_symbol(ecs_world_t* s_world, char* path)
{
    ecs_entity_t node;
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
    if (isDirectory) return node;
    int error;
    GifFileType* gif = DGifOpenFileName(path, &error);
    bool isGif = gif != NULL;
    printf("%p:%d\n", (void*)gif, error);

    if (!isDirectory)
    {
        node = ecs_new_id(s_world);
        double xpos, ypos;
        glfwGetCursorPos(window, &xpos, &ypos);
        unsigned int twidth, theight;
        if (isGif)
        {
            DGifSlurp(gif); // TODO: Move logic to GifAnimator system
            printf("Gif has %d images!\n" ,gif->ImageCount);
            SDL_Surface* firstFrame;
            create_multitexture_from_gif(gif, node, &firstFrame);
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
            bool created = create_texture(path, node, &twidth, &theight);
            if (!created) return node;
        }
        const Camera* camera = ecs_get(s_world, renderer, Camera);
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
        vec2 screen = {xpos - twidth/2.0, ypos - theight/2};
        vec2 position;
        memset(position, 0, sizeof(vec2));
        glm_vec2_sub(screen, translate, position);
        ecs_set(s_world, node, Transform2D, {{position[0], position[1]}, 0.0f, 1.0f, 0});
        ecs_set(s_world, node, Local2D, {0, 0});
        ecs_set(s_world, node, LocalFile, {path});
    }
    return node;
}

void LoadDroppedFiles(ecs_iter_t* it)
{
    EventDropFiles* drop = ecs_term(it, EventDropFiles, 1);
    printf("Load dropped files\n");
    for (int i = 0; i < drop->count; i++)
    {
        load_visual_symbol(it->world, drop->paths[i]);
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
    NanoVG* nano = ecs_term(it, NanoVG, 1);
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
    ecs_entity_t pair = ecs_term_id(it, 1);
    ecs_entity_t comp = ecs_pair_object(it->world, pair);
    for (int32_t i = 0; i < it->count; i++)
    {
        ecs_remove_id(it->world, it->entities[i], comp);
        ecs_remove_id(it->world, it->entities[i], pair);
        // printf("Consume event at %f\n", it->world_time);
    }
}

void DeleteSelected(ecs_iter_t* it)
{
  EventKey* event = ecs_term(it, EventKey, 1);
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
    Camera* camera = ecs_term(it, Camera, 1);
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
    EventKey* event = ecs_term(it, EventKey, 1);
    if (event->key == GLFW_KEY_Z && event->mods & GLFW_MOD_CONTROL && event->action == GLFW_PRESS)
    {
        buffer.time_step--;
    }
}

void MoveGrabbedTransforms(ecs_iter_t* it)
{
    Camera* camera = ecs_term(it, Camera, 1);
    EventMouseMotion* motion = ecs_term(it, EventMouseMotion, 2);
    Transform2D* transform = ecs_term(it, Transform2D, 3);

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
    Camera* camera = ecs_term(it, Camera, 1);
    EventMouseMotion* motion = ecs_term(it, EventMouseMotion, 2);
    int state = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE);
    if (state == GLFW_PRESS)
    {
        glm_vec2_add(camera->targetPos, motion->delta, camera->targetPos);
    }
}

void UnGrab(ecs_iter_t* it)
{
    EventMouseButton* mouse = ecs_term(it, EventMouseButton, 1);

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
    NanoVG* nano = ecs_term(it, NanoVG, 1);
    DragSelector* selector = ecs_term(it, DragSelector, 2);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);

    nvgBeginPath(nano->vg);
    nvgRect(nano->vg, selector->x, selector->y, selector->w, 1);
    nvgRect(nano->vg, selector->x, selector->y, 1, selector->h);
    nvgRect(nano->vg, selector->x, selector->y + selector->h, selector->w, 1);
    nvgRect(nano->vg, selector->x + selector->w, selector->y, 1, selector->h);
    nvgFillColor(nano->vg, nvgRGBA(34, 224, 107, 255));
    nvgFill(nano->vg);
    nvgClosePath(nano->vg);

    nvgBeginPath(nano->vg);
    nvgRect(nano->vg, selector->x, selector->y, selector->w, selector->h);
    nvgFillColor(nano->vg, nvgRGBA(34, 224, 107, 4));
    nvgFill(nano->vg);
    nvgClosePath(nano->vg);
    // glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
}

void MoveDragSelector(ecs_iter_t* it)
{
    DragSelector* selector = ecs_term(it, DragSelector, 1);
    EventMouseMotion* event = ecs_term(it, EventMouseMotion, 2);

    selector->w = event->pos[0] - selector->x;
    selector->h = event->pos[1] - selector->y;
}

void ScaleVisualSymbol(ecs_iter_t* it)
{
    EventMouseButton* event = ecs_term(it, EventMouseButton, 1);
    Transform2D* transform = ecs_term(it, Transform2D, 2);
    Texture2D* texture = ecs_term(it, Texture2D, 3);
}

void MouseStartAction(ecs_iter_t* it)
{
    EventMouseButton* event = ecs_term(it, EventMouseButton, 1);
    ActionOnMouseInput* action = ecs_term(it, ActionOnMouseInput, 2);

    if (event->action == GLFW_PRESS && event->button == GLFW_MOUSE_BUTTON_LEFT)
    {
        printf("Mouse start\n");
        action->active = true;
    }
}

void MouseAffectAction(ecs_iter_t* it)
{
    EventMouseMotion* event = ecs_term(it, EventMouseMotion, 1);
    ActionOnMouseInput* action = ecs_term(it, ActionOnMouseInput, 2);
    Camera* camera = ecs_term(it, Camera, 3);
    Transform2D* transform = ecs_term(it, Transform2D, 4);
    Texture2D* texture = ecs_term(it, Texture2D, 5);

    for (int32_t i = 0; i < it->count; i++)
    {
        if (it->entities[i] == action->symbol)
        {
            vec2 cursorWorldPos;
            screen_to_world(camera->view, event->pos, cursorWorldPos);
            vec2 diff;

            if (action->op == SCALE_LOWER_RIGHT)
            {
                glm_vec2_sub(cursorWorldPos, transform[i].pos, diff);
                vec2 ratio = {diff[0]/texture[i].width, diff[1]/texture[i].height};

                float greater = c2Max(1.0/ratio[0], 1.0/ratio[1]);
                texture[i].scale[0] = greater;
                texture[i].scale[1] = greater;
            }
            else if (action->op == SCALE_LOWER_LEFT)
            {
                vec2 opposite = {action->origin[0] + texture[i].width/action->startScale[0], action->origin[1]};
                diff[0] = cursorWorldPos[0] - (opposite[0]);
                diff[1] = cursorWorldPos[1] - transform[i].pos[1];
                vec2 ratio = {diff[0]/texture[i].width, diff[1]/texture[i].height};

                float greater = c2Max(-1.0/ratio[0], 1.0/ratio[1]);
                texture[i].scale[0] = greater;
                texture[i].scale[1] = greater;
                transform[i].pos[0] = opposite[0] - texture[i].width/greater;
            }
            else if (action->op == SCALE_UPPER_RIGHT)
            {
                vec2 opposite = {action->origin[0], action->origin[1] + texture[i].height/action->startScale[1]};
                diff[0] = cursorWorldPos[0] - transform[i].pos[0];
                diff[1] = cursorWorldPos[1] - opposite[1];
                vec2 ratio = {diff[0]/texture[i].width, diff[1]/texture[i].height};

                float greater = c2Max(1.0/ratio[0], 1.0/ratio[1]);
                texture[i].scale[0] = greater;
                texture[i].scale[1] = greater;
                transform[i].pos[1] = opposite[1] - texture[i].height/greater;
            }
            else if (action->op == SCALE_UPPER_LEFT)
            {
                vec2 opposite = {action->origin[0] + texture[i].width/action->startScale[0], action->origin[1] + texture[i].height/action->startScale[1]};
                diff[0] = cursorWorldPos[0] - opposite[0];
                diff[1] = cursorWorldPos[1] - opposite[1];
                vec2 ratio = {-diff[0]/texture[i].width, -diff[1]/texture[i].height};

                float greater = c2Max(1.0/ratio[0], 1.0/ratio[1]);
                texture[i].scale[0] = greater;
                texture[i].scale[1] = greater;
                transform[i].pos[0] = opposite[0] - texture[i].width/greater;
                transform[i].pos[1] = opposite[1] - texture[i].height/greater;
            }
        }
    }
}

void MouseEndAction(ecs_iter_t* it)
{
    EventMouseButton* event = ecs_term(it, EventMouseButton, 1);
    ActionOnMouseInput* action = ecs_term(it, ActionOnMouseInput, 2);

    printf("Action %d", event->action);
    if (event->action == GLFW_RELEASE)
    {
        printf("Mouse end\n");
        ecs_remove(it->world, input, ActionOnMouseInput);
    }
}

void UpdateCursorAction(ecs_iter_t* it)
{
    Camera* camera = ecs_term(it, Camera, 1);
    EventMouseMotion* event = ecs_term(it, EventMouseMotion, 2);
    CircleActionIndicator* indicator = ecs_term(it, CircleActionIndicator, 3);
    Transform2D* transform = ecs_term(it, Transform2D, 4);
    Texture2D* texture = ecs_term(it, Texture2D, 5);

    vec2 worldPos;
    screen_to_world(camera->view, event->pos, worldPos);
    c2v cursorWorldPos = {worldPos[0], worldPos[1]};
    for (int32_t i = 0; i < it->count; i++)
    {
        vec2 transformScreenPos;
        world_to_screen(camera->view, transform[i].pos, transformScreenPos);
        glm_vec2_add(transformScreenPos, indicator[i].screenOffset, transformScreenPos);
        vec2 transformWorldPos;
        screen_to_world(camera->view, transformScreenPos, transformWorldPos);
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
            ecs_set(it->world, input, ActionOnMouseInput, {false, indicator->symbol, indicator[i].op, {transform[i].pos[0], transform[i].pos[1]}, {texture->scale[0], texture->scale[1]}});
        }
    }
}

void select_visual_symbol(ecs_world_t* world, ecs_entity_t* entity)
{
    Texture2D* selected = ecs_get(world, entity, Texture2D);
    ecs_entity_t indicators[4];
    const int cursorTypes[4] = {GLFW_RESIZE_NWSE_CURSOR, GLFW_RESIZE_NESW_CURSOR, GLFW_RESIZE_NWSE_CURSOR, GLFW_RESIZE_NESW_CURSOR};
    const c2v coords[4] = {{0, 0}, {selected->width, 0}, {selected->width, selected->height}, {0, selected->height}};
    vec2 screenOffsets[4] = {{-6, -6}, {6, -6}, {6, 6}, {-6, 6}};
    vec2 anchors[4] = {{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}};
    for (size_t i = 0; i < 4; i++)
    {
        indicators[i] = ecs_new_id(world);
        c2Circle bounds = {coords[i], 6};
        size_t b = c2Min((size_t)coords[i].x, selected->width - 1) * 4 + (c2Min((size_t)coords[i].y, selected->height - 1) * selected->pow_w * 4);
	    b = 0;
	    // printf("(%d, %d)\n", selected->width, selected->height);
	    // b = (selected->width - 1) * 4 + (selected->height - 1) * selected->width;
	    SDL_LockSurface(selected->img);
	    unsigned char* pixels = (unsigned char*) selected->img->pixels;
	    NVGcolor color = nvgRGBA(pixels[b], pixels[b + 1], pixels[b + 2], 255);
	    SDL_UnlockSurface(selected->img);
        ecs_set(world, indicators[i], CircleActionIndicator, {bounds, cursorTypes[i], i, {screenOffsets[i][0], screenOffsets[i][1]}, entity, color});
        ecs_add(world, indicators[i], Transform2D);
        // ecs_set(world, indicators[i], Local2D, {coords[i].x, coords[i].y});
        ecs_set(world, indicators[i], Anchor, {anchors[i][0], anchors[i][1]});
        ecs_add_pair(world, indicators[i], EcsChildOf, entity);
    }
    ecs_set(world, entity, Selected, {indicators});
}

void SaveProjectShortcut(ecs_iter_t* it)
{
    EventKey* event = ecs_term(it, EventKey, 1);
    if (event->key == GLFW_KEY_S && event->mods & GLFW_MOD_CONTROL)
    {
        printf("Save!\n");
        ecs_entity_t t = ecs_struct_init(it->world, &(ecs_struct_desc_t) {
            .entity.name = "T",
            .members = {
                "path", {ecs_id(ecs_string_t)}
            }
        });
        ecs_query_t* query = ecs_query_new(world, "Transform2D, LocalFile");
        ecs_iter_t qIt = ecs_query_iter(world, query);

        size_t visualSymbolCount = 0;
        while (ecs_query_next(&qIt))
        {
            Transform2D* transform = ecs_term(&qIt, Transform2D, 1);
            LocalFile* file = ecs_term(&qIt, LocalFile, 2);
            for (int32_t i = 0; i < qIt.count; i++)
            {
                printf("%s", file[i].path);
                LocalFile value = {"Test"};
                char *expr = ecs_ptr_to_json(qIt.world, t, &file[i]);
                // char* expr = ecs_ptr_to_json(qIt.world, qIt.entities[i], &file[i]);
                printf("%s\n", expr);
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
    Transform2D* parent = ecs_term(it, Transform2D, 1);
    Texture2D* texture = ecs_term(it, Texture2D, 2);
    Transform2D* transform = ecs_term(it, Transform2D, 3);
    Anchor* anchor = ecs_term(it, Anchor, 4);

    if (parent)
    {
        // printf("Let's go %d!\n", it->count);
        for (int32_t i = 0; i < it->count; i++)
        {
            transform[i].pos[0] = parent->pos[0] + anchor[i].horizontal * texture->width / texture->scale[0];
            transform[i].pos[1] = parent->pos[1] + anchor[i].vertical * texture->height / texture->scale[1];
        }
    }
}

void TransformCascadeHierarchy(ecs_iter_t* it)
{
    Transform2D* parent = ecs_term(it, Transform2D, 1);
    Transform2D* transform = ecs_term(it, Transform2D, 2);
    Local2D* local = ecs_term(it, Local2D, 3);

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
    Camera* camera = ecs_term(it, Camera, 1);
    EventMouseButton* event = ecs_term(it, EventMouseButton, 2);
    if (event->action == GLFW_PRESS && event->button == GLFW_MOUSE_BUTTON_RIGHT)
    {
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
            Transform2D* transform = ecs_term(&qIt, Transform2D, 1);
            Texture2D* texture = ecs_term(&qIt, Texture2D, 2);
            LocalFile* file = ecs_term(&qIt, LocalFile, 3);

            for (int32_t i = 0; i < qIt.count; i++)
            {
                vec2 dist = {0.0f, 0.0f};
                glm_vec2_sub(worldPos, transform[i].pos, dist);
                printf("Check within\n");
                if (dist[0] > 0.0 && dist[0] <= texture[i].width / texture[i].scale[0] &&
                dist[1] > 0.0 && dist[1] <= texture[i].height / texture[i].scale[1])
                {
                    indication = qIt.entities[i];
                    char command[2048];
                    sprintf(command, "xdg-open \"%s\"", file[i].path);
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

void SelectVisualSymbolQuery(ecs_iter_t* it)
{
    // printf("Select visual symbol\n");
    Camera* camera = ecs_term(it, Camera, 1);
    EventMouseButton* event = ecs_term(it, EventMouseButton, 2);
    DragSelector* selector = NULL;
    if (ecs_has(it->world, input, DragSelector))
    {
        selector = ecs_get_mut(it->world, input, DragSelector, NULL);
    };
    bool isDragSelected = selector != NULL;

    ecs_defer_begin(it->world);
    if (event->button == GLFW_MOUSE_BUTTON_LEFT)
    {
        ecs_query_t* query = ecs_query_new(world, "Transform2D, Texture2D"); // TODO: Store?
        ecs_iter_t qIt = ecs_query_iter(it->world, query);

        size_t visualSymbolCount = 0;
        while (ecs_query_next(&qIt))
        {
            visualSymbolCount += qIt.count;
        }
        ecs_entity_t curSelectedNodes[visualSymbolCount];
        size_t curSelectedCount = 0;
        ecs_entity_t toSelectNodes[visualSymbolCount];
        size_t toSelectCount = 0;
        qIt = ecs_query_iter(it->world, query);

        if (isDragSelected && event->action == GLFW_RELEASE)
        {
            while (ecs_query_next(&qIt))
            {
                Transform2D* transform = ecs_term(&qIt, Transform2D, 1);
                Texture2D* texture = ecs_term(&qIt, Texture2D, 2);

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
                    c2AABB visualSymbolBounds = {{transform[i].pos[0], transform[i].pos[1]}, {transform[i].pos[0] + texture[i].width / texture[i].scale[0], transform[i].pos[1] + texture[i].height / texture[i].scale[0]}};
                    bool selectorOverlaps = c2AABBtoAABB(selectorBox, visualSymbolBounds);

                    if (selectorOverlaps)
                    {
                        select_visual_symbol(it->world, qIt.entities[i]);
                    }
                }

            }
            ecs_remove(it->world, input, DragSelector);
        }
        if (event->action == GLFW_PRESS)
        {
            printf("Select query start %f", it->world_time);
            double xpos, ypos;
            glfwGetCursorPos(event->window, &xpos, &ypos);
            vec2 screenPos = {xpos, ypos};
            vec2 worldPos;
            screen_to_world(camera->view, screenPos, worldPos);
            int right = glfwGetKey(event->window, GLFW_KEY_RIGHT_SHIFT);
            int left = glfwGetKey(event->window, GLFW_KEY_LEFT_SHIFT);

            bool selectAdd = false;
            bool grabSelected = false;
            while (ecs_query_next(&qIt))
            {
                Transform2D* transform = ecs_term(&qIt, Transform2D, 1);
                Texture2D* texture = ecs_term(&qIt, Texture2D, 2);

                if (right == GLFW_PRESS || left == GLFW_PRESS)
                {
                    selectAdd = true;
                }
                for (int32_t i = 0; i < qIt.count; i++)
                {
                    vec2 dist = {0.0f, 0.0f};
                    glm_vec2_sub(worldPos, transform[i].pos, dist);
                    bool isSelected = ecs_has(qIt.world, qIt.entities[i], Selected);
                    if (isSelected)
                    {
                        curSelectedNodes[curSelectedCount] = qIt.entities[i];
                        curSelectedCount++;
                    }
                    if (dist[0] > 0.0 && dist[0] <= texture[i].width / texture[i].scale[0] &&
                    dist[1] > 0.0 && dist[1] <= texture[i].height / texture[i].scale[1])
                    {
                        if (isSelected)
                        {
                            grabSelected = true;
                        }
                        toSelectNodes[toSelectCount] = qIt.entities[i];
                        toSelectCount++;
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
                    // Deselect nodes
                    for (int32_t i = 0; i < curSelectedCount; i++)
                    {
                        // ecs_term_iter is not correct! Need to use something else to create the iter
                        ecs_iter_t children = ecs_term_iter(it->world, &(ecs_term_t){.id = ecs_childof(curSelectedNodes[i])});
                        ecs_defer_begin(children.world);
                        while (ecs_term_next(&children)) {
                            for (int c = 0; c < children.count; c++) {
                                ecs_delete(children.world, children.entities[c]);
                            }
                        }
                        ecs_defer_end(children.world);
                        ecs_remove(it->world, curSelectedNodes[i], Selected);
                    }
                    if (toSelectCount > 0)
                    {
                        add_command_on_grab(it, &buffer);
                        select_visual_symbol(it->world, toSelectNodes[0]);
                        ecs_add(it->world, toSelectNodes[0], Grabbed);
                    } else
                    {
                        ecs_set(it->world, input, DragSelector, {xpos, ypos, 0, 0});
                    }
                }
            }
            printf(", Select query end %f\n", it->world_time);
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
    Camera* camera = ecs_term(it, Camera, 1);
    EventScroll* scroll = ecs_term(it, EventScroll, 2);

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
    NanoVG* nano = ecs_term(it, NanoVG, 1);
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
    NanoVG* nano = ecs_term(it, NanoVG, 1);
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
    Camera* camera = ecs_term(it, Camera, 1);
    float p = fmin(1.0, it->delta_time/camera->interpTime);
    camera->scale = interp(camera->scale, camera->targetScale, p);
    camera->pos[0] = interp(camera->pos[0], camera->targetPos[0], p);
    camera->pos[1] = interp(camera->pos[1], camera->targetPos[1], p);
}

void RenderSelectionIndicators(ecs_iter_t* it)
{
    NanoVG* nano = ecs_term(it, NanoVG, 1);
    Camera* camera = ecs_term(it, Camera, 2);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);

    nvgBeginPath(nano->vg);
    float radius = 6;

    for (int32_t i = 0; i < it->count; i++)
    {
        Transform2D* transform = ecs_term(it, Transform2D, 3);
        Texture2D* texture = ecs_term(it, Texture2D, 4);
        vec2 upperLeft = {transform[i].pos[0], transform[i].pos[1]};
        world_to_screen(camera->view, upperLeft, upperLeft);
        vec2 upperRight = {transform[i].pos[0] + texture[i].width / texture[i].scale[0], transform[i].pos[1]};
        world_to_screen(camera->view, upperRight, upperRight);
        vec2 lowerLeft = {transform[i].pos[0], transform[i].pos[1] + texture[i].height / texture[i].scale[1]};
        world_to_screen(camera->view, lowerLeft, lowerLeft);
        vec2 lowerRight = {transform[i].pos[0] + texture[i].width / texture[i].scale[0], transform[i].pos[1] + texture[i].height / texture[i].scale[1]};
        world_to_screen(camera->view, lowerRight, lowerRight);

        float w = upperRight[0] - upperLeft[0];
        float h = lowerLeft[1] - upperLeft[1];

        nvgRect(nano->vg, upperLeft[0] - radius, upperLeft[1] - radius, 1, h + radius*2);
        nvgRect(nano->vg, upperLeft[0] - radius, upperLeft[1] - radius, w + radius*2, 1);
        nvgRect(nano->vg, lowerRight[0] + radius, lowerRight[1] + radius, 1, -h - radius*2);
        nvgRect(nano->vg, lowerRight[0] + radius, lowerRight[1] + radius, -w - radius*2, 1);

        nvgRect(nano->vg, upperLeft[0] - radius - 1, upperLeft[1] - radius, 3, h/4 + radius*2);
        nvgRect(nano->vg, upperLeft[0] - radius, upperLeft[1] - radius - 1, w/4 + radius*2, 3);

        nvgRect(nano->vg, lowerRight[0] + radius - 1, lowerRight[1] + radius, 3, -h/4 - radius*2);
        nvgRect(nano->vg, lowerRight[0] + radius, lowerRight[1] + radius - 1, -w/4 - radius*2, 3);

        nvgRect(nano->vg, upperRight[0] + radius - 1, upperRight[1] - radius, 3, h/4 + radius*2);
        nvgRect(nano->vg, upperRight[0] + radius, upperRight[1] - radius - 1, -w/4 - radius*2, 3);

        nvgRect(nano->vg, lowerLeft[0] - radius - 1, lowerLeft[1] + radius, 3, -h/4 - radius*2);
        nvgRect(nano->vg, lowerLeft[0] - radius, lowerLeft[1] + radius - 1, w/4 + radius*2, 3);

        // nvgCircle(nano->vg, upperLeft[0] - radius, upperLeft[1] - radius, radius);
        // nvgCircle(nano->vg, upperRight[0] + radius, upperRight[1] - radius, radius);
        // nvgCircle(nano->vg, lowerLeft[0] - radius, lowerLeft[1] + radius, radius);
        // nvgCircle(nano->vg, lowerRight[0] + radius, lowerRight[1] + radius, radius);
    }
    nvgFillColor(nano->vg, nvgRGBA(157, 3, 252,255));
    nvgFill(nano->vg);
    nvgClosePath(nano->vg);
    // glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
}

void RenderImageSelectionIndicators(ecs_iter_t* it)
{
    Camera* camera = ecs_term(it, Camera, 1);
    BatchSpriteRenderer* renderer = ecs_term(it, BatchSpriteRenderer, 2);
    Transform2D* transform = ecs_term(it, Transform2D, 3);
    Texture2D* texture = ecs_term(it, Texture2D, 4);

    glUseProgram(renderer->shader.programId);
    mat4 identity;
    glm_mat4_identity(identity);
    glUniformMatrix4fv(glGetUniformLocation(renderer->shader.programId, "view"), 1, false, identity);
    mat4 proj;
    int wwidth, wheight;
    vec4 box = {0.0, 0.0, 1.0, 1.0};
    glUniform4f(glGetUniformLocation(renderer->shader.programId, "box"), box[0], box[1], box[2], box[3]);
    glfwGetWindowSize(window, &wwidth, &wheight);
    glm_ortho(0.0, wwidth, wheight, 0.0, -1.0, 10.0, proj); // TODO: Window component
    glUniformMatrix4fv(glGetUniformLocation(renderer->shader.programId, "projection"), 1, false, proj[0]);


    for (int i = 0; i < it->count; i++)
    {
        const c2v coords[4] = {{0, 0}, {texture[i].width / texture[i].scale[0], 0}, {0, texture[i].height / texture[i].scale[1]}, {0, 0}};
        vec2 screenOffsets[4] = {{0, -6}, {6, 0}, {0, 6}, {-6, 0}};
        const c2v scales[4] = {{camera->scale / texture[i].scale[0], 1.0/texture[i].height}, {1.0/texture[i].width, camera->scale / texture[i].scale[1]}, {camera->scale / texture[i].scale[0], 1.0/texture[i].height}, {1.0/texture[i].width, camera->scale / texture[i].scale[1]}};
        float widthRatio = texture[i].width/(float)texture[i].pow_w;
        const vec4 boxes[4] = {{0.0, 0.0, 1.0, 1.0/texture[i].height},
        {texture[i].width/(float)texture[i].pow_w - 1.0/texture[i].pow_w, 0.0, 1.0/texture[i].width, 1.0},
        {0.0, texture[i].height/(float)texture[i].pow_h - 1.0/texture[i].pow_h, 1.0, 1.0/texture[i].height},
        {0.0, 0.0, 1.0/texture[i].width, 1.0}};

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture[i].id);
        glBindVertexArray(renderer->quadVAO);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, renderer->indexBuffer);
        for (int e = 0; e < 4; e++)
        {
            mat4 model;
            glm_mat4_identity(model);
            vec2 offset = {transform[i].pos[0] + coords[e].x, transform[i].pos[1] + coords[e].y, 0.0f};
            vec2 screen;
            world_to_screen(camera->view, offset, screen);
            glm_vec2_add(screen, screenOffsets[e], screen);
            glm_translate(model, screen);
            vec3 scale = {texture[i].pow_w  * scales[e].x, texture[i].pow_h * scales[e].y, 1.0};
            glm_scale(model, scale);
            glUniformMatrix4fv(glGetUniformLocation(renderer->shader.programId, "model"), 1, false, model[0]);
            // vec4 box = {10.0, 0.0, texture->surface->w, texture->surface->h};
            glUniform4f(glGetUniformLocation(renderer->shader.programId, "box"), boxes[e][0], boxes[e][1], boxes[e][2], boxes[e][3]);
            glUniform2f(glGetUniformLocation(renderer->shader.programId, "scale"), 1.0, 1.0);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, (void*) 0);
        }
        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
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
    SavedData* data = ecs_term(it, SavedData, 2);
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
        ecs_entity_t node = load_visual_symbol(it->world, buf);
        if (ecs_is_valid(it->world, node))
        {
            test[symbol_count] = node;
            symbol_count++;
        }
        memset(&buf[index], 0, sizeof(next->d_name));
    }
    closedir(dir);
    
    ecs_delete(it->world, it->entities[0]);
}

void RenderActionIndicators(ecs_iter_t* it)
{
    NanoVG* nano = ecs_term(it, NanoVG, 1);
    Camera* camera = ecs_term(it, Camera, 2);
    Transform2D* transform = ecs_term(it, Transform2D, 3);
    CircleActionIndicator* indicator = ecs_term(it, CircleActionIndicator, 4);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);

    float radius = 6;

    for (int32_t i = 0; i < it->count; i++)
    {
        nvgBeginPath(nano->vg);
        vec2 screenPos;
        world_to_screen(camera->view, transform[i].pos, screenPos);

        nvgCircle(nano->vg, screenPos[0] + indicator[i].screenOffset[0], screenPos[1] + indicator[i].screenOffset[1], radius + 1);
        NVGcolor inverted = nvgRGBAf(1.0 - indicator[i].color.r, 1.0 - indicator[i].color.g, 1.0 - indicator[i].color.b, 1.0);
        float avg = (inverted.r + inverted.g + inverted.b)/3;
        NVGcolor greyscale = nvgRGBAf(avg, avg, avg, 1.0);

        float indAvg = (indicator->color.r + indicator->color.g + indicator->color.b)/3;
        bool invertOutline = indAvg > avg;

        if (invertOutline)
        {
            nvgFillColor(nano->vg, indicator[i].color);
        }
        else
        {
            nvgFillColor(nano->vg, greyscale);
        }
        nvgFill(nano->vg);
        nvgClosePath(nano->vg);

        nvgBeginPath(nano->vg);
        nvgCircle(nano->vg, screenPos[0] + indicator[i].screenOffset[0], screenPos[1] + indicator[i].screenOffset[1], radius);
        // nvgPathWinding(nano->vg, NVG_HOLE);
        nvgCircle(nano->vg, screenPos[0] + indicator[i].screenOffset[0], screenPos[1] + indicator[i].screenOffset[1], radius);
        if (invertOutline)
        {
            nvgFillColor(nano->vg, greyscale);
        }
        else
        {
            nvgFillColor(nano->vg, indicator[i].color);
        }
        // nvgFillColor(nano->vg, nvgRGBA(157, 3, 252,255));
        nvgFill(nano->vg);

        nvgClosePath(nano->vg);
    }
    // glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
}

double lastXPos, lastYPos;
void cursor_position_callback(GLFWwindow* window, double xpos, double ypos)
{
    bool isAdded = false;
    bool exists = ecs_has(world, input, EventMouseMotion);
    if (exists)
    {
        EventMouseMotion* motion = ecs_get_mut(world, input, EventMouseMotion, NULL);
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
    ECS_COMPONENT_DEFINE(world, EventDropFiles);
    ECS_COMPONENT_DEFINE(world, DragSelector);
    ECS_COMPONENT_DEFINE(world, NineSlice);
    ECS_COMPONENT_DEFINE(world, EventKey);
    ECS_COMPONENT_DEFINE(world, NanoVG);
    ECS_COMPONENT_DEFINE(world, CircleActionIndicator);
    ECS_COMPONENT_DEFINE(world, Selected);
    ECS_COMPONENT_DEFINE(world, Local2D);
    ECS_COMPONENT_DEFINE(world, ActionOnMouseInput);
    ECS_COMPONENT_DEFINE(world, Anchor);
    ECS_COMPONENT_DEFINE(world, SavedData);
    ECS_TAG_DEFINE(world, Grabbed);
    ECS_TAG_DEFINE(world, DragHover);
    ECS_TAG_DEFINE(world, TakeSnapshot);
    ECS_TAG_DEFINE(world, BrowseDirectory);

    // ECS_IMPORT(world, InputModule);
    input = ecs_set_name(world, 0, "input");

    ECS_OBSERVER(world, SetupBatchRenderer, EcsOnSet, BatchSpriteRenderer);
    ECS_OBSERVER(world, SetupCamera, EcsOnSet, Camera);
    ECS_OBSERVER(world, SetInitialMultitexture, EcsOnSet, Texture2D, MultiTexture2D);
    ECS_OBSERVER(world, deallocate_texture, EcsOnRemove, Texture2D);
    ECS_OBSERVER(world, LoadDroppedFiles, EcsOnSet, EventDropFiles);
    ECS_OBSERVER(world, ApparateVisualSymbols, EcsOnSet, BrowseDirectory, SavedData, [inout] *());

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
    window = glfwCreateWindow(640, 480, "Gui Gal ???????????", NULL, NULL);
    glfwMakeContextCurrent(window);
    gladLoadGL();
    glfwSwapInterval(0);
    glfwSetKeyCallback(window, key_callback);
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

    if (argc > 1)
    {
        ecs_entity_t nav_command = ecs_new_id(world);
        ecs_add(world, nav_command, Transform2D);
        ecs_add(world, nav_command, BrowseDirectory);
        ecs_set(world, nav_command, SavedData, {argv[1]});
    }

    ECS_SYSTEM(world, ResetCursor, EcsPreFrame, EventMouseMotion);
    ECS_SYSTEM(world, AnimateGif, EcsPreUpdate, GifAnimator, Texture2D, MultiTexture2D);
    ECS_SYSTEM(world, RenderSprites, EcsOnUpdate, Camera(renderer), BatchSpriteRenderer(renderer), Transform2D, Texture2D);
    //ECS_SYSTEM(world, RenderNineSlices, EcsOnUpdate, renderer:Camera, renderer:BatchSpriteRenderer, Transform2D, Texture2D, NineSlice);
    ECS_SYSTEM(world, GrabMoveCamera, EcsPreUpdate, Camera(renderer), EventMouseMotion(input));
    ECS_SYSTEM(world, ScrollZoomCamera, EcsPreUpdate, Camera(renderer), EventScroll(input));
    ECS_SYSTEM(world, CameraCalculateView, EcsOnUpdate, Camera);
    ECS_SYSTEM(world, SelectVisualSymbolQuery, EcsPreUpdate, Camera(renderer), EventMouseButton(input), [inout] *());
    ECS_SYSTEM(world, OpenSymbolPath, EcsPreUpdate, Camera(renderer), EventMouseButton(input), !ActionOnMouseInput(input), [inout] *());
    ECS_SYSTEM(world, MoveGrabbedTransforms, EcsPreFrame, Camera(renderer), EventMouseMotion(input), Transform2D, Grabbed);
    ECS_SYSTEM(world, ConsumeEvents, EcsPostFrame, (ConsumeEvent, *));
    ECS_SYSTEM(world, UnGrab, EcsOnUpdate, EventMouseButton(input), Grabbed);
    ECS_SYSTEM(world, DeleteSelected, EcsOnUpdate, EventKey(input), Selected)
    ECS_SYSTEM(world, StartNanoVGFrame, EcsPreFrame, NanoVG(renderer));
    ECS_SYSTEM(world, EndNanoVGFrame, EcsPostUpdate, NanoVG(renderer));
    ECS_SYSTEM(world, RenderDragHover, EcsPostUpdate, NanoVG(renderer), DragHover(input));
    ECS_SYSTEM(world, MoveDragSelector, EcsOnUpdate, DragSelector(input), EventMouseMotion(input));
    ECS_SYSTEM(world, UndoCommand, EcsOnUpdate, EventKey(input));
    ECS_SYSTEM(world, SaveProjectShortcut, EcsPostUpdate, EventKey(input), [inout] *());
    ECS_SYSTEM(world, InterpolateCamera, EcsPostUpdate, Camera);

    ECS_SYSTEM(world, AnchorPropagate, EcsPreUpdate, ?Transform2D(parent|cascade), Texture2D(parent), Transform2D, Anchor);

    ECS_SYSTEM(world, RenderActionIndicators, EcsPostUpdate, NanoVG(renderer), Camera(renderer), Transform2D, CircleActionIndicator);
    ECS_SYSTEM(world, RenderImageSelectionIndicators, EcsPostUpdate, Camera(renderer), BatchSpriteRenderer(renderer), Transform2D, Texture2D, Selected);
    // ECS_SYSTEM(world, RenderSelectionIndicators, EcsPostUpdate, renderer:NanoVG, renderer:Camera, Transform2D, Texture2D, Selected);

    ECS_SYSTEM(world, RenderDragSelector, EcsPostUpdate, NanoVG(renderer), DragSelector(input));
    ECS_SYSTEM(world, LoadClipboardFiles, EcsOnUpdate, EventMouseButton(input));
    ECS_SYSTEM(world, UpdateCursorAction, EcsOnUpdate, Camera(renderer), EventMouseMotion(input), CircleActionIndicator, Transform2D, Texture2D(parent), !ActionOnMouseInput(input));
    ECS_SYSTEM(world, TransformCascadeHierarchy, EcsPreFrame, ?Transform2D(parent|cascade), Transform2D, Local2D);
// PARENT:Transform2D,
    ECS_SYSTEM(world, MouseStartAction, EcsOnUpdate, EventMouseButton, ActionOnMouseInput);
    ECS_SYSTEM(world, MouseEndAction, EcsPreFrame, EventMouseButton, ActionOnMouseInput);
    ECS_SYSTEM(world, MouseAffectAction, EcsOnUpdate, EventMouseMotion(input), ActionOnMouseInput(input), Camera(renderer), Transform2D, Texture2D);

    glfwShowWindow(window);
    
    while (!glfwWindowShouldClose(window))
    {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        glfwPollEvents();
        ecs_progress(world, 0);
        if (ecs_has(world, input, TakeSnapshot))
        {
            ecs_remove(world, input, TakeSnapshot);
            // Create filter from just the grabbed transforms!

            ecs_filter_t filter;
            ecs_filter_init(world, &filter, &(ecs_filter_desc_t) {
                .terms = {{ ecs_id(Selected) } }
            });
            ecs_iter_t change = ecs_filter_iter(world, &filter);
            ecs_snapshot_t *s = ecs_snapshot_take_w_iter(&change, ecs_filter_next);
            // ecs_snapshot_t *s = ecs_snapshot_take(world);
            push_command(&buffer, s);
        }
        while (buffer.time_step < 0)
        {
            undo_command(world, &buffer);
        }
        glfwSwapBuffers(window);
    }
    glfwDestroyWindow(window);
    glfwTerminate();
    free(buffer.snapshots);
    ecs_fini(world);
    // nvgDeleteGL3(vg);
    return 0;
}
