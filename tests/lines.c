//========================================================================
// Simple multi-window test
// Copyright (c) Camilla Berglund <elmindreda@elmindreda.org>
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would
//    be appreciated but is not required.
//
// 2. Altered source versions must be plainly marked as such, and must not
//    be misrepresented as being the original software.
//
// 3. This notice may not be removed or altered from any source
//    distribution.
//
//========================================================================
//
// This test creates four windows and clears each in a different color
//
//========================================================================

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <stdio.h>
#include <stdlib.h>
#include <linmath.h>

#define BUFFER_OFFSET(x)  ((const void*) (x))

typedef struct
{
	GLenum       type;
	const char*  filename;
	GLuint       shader;
} ShaderInfo;

static GLchar* ReadShader(const char* filename)
{
#ifdef WIN32
	FILE* infile;
	fopen_s(&infile, filename, "rb");
#else
	FILE* infile = fopen(filename, "rb");
#endif // WIN32

	if (!infile) {
#ifdef _DEBUG
        fprintf(stderr, "Unable to open file '%s'\n", filename);
#endif
		return NULL;
	}

	fseek(infile, 0, SEEK_END);
	int len = ftell(infile);
	fseek(infile, 0, SEEK_SET);

    GLchar* source = malloc(len + 1);

	fread(source, 1, len, infile);
	fclose(infile);

	source[len] = '\0';

    return source;
}

GLuint LoadShaders(ShaderInfo* shaders)
{
	if (shaders == NULL) { return 0; }

	GLuint program = glCreateProgram();

	ShaderInfo* entry = shaders;
	while (entry->type != GL_NONE) {
		GLuint shader = glCreateShader(entry->type);
		entry->shader = shader;

		GLchar* source = ReadShader(entry->filename);
		if (source == NULL) {
			for (entry = shaders; entry->type != GL_NONE; ++entry) {
				glDeleteShader(entry->shader);
				entry->shader = 0;
			}

			return 0;
		}

		glShaderSource(shader, 1, &source, NULL);
        free(source);

		glCompileShader(shader);

		GLint status;
		glGetShaderiv(shader, GL_COMPILE_STATUS, &status);

		if (status != GL_TRUE) {
#ifdef _DEBUG
			GLsizei len;
			glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);

			GLchar* log = malloc(len + 1);
			glGetShaderInfoLog(shader, len, NULL, log);
            fprintf(stderr, "Shader compilation failed:\n%s\n", log);
            free(log);
#endif // _DEBUG
			for (entry = shaders; entry->type != GL_NONE; ++entry) {
				glDeleteShader(entry->shader);
				entry->shader = 0;
			}

			return 0;
		}

		glAttachShader(program, shader);

		entry++;
	}

	glLinkProgram(program);

	GLint status;
	glGetProgramiv(program, GL_LINK_STATUS, &status);

	if (status != GL_TRUE) {
#ifdef _DEBUG
		GLsizei len;
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &len);
        
		GLchar* log = malloc(len + 1);
		glGetProgramInfoLog(program, len, NULL, log);
        fprintf(stderr, "Shader linking failed:\n%s\n", log);
        free(log);
#endif // _DEBUG

		for (entry = shaders; entry->type != GL_NONE; ++entry) {
			glDeleteShader(entry->shader);
			entry->shader = 0;
		}

		return 0;
	}

	return program;
}

//////////////////////////////////////////////////////////////////////////

typedef struct
{
    float aspect;
    GLuint render_prog;
	GLuint vao[1];
	GLuint vbo[1];
	GLuint ebo[1];
    
	GLint render_line_color_loc;
	GLint render_model_matrix_loc;
	GLint render_projection_matrix_loc;
} Context;

