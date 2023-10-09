/*****************************************************************************\
*
*  Module Name    gl_interop.cpp
*  Project        Radeon ProRender SDK rendering tutorial
*
*  Description    Radeon ProRender SDK tutorials 
*                 Demo of an OpenGL window rendering RPR.
*
*  Copyright(C) 2011-2021 Advanced Micro Devices, Inc. All rights reserved.
*
\*****************************************************************************/



//
// Demo covering an RPR rendering inside an OpenGL app.
// rotate camera with mouse left-click + move
// move camera with W/A/S/D keys
// press X key to exit
//


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
#include "ShaderManager.h"
#include "RprLoadStore.h"//For Export //202310

#include "../common/picojson.h" //202310
#define TINYOBJLOADER_IMPLEMENTATION //202310
#include "../common/tiny_obj_loader.h" //202310


#ifdef __APPLE__
	#ifndef GL_RGBA32F
	#define GL_RGBA32F GL_RGBA32F_ARB
	#endif 
#endif

#include <cassert>
#include <iostream>
#include <thread>
#include <memory>
#include <map> //202310

//#include <cuda_runtime.h>
//#include <device_launch_parameters.h>

//const unsigned int WINDOW_WIDTH = 640;
//const unsigned int WINDOW_HEIGHT = 480;
const unsigned int WINDOW_WIDTH  = 80*15;
const unsigned int WINDOW_HEIGHT = 60*15;

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
	void notifyUpdate( float x, void* userData )
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
rpr_scene			g_scene=nullptr;
rpr_camera			g_camera=nullptr;
std::shared_ptr<float>	g_fbdata = nullptr;
RPRGarbageCollector g_gc;
bool                g_askExit = false; // push X key to exit



GuiRenderImpl::Update g_update;
const auto g_invalidTime = std::chrono::time_point<std::chrono::high_resolution_clock>::max();
int					g_benchmark_numberOfRenderIteration = 0;
auto g_benchmark_start = g_invalidTime;

//202309
const size_t n = 128;
float atomposi[] = {
	20.0f,30.0f,60.0f,
	20.0f,40.0f,70.0f,
	40.0f,30.0f,60.0f,
	50.0f,70.0f,80.0f,
	50.0f,80.0f,90.0f,
	70.0f,70.0f,80.0f,
};
const int atomnum = sizeof(atomposi) / 3.0f;
const float atomrad = 20.0f;


//202310 read lammps format-v1

void loadDataFromFile(const std::string& filename,
	std::vector<int>& sphere_species,
	std::vector<std::tuple<float, float, float>>& sphere_positions,
	float x_length, float y_length, float z_length) {

	float x_min = std::numeric_limits<float>::max();
	float y_min = std::numeric_limits<float>::max();
	float z_min = std::numeric_limits<float>::max();
	float x_max = std::numeric_limits<float>::lowest();
	float y_max = std::numeric_limits<float>::lowest();
	float z_max = std::numeric_limits<float>::lowest();

	//open file
	std::ifstream file(filename); 

	//check error
	if (!file.is_open()) { 
		std::cerr << "Failed to open the file!" << std::endl;
		return;
	}

	std::string line;
	int total_atoms = 0;
	bool atoms_section = false;

	// read each line
	while (std::getline(file, line)) {
		std::istringstream ss(line);

		if (line.find("atoms") != std::string::npos) {
			ss >> total_atoms;
		}
		else if (line.find("Atoms") != std::string::npos) {
			atoms_section = true;
			std::getline(file, line);
			continue;
		}
		else if (atoms_section) {
			if (sphere_species.size() >= total_atoms) {
				break; 
			}

			int index, type;
			float x, y, z, dummy;

			ss >> index >> type >> dummy >> x >> y >> z;

			x_min = std::min(x_min, x);
			y_min = std::min(y_min, y);
			z_min = std::min(z_min, z);
			x_max = std::max(x_max, x);
			y_max = std::max(y_max, y);
			z_max = std::max(z_max, z);

			sphere_species.push_back(type);
			sphere_positions.push_back({ x, y, z });
		}
	}

	// Normalize the positions
	for (auto& position : sphere_positions) {
		float& x = std::get<0>(position);
		float& y = std::get<1>(position);
		float& z = std::get<2>(position);

		x = ((x - x_min) / (x_max - x_min) * x_length) - x_length / 2.0f;
		y = ((y - y_min) / (y_max - y_min) * y_length) - y_length / 2.0f;
		z = ((z - z_min) / (z_max - z_min) * z_length) - z_length / 2.0f;
	}

	if (sphere_species.size() < total_atoms) {
		std::cerr << "Warning: Expected " << total_atoms << " atoms, but found only " << sphere_species.size() << std::endl;
	}

	file.close();
}

//202310

std::tuple<std::vector<int>, std::vector<std::tuple<float, float, float>>, int, float> getData() {
	int readfileflag = 1; //** Please change **//
	std::vector<int> sphere_species;
	std::vector<std::tuple<float, float, float>> sphere_positions;
	int atom_num = 0;
	const float atom_rad = 0.5f;

	if (readfileflag != 0) {
		std::cout << "Data mode: FILE Reading \n";
		float x_length = 10.0f;
		float y_length = 10.0f;
		float z_length = 10.0f;
		loadDataFromFile("../89_sample_data/data.FeOH3", sphere_species, sphere_positions, x_length, y_length, z_length);
		atom_num = sphere_positions.size();
	}
	else {
		std::cout << "Data mode: Sample vaules \n";
		sphere_positions = {
			{1.0f,1.0f,1.0f},
			{2.0f,2.0f,2.0f},
			{3.0f,5.0f,8.0f},
			{4.0f,9.0f,7.0f},
			{5.0f,1.0f,4.0f},
			{6.0f,6.0f,6.0f}
		};
		sphere_species = { 1, 2, 3, 1, 2, 2 };
		atom_num = sphere_positions.size();
	}

	for (size_t i = 0; i < sphere_species.size(); ++i) {
		std::cout << i << " " << sphere_species[i] << " ";
		float x0 = std::get<0>(sphere_positions[i]);
		float y0 = std::get<1>(sphere_positions[i]);
		float z0 = std::get<2>(sphere_positions[i]);
		std::cout << x0 << " " << y0 << " " << z0 << std::endl;
	}

	return std::make_tuple(sphere_species, sphere_positions, atom_num, atom_rad);
}



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
void renderJob( rpr_context ctxt, GuiRenderImpl::Update* update )
{
	CHECK( rprContextRender( ctxt ) );
	update->m_done = 1;
	return;
}


