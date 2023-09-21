#pragma once

#ifdef __APPLE__
#include "GL/glew.h"
#elif WIN32
#define NOMINMAX
#include <Windows.h>
#include "GL/glew.h"
#include "GLUT/GLUT.h"
#endif

#include "RadeonProRender.h"
#include "RadeonProRender_GL.h"
#include "Math/mathutils.h"
#include "../common/common.h"
#include "../32_gl_interop/ShaderManager.h"


#ifdef __APPLE__
#ifndef GL_RGBA32F
#define GL_RGBA32F GL_RGBA32F_ARB
#endif 
#endif

#include <cassert>
#include <iostream>
#include <thread>
#include <memory>
const unsigned int WINDOW_WIDTH = 640;
const unsigned int WINDOW_HEIGHT = 480;
//
void Display();
void OnExit();

class GuiRenderImpl
{
public:
	struct Update
	{
		Update()
		{
			clear();
			m_progress = 0.0f;
		}

		volatile int m_hasUpdate;
		volatile int m_done;
		volatile int m_aborted;
		int m_camUpdated;
		float m_progress;

		void clear()
		{
			m_hasUpdate = m_done = m_aborted = m_camUpdated = 0;
		}
	};
	static
		void notifyUpdate(float x, void* userData)
	{
		Update* demo = (Update*)userData;
		demo->m_hasUpdate = 1;
		demo->m_progress = x;
	}
};


GLuint              g_vertex_buffer_id = 0;
GLuint              g_index_buffer_id = 0;
GLuint              g_texture = 0;
rpr_framebuffer		g_frame_buffer = NULL;
rpr_context         g_context = NULL;
rpr_material_system g_matsys = NULL;
rpr_framebuffer     g_frame_buffer_2 = NULL;
ShaderManager       g_shader_manager;
rpr_scene			g_scene = nullptr;
rpr_camera			g_camera = nullptr;
std::shared_ptr<float>	g_fbdata = nullptr;
RPRGarbageCollector g_gc;
bool                g_askExit = false; // push X key to exit



GuiRenderImpl::Update g_update;
const auto g_invalidTime = std::chrono::time_point<std::chrono::high_resolution_clock>::max();
int					g_benchmark_numberOfRenderIteration = 0;
auto g_benchmark_start = g_invalidTime;




struct MOUSE_DRAG_INFO
{
	MOUSE_DRAG_INFO()
	{
		leftMouseButtonDown = false;
		mousePosAtMouseButtonDown_X = -1;
		mousePosAtMouseButtonDown_Y = -1;
	}

	RadeonProRender::float3 lookat;
	RadeonProRender::float3 up;
	RadeonProRender::float3 pos;
	RadeonProRender::matrix mat;

	int	mousePosAtMouseButtonDown_X;
	int	mousePosAtMouseButtonDown_Y;

	bool leftMouseButtonDown;
};
MOUSE_DRAG_INFO g_mouse_camera;



// High batch size will increase the RPR performance ( rendering iteration per second ), but lower the render feedback FPS on the OpenGL viewer.
// Note that for better OpenGL FPS (and decreased RPR render quality), API user can also tune the `RPR_CONTEXT_PREVIEW` value.
const int			g_batchSize = 15;


// thread rendering 'g_batchSize' iteration(s)
void renderJob(rpr_context ctxt, GuiRenderImpl::Update* update)
{
	CHECK(rprContextRender(ctxt));
	update->m_done = 1;
	return;
}


