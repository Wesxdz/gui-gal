#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <flecs/flecs.h>
#include <SDL2/SDL_image.h>
#include <string.h>
#include <gif_lib.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "components.h"

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
        vec3 scale = {texture[i].pow_w, texture[i].pow_h, 1.0};
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

void create_texture(const char* file, ecs_entity_t entity, unsigned int* twidth, unsigned int* theight)
{
    GLuint id;
    SDL_Surface* img = IMG_Load(file);
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
    SDL_FreeSurface(img);
    SDL_FreeSurface(img_rgba8888);
    ecs_set(world, entity, Texture2D, {id, pow_w, pow_h, img->w, img->h});
    *twidth = img->w; *theight = img->h;
}

void create_multitexture_from_gif(GifFileType* gif, ecs_entity_t entity)
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
        SDL_Surface *img_rgba8888 = SDL_CreateRGBSurfaceFrom(pixelBuffer, gif->SWidth, gif->SHeight, 
        bpp, 4, Rmask, Gmask, Bmask, Amask);
        SDL_SetSurfaceAlphaMod(img_rgba8888, 0xFF);
        SDL_SetSurfaceBlendMode(img_rgba8888, SDL_BLENDMODE_NONE);
        glBindTexture(GL_TEXTURE_2D, ids[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, pow_w, pow_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, clearSurface);
        // (pow_w - gif->SWidth)/2.0 (pow_h - gif->SHeight)/2.0
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0.0, 0.0, gif->SWidth, gif->SHeight, GL_RGBA, GL_UNSIGNED_BYTE, img_rgba8888->pixels);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
        glTexParameteri ( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri ( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        memmove(pixelBuffer2, pixelBuffer1, bufferSize);
        memmove(pixelBuffer1, pixelBuffer, bufferSize);
        shiftBufferCount += 2;
        prevSaved = &gif->SavedImages[i - 1];
        prevDesc = &prevSaved->ImageDesc;
        prevColorMap = prevDesc->ColorMap ? prevDesc->ColorMap : gif->SColorMap;
        SDL_FreeSurface(img_rgba8888);
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

void LoadDroppedFiles(ecs_iter_t* it)
{
    EventDropFiles* drop = ecs_term(it, EventDropFiles, 1);
    for (int i = 0; i < drop->count; i++)
    {
        // struct stat* buf = malloc(sizeof stat);
        // stat(paths[i], buf);
        //  = S_ISDIR(buf->st_mode);
         bool isDirectory = false;
        // free(buf);
        int error;
        GifFileType* gif = DGifOpenFileName(drop->paths[i], &error);
        bool isGif = gif != NULL;
        printf("%p:%d\n", (void*)gif, error);
        // printf("Is Gif? %d\n", isGif);
        printf("%p\n", drop->paths[i]);

        if (!isDirectory)
        {
            ecs_entity_t node = ecs_new_id(world);
            double xpos, ypos;
            glfwGetCursorPos(window, &xpos, &ypos);
            unsigned int twidth, theight;
            if (isGif)
            {
                DGifSlurp(gif); // TODO: Move logic to GifAnimator system
                printf("Gif has %d images!\n" ,gif->ImageCount);
                ecs_set(world, node, Texture2D, {NULL, nearest_pow2(gif->SWidth), nearest_pow2(gif->SHeight), gif->SWidth, gif->SHeight});
                create_multitexture_from_gif(gif, node);
                // printf("%d stbi frame count!\n", z);
                twidth = gif->SWidth; theight = gif->SHeight;
                if (gif->ImageCount > 1)
                {
                    ecs_set(world, node, GifAnimator, {gif, 24, 0.0, 0}); // data, delays, y, comp
                }
            } else
            {
                printf("Loading %s\n", drop->paths[i]);
                create_texture(drop->paths[i], node, &twidth, &theight);
            }
            const Camera* camera = ecs_get(world, renderer, Camera);
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
            glm_vec2_sub(screen, translate, position);
            ecs_set(world, node, Transform2D, {{position[0], position[1]}, 0.0f, 1.0f, 0});
            // ecs_set(world, node, LocalFile, {drop->paths[i]});
        }
    }
    free(drop->paths); // TODO: Move to another system and free unused paths
}

void drop_callback(GLFWwindow* window, int count, const char** paths)
{
    char** savedPaths = malloc(sizeof(char*) * count);
    for (int i = 0; i < count; i++)
    {
        size_t pathSize = sizeof(char*) * strlen(paths[i]);
        savedPaths[i] = malloc(pathSize);
        memcpy(savedPaths[i], paths[i], pathSize);
    }
    ecs_set(world, input, EventDropFiles, {window, count, savedPaths});
    ecs_set_pair(world, input, ConsumeEvent, ecs_id(EventDropFiles), {});

    // for (int i = 0; i < count; i++)
    // {
    //     // struct stat* buf = malloc(sizeof stat);
    //     // stat(paths[i], buf);
    //     //  = S_ISDIR(buf->st_mode);
    //      bool isDirectory = false;
    //     // free(buf);
    //     int error;
    //     GifFileType* gif = DGifOpenFileName(paths[i], &error);
    //     bool isGif = gif != NULL;
    //     printf("%p:%d\n", (void*)gif, error);
    //     // printf("Is Gif? %d\n", isGif);
    //     printf("%p\n", paths[i]);
    //     if (!isDirectory)
    //     {
    //         ecs_entity_t node = ecs_new_id(world);
    //         double xpos, ypos;
    //         glfwGetCursorPos(window, &xpos, &ypos);
    //         if (isGif)
    //         {
    //             DGifSlurp(gif); // TODO: Move logic to GifAnimator system
    //             printf("Gif has %d images!\n" ,gif->ImageCount);
    //             ecs_set(world, node, Texture2D, {NULL, nearest_pow2(gif->SWidth), nearest_pow2(gif->SHeight), gif->SWidth, gif->SHeight});
    //             create_multitexture_from_gif(gif, node);
    //             // printf("%d stbi frame count!\n", z);
    //             if (gif->ImageCount > 1)
    //             {
    //                 ecs_set(world, node, GifAnimator, {gif, 24, 0.0, 0}); // data, delays, y, comp
    //             }
    //         } else
    //         {
    //             printf("Loading %s\n", paths[i]);
    //             create_texture(paths[i], node, NULL, NULL);
    //         }
    //         const Texture2D* texture = ecs_get(world, node, Texture2D);
    //         const Camera* camera = ecs_get(world, renderer, Camera);
    //         vec4 t;
    //         mat4 r;
    //         vec3 s;
    //         glm_decompose(camera->view, t, r, s);
    //         vec3 inverse_s = {1.0/s[0], 1.0/s[1], 1.0/s[2]};
    //         xpos *= inverse_s[0];
    //         ypos *= inverse_s[1];
    //         vec2 translate;
    //         glm_vec2_copy(camera->view[3], translate);
    //         glm_vec2_mul(translate, inverse_s, translate);
    //         vec2 screen = {xpos - texture->width/2.0, ypos - texture->height/2};
    //         vec2 position;
    //         glm_vec2_sub(screen, translate, position);
    //         ecs_set(world, node, Transform2D, {{position[0], position[1]}, 0.0f, 1.0f, 0});
    //         // ecs_set(world, node, LocalFile, {drop->paths[i]});
    //     }
    // }
}

void ConsumeEvents(ecs_iter_t* it)
{
    ecs_entity_t pair = ecs_term_id(it, 1);
    ecs_entity_t comp = ecs_pair_object(it->world, pair);
    for (int32_t i = 0; i < it->count; i++)
    {
        ecs_remove_id(it->world, it->entities[i], comp);
        ecs_remove_id(it->world, it->entities[i], pair);
    }
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

void CameraCalculateView(ecs_iter_t* it)
{
    Camera* camera = ecs_term(it, Camera, 1);
    vec3 eye = {0.0f, 0.0f, 0.0};
    vec3 center = {0.0, 0.0, -1.0};
    vec3 up = {0.0, 1.0, 0.0};
    for (int32_t i = 0; i < it->count; i++)
    {
        glm_lookat(eye, center, up, camera[i].view);
        vec3 pos = {camera[i].pos[0], camera[i].pos[1], 0.0};
        glm_translate(camera[i].view, pos);
        vec3 scale = {camera[i].scale, camera[i].scale, 1.0};
        glm_scale(camera[i].view, scale);
    }
}

void MoveGrabbedTransforms(ecs_iter_t* it)
{
    Camera* camera = ecs_term(it, Camera, 1);
    EventMouseMotion* motion = ecs_term(it, EventMouseMotion, 2);
    Transform2D* transform = ecs_term(it, Transform2D, 3);
    for (int32_t i = 0; i < it->count; i++)
    {
        printf("Move grabbed!\n");
        vec2 worldPosLast;
        vec2 worldPosNow;
        vec2 motionPosLast;
        vec2 worldDelta;
        glm_vec2_sub(motion->pos, motion->delta, motionPosLast);
        screen_to_world(camera->view, motion->pos, worldPosNow);
        screen_to_world(camera->view, motionPosLast, worldPosLast);
        glm_vec2_sub(worldPosNow, worldPosLast, worldDelta);
        glm_vec2_add(transform->pos, worldDelta, transform->pos);
    }
}

void GrabMoveCamera(ecs_iter_t* it)
{
    Camera* camera = ecs_term(it, Camera, 1);
    EventMouseMotion* motion = ecs_term(it, EventMouseMotion, 2);
    int state = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE);
    if (state == GLFW_PRESS)
    {
        glm_vec2_add(camera->pos, motion->delta, camera->pos);
    }
}

void UnGrab(ecs_iter_t* it)
{
    EventMouseButton* mouse = ecs_term(it, EventMouseButton, 1);
    if (mouse->button == GLFW_MOUSE_BUTTON_LEFT && mouse->action == GLFW_RELEASE)
    {
        ecs_bulk_remove_type(it->world, ecs_type(Grabbed), &(ecs_filter_t){
            .include = ecs_type(Grabbed)
        });
    }
}

void SelectVisualSymbol(ecs_iter_t* it)
{
    Camera* camera = ecs_term(it, Camera, 1);
    EventMouseButton* event = ecs_term(it, EventMouseButton, 2);
    if (event->button == GLFW_MOUSE_BUTTON_LEFT && event->action == GLFW_PRESS)
    {
        Transform2D* transform = ecs_term(it, Transform2D, 3);
        Texture2D* texture = ecs_term(it, Texture2D, 4);
        
        double xpos, ypos;
        glfwGetCursorPos(event->window, &xpos, &ypos);
        vec2 screenPos = {xpos, ypos};
        vec2 worldPos;
        screen_to_world(camera->view, screenPos, worldPos);
        // remove Selected from all components
        int right = glfwGetKey(event->window, GLFW_KEY_RIGHT_SHIFT);
        int left = glfwGetKey(event->window, GLFW_KEY_LEFT_SHIFT);
        bool selectAdd = false;
        if (right == GLFW_PRESS || left == GLFW_PRESS)
        {
            selectAdd = true;
        }
        uint32_t potentialSelections[it->count];
        size_t selectedCount = 0;
        bool grabSelected = false;
        for (int32_t i = 0; i < it->count; i++)
        {
            vec2 dist = {0.0f, 0.0f};
            glm_vec2_sub(worldPos, transform[i].pos, dist);
            printf("Dist: (%f, %f)\n", dist[0], dist[1]);
            if (dist[0] > 0.0 && dist[0] <= texture[i].width &&
            dist[1] > 0.0 && dist[1] <= texture[i].height)
            {
                potentialSelections[selectedCount] = i;
                selectedCount++;
                printf("Selected visual symbol! (%f, %f)\n", dist[0], dist[1]);
            }
        }
        ecs_defer_begin(world);
        if (!selectAdd)
        {
            if (grabSelected)
            {
                ecs_bulk_remove_type(it->world, ecs_type(Selected), &(ecs_filter_t){
                    .include = ecs_type(Selected)
                });
            } else
            {
                ecs_bulk_remove_type(it->world, ecs_type(Selected), &(ecs_filter_t){
                    .include = ecs_type(Selected)
                });
            }
        }
        for (size_t i = 0; i < selectedCount; i++)
        {
            ecs_add(world, it->entities[potentialSelections[i]], Selected);
            ecs_add(world, it->entities[potentialSelections[i]], Grabbed);
        }
        ecs_defer_end(world);
    }
    
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
    printf("(%f, %f)\n", worldPos[0], worldPos[1]);
    world[0] = worldPos[0];
    world[1] = worldPos[1];
}

void register_components()
{

}

void ScrollZoomCamera(ecs_iter_t* it)
{
    Camera* camera = ecs_term(it, Camera, 1);
    EventScroll* scroll = ecs_term(it, EventScroll, 2);
    camera->scale = camera->scale + scroll->yoffset * 0.05;
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
    ECS_TAG_DEFINE(world, Selected);
    ECS_TAG_DEFINE(world, Grabbed);
    
    // ECS_IMPORT(world, InputModule);
    input = ecs_set_name(world, 0, "input");

    ECS_SYSTEM(world, SetupBatchRenderer, EcsOnSet, BatchSpriteRenderer);
    ECS_SYSTEM(world, SetupCamera, EcsOnSet, Camera);
    ECS_SYSTEM(world, SetInitialMultitexture, EcsOnSet, Texture2D, MultiTexture2D);
    ECS_SYSTEM(world, deallocate_texture, EcsOnRemove, Texture2D);

    IMG_Init(IMG_INIT_JPG | IMG_INIT_PNG | IMG_INIT_WEBP);
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GL_TRUE);
    glfwWindowHint(GLFW_DECORATED, GL_FALSE);
    glfwWindowHint(GLFW_VISIBLE, GL_FALSE);
    // glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, 1);
    glfwSetErrorCallback(error_callback);
    window = glfwCreateWindow(640, 480, "Gui Gal 👩‍💻", NULL, NULL);
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

    renderer = ecs_set_name(world, 0, "renderer");
    ecs_set(world, renderer, Camera, {1.0});
    ecs_set(world, renderer, BatchSpriteRenderer, {});

    ECS_SYSTEM(world, AnimateGif, EcsPreUpdate, GifAnimator, Texture2D, MultiTexture2D);
    ECS_SYSTEM(world, RenderSprites, EcsOnUpdate, renderer:Camera, renderer:BatchSpriteRenderer, Transform2D, Texture2D);
    ECS_SYSTEM(world, GrabMoveCamera, EcsPreUpdate, renderer:Camera, input:EventMouseMotion);
    ECS_SYSTEM(world, ScrollZoomCamera, EcsPreUpdate, renderer:Camera, input:EventScroll);
    ECS_SYSTEM(world, CameraCalculateView, EcsOnUpdate, Camera);
    // ECS_SYSTEM(world, SelectVisualSymbol, EcsOnUpdate, renderer:Camera, input:EventMouseButton, Transform2D, Texture2D);
    // ECS_SYSTEM(world, MoveGrabbedTransforms, EcsOnUpdate, renderer:Camera, input:EventMouseMotion, Transform2D, Grabbed);
    ECS_SYSTEM(world, ConsumeEvents, EcsPostFrame, (ConsumeEvent, *));
    // ECS_SYSTEM(world, UnGrab, EcsOnUpdate, input:EventMouseButton);
    ECS_SYSTEM(world, LoadDroppedFiles, EcsOnSet, EventDropFiles);

    glfwShowWindow(window);

    while (!glfwWindowShouldClose(window))
    {
        glClear(GL_COLOR_BUFFER_BIT);
        glfwPollEvents();
        ecs_progress(world, 0);
        glfwSwapBuffers(window);
    }
    glfwDestroyWindow(window);
    glfwTerminate();
    ecs_fini(world);
    return 0;
}