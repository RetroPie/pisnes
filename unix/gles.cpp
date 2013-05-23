/*
 * video.c - Raspberry Pi suport by djdron
 *
 * Copyright (c) 2013 Atari800 development team (see DOC/CREDITS)
 *
 * This file is part of the Atari800 emulator project which emulates
 * the Atari 400, 800, 800XL, 130XE, and 5200 8-bit computers.
 *
 * This file is based on Portable ZX-Spectrum emulator.
 * Copyright (C) 2001-2012 SMT, Dexus, Alone Coder, deathsoft, djdron, scor
 *
 * C++ to C code conversion by Pate
 *
 * Atari800 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "GLES2/gl2.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

float op_zoom = 1.0f;

int video_smooth = 1;

extern unsigned short *screen;

//IMPORTANT, make sure this function is commented out at runtime
//as it has a big performance impact!
#define	SHOW_ERROR	//gles_show_error();

static const char* vertex_shader =
    "uniform mat4 u_vp_matrix;                              \n"
    "attribute vec4 a_position;                             \n"
    "attribute vec2 a_texcoord;                             \n"
    "varying mediump vec2 v_texcoord;                       \n"
    "void main()                                            \n"
    "{                                                      \n"
    "   v_texcoord = a_texcoord;                            \n"
    "   gl_Position = u_vp_matrix * a_position;             \n"
    "}                                                      \n";

static const char* fragment_shader_none =
	"varying mediump vec2 v_texcoord;												\n"
	"uniform sampler2D u_texture;													\n"
	"void main()																	\n"
	"{																				\n"
	"	gl_FragColor = texture2D(u_texture, v_texcoord);							\n" 
	"}																				\n";

// scanline-3x.shader
static const char* fragment_shader_scanline =
	"uniform sampler2D u_texture; 													\n"
	"varying mediump vec2 v_texcoord; 												\n"
	"void main()     																\n"
	"{																				\n"
	"	vec4 rgb = texture2D(u_texture, v_texcoord);  								\n"
	"	vec4 intens ;  																\n"
	"	if (fract(gl_FragCoord.y * (0.5*4.0/3.0)) > 0.5)  							\n"
	"		intens = vec4(0);  														\n"
	"	else  																		\n"
	"		intens = smoothstep(0.2,0.8,rgb) + normalize(vec4(rgb.xyz, 1.0));  		\n"
	"	float level = (4.0-0.0) * 0.19;  											\n"
	"	gl_FragColor = intens * (0.5-level) + rgb * 1.1 ;  							\n"
	"} 																				\n";

// Phospher effect shader, too slow to run at full speed on the RPI
static const char* fragment_shader_phospher =
    "uniform sampler2D u_texture; 														\n"
	"vec2 rubyTextureSize=vec2(512.0,448.0);											\n"
	"varying mediump vec2 v_texcoord; 													\n"
	"																					\n"
	"vec3 to_focus(float pixel)															\n"
	"{																					\n"
	"   pixel = mod(pixel + 3.0, 3.0);													\n"
	"   if (pixel >= 2.0) // Blue														\n"
	"      return vec3(pixel - 2.0, 0.0, 3.0 - pixel);									\n"
	"   else if (pixel >= 1.0) // Green													\n"
	"      return vec3(0.0, 2.0 - pixel, pixel - 1.0);									\n"
	"   else // Red																		\n"
	"      return vec3(1.0 - pixel, pixel, 0.0);										\n"
	"}																					\n"
	"																					\n"
	"void main()																		\n"
	"{																					\n"
	"   float y = mod(v_texcoord.y * rubyTextureSize.y, 1.0);							\n"
	"   float intensity = exp(-0.2 * y);												\n"
	"   vec2 one_x = vec2(1.0 / (3.0 * rubyTextureSize.x), 0.0);						\n"
	"   vec3 color = texture2D(u_texture, v_texcoord.xy - 0.0 * one_x).rgb;				\n"
	"   vec3 color_prev = texture2D(u_texture, v_texcoord.xy - 1.0 * one_x).rgb;		\n"
	"   vec3 color_prev_prev = texture2D(u_texture, v_texcoord.xy - 2.0 * one_x).rgb;	\n"
	"   float pixel_x = 3.0 * v_texcoord.x * rubyTextureSize.x;							\n"
	"   vec3 focus = to_focus(pixel_x - 0.0);											\n"
	"   vec3 focus_prev = to_focus(pixel_x - 1.0);										\n"
	"   vec3 focus_prev_prev = to_focus(pixel_x - 2.0);									\n"
	"   vec3 result =																	\n"
	"      0.8 * color * focus +														\n"
	"      0.6 * color_prev * focus_prev +												\n"
	"      0.3 * color_prev_prev * focus_prev_prev;										\n"
	"   result = 2.3 * pow(result, vec3(1.4));											\n"
	"   gl_FragColor = vec4(intensity * result, 1.0);									\n"
	"}																					\n";

// Bilinear smoothing, required when using palette as
// automatic smoothing won't work.
static const char* fragment_shader_smooth =
	"varying mediump vec2 v_texcoord;												\n"
	"uniform sampler2D u_texture;													\n"
	"void main()																	\n"
	"{																				\n"
	"	vec4 p0 = texture2D(u_texture, v_texcoord);									\n" 
	"	vec4 p1 = texture2D(u_texture, v_texcoord + vec2(1.0/512.0, 0)); 			\n"
	"	vec4 p2 = texture2D(u_texture, v_texcoord + vec2(0, 1.0/256.0)); 			\n"
	"	vec4 p3 = texture2D(u_texture, v_texcoord + vec2(1.0/512.0, 1.0/256.0)); 	\n"
	"	vec2 l = vec2(fract(512.0*v_texcoord.x), fract(256.0*v_texcoord.y)); 		\n"
	"	gl_FragColor = mix(mix(c0, c1, l.x), mix(c2, c3, l.x), l.y); 				\n"
	"}																				\n";

static const GLfloat vertices[] =
{
	-0.5f, -0.5f, 0.0f,
	+0.5f, -0.5f, 0.0f,
	+0.5f, +0.5f, 0.0f,
	-0.5f, +0.5f, 0.0f,
};

#define	TEX_WIDTH	512
#define	TEX_HEIGHT	448
//sq #define	WIDTH		Screen_WIDTH
//sq #define	HEIGHT		Screen_HEIGHT
#define	WIDTH		256
#define	HEIGHT		224

//Define the rectangle that is drawn
#define	min_u		0.0f
#define	max_u		(float)WIDTH/TEX_WIDTH
#define	min_v		0.0f
#define	max_v		(float)HEIGHT/TEX_HEIGHT

static const GLfloat uvs[] =
{
	min_u, min_v,
	max_u, min_v,
	max_u, max_v,
	min_u, max_v,
};

static const GLushort indices[] =
{
	0, 1, 2,
	0, 2, 3,
};

static const int kVertexCount = 4;
static const int kIndexCount = 6;

void gles_show_error()
{
	GLenum error = GL_NO_ERROR;
    error = glGetError();
    if (GL_NO_ERROR != error)
        printf("GL Error %x encountered!\n", error);
}

static GLuint CreateShader(GLenum type, const char *shader_src)
{
	GLuint shader = glCreateShader(type);
	if(!shader)
		return 0;

	// Load and compile the shader source
	glShaderSource(shader, 1, &shader_src, NULL);
	glCompileShader(shader);

	// Check the compile status
	GLint compiled = 0;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
	if(!compiled)
	{
		GLint info_len = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &info_len);
		if(info_len > 1)
		{
			char* info_log = (char *)malloc(sizeof(char) * info_len);
			glGetShaderInfoLog(shader, info_len, NULL, info_log);
			// TODO(dspringer): We could really use a logging API.
			printf("Error compiling shader:\n%s\n", info_log);
			free(info_log);
		}
		glDeleteShader(shader);
		return 0;
	}
	return shader;
}

static GLuint CreateProgram(const char *vertex_shader_src, const char *fragment_shader_src)
{
	GLuint vertex_shader = CreateShader(GL_VERTEX_SHADER, vertex_shader_src);
	if(!vertex_shader)
		return 0;
	GLuint fragment_shader = CreateShader(GL_FRAGMENT_SHADER, fragment_shader_src);
	if(!fragment_shader)
	{
		glDeleteShader(vertex_shader);
		return 0;
	}

	GLuint program_object = glCreateProgram();
	if(!program_object)
		return 0;
	glAttachShader(program_object, vertex_shader);
	glAttachShader(program_object, fragment_shader);

	// Link the program
	glLinkProgram(program_object);

	// Check the link status
	GLint linked = 0;
	glGetProgramiv(program_object, GL_LINK_STATUS, &linked);
	if(!linked)
	{
		GLint info_len = 0;
		glGetProgramiv(program_object, GL_INFO_LOG_LENGTH, &info_len);
		if(info_len > 1)
		{
			char* info_log = (char *)malloc(info_len);
			glGetProgramInfoLog(program_object, info_len, NULL, info_log);
			// TODO(dspringer): We could really use a logging API.
			printf("Error linking program:\n%s\n", info_log);
			free(info_log);
		}
		glDeleteProgram(program_object);
		return 0;
	}
	// Delete these here because they are attached to the program object.
	glDeleteShader(vertex_shader);
	glDeleteShader(fragment_shader);
	return program_object;
}

typedef	struct ShaderInfo {
		GLuint program;
		GLint a_position;
		GLint a_texcoord;
		GLint u_vp_matrix;
		GLint u_texture;
} ShaderInfo;

static ShaderInfo shader;
static GLuint buffers[3];
static GLuint textures[2];

void gles2_create()
{
	memset(&shader, 0, sizeof(ShaderInfo));
	shader.program = CreateProgram(vertex_shader, fragment_shader_none);
	//sq shader.program = CreateProgram(vertex_shader, fragment_shader_scanline);
	//sq shader.program = CreateProgram(vertex_shader, fragment_shader_phospher);
	if(shader.program)
	{
		shader.a_position	= glGetAttribLocation(shader.program,	"a_position");
		shader.a_texcoord	= glGetAttribLocation(shader.program,	"a_texcoord");
		shader.u_vp_matrix	= glGetUniformLocation(shader.program,	"u_vp_matrix");
		shader.u_texture	= glGetUniformLocation(shader.program,	"u_texture");
	}
	if(!shader.program) return;

	glGenTextures(2, textures);	SHOW_ERROR
	glBindTexture(GL_TEXTURE_2D, textures[0]); SHOW_ERROR
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, TEX_WIDTH, TEX_HEIGHT, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, NULL); SHOW_ERROR

	glGenBuffers(3, buffers); SHOW_ERROR
	glBindBuffer(GL_ARRAY_BUFFER, buffers[0]); SHOW_ERROR
	glBufferData(GL_ARRAY_BUFFER, kVertexCount * sizeof(GLfloat) * 3, vertices, GL_STATIC_DRAW); SHOW_ERROR
	glBindBuffer(GL_ARRAY_BUFFER, buffers[1]); SHOW_ERROR
	glBufferData(GL_ARRAY_BUFFER, kVertexCount * sizeof(GLfloat) * 2, uvs, GL_STATIC_DRAW); SHOW_ERROR
	glBindBuffer(GL_ARRAY_BUFFER, 0); SHOW_ERROR
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffers[2]); SHOW_ERROR
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, kIndexCount * sizeof(GL_UNSIGNED_SHORT), indices, GL_STATIC_DRAW); SHOW_ERROR
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0); SHOW_ERROR

	glClearColor(0.0f, 0.0f, 0.0f, 1.0f); SHOW_ERROR
	glDisable(GL_DEPTH_TEST); SHOW_ERROR
	glDisable(GL_DITHER); SHOW_ERROR
}

void gles2_destroy()
{
	if(!shader.program) return;
	if(shader.program) glDeleteProgram(shader.program);
	glDeleteBuffers(3, buffers); SHOW_ERROR
	glDeleteTextures(2, textures); SHOW_ERROR
}

static void SetOrtho(float m[4][4], float left, float right, float bottom, float top, float near, float far, float scale_x, float scale_y)
{
	memset(m, 0, 4*4*sizeof(float));
	m[0][0] = 2.0f/(right - left)*scale_x;
	m[1][1] = 2.0f/(top - bottom)*scale_y;
	m[2][2] = -2.0f/(far - near);
	m[3][0] = -(right + left)/(right - left);
	m[3][1] = -(top + bottom)/(top - bottom);
	m[3][2] = -(far + near)/(far - near);
	m[3][3] = 1;
}

static void gles2_DrawQuad(const ShaderInfo *sh, GLuint textures[2])
{
	glUniform1i(sh->u_texture, 0); SHOW_ERROR

	if (video_smooth) {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR); SHOW_ERROR 
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); SHOW_ERROR
	}
	else {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST); SHOW_ERROR 
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); SHOW_ERROR
	}

	glBindBuffer(GL_ARRAY_BUFFER, buffers[0]); SHOW_ERROR
	glVertexAttribPointer(sh->a_position, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), NULL); SHOW_ERROR
	glEnableVertexAttribArray(sh->a_position); SHOW_ERROR

	glBindBuffer(GL_ARRAY_BUFFER, buffers[1]); SHOW_ERROR
	glVertexAttribPointer(sh->a_texcoord, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), NULL); SHOW_ERROR
	glEnableVertexAttribArray(sh->a_texcoord); SHOW_ERROR

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffers[2]); SHOW_ERROR

	glDrawElements(GL_TRIANGLES, kIndexCount, GL_UNSIGNED_SHORT, 0); SHOW_ERROR
}

inline unsigned short BGR565(unsigned char r, unsigned char g, unsigned char b) { return ((r&~7) << 8)|((g&~3) << 3)|(b >> 3); }

void gles2_draw(int _w, int _h)
{
	if(!shader.program) return;

	float sx = 1.0f, sy = 1.0f;

	// Screen aspect ratio adjustment
	float a = (float)_w/_h;
	//sq float a0 = 336.0f/240.0f;
	float a0 = 256.0f/224.0f;
	if(a > a0)
		sx = a0/a;
	else
		sy = a/a0;

	glClear(GL_COLOR_BUFFER_BIT); SHOW_ERROR
	glViewport(0, 0, _w, _h); SHOW_ERROR

	float proj[4][4];
	glDisable(GL_BLEND); SHOW_ERROR
	glUseProgram(shader.program); SHOW_ERROR
	SetOrtho(proj, -0.5f, +0.5f, +0.5f, -0.5f, -1.0f, 1.0f, sx*op_zoom, sy*op_zoom);
	glUniformMatrix4fv(shader.u_vp_matrix, 1, GL_FALSE, &proj[0][0]); SHOW_ERROR

	glActiveTexture(GL_TEXTURE0); SHOW_ERROR
	glBindTexture(GL_TEXTURE_2D, textures[0]); SHOW_ERROR
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, WIDTH, HEIGHT, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, screen); SHOW_ERROR
	gles2_DrawQuad(&shader, textures);

	glBindBuffer(GL_ARRAY_BUFFER, 0); SHOW_ERROR
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0); SHOW_ERROR
}