void Update()
{
	const auto timeUpdateStarts = std::chrono::high_resolution_clock::now();


	//
	// print render stats every ~100 iterations.
	//
	if ( g_benchmark_start == g_invalidTime )
		g_benchmark_start = timeUpdateStarts;
	if ( g_benchmark_numberOfRenderIteration >= 100 )
	{
		double elapsed_time_ms = std::chrono::duration<double, std::milli>(timeUpdateStarts - g_benchmark_start).count();
		double renderPerSecond = (double)g_benchmark_numberOfRenderIteration * 1000.0 / elapsed_time_ms;
		std::cout<<renderPerSecond<<" iterations per second."<<std::endl;
		g_benchmark_numberOfRenderIteration = 0;
		g_benchmark_start = timeUpdateStarts;

	}


	// clear state
	g_update.clear();

	// start the rendering thread
	std::thread t( &renderJob, g_context, &g_update );

	// wait the rendering thread
	while( !g_update.m_done )
	{
		// at each update of the rendering thread
		if( g_update.m_hasUpdate )
		{
			// Read the frame buffer from RPR
			// Note that rprContextResolveFrameBuffer and rprFrameBufferGetInfo(fb,RPR_FRAMEBUFFER_DATA)  can be called asynchronous while rprContextRender is running.

			CHECK( rprContextResolveFrameBuffer( g_context, g_frame_buffer, g_frame_buffer_2, false ) );
			size_t frame_buffer_dataSize = 0;
			CHECK( rprFrameBufferGetInfo( g_frame_buffer_2, RPR_FRAMEBUFFER_DATA, 0 , NULL , &frame_buffer_dataSize ) );

			// check that the size fits with original buffer alloc
			if ( frame_buffer_dataSize != WINDOW_WIDTH * WINDOW_HEIGHT * 4 * sizeof(float) )
			{
				CHECK(RPR_ERROR_INTERNAL_ERROR)
			}

			CHECK( rprFrameBufferGetInfo( g_frame_buffer_2, RPR_FRAMEBUFFER_DATA, frame_buffer_dataSize , g_fbdata.get() , NULL ) );

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

	if ( g_askExit )
	{
		OnExit();
		std::exit(EXIT_SUCCESS);
	}

	g_benchmark_numberOfRenderIteration += g_batchSize;

	return;
}

void OnMouseMoveEvent(int x, int y)
{
	int delaX =  (x - g_mouse_camera.mousePosAtMouseButtonDown_X);
	int delaY = -(y - g_mouse_camera.mousePosAtMouseButtonDown_Y);

	if ( g_mouse_camera.leftMouseButtonDown )
	{
		RadeonProRender::matrix rotZ = RadeonProRender::rotation(g_mouse_camera.up, (float)delaX * 0.001);

		RadeonProRender::float3 lookAtVec = g_mouse_camera.lookat - g_mouse_camera.pos;
		lookAtVec.normalize();

		RadeonProRender::float3 left = RadeonProRender::cross(g_mouse_camera.up, lookAtVec);
		left.normalize();

		RadeonProRender::matrix rotleft = RadeonProRender::rotation(left, (float)delaY * 0.001);
		RadeonProRender::matrix newMat =  rotleft * rotZ * g_mouse_camera.mat ;

		rprCameraSetTransform( g_camera, false, &newMat.m00 );
	}

	// camera moved, so we need to redraw the framebuffer.
	CHECK( rprFrameBufferClear(g_frame_buffer) );

	return;
}

void OnMouseEvent(int button, int state, int x, int y)
{
	if ( button ==  GLUT_LEFT_BUTTON )
	{
		if ( state == GLUT_DOWN )
		{
			g_mouse_camera.leftMouseButtonDown = true;

			rprCameraGetInfo(g_camera,RPR_CAMERA_LOOKAT,sizeof(g_mouse_camera.lookat),&g_mouse_camera.lookat,0);
			rprCameraGetInfo(g_camera,RPR_CAMERA_UP,sizeof(g_mouse_camera.up),&g_mouse_camera.up,0);
			rprCameraGetInfo(g_camera,RPR_CAMERA_POSITION,sizeof(g_mouse_camera.pos),&g_mouse_camera.pos,0);
			rprCameraGetInfo(g_camera,RPR_CAMERA_TRANSFORM,sizeof(g_mouse_camera.mat),&g_mouse_camera.mat,0);

			g_mouse_camera.mousePosAtMouseButtonDown_X = x;
			g_mouse_camera.mousePosAtMouseButtonDown_Y = y;

		}
		else if ( state == GLUT_UP )
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
	case 'r':
	case 'f':
	{
		rprCameraGetInfo(g_camera, RPR_CAMERA_LOOKAT, sizeof(g_mouse_camera.lookat), &g_mouse_camera.lookat, 0);
		rprCameraGetInfo(g_camera, RPR_CAMERA_UP, sizeof(g_mouse_camera.up), &g_mouse_camera.up, 0);
		rprCameraGetInfo(g_camera, RPR_CAMERA_TRANSFORM, sizeof(g_mouse_camera.mat), &g_mouse_camera.mat, 0);
		rprCameraGetInfo(g_camera, RPR_CAMERA_POSITION, sizeof(g_mouse_camera.pos), &g_mouse_camera.pos, 0);
		RadeonProRender::float3 lookAtVec = g_mouse_camera.lookat - g_mouse_camera.pos;
		lookAtVec.normalize();
		RadeonProRender::float3 vecRight = RadeonProRender::cross(g_mouse_camera.up, lookAtVec);
		vecRight.normalize();
		RadeonProRender::float3 vecUp = g_mouse_camera.up;
		vecUp.normalize();
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
		case 'r':
		{
			g_mouse_camera.pos += vecUp * speed;
			break;
		}
		case 'f':
		{
			g_mouse_camera.pos -= vecUp * speed;
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
	CHECK_GE(texture_loc , 0);

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
	std::cout <<"Release memory...\n";
	CHECK(rprObjectDelete(g_camera));g_camera=nullptr;
	CHECK(rprObjectDelete(g_matsys));g_matsys=nullptr;
	CHECK(rprObjectDelete(g_frame_buffer));g_frame_buffer=nullptr;
	CHECK(rprObjectDelete(g_frame_buffer_2));g_frame_buffer_2=nullptr;
	g_gc.GCClean();
	CHECK(rprObjectDelete(g_scene));g_scene=nullptr;
	CheckNoLeak(g_context);
	CHECK(rprObjectDelete(g_context));g_context=nullptr; // Always delete the RPR Context in last.
}

/*

__global__ void processKernel(unsigned int atomnum, int n, float* atomposi, unsigned int* indicesList, float* gridVector1, float* gridVector2)
{
	// Thread identifiers
	unsigned int x = blockIdx.x * blockDim.x + threadIdx.x;
	unsigned int y = blockIdx.y * blockDim.y + threadIdx.y;
	unsigned int z = blockIdx.z * blockDim.z + threadIdx.z;

	// Ensure within grid dimensions
	if (x < n && y < n && z < n)
	{
		for (unsigned int i = 0; i < atomnum; i++)
		{
			const int j = i * 3;
			float radius = sqrtf((x - atomposi[j]) * (x - atomposi[j])
				+ (y - atomposi[j + 1]) * (y - atomposi[j + 1])
				+ (z - atomposi[j + 2]) * (z - atomposi[j + 2]));

			if (radius < atomrad)
			{
				// NOTE: You cannot directly use push_back() in CUDA.
				// You might need an atomic operation or preallocated memory to store these values.

				if (radius <= atomrad)
				{
					gridVector1[x * n * n + y * n + z] = 1.0f;  // Example of indexing for 3D data
				}
				else
				{
					gridVector1[x * n * n + y * n + z] = 0.0f;
				}

				gridVector2[x * n * n + y * n + z] = 1.0f //(float)j / (float)atomnum;
			}
		}
	}
}

*/


//202310
struct Vertex {
	float x, y, z;
};

rpr_shape createSphere(rpr_context context, float radius, unsigned int segments) {
	std::vector<Vertex> vertices;
	std::vector<int> indices;

	for (unsigned int y = 0; y <= segments; ++y) {
		for (unsigned int x = 0; x <= segments; ++x) {
			float xSegment = (float)x / (float)segments;
			float ySegment = (float)y / (float)segments;
			float M_PI = 3.14159265f;
			float xPos = std::cos(xSegment * 2.0f * M_PI) * std::sin(ySegment * M_PI) * radius;
			float yPos = std::cos(ySegment * M_PI) * radius;
			float zPos = std::sin(xSegment * 2.0f * M_PI) * std::sin(ySegment * M_PI) * radius;

			vertices.push_back({ xPos, yPos, zPos });
		}
	}

	bool oddRow = false;
	for (int y = 0; y < segments; ++y) {
		if (!oddRow) {
			for (int x = 0; x < segments; ++x) {
				indices.push_back(y * (segments + 1) + x);
				indices.push_back((y + 1) * (segments + 1) + x);
				indices.push_back(y * (segments + 1) + x + 1);

				indices.push_back(y * (segments + 1) + x + 1);
				indices.push_back((y + 1) * (segments + 1) + x);
				indices.push_back((y + 1) * (segments + 1) + x + 1);
			}
		}
		else {
			for (int x = segments; x > 0; --x) {
				indices.push_back((y + 1) * (segments + 1) + x);
				indices.push_back(y * (segments + 1) + x - 1);
				indices.push_back(y * (segments + 1) + x);

				indices.push_back((y + 1) * (segments + 1) + x);
				indices.push_back((y + 1) * (segments + 1) + x - 1);
				indices.push_back(y * (segments + 1) + x - 1);
			}
		}
		oddRow = !oddRow;
	}

	rpr_shape sphere;
	rpr_float* vertexData = reinterpret_cast<rpr_float*>(vertices.data());
	rpr_int* indexData = indices.data();
	rpr_uint vertexStride = sizeof(Vertex);
	rpr_uint numIndices = static_cast<rpr_uint>(indices.size());
	size_t numTriangles = numIndices / 3; 
	std::vector<rpr_int> num_face_vertices(numTriangles, 3);  

	if (rprContextCreateMesh(context,
		vertexData, vertices.size(), vertexStride,
		NULL, 0, 0,
		NULL, 0, 0,
		indexData, sizeof(rpr_int),
		NULL, sizeof(rpr_int),
		NULL, sizeof(rpr_int),
		num_face_vertices.data(),(numIndices/3), &sphere )!= RPR_SUCCESS)
	{
		std::cout << "ERROR at rprContextCreateMesh \n";
	}

	return sphere;
}

void setShapeTransform(rpr_shape shape, const float* matrix) {
	rprShapeSetTransform(shape, RPR_TRUE, matrix);
}
std::vector<float> translateMatrix(float x, float y, float z) {
	std::vector<float> matrix = {
		1.0f, 0.0f, 0.0f, x,
		0.0f, 1.0f, 0.0f, y,
		0.0f, 0.0f, 1.0f, z,
		0.0f, 0.0f, 0.0f, 1.0f
	};

	return matrix;
}

rpr_material_node createUberMaterial(rpr_material_system matsys, float r, float g, float b);

rpr_material_node createUberMaterial(rpr_material_system matsys, float r, float g, float b) {
	rpr_material_node uberMaterial = nullptr;
	CHECK(rprMaterialSystemCreateNode(g_matsys, RPR_MATERIAL_NODE_UBERV2, &uberMaterial));
	CHECK(rprMaterialNodeSetInputFByKey(uberMaterial, RPR_MATERIAL_INPUT_UBER_DIFFUSE_COLOR,     r, g, b, 1.0f));

	CHECK(rprMaterialNodeSetInputFByKey(uberMaterial, RPR_MATERIAL_INPUT_UBER_DIFFUSE_WEIGHT,    1.5f, 1.5f, 1.5f, 1.5f));
	CHECK(rprMaterialNodeSetInputFByKey(uberMaterial, RPR_MATERIAL_INPUT_UBER_REFLECTION_COLOR,  r, g, b, 1.0f));
	CHECK(rprMaterialNodeSetInputFByKey(uberMaterial, RPR_MATERIAL_INPUT_UBER_REFLECTION_WEIGHT, 0.2f, 0.2f, 0.2f, 0.2f));
	CHECK(rprMaterialNodeSetInputFByKey(uberMaterial, RPR_MATERIAL_INPUT_UBER_REFLECTION_ROUGHNESS, 0.3f, 0.3f, 0.3f, 0.3f));
	CHECK(rprMaterialNodeSetInputFByKey(uberMaterial, RPR_MATERIAL_INPUT_UBER_REFLECTION_ANISOTROPY,0, 0, 0, 0));
	CHECK(rprMaterialNodeSetInputFByKey(uberMaterial, RPR_MATERIAL_INPUT_UBER_REFLECTION_ANISOTROPY_ROTATION, 0, 0, 0, 0));
//	CHECK(rprMaterialNodeSetInputUByKey(uberMaterial, RPR_MATERIAL_INPUT_UBER_REFLECTION_MODE,RPR_UBER_MATERIAL_IOR_MODE_METALNESS));
	CHECK(rprMaterialNodeSetInputFByKey(uberMaterial, RPR_MATERIAL_INPUT_UBER_REFLECTION_IOR, 0.8f, 0.8f, 0.8f, 0.8f));
	return uberMaterial;
}
////	CHECK(rprMaterialNodeSetInputNByKey(uberMat2, RPR_MATERIAL_INPUT_UBER_DIFFUSE_COLOR   ,uberMat2_imgTexture1));
//	CHECK(rprMaterialNodeSetInputNByKey(uberMat2, RPR_MATERIAL_INPUT_UBER_DIFFUSE_NORMAL   ,matNormalMap));
//	CHECK(rprMaterialNodeSetInputFByKey(uberMat2, RPR_MATERIAL_INPUT_UBER_DIFFUSE_WEIGHT    ,1, 1, 1, 1));
//
//	CHECK(rprMaterialNodeSetInputNByKey(uberMat2, RPR_MATERIAL_INPUT_UBER_REFLECTION_COLOR  ,uberMat2_imgTexture1));
//	CHECK(rprMaterialNodeSetInputNByKey(uberMat2, RPR_MATERIAL_INPUT_UBER_REFLECTION_NORMAL   ,matNormalMap));
////	CHECK(rprMaterialNodeSetInputFByKey(uberMat2, RPR_MATERIAL_INPUT_UBER_REFLECTION_WEIGHT  ,1, 1, 1, 1));
////	CHECK(rprMaterialNodeSetInputFByKey(uberMat2, RPR_MATERIAL_INPUT_UBER_REFLECTION_ROUGHNESS     ,0, 0, 0, 0));
//	CHECK(rprMaterialNodeSetInputFByKey(uberMat2, RPR_MATERIAL_INPUT_UBER_REFLECTION_ANISOTROPY    ,0, 0, 0, 0));
//	CHECK(rprMaterialNodeSetInputFByKey(uberMat2, RPR_MATERIAL_INPUT_UBER_REFLECTION_ANISOTROPY_ROTATION  ,0, 0, 0, 0));
//	CHECK(rprMaterialNodeSetInputUByKey(uberMat2, RPR_MATERIAL_INPUT_UBER_REFLECTION_MODE   ,RPR_UBER_MATERIAL_IOR_MODE_METALNESS));
//	CHECK(rprMaterialNodeSetInputFByKey(uberMat2, RPR_MATERIAL_INPUT_UBER_REFLECTION_IOR   ,1.36, 1.36, 1.36, 1.36));

int main(int argc, char** argv)
{
	//202310
	std::vector<int> sphere_species;
	std::vector<std::tuple<float, float, float>> sphere_positions;
	int atom_num;
	float atom_rad;

	std::tie(sphere_species, sphere_positions, atom_num, atom_rad) = getData();


	//	enable RPR API trace
	//	set this before any RPR API calls
	//	rprContextSetParameter1u(0,RPR_CONTEXT_TRACING_ENABLED,1);

	//GL setup
	{
		// Initialize GLUT and GLEW libraries
		glutInit(&argc, (char**)argv);
		glutInitWindowSize(WINDOW_WIDTH, WINDOW_HEIGHT);
		glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE);
		glutCreateWindow("gl_interop");
		GLenum err = glewInit();
		if (err != GLEW_OK)
		{
			std::cout << "GLEW initialization failed\n";
			return -1;
		}

		// Set OpenGL states
		InitGraphics();
	}

	std::cout << "RPR SDK simple rendering tutorial.\n";

	rpr_int status = RPR_SUCCESS;

	// Register the plugin.
	rpr_int tahoePluginID = rprRegisterPlugin(RPR_PLUGIN_FILE_NAME); 
	CHECK_NE(tahoePluginID , -1)
	rpr_int plugins[] = { tahoePluginID };
	size_t pluginCount = sizeof(plugins) / sizeof(plugins[0]);

	// Create context using a single GPU 
	status = rprCreateContext(RPR_API_VERSION, plugins, pluginCount, g_ContextCreationFlags 
		#ifndef USING_NORTHSTAR   // note that for Northstar, we don't need the GL_INTEROP flag
		| RPR_CREATION_FLAGS_ENABLE_GL_INTEROP
		#endif
		, g_contextProperties, NULL, &g_context);
	CHECK(status);

	// Set active plugin.
	CHECK( rprContextSetActivePlugin(g_context, plugins[0]) );

	CHECK( rprContextCreateMaterialSystem(g_context, 0, &g_matsys) );

	std::cout << "Context successfully created.\n";

	// Create a scene
	CHECK( rprContextCreateScene(g_context, &g_scene) );

	// Create an environment light
	//CHECK( CreateNatureEnvLight(g_context, g_scene, g_gc, 0.9f) );
	CHECK(CreateNatureEnvLightIN(g_context, g_scene, g_gc, 0.9f, "../../Resources/Textures/amd.png")); //"../../Resources/Textures/turning_area_4k.hdr"

	// Create camera
	{
		CHECK( rprContextCreateCamera(g_context, &g_camera) );

		// Position camera in world space: 
		CHECK( rprCameraLookAt(g_camera, 0.0f, 5.0f, 20.0f,    0, 1, 0,   0, 1, 0) );

		// set camera field of view
		CHECK( rprCameraSetFocalLength(g_camera, 30.0f) );

		// Set camera for the scene
		CHECK( rprSceneSetCamera(g_scene, g_camera) );
	}
	// Set scene to render for the context
	CHECK( rprContextSetScene(g_context, g_scene) );


	// create a teapot shape
	//rpr_shape teapot01 = nullptr;
	//{
	//	teapot01 = ImportOBJ("../../Resources/Meshes/teapot.obj",g_scene,g_context);
	//	g_gc.GCAdd(teapot01);
	//
	//	RadeonProRender::matrix m0 = RadeonProRender::rotation_x(MY_PI);
	//	CHECK(rprShapeSetTransform(teapot01, RPR_TRUE, &m0.m00));
	//}
	//
	//// create the floor
	//CHECK( CreateAMDFloor(g_context, g_scene, g_matsys, g_gc, 1.0f, 1.0f)  );
	//
	// Create material for the teapot
	//{
	//	rpr_image uberMat2_img1 = nullptr;
	//	CHECK(rprContextCreateImageFromFile(g_context,"../../Resources/Textures/lead_rusted_Base_Color.jpg",&uberMat2_img1));
	//	g_gc.GCAdd(uberMat2_img1);
	//
	//	rpr_image uberMat2_img2 = nullptr;
	//	CHECK(rprContextCreateImageFromFile(g_context,"../../Resources/Textures/lead_rusted_Normal.jpg",&uberMat2_img2));
	//	g_gc.GCAdd(uberMat2_img2);
	//
	//	rpr_material_node uberMat2_imgTexture1 = nullptr;
	//	CHECK(rprMaterialSystemCreateNode(g_matsys,RPR_MATERIAL_NODE_IMAGE_TEXTURE,&uberMat2_imgTexture1));
	//	g_gc.GCAdd(uberMat2_imgTexture1);
	//	CHECK(rprMaterialNodeSetInputImageDataByKey(uberMat2_imgTexture1,   RPR_MATERIAL_INPUT_DATA  ,uberMat2_img1));
	//
	//	rpr_material_node uberMat2_imgTexture2 = nullptr;
	//	CHECK(rprMaterialSystemCreateNode(g_matsys,RPR_MATERIAL_NODE_IMAGE_TEXTURE,&uberMat2_imgTexture2));
	//	g_gc.GCAdd(uberMat2_imgTexture2);
	//	CHECK(rprMaterialNodeSetInputImageDataByKey(uberMat2_imgTexture2,   RPR_MATERIAL_INPUT_DATA  ,uberMat2_img2));
	//
	//	rpr_material_node matNormalMap = nullptr;
	//	CHECK( rprMaterialSystemCreateNode(g_matsys,RPR_MATERIAL_NODE_NORMAL_MAP,&matNormalMap));
	//	g_gc.GCAdd(matNormalMap);
	//	CHECK( rprMaterialNodeSetInputFByKey(matNormalMap,RPR_MATERIAL_INPUT_SCALE,1.0f,1.0f,1.0f,1.0f));
	//	CHECK( rprMaterialNodeSetInputNByKey(matNormalMap,RPR_MATERIAL_INPUT_COLOR,uberMat2_imgTexture2));
	//
	//	rpr_material_node uberMat2 = nullptr;
	//	CHECK(rprMaterialSystemCreateNode(g_matsys,RPR_MATERIAL_NODE_UBERV2,&uberMat2));
	//	g_gc.GCAdd(uberMat2);
	//
	//	CHECK(rprMaterialNodeSetInputNByKey(uberMat2, RPR_MATERIAL_INPUT_UBER_DIFFUSE_COLOR   ,uberMat2_imgTexture1));
	//	CHECK(rprMaterialNodeSetInputNByKey(uberMat2, RPR_MATERIAL_INPUT_UBER_DIFFUSE_NORMAL   ,matNormalMap));
	//	CHECK(rprMaterialNodeSetInputFByKey(uberMat2, RPR_MATERIAL_INPUT_UBER_DIFFUSE_WEIGHT    ,1, 1, 1, 1));
	//
	//	CHECK(rprMaterialNodeSetInputNByKey(uberMat2, RPR_MATERIAL_INPUT_UBER_REFLECTION_COLOR  ,uberMat2_imgTexture1));
	//	CHECK(rprMaterialNodeSetInputNByKey(uberMat2, RPR_MATERIAL_INPUT_UBER_REFLECTION_NORMAL   ,matNormalMap));
	//	CHECK(rprMaterialNodeSetInputFByKey(uberMat2, RPR_MATERIAL_INPUT_UBER_REFLECTION_WEIGHT  ,1, 1, 1, 1));
	//	CHECK(rprMaterialNodeSetInputFByKey(uberMat2, RPR_MATERIAL_INPUT_UBER_REFLECTION_ROUGHNESS     ,0, 0, 0, 0));
	//	CHECK(rprMaterialNodeSetInputFByKey(uberMat2, RPR_MATERIAL_INPUT_UBER_REFLECTION_ANISOTROPY    ,0, 0, 0, 0));
	//	CHECK(rprMaterialNodeSetInputFByKey(uberMat2, RPR_MATERIAL_INPUT_UBER_REFLECTION_ANISOTROPY_ROTATION  ,0, 0, 0, 0));
	//	CHECK(rprMaterialNodeSetInputUByKey(uberMat2, RPR_MATERIAL_INPUT_UBER_REFLECTION_MODE   ,RPR_UBER_MATERIAL_IOR_MODE_METALNESS));
	//	CHECK(rprMaterialNodeSetInputFByKey(uberMat2, RPR_MATERIAL_INPUT_UBER_REFLECTION_IOR   ,1.36, 1.36, 1.36, 1.36));
	//
	//	CHECK(rprShapeSetMaterial(teapot01, uberMat2));
	//}

	//CHECK( rprContextSetParameterByKey1f(g_context, RPR_CONTEXT_DISPLAY_GAMMA , 2.2f ) ); // set display gamma


	//20230920
	//Create material
	{
	 	//CHECK(CreateAMDFloor(g_context, g_scene, g_matsys, g_gc, 0.20f, 0.20f, 0.0f, -1.0f, 0.0f));
		//char pathImageFileA = "../../Resources/Textures/art.jpg";
		CHECK(CreateAMDFloorIN(g_context, g_scene, g_matsys, g_gc, 0.20f, 0.20f, 0.0f, -2.0f, 0.0f, "../../Resources/Textures/amd.png"));

		rpr_mesh_info mesh_properties[16];
		mesh_properties[0] = (rpr_mesh_info)RPR_MESH_VOLUME_FLAG;
		mesh_properties[1] = (rpr_mesh_info)1; // enable the Volume flag for the Mesh
		mesh_properties[2] = (rpr_mesh_info)0;

		// Volume shapes don't need any vertices data: the bounds of volume will only be defined by the grid.
		// Also, make sure to enable the RPR_MESH_VOLUME_FLAG
		rpr_shape cube = 0;
		CHECK(rprContextCreateMeshEx2(g_context,
			nullptr, 0, 0,
			nullptr, 0, 0,
			nullptr, 0, 0, 0,
			nullptr, nullptr, nullptr, nullptr, 0,
			nullptr, 0, nullptr, nullptr, nullptr, 0,
			mesh_properties,
			&cube));

		// bounds of volume will always be a box defined by the rprShapeSetTransform
		RadeonProRender::matrix cubeTransform1 = RadeonProRender::translation(RadeonProRender::float3(0, +0.0f, 0)) * RadeonProRender::rotation_y(0.0f) * RadeonProRender::scale(RadeonProRender::float3(1.0f, 1.0f, 1.0f));
		CHECK(rprShapeSetTransform(cube, true, &cubeTransform1.m00));
		CHECK(rprSceneAttachShape(g_scene, cube));
		// 
		// define the grids data used by the Volume material.
		//const size_t n = 128; //128;
	//	std::vector<unsigned int> indicesList;
	//	std::vector<float> gridVector1;
	//	std::vector<float> gridVector2;
		//const float radiusFadeoutStart = 130.0f;
		//const float radiusFadeoutEnd   = 130.0f;
		//for (unsigned int x = 0; x < n; x++)
		//{
		//	for (unsigned int y = 0; y < n; y++)
		//	{
		//		for (unsigned int z = 0; z < n; z++)
		//		{
		//			float radius = sqrtf(((float)x - (float)n / 2.0f) wwwwwwwwwwwwwwwww* ((float)x - (float)n / 2.0f) + ((float)z - (float)n / 2.0f) * ((float)z - (float)n / 2.0f));
		//			//float radius = sqrtf(((float)x - (float)n / 2.0f) * ((float)x - (float)n / 2.0f) + ((float)y - (float)n / 2.0f) * ((float)y - (float)n / 2.0f) + ((float)z - (float)n / 2.0f) * ((float)z - (float)n / 2.0f));
		//
		//			if (radius < radiusFadeoutEnd)
		//			{
		//				indicesList.push_back(x);
		//				indicesList.push_back(y);
		//				indicesList.push_back(z);
		//
		//				// "gridVector1" is going to be a cylinder
		//				if (radius <= radiusFadeoutStart)
		//				{
		//					gridVector1.push_back(1.0f);
		//				}
		//				else
		//				{
		//					gridVector1.push_back(1.0f - (radius - radiusFadeoutStart) / (radiusFadeoutEnd - radiusFadeoutStart));
		//				}
		//
		//				// "gridVector2" will be a 0->1 ramp along Y-axis
		//				gridVector2.push_back((float)y / (float)n);
		//			}
		//		}
		//	}
		//}
		// 
		//float atomposi[] = {
		//	0.1f,0.1f,0.1f,
		//	0.1f,0.1f,0.2f,
		//	0.2f,0.1f,0.1f,
		//	0.5f,0.5f,0.5f,
		//	0.5f,0.5f,0.6f,
		//	0.6f,0.5f,0.5f,
		//};
		//float atomposi[] = {
		//	10.0f,10.0f,10.0f,
		//	10.0f,10.0f,20.0f,
		//	20.0f,10.0f,10.0f,
		//	50.0f,50.0f,50.0f,
		//	50.0f,50.0f,60.0f,
		//	60.0f,50.0f,50.0f,
		//};
		//const int atomnum = sizeof(atomposi) / 3.0f;
		//const float atomrad = 10.0f;

		////202309
		//dim3 blockDim(8, 8, 8);  // Example block dimensions
		//dim3 gridDim((n + blockDim.x - 1) / blockDim.x,
		//	(n + blockDim.y - 1) / blockDim.y,
		//	(n + blockDim.z - 1) / blockDim.z);
		//
		//processKernel << <gridDim, blockDim >> > (atomnum, n, d_atomposi, d_indicesList, d_gridVector1, d_gridVector2);

	//	for (unsigned int i = 0; i < atomnum; i++)
	//	{
	//		for (unsigned int x = 0; x < n; x++)
	//		{
	//			for (unsigned int y = 0; y < n; y++)
	//			{
	//				for (unsigned int z = 0; z < n; z++)
	//				{
	//					const int j = i * 3;
	//					float radius = sqrtf(((float)x - (float)atomposi[j]) * ((float)x - (float)atomposi[j])
	//								    	+ ((float)y - (float)atomposi[j+1]) * ((float)y - (float)atomposi[j+1])
	//								    	+ ((float)z - (float)atomposi[j+2]) * ((float)z - (float)atomposi[j+2]));
	//					if (radius < atomrad)
	//					{
	//						indicesList.push_back(x);
	//						indicesList.push_back(y);
	//						indicesList.push_back(z);
	//	
	//						//if (radius <= atomrad)
	//						//{
	//						//	gridVector1.push_back(1.0f);
	//						//}
	//						//else
	//						//{
	//						//	gridVector1.push_back(0.0f);
	//						//}
	//	
	//						gridVector1.push_back(1.0f); 
	//						gridVector2.push_back(1.0f);// (float)j / (float)atomnum);
	//					}
	//				}
	//			}
	//		}
	//	}

		//202310v2
		std::vector<std::tuple<float, float, float>> sphere_colors = {
			{1.0f, 0.0f, 0.0f},
			{0.0f, 1.0f, 0.0f},
			{0.0f, 0.0f, 1.0f}
		};
		std::vector<rpr_material_node> materials;

		for (const auto& color : sphere_colors) {
			rpr_material_node uberMaterial = createUberMaterial(g_matsys, std::get<0>(color), std::get<1>(color), std::get<2>(color));
			materials.push_back(uberMaterial);
		}

		for (size_t i = 0; i < sphere_positions.size(); i++) {
			rpr_shape sphere = createSphere(g_context, atom_rad, 100);

			int colorIndex = sphere_species[i] - 1; // -1 is used, because array begin at 0
			rprShapeSetMaterial(sphere, materials[colorIndex]);

			float x, y, z;
			std::tie(x, y, z) = sphere_positions[i];
			setShapeTransform(sphere, translateMatrix(x, y, z).data());
			rprSceneAttachShape(g_scene, sphere);
		}

		//202310v1
	//	for (const auto& color : sphere_colors) {
	//		rpr_material_node diffuseMaterial = nullptr;
	//		CHECK(rprMaterialSystemCreateNode(g_matsys, RPR_MATERIAL_NODE_DIFFUSE, &diffuseMaterial));
	//
	//		float r, g, b;
	//		std::tie(r, g, b) = color;
	//		CHECK(rprMaterialNodeSetInputFByKey(diffuseMaterial, RPR_MATERIAL_INPUT_COLOR, r, g, b, 1.0f));
	//		materials.push_back(diffuseMaterial);
	//	}
	//
	//	for (size_t i = 0; i < sphere_positions.size(); i++) {
	//		rpr_shape sphere = createSphere(g_context, atom_rad, 50);
	//
	//		int colorIndex = sphere_species[i] - 1; // -1 is used, because array begin at 0
	//		rprShapeSetMaterial(sphere, materials[colorIndex]);
	//
	//		float x, y, z;
	//		std::tie(x, y, z) = sphere_positions[i];
	//		setShapeTransform(sphere, translateMatrix(x, y, z).data());
	//		rprSceneAttachShape(g_scene, sphere);
	//	}



		//202310v0
	//	for (size_t i = 0; i < sphere_positions.size(); i++) {
	//		rpr_shape sphere = createSphere(g_context, atom_rad, 50); //50=segments
	//
	//		float r, g, b;
	//		std::tie(r, g, b) = sphere_colors[i % sphere_colors.size()];
	//
	//		rpr_material_node diffuseMaterial = nullptr;
	//		CHECK(rprMaterialSystemCreateNode(g_matsys, RPR_MATERIAL_NODE_DIFFUSE, &diffuseMaterial));
	//		CHECK(rprMaterialNodeSetInputFByKey(diffuseMaterial, RPR_MATERIAL_INPUT_COLOR, r, g, b, 1.0f));
	//		rprShapeSetMaterial(sphere, diffuseMaterial);
	//
	//		//rprShapeSetMaterial(sphere, materials[i % materials.size()]);
	//		float x, y, z;
	//		std::tie(x, y, z) = sphere_positions[i];
	//		setShapeTransform(sphere, translateMatrix(x, y, z).data());
	//		rprSceneAttachShape(g_scene, sphere);
	//	}








//		// this first grid defines a cylinder
//		rpr_grid rprgrid1 = 0;
//		CHECK(rprContextCreateGrid(g_context, &rprgrid1,
//			n, n, n,
//			&indicesList[0], indicesList.size() / 3, RPR_GRID_INDICES_TOPOLOGY_XYZ_U32,
//			&gridVector1[0], gridVector1.size() * sizeof(gridVector1[0]), 0
//		));
//
//		// GRID_SAMPLER could be compared to a 3d-texture sampler. 
//		// input is a 3d grid,  output is the sampled value from grid
//		rpr_material_node gridSampler1 = NULL;
//		CHECK(rprMaterialSystemCreateNode(g_matsys, RPR_MATERIAL_NODE_GRID_SAMPLER, &gridSampler1));
//		CHECK(rprMaterialNodeSetInputGridDataByKey(gridSampler1, RPR_MATERIAL_INPUT_DATA, rprgrid1));
//
//		// This second grid is a gradient along the Y axis.
//		rpr_grid rprgrid2 = 0;
//		CHECK(rprContextCreateGrid(g_context, &rprgrid2,
//			n, n, n,
//			&indicesList[0], indicesList.size() / 3, RPR_GRID_INDICES_TOPOLOGY_XYZ_U32,
//			&gridVector2[0], gridVector2.size() * sizeof(gridVector2[0]), 0
//		));
//
//		// create grid sample for grid2
//		rpr_material_node gridSampler2 = NULL;
//		CHECK(rprMaterialSystemCreateNode(g_matsys, RPR_MATERIAL_NODE_GRID_SAMPLER, &gridSampler2));
//		CHECK(rprMaterialNodeSetInputGridDataByKey(gridSampler2, RPR_MATERIAL_INPUT_DATA, rprgrid2));
//
//		// create a gradient color texture, here 3 pixels : Red, Green, Blue.
//		// will be used as a lookup output 
//		float rampData2[] = {
//			1.f,0.f,0.f,
//			0.f,1.f,0.f,
//			0.f,0.f,1.f 
//		};
//		rpr_image rampimg2 = 0;
//		rpr_image_desc rampDesc2;
//		rampDesc2.image_width = sizeof(rampData2) / (3 * sizeof(float));
//		rampDesc2.image_height = 1;
//		rampDesc2.image_depth = 0;
//		rampDesc2.image_row_pitch = rampDesc2.image_width * sizeof(rpr_float) * 3;
//		rampDesc2.image_slice_pitch = 0;
//		CHECK(rprContextCreateImage(g_context, { 3, RPR_COMPONENT_TYPE_FLOAT32 }, & rampDesc2, rampData2, & rampimg2));
//
//		// this texture will be used for the color of the volume material.
//		// UV input is the 0->1 gradient created by the scalar grid "rprgrid2".
//		// Output is the red,green,blue texture.
//		// This demonstrates how we can create a lookup table from scalar grid to vector values.
//		rpr_material_node rampSampler2 = NULL;
//		CHECK(rprMaterialSystemCreateNode(g_matsys, RPR_MATERIAL_NODE_IMAGE_TEXTURE, &rampSampler2));
//		CHECK(rprMaterialNodeSetInputImageDataByKey(rampSampler2, RPR_MATERIAL_INPUT_DATA, rampimg2));
//		CHECK(rprMaterialNodeSetInputNByKey(rampSampler2, RPR_MATERIAL_INPUT_UV, gridSampler2));
//
//		// for ramp texture, it's better to clamp it to edges.
//		CHECK(rprMaterialNodeSetInputUByKey(rampSampler2, RPR_MATERIAL_INPUT_WRAP_U, RPR_IMAGE_WRAP_TYPE_CLAMP_TO_EDGE));
//		CHECK(rprMaterialNodeSetInputUByKey(rampSampler2, RPR_MATERIAL_INPUT_WRAP_V, RPR_IMAGE_WRAP_TYPE_CLAMP_TO_EDGE));
//
//		// create the Volume material
//		rpr_material_node materialVolume = NULL;
//		CHECK(rprMaterialSystemCreateNode(g_matsys, RPR_MATERIAL_NODE_VOLUME, &materialVolume));
//
//		// density is defined by the "cylinder" grid
//		CHECK(rprMaterialNodeSetInputNByKey(materialVolume, RPR_MATERIAL_INPUT_DENSITYGRID, gridSampler1));
//
//		// apply the volume material to the shape.
//		// Note that here we use   rprShapeSetVolumeMaterial  instead of the classic  rprShapeSetMaterial  call.
//		CHECK(rprShapeSetVolumeMaterial(cube, materialVolume));
//
//		// RPR_MATERIAL_INPUT_DENSITY is just a multiplier for DENSITYGRID
//	//	CHECK(rprMaterialNodeSetInputFByKey(materialVolume, RPR_MATERIAL_INPUT_DENSITY, 100.0f, 0.0f, 0.0f, 0.0f));
//
//		// define the color of the volume
//		CHECK(rprMaterialNodeSetInputNByKey(materialVolume, RPR_MATERIAL_INPUT_COLOR, rampSampler2));

		// more iterations will increase the light penetration inside the volume.
		CHECK(rprContextSetParameterByKey1u(g_context, RPR_CONTEXT_MAX_RECURSION, (rpr_uint)2)); // 5

		// when using volumes, we usually need high number of iterations.
		CHECK(rprContextSetParameterByKey1u(g_context, RPR_CONTEXT_ITERATIONS, 6000));

		// set rendering gamma
		CHECK(rprContextSetParameterByKey1f(g_context, RPR_CONTEXT_DISPLAY_GAMMA, 2.2f));


	}


	
	










	// Create framebuffer to store rendering result
	rpr_framebuffer_desc desc = { WINDOW_WIDTH,WINDOW_HEIGHT };

	// 4 component 32-bit float value each
	rpr_framebuffer_format fmt = { 4, RPR_COMPONENT_TYPE_FLOAT32 };
	CHECK(rprContextCreateFrameBuffer(g_context, fmt, &desc, &g_frame_buffer));
	CHECK(rprContextCreateFrameBuffer(g_context, fmt, &desc, &g_frame_buffer_2));

	// Set framebuffer for the context
	CHECK(rprContextSetAOV(g_context, RPR_AOV_COLOR, g_frame_buffer));

	// This line can be added for faster RPR rendering.
	// The higher RPR_CONTEXT_PREVIEW, the faster RPR rendering, ( but more pixelated ).
	// 0 = disabled (defaut value)
	CHECK( rprContextSetParameterByKey1u(g_context,RPR_CONTEXT_PREVIEW, 1u ) );

	// Set framebuffer for the context
	///CHECK(rprContextSetAOV(g_context, RPR_AOV_COLOR, g_frame_buffer));

	// Define the update callback.
	// During the rprContextRender execution, RPR will call it regularly
	// The 'CALLBACK_DATA' : 'g_update' is not used by RPR. it can be any data structure that the API user wants.
	CHECK(rprContextSetParameterByKeyPtr(g_context, RPR_CONTEXT_RENDER_UPDATE_CALLBACK_FUNC, (void*)GuiRenderImpl::notifyUpdate));
	CHECK(rprContextSetParameterByKeyPtr(g_context, RPR_CONTEXT_RENDER_UPDATE_CALLBACK_DATA, &g_update));


	// do a first rendering iteration, just for to force scene/cache building.
	std::cout << "Cache and scene building... ";
	CHECK(rprContextSetParameterByKey1u(g_context,RPR_CONTEXT_ITERATIONS,1));
	CHECK( rprContextRender( g_context ) );
	std::cout << "done\n";

	// each rprContextRender call will do `g_batchSize` iterations. 
	// Note that calling rprContextRender 1 time with RPR_CONTEXT_ITERATIONS = `g_batchSize` is faster than calling rprContextRender `g_batchSize` times with RPR_CONTEXT_ITERATIONS = 1
	CHECK(rprContextSetParameterByKey1u(g_context,RPR_CONTEXT_ITERATIONS,g_batchSize));

	// allocate the data that will be used the read RPR framebuffer, and give it to OpenGL.
	g_fbdata = std::shared_ptr<float>(new float[WINDOW_WIDTH * WINDOW_HEIGHT * 4], std::default_delete<float[]>());

	std::cout << "Press W or S to move the camera.\n";

	glutDisplayFunc(Display);
	glutIdleFunc(Update);
	glutKeyboardFunc(OnKeyboardEvent);
	glutMouseFunc(OnMouseEvent);
	glutMotionFunc(OnMouseMoveEvent);
	glutMainLoop();


	// this should be called when the OpenGL Application closes
	// ( note that it may not be called, as GLUT doesn't have "Exit" callback - press X key in order to have a cleaned exit )
	OnExit();


	return 0;
}