void Update()
{
	const auto timeUpdateStarts = std::chrono::high_resolution_clock::now();


	//
	// print render stats every ~100 iterations.
	//
	if (g_benchmark_start == g_invalidTime)
		g_benchmark_start = timeUpdateStarts;
	if (g_benchmark_numberOfRenderIteration >= 100)
	{
		double elapsed_time_ms = std::chrono::duration<double, std::milli>(timeUpdateStarts - g_benchmark_start).count();
		double renderPerSecond = (double)g_benchmark_numberOfRenderIteration * 1000.0 / elapsed_time_ms;
		std::cout << renderPerSecond << " iterations per second." << std::endl;
		g_benchmark_numberOfRenderIteration = 0;
		g_benchmark_start = timeUpdateStarts;

	}


	// clear state
	g_update.clear();

	// start the rendering thread
	std::thread t(&renderJob, g_context, &g_update);

	// wait the rendering thread
	while (!g_update.m_done)
	{
		// at each update of the rendering thread
		if (g_update.m_hasUpdate)
		{
			// Read the frame buffer from RPR
			// Note that rprContextResolveFrameBuffer and rprFrameBufferGetInfo(fb,RPR_FRAMEBUFFER_DATA)  can be called asynchronous while rprContextRender is running.

			CHECK(rprContextResolveFrameBuffer(g_context, g_frame_buffer, g_frame_buffer_2, false));
			size_t frame_buffer_dataSize = 0;
			CHECK(rprFrameBufferGetInfo(g_frame_buffer_2, RPR_FRAMEBUFFER_DATA, 0, NULL, &frame_buffer_dataSize));

			// check that the size fits with original buffer alloc
			if (frame_buffer_dataSize != WINDOW_WIDTH * WINDOW_HEIGHT * 4 * sizeof(float))
			{
				CHECK(RPR_ERROR_INTERNAL_ERROR)
			}

			CHECK(rprFrameBufferGetInfo(g_frame_buffer_2, RPR_FRAMEBUFFER_DATA, frame_buffer_dataSize, g_fbdata.get(), NULL));

			// update the OpenGL texture with the new image from RPR
			glBindTexture(GL_TEXTURE_2D, g_texture);
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, GL_RGBA, GL_FLOAT, static_cast<const GLvoid*>(g_fbdata.get()));
			glBindTexture(GL_TEXTURE_2D, 0);

			// clear the update flag
			g_update.m_hasUpdate = false;
		}

		// Request a new OpenGL Display call.
		// Note from documentation: "Multiple calls to glutPostRedisplay before the next display callback opportunity generates only a single redisplay callback".
		// So we are not actually doing the OpenGL render in this Update loop. It would be a bad design as it would stress this thread and may reduce performance of the 
		//   `renderJob` thread which is the most important here.
		glutPostRedisplay();

	}

	// wait the end of the rendering thread
	t.join();

	if (g_askExit)
	{
		OnExit();
		std::exit(EXIT_SUCCESS);
	}

	g_benchmark_numberOfRenderIteration += g_batchSize;

	return;
}

void OnMouseMoveEvent(int x, int y)
{
	int delaX = (x - g_mouse_camera.mousePosAtMouseButtonDown_X);
	int delaY = -(y - g_mouse_camera.mousePosAtMouseButtonDown_Y);

	if (g_mouse_camera.leftMouseButtonDown)
	{
		RadeonProRender::matrix rotZ = RadeonProRender::rotation(g_mouse_camera.up, (float)delaX * 0.001);

		RadeonProRender::float3 lookAtVec = g_mouse_camera.lookat - g_mouse_camera.pos;
		lookAtVec.normalize();

		RadeonProRender::float3 left = RadeonProRender::cross(g_mouse_camera.up, lookAtVec);
		left.normalize();

		RadeonProRender::matrix rotleft = RadeonProRender::rotation(left, (float)delaY * 0.001);
		RadeonProRender::matrix newMat = rotleft * rotZ * g_mouse_camera.mat;

		rprCameraSetTransform(g_camera, false, &newMat.m00);
	}

	// camera moved, so we need to redraw the framebuffer.
	CHECK(rprFrameBufferClear(g_frame_buffer));

	return;
}

void OnMouseEvent(int button, int state, int x, int y)
{
	if (button == GLUT_LEFT_BUTTON)
	{
		if (state == GLUT_DOWN)
		{
			g_mouse_camera.leftMouseButtonDown = true;

			rprCameraGetInfo(g_camera, RPR_CAMERA_LOOKAT, sizeof(g_mouse_camera.lookat), &g_mouse_camera.lookat, 0);
			rprCameraGetInfo(g_camera, RPR_CAMERA_UP, sizeof(g_mouse_camera.up), &g_mouse_camera.up, 0);
			rprCameraGetInfo(g_camera, RPR_CAMERA_POSITION, sizeof(g_mouse_camera.pos), &g_mouse_camera.pos, 0);
			rprCameraGetInfo(g_camera, RPR_CAMERA_TRANSFORM, sizeof(g_mouse_camera.mat), &g_mouse_camera.mat, 0);

			g_mouse_camera.mousePosAtMouseButtonDown_X = x;
			g_mouse_camera.mousePosAtMouseButtonDown_Y = y;

		} else if (state == GLUT_UP)
		{
			g_mouse_camera.leftMouseButtonDown = false;

		}
	}

	return;
}

