//#########################
//Pekka Kana 2
//Copyright (c) 2003 Janne Kivilahti
//#########################
#include "engine/PRender.hpp"

#include "engine/PDraw.hpp"
#include "engine/PLog.hpp"
#include "engine/types.hpp"

#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <SDL.h>

namespace PRender {

struct GLRect {
	GLfloat x, y, w, h;
};

const GLfloat vbo_data[] = {
	0.0f,  1.0f,
	1.0f,  0.0f,
	0.0f,  0.0f,
	0.0f,  1.0f,
	1.0f,  1.0f,
	1.0f,  0.0f
};

static GLRect screen_dest = {0.f, 0.f, 1.f, 1.f};

static const char* window_name;
static SDL_Window* window = NULL;
static SDL_GLContext context;

static bool vsync_set = true;

static GLchar Log_message[1024];

GLuint vbo;
GLuint screen_vbo;

// Screen shader
static GLuint screen_vs;
static GLuint screen_fs;
static GLuint screen_program;

static GLfloat shader_time;

static GLint  uniScreenTex;
static GLint  uniScreenIdxRes;
static GLint  uniScreenRes;
static GLint  uniScreenTime;

static GLuint screen_texture;

// Indexed shader
static GLuint indexed_vs;
static GLuint indexed_fs;
static GLuint indexed_program;

static GLint uniIndexRes;
static GLint uniIndexTime;
static GLint uniIndexPalette;
static GLint uniIndexTex;

static GLuint  indexed_texture;
static GLfloat indexed_palette[256*3];

static GLuint indexed_buffer;
static GLint  screen_buffer;

void set_fullscreen(bool set) {

    #ifndef __ANDROID__
    if(set)
        SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
    else {
        int buf_w, buf_h;
        PDraw::get_buffer_size(&buf_w, &buf_h);

        SDL_SetWindowFullscreen(window, 0);
        SDL_SetWindowSize(window, buf_w, buf_h);
        SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED , SDL_WINDOWPOS_CENTERED);
        //TODO - adjust dst_rect too and turn off filters
    }

    adjust_screen();
    #endif

}

void adjust_screen() {

    int w, h;
    SDL_GetWindowSize(window, &w, &h);

    int buf_w, buf_h;
    PDraw::get_buffer_size(&buf_w, &buf_h);

    float screen_prop = (float)w / h;
    float buff_prop   = (float)buf_w / buf_h;

    if (buff_prop > screen_prop) {

        screen_dest.w = 1.f;
        screen_dest.h = 1.f / buff_prop;
        screen_dest.x = 0.f;
        screen_dest.y = (1.f - screen_dest.h) / 2;
    
    } else {

        screen_dest.w = buff_prop;
        screen_dest.h = 1.f;
        screen_dest.x = (1.f - screen_dest.w) / 2;
        screen_dest.y = 0.f;

    }

	float x = screen_dest.x;
	float y = screen_dest.y;
	float a = x + screen_dest.w;
	float b = y + screen_dest.h;

	a /= 2;
	x += 0.5;
	y += 1;

	GLfloat screen_vbo_data[] = {

		a,  b,
		a,  y,
		x,  y,
		x,  b,
		a,  b,
		a,  y

	};

	glBindBuffer(GL_ARRAY_BUFFER, screen_vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(screen_vbo_data), screen_vbo_data, GL_DYNAMIC_DRAW);

	//printf("%f %f %f %f\n", x, y, a, b);

}

int set_filter(const char* filter) {

	glBindTexture(GL_TEXTURE_2D, screen_texture);

	if (strcmp(filter, FILTER_LINEAR) == 0) {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	} else if (strcmp(filter, FILTER_NEAREST) == 0) {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	}

    return 0;
}

void get_window_size(int* w, int* h) {

    SDL_GetWindowSize(window, w, h);

}

void get_window_position(int* x, int* y) {

    SDL_GetWindowPosition(window, x, y);

}

int set_vsync(bool set) {

    #ifndef PK2_NO_THREAD

	if (set == vsync_set)
		return 0;

	if (!set) {
		SDL_GL_SetSwapInterval(0);
	} else {
		// Try adaptative vsync
		int ret = SDL_GL_SetSwapInterval(-1);

		// If fail try normal vsync
		if (ret == -1)
			SDL_GL_SetSwapInterval(1);

	}
	vsync_set = set;

    return 0;

    #else

    return -1;

    #endif

}

bool is_vsync() {

    return vsync_set;
    
}