void init(Context* c)
{
    static ShaderInfo shader_info[] = {
        {GL_VERTEX_SHADER, "lines.vert"},
        {GL_FRAGMENT_SHADER, "lines.frag"},
        {GL_NONE, NULL},
    };

    c->render_prog = LoadShaders(shader_info);
    glUseProgram(c->render_prog);
    
	c->render_line_color_loc = glGetUniformLocation(c->render_prog, "line_color");
	c->render_model_matrix_loc = glGetUniformLocation(c->render_prog, "model_matrix");
	c->render_projection_matrix_loc = glGetUniformLocation(c->render_prog, "projection_matrix");

    // A single triangle
	static const GLfloat vertex_positions[] =
    {
        -1.0f, -1.0f,  0.0f, 1.0f,
         1.0f, -1.0f,  0.0f, 1.0f,
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 1.0f,
    };

    // Indices for the triangle strips
	static const GLushort vertex_indices[] =
	{
		0, 1, 1, 2, 2, 0
	};
    
	glGenBuffers(1, c->ebo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, c->ebo[0]);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(vertex_indices), vertex_indices, GL_STATIC_DRAW);

	// Set up the vertex attribute
	glGenVertexArrays(1, c->vao);
	glBindVertexArray(c->vao[0]);

    glGenBuffers(1, c->vbo);
	glBindBuffer(GL_ARRAY_BUFFER, c->vbo[0]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_positions), vertex_positions, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, NULL);
	glEnableVertexAttribArray(0);
    
    glClearColor(1.f, 1.f, 1.f, 1.f);
}

void render(Context* c)
{
    float a = c->aspect;
    mat4x4 proj, model;
    vec3 color = { 0.0f, 0.0f, 1.0f };
    mat4x4_identity(proj);
    mat4x4_identity(model);

	glEnable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);

	glClear(GL_COLOR_BUFFER_BIT);
    
	// Activate simple shading program
	glUseProgram(c->render_prog);
    
    // Set up line color
    glUniform3fv(c->render_line_color_loc, 1, color);

	// Set up the model and projection matrix
    mat4x4_frustum(proj, -1.0f, 1.0f, -a, a, 1.0f, 500.f);
	glUniformMatrix4fv(c->render_projection_matrix_loc, 1, GL_FALSE, proj[0]);

	// Set up for a glDrawElements call
	glBindVertexArray(c->vao[0]);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, c->ebo[0]);

	// DrawElements
    mat4x4_translate(model, 0.0f, 0.0f, -5.0f);
	glUniformMatrix4fv(c->render_model_matrix_loc, 1, GL_FALSE, model[0]);
	glDrawElements(GL_LINES, 6, GL_UNSIGNED_SHORT, BUFFER_OFFSET(0));
}

void finalize(Context* c)
{
	glDeleteBuffers(1, c->vbo);
	glDeleteVertexArrays(1, c->vao);
	glDeleteBuffers(1, c->ebo);
	glUseProgram(0);
	glDeleteProgram(c->render_prog);
}

static void error_callback(int error, const char* description)
{
    fprintf(stderr, "Error: %s\n", description);
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (action != GLFW_PRESS)
        return;

    switch (key)
    {
        case GLFW_KEY_ESCAPE:
            glfwSetWindowShouldClose(window, GLFW_TRUE);
            break;
    }
}

static void size_callback(GLFWwindow* window, int width, int height)
{
    Context* c = (Context*) glfwGetWindowUserPointer(window);
	glViewport(0, 0, width, height);

	c->aspect = (float) height / (float) width;
}

static const char* get_api_name(int api)
{
    if (api == GLFW_OPENGL_API)
        return "OpenGL";
    else if (api == GLFW_OPENGL_ES_API)
        return "OpenGL ES";

    return "Unknown API";
}

GLvoid* APIENTRY opengl_callback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const GLvoid* userParam)
{
    return NULL;
}


int main(int argc, char** argv)
{
    int width = 640;
    int height = 480;
    int running = GLFW_TRUE;
    GLFWwindow* window;
    Context context;

    glfwSetErrorCallback(error_callback);

    if (!glfwInit())
        exit(EXIT_FAILURE);
    
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    window = glfwCreateWindow(640, 480, "Debug Lines", NULL, NULL);
    if (!window)
    {
        fprintf(stderr, "Failed to open GLFW window.");
        glfwTerminate();
        exit(EXIT_FAILURE);
    }


    glfwSetWindowUserPointer(window, &context);

    glfwSetKeyCallback(window, key_callback);
    glfwSetWindowSizeCallback(window, size_callback);

    glfwMakeContextCurrent(window);
	gladLoadGLLoader((GLADloadproc) glfwGetProcAddress);

    init(&context);
    
	glViewport(0, 0, width, height);
	context.aspect = (float) height / (float) width;

    glfwShowWindow(window);

    while (running)
    {
        render(&context);

        glfwSwapBuffers(window);

        if (glfwWindowShouldClose(window))
            running = GLFW_FALSE;

        glfwPollEvents();
    }

    finalize(&context);

    glfwTerminate();
    exit(EXIT_SUCCESS);
}