void OnKeyboardEvent(unsigned char key, int xmouse, int ymouse)
{
	bool cameraMoves = false;
	switch (key)
	{
	case 'w':
	case 'z':
	case 's':
	case 'a':
	case 'q':
	case 'd':
	{

		rprCameraGetInfo(g_camera, RPR_CAMERA_LOOKAT, sizeof(g_mouse_camera.lookat), &g_mouse_camera.lookat, 0);
		rprCameraGetInfo(g_camera, RPR_CAMERA_UP, sizeof(g_mouse_camera.up), &g_mouse_camera.up, 0);
		rprCameraGetInfo(g_camera, RPR_CAMERA_TRANSFORM, sizeof(g_mouse_camera.mat), &g_mouse_camera.mat, 0);
		rprCameraGetInfo(g_camera, RPR_CAMERA_POSITION, sizeof(g_mouse_camera.pos), &g_mouse_camera.pos, 0);

		RadeonProRender::float3 lookAtVec = g_mouse_camera.lookat - g_mouse_camera.pos;
		lookAtVec.normalize();

		RadeonProRender::float3 vecRight = RadeonProRender::cross(g_mouse_camera.up, lookAtVec);
		vecRight.normalize();

		cameraMoves = true;

		const float speed = 0.5f;

		switch (key)
		{

		case 'w':
		case 'z': // for azerty keyboard
		{
			g_mouse_camera.pos += lookAtVec * speed;
			break;
		}

		case 's':
		{
			g_mouse_camera.pos -= lookAtVec * speed;
			break;
		}

		case 'a':
		case 'q': // for azerty keyboard
		{
			g_mouse_camera.pos += vecRight * speed;
			break;
		}

		case 'd':
		{
			g_mouse_camera.pos -= vecRight * speed;
			break;
		}


		}

		break;
	}


	case 'x':
	{
		g_askExit = true;
		break;
	}

	default:
		break;
	}

	if (cameraMoves)
	{
		g_mouse_camera.mat.m30 = g_mouse_camera.pos.x;
		g_mouse_camera.mat.m31 = g_mouse_camera.pos.y;
		g_mouse_camera.mat.m32 = g_mouse_camera.pos.z;

		rprCameraSetTransform(g_camera, false, &g_mouse_camera.mat.m00);

		// camera moved, so we need to redraw the framebuffer.
		CHECK(rprFrameBufferClear(g_frame_buffer));
	}

}

void Display()
{
	// Clear backbuffer
	glClear(GL_COLOR_BUFFER_BIT);

	// Bind vertex & index buffers of a quad
	glBindBuffer(GL_ARRAY_BUFFER, g_vertex_buffer_id);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_index_buffer_id);

	// Set shaders
	GLuint program = g_shader_manager.GetProgram("../32_gl_interop/simple");
	glUseProgram(program);

	// Set texture with the image rendered by FR
	GLuint texture_loc = glGetUniformLocation(program, "g_Texture");
	CHECK_GE(texture_loc, 0);

	glUniform1i(texture_loc, 0);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, g_texture);
	//glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, GL_RGBA, GL_FLOAT, static_cast<const GLvoid*>(g_fbdata));         

	GLuint position_attr_id = glGetAttribLocation(program, "inPosition");
	GLuint texcoord_attr_id = glGetAttribLocation(program, "inTexcoord");

	glVertexAttribPointer(position_attr_id, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 5, 0);
	glVertexAttribPointer(texcoord_attr_id, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 5, (void*)(sizeof(float) * 3));

	glEnableVertexAttribArray(position_attr_id);
	glEnableVertexAttribArray(texcoord_attr_id);

	// Draw quad with the texture on top of it
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, NULL);

	glDisableVertexAttribArray(position_attr_id);
	glDisableVertexAttribArray(texcoord_attr_id);

	glBindTexture(GL_TEXTURE_2D, 0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glUseProgram(0);

	// Present backbuffer
	glutSwapBuffers();
}


void InitGraphics()
{
	// Set states
	glClearColor(0.0, 0.0, 0.0, 1.0);
	glCullFace(GL_NONE);
	glDisable(GL_DEPTH_TEST);

	// Viewport
	glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
	// Enable texturing
	glEnable(GL_TEXTURE_2D);

	// Create vertex and index buffer for a quad
	glGenBuffers(1, &g_vertex_buffer_id);
	glGenBuffers(1, &g_index_buffer_id);

	glBindBuffer(GL_ARRAY_BUFFER, g_vertex_buffer_id);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_index_buffer_id);

	float quadVertexData[] =
	{
		-1, -1, 0.5, 0, 0,
		1, -1, 0.5, 1, 0,
		1,  1, 0.5, 1, 1,
		-1,  1, 0.5, 0, 1
	};

	GLshort quadIndexData[] =
	{
		0, 1, 3,
		3, 1, 2
	};

	glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertexData), quadVertexData, GL_STATIC_DRAW);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(quadIndexData), quadIndexData, GL_STATIC_DRAW);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	// Create a texture for FR rendering
	glActiveTexture(GL_TEXTURE0);
	glGenTextures(1, &g_texture);

	glBindTexture(GL_TEXTURE_2D, g_texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, WINDOW_WIDTH, WINDOW_HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);

	glBindTexture(GL_TEXTURE_2D, 0);
}

void OnExit()
{
	//// Release the stuff we created
	std::cout << "Release memory...\n";
	CHECK(rprObjectDelete(g_camera)); g_camera = nullptr;
	CHECK(rprObjectDelete(g_matsys)); g_matsys = nullptr;
	CHECK(rprObjectDelete(g_frame_buffer)); g_frame_buffer = nullptr;
	CHECK(rprObjectDelete(g_frame_buffer_2)); g_frame_buffer_2 = nullptr;
	g_gc.GCClean();
	CHECK(rprObjectDelete(g_scene)); g_scene = nullptr;
	CheckNoLeak(g_context);
	CHECK(rprObjectDelete(g_context)); g_context = nullptr; // Always delete the RPR Context in last.
}