GLchar* load_source(const char* file) {

	SDL_RWops* rw =  SDL_RWFromFile(file, "rb");
	if (!rw) return NULL;

	Sint64 size = SDL_RWsize(rw);
	if (size <= 0) {
		SDL_RWclose(rw);
		return NULL;
	}

	Uint8* ret = (Uint8*)malloc(size + 1);
	if (!ret) {
		SDL_RWclose(rw);
		return NULL;
	}

	SDL_RWread(rw, ret, size, 1);
	ret[size] = '\0';

	SDL_RWclose(rw);
	return (GLchar*)ret;

}

GLuint load_shader(const char* file_name, GLenum shaderType) {

	GLuint shader = glCreateShader(shaderType);
	if (!shader) return 0;

	GLchar* source = load_source(file_name);
	if (!source) {
		PLog::Write(PLog::ERR, "PRender", "Can't load %s", file_name);
		return 0;
	}

	glShaderSource(shader, 1, &source, NULL);
	glCompileShader(shader);

	free(source);

	GLint compiled;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);

	if (!compiled) {
		
	    glGetShaderInfoLog(shader, 1024, NULL, Log_message);
		PLog::Write(PLog::ERR, "PRender", "Error compiling %s:\n%s", file_name, Log_message);

		glDeleteShader(shader);
		return 0;

	}

	return shader;

}

int create_program(const char* vs_file, const char* fs_file, GLuint* vs, GLuint* fs) {

	GLuint _vs, _fs;
	if (!vs) vs = &_vs;
	if (!fs) fs = &_fs;

	*vs = load_shader(vs_file, GL_VERTEX_SHADER);
	if(!*vs) {
		PLog::Write(PLog::ERR, "PRender", "Can't load vertex shader");
		return 0;
	}

	*fs = load_shader(fs_file, GL_FRAGMENT_SHADER);
	if(!*fs) {
		PLog::Write(PLog::ERR, "PRender", "Can't load frament shader");
		glDeleteShader(*vs);
		return 0;
	}
	
	GLuint program = glCreateProgram();
	if(!program) {
		PLog::Write(PLog::ERR, "PRender", "Can't create program");
		glDeleteShader(*vs);
		glDeleteShader(*fs);
		glDeleteProgram(program);
		return 0;
	}

	glAttachShader(program, *vs);
	glAttachShader(program, *fs);
	glLinkProgram(program);
	
	GLint linked;
	glGetProgramiv(program, GL_LINK_STATUS, &linked);

	if (!linked) {
		glGetProgramInfoLog(program, 1024, NULL, Log_message);
		PLog::Write(PLog::ERR, "PRender", "Can't link program\n%s", Log_message);
		glDeleteShader(*vs);
		glDeleteShader(*fs);
		glDeleteProgram(program);
		return 0;
	}

	return program;

}



void create_screen_program() {

	screen_program = create_program("shader/screen.vs", "shader/screen.fs", &screen_vs, &screen_fs);
	if (!screen_program) {
		PLog::Write(PLog::ERR, "PRender", "Can't create screen program");
		return;
	}

	glUseProgram(screen_program);

	GLint vbo_att = glGetAttribLocation(screen_program, "position");
	glEnableVertexAttribArray(vbo_att);
	glVertexAttribPointer(vbo_att, 2, GL_FLOAT, GL_FALSE, 0, 0);

	uniScreenTex    = glGetUniformLocation(screen_program, "screen_tex");
	uniScreenIdxRes = glGetUniformLocation(screen_program, "buffer_res");
	uniScreenRes    = glGetUniformLocation(screen_program, "screen_res");
	uniScreenTime   = glGetUniformLocation(screen_program, "time");

	glBindFragDataLocation(screen_program, 0, "color");

}

void create_indexed_program() {

	indexed_program = create_program("shader/indexed.vs", "shader/indexed.fs", &indexed_vs, &indexed_fs);
	if (!indexed_program) {
		PLog::Write(PLog::ERR, "PRender", "Can't create indexed program");
		return;
	}

	glUseProgram(indexed_program);

	GLint vbo_att = glGetAttribLocation(indexed_program, "position");
	glEnableVertexAttribArray(vbo_att);
	glVertexAttribPointer(vbo_att, 2, GL_FLOAT, GL_FALSE, 0, 0);

	uniIndexPalette = glGetUniformLocation(indexed_program, "palette");
	uniIndexTex     = glGetUniformLocation(indexed_program, "indexed_tex");
	uniIndexRes     = glGetUniformLocation(indexed_program, "indexed_res");
	uniIndexTime    = glGetUniformLocation(indexed_program, "time");

	glBindFragDataLocation(indexed_program, 0, "color");

}

void load_buffers() {
	
	// Indexed texture (recieve game buffer)
	glGenTextures(1, &indexed_texture);
	glBindTexture(GL_TEXTURE_2D, indexed_texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_MIRROR_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRROR_CLAMP_TO_EDGE);


	// RGB texture (indexed texture converted to RGB)
	int w, h;
	PDraw::get_buffer_size(&w, &h);
	printf("AAAAAAAAAAA %i %i \n", w, h);
	glGenTextures(1, &screen_texture);
	glBindTexture(GL_TEXTURE_2D, screen_texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_MIRROR_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRROR_CLAMP_TO_EDGE);


	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &screen_buffer);
	glGenFramebuffers(1, &indexed_buffer);
	glBindFramebuffer(GL_FRAMEBUFFER, indexed_buffer);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, screen_texture, 0);

	GLenum DrawBuffers[1] = {GL_COLOR_ATTACHMENT0};
	glDrawBuffers(1, DrawBuffers);

	if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {

		PLog::Write(PLog::ERR, "PRender", "Error creating frame buffer");
		return;

	}

	glBindFramebuffer(GL_FRAMEBUFFER, screen_buffer);

	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vbo_data), vbo_data, GL_STATIC_DRAW);

	glGenBuffers(1, &screen_vbo);

}

int init(int width, int height, const char* name, const char* icon) {

    window_name = name;

    window = SDL_CreateWindow(name, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL);
    if (!window) {

        PLog::Write(PLog::FATAL, "PRender", "Couldn't create window!");
        return -2;

    }

    #ifndef __ANDROID__

    SDL_Surface* window_icon = SDL_LoadBMP(icon);
    if (window_icon) {
        SDL_SetWindowIcon(window, window_icon);
        SDL_FreeSurface(window_icon);
    }
    
    SDL_ShowCursor(SDL_DISABLE);
    
    #endif

    context = SDL_GL_CreateContext(window);
	PLog::Write(PLog::DEBUG, "PRender", "OpenGL version: %s", glGetString(GL_VERSION));   

	glDisable(GL_LIGHTING);
	glDisable(GL_DITHER);
	glDisable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);

    load_buffers();

    create_screen_program();
	create_indexed_program();

    adjust_screen();

    return 0;

}

void terminate() {

    SDL_DestroyWindow(window);
	SDL_GL_DeleteContext(context);

}

void update(void* _buffer8, int alpha) {

	shader_time += 1.f/60;

    SDL_Surface* buffer8 = (SDL_Surface*)_buffer8;

    SDL_LockSurface(buffer8);

	float a = float(alpha) / 100;

    // Only if changed?
    for (int i = 0; i < 256; i++) {
		indexed_palette[3*i] = float(buffer8->format->palette->colors[i].r) * a / 256;
		indexed_palette[3*i + 1] = float(buffer8->format->palette->colors[i].g) * a / 256;
		indexed_palette[3*i + 2] = float(buffer8->format->palette->colors[i].b) * a / 256;
	}
    
	glViewport(0, 0, buffer8->w, buffer8->h);
	glBindFramebuffer(GL_FRAMEBUFFER, indexed_buffer);
	glUseProgram(indexed_program);
	glBindTexture(GL_TEXTURE_2D, indexed_texture);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R8UI, buffer8->w, buffer8->h, 0, GL_RED_INTEGER, GL_UNSIGNED_BYTE, buffer8->pixels);
	glUniform1i(uniIndexTex, 0);
	glUniform3fv(uniIndexPalette, 256, indexed_palette);
	glUniform1f(uniIndexTime, shader_time);
	glUniform2f(uniIndexRes, buffer8->w, buffer8->h);
	glDrawArrays(GL_TRIANGLES, 0, 6);

	// Only if changed?
	int w, h;
	SDL_GetWindowSize(window, &w, &h);
	glViewport(0, 0, w, h);
	glBindFramebuffer(GL_FRAMEBUFFER, screen_buffer);
	glUseProgram(screen_program);
	glBindTexture(GL_TEXTURE_2D, screen_texture);
	glBindBuffer(GL_ARRAY_BUFFER, screen_vbo);
	glUniform1i(uniScreenTex, 0);
	glUniform2f(uniScreenIdxRes, buffer8->w, buffer8->h);
	glUniform2f(uniScreenRes, w, h);
	glUniform1f(uniScreenTime, shader_time);
	glClearColor(0, 0, 0, 1);
	glClear(GL_COLOR_BUFFER_BIT);
	glDrawArrays(GL_TRIANGLES, 0, 6);

	SDL_UnlockSurface(buffer8);
    SDL_GL_SwapWindow(window);

}

}