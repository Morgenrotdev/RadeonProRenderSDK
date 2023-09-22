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

#include <iostream>
#include <string>
#include <vector>
#include "gui.h"

#define shimojima2
//#define shimojima
//#define original
//#define original_test
//
// This demo demonstrates Volumes with RPR
//

//RPRGarbageCollector g_gc;

int main(int argc, char** argv)
{
	//	for Debugging you can enable Radeon ProRender API trace
	//	set this before any RPR API calls
	//rprContextSetParameterByKey1u(0, RPR_CONTEXT_TRACING_ENABLED, 1);

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
	// the RPR context object.
	//rpr_context context = nullptr;

	// Register the RPR DLL  //Reguster the plugin
	rpr_int tahoePluginID = rprRegisterPlugin(RPR_PLUGIN_FILE_NAME);
	CHECK_NE(tahoePluginID, -1);
	rpr_int plugins[] = { tahoePluginID };
	size_t pluginCount = sizeof(plugins) / sizeof(plugins[0]);

	// Create context using a single GPU 
	// note that multiple GPUs can be enabled for example with creation_flags = RPR_CREATION_FLAGS_ENABLE_GPU0 | RPR_CREATION_FLAGS_ENABLE_GPU1
	CHECK(rprCreateContext(RPR_API_VERSION, plugins, pluginCount, g_ContextCreationFlags, g_contextProperties, NULL, &g_context));

	// Set the active plugin.
	CHECK(rprContextSetActivePlugin(g_context, plugins[0]));

	std::cout << "RPR Context creation succeeded." << std::endl;


	char deviceName_gpu0[1024]; deviceName_gpu0[0] = 0;
	CHECK(rprContextGetInfo(g_context, RPR_CONTEXT_GPU1_NAME, sizeof(deviceName_gpu0), deviceName_gpu0, 0));

	// Output the name of the GPU
	std::cout << "GPU0 name : " << std::string(deviceName_gpu0) << std::endl;

	// Release the stuff we created
	// Create material system
	//rpr_material_system matsys = 0;
	CHECK(rprContextCreateMaterialSystem(g_context, 0, &g_matsys));

	// Create a scene
	//rpr_scene scene = nullptr;
	CHECK(rprContextCreateScene(g_context, &g_scene)); // create the scene
	//CHECK(rprContextSetScene(g_context, g_scene)); // set this scene as the "active" scene used for rendering.
	// Create an environment light
	CHECK(CreateNatureEnvLight(g_context, g_scene, g_gc, 0.9f));


	// Create the camera
	//rpr_camera camera = nullptr;
	{
		CHECK(rprContextCreateCamera(g_context, &g_camera));
		//CHECK(rprCameraLookAt(camera, 2.5f, 1.5f, 3.5f, 0.0f, 0.1f, 0.0f, 0, 1, 0));
		//CHECK(rprSceneSetCamera(scene, camera));
				// Position camera in world space: 
		//CHECK(rprCameraLookAt(g_camera, 0.0f, 5.0f, 20.0f, 0, 1, 0, 0, 1, 0));
		CHECK(rprCameraLookAt(g_camera, 0.0f, 5.0f, 5.0f, 0, 0, 1, 0, 0, 1));

		// set camera field of view
		CHECK(rprCameraSetFocalLength(g_camera, 10.f));

		// Set camera for the scene
		CHECK(rprSceneSetCamera(g_scene, g_camera));
	}
	// Set scene to render for the context
	CHECK(rprContextSetScene(g_context, g_scene));



	// create the floor
	//CHECK(CreateAMDFloor(g_context, g_scene, g_matsys, g_gc, 0.20f, 0.20f, 0.0f, -1.0f, 0.0f));

	// Create an environment light
	//CHECK(CreateNatureEnvLight(context, scene, g_gc, 0.8f));


	//// Create framebuffer 
	//rpr_framebuffer_desc desc = { WINDOW_WIDTH,WINDOW_HEIGHT }; // resolution in pixels
	//rpr_framebuffer_format fmt = { 4, RPR_COMPONENT_TYPE_FLOAT32 }; // format: 4 component 32-bit float value each
	////rpr_framebuffer frame_buffer = nullptr;
	////rpr_framebuffer frame_buffer_resolved = nullptr

	//// 4 component 32-bit float value each
	////rpr_framebuffer_format fmt = { 4, RPR_`COMPONENT_TYPE_FLOAT32 }; // format: 4 component 32-bit float value each
	//CHECK(rprContextCreateFrameBuffer(g_context, fmt, &desc, &g_frame_buffer));
	//CHECK(rprContextCreateFrameBuffer(g_context, fmt, &desc, &g_frame_buffer_2));
	//// Set framebuffer for the context
	//CHECK(rprContextSetAOV(g_context, RPR_AOV_COLOR, g_frame_buffer)); // attach 'frame_buffer' to the Color AOV ( this is the main AOV for final rendering )

#ifdef original
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
	RadeonProRender::matrix cubeTransform1 = RadeonProRender::translation(RadeonProRender::float3(0, +0.0f, 0)) * RadeonProRender::rotation_y(0.0f) * RadeonProRender::scale(RadeonProRender::float3(2.0f, 2.0f, 2.0f));
	CHECK(rprShapeSetTransform(cube, true, &cubeTransform1.m00));
	CHECK(rprSceneAttachShape(g_scene, cube));


	// define the grids data used by the Volume material.
	const size_t n = 128;
	std::vector<unsigned int> indicesList;
	std::vector<float> gridVector1;
	std::vector<float> gridVector2;
	const float radiusFadeoutStart = 100.0f;
	const float radiusFadeoutEnd = 100.0f;
	for (unsigned int x = 0; x < n; x++)
	{
		for (unsigned int y = 0; y < n; y++)
		{
			for (unsigned int z = 0; z < n; z++)
			{
				float radius = sqrtf(((float)x - (float)n / 2.0f) * ((float)x - (float)n / 2.0f) + ((float)z - (float)n / 2.0f) * ((float)z - (float)n / 2.0f));

				if (radius < radiusFadeoutEnd)
				{
					indicesList.push_back(x);
					indicesList.push_back(y);
					indicesList.push_back(z);

					// "gridVector1" is going to be a cylinder
					if (radius <= radiusFadeoutStart)
					{
						gridVector1.push_back(1.0f);
					} else
					{
						gridVector1.push_back(1.0f - (radius - radiusFadeoutStart) / (radiusFadeoutEnd - radiusFadeoutStart));
					}

					// "gridVector2" will be a 0->1 ramp along Y-axis
					gridVector2.push_back((float)y / (float)n);
				}
			}
		}
	}


	// this first grid defines a cylinder
	rpr_grid rprgrid1 = 0;
	CHECK(rprContextCreateGrid(g_context, &rprgrid1,
		n, n, n,
		&indicesList[0], indicesList.size() / 3, RPR_GRID_INDICES_TOPOLOGY_XYZ_U32,
		&gridVector1[0], gridVector1.size() * sizeof(gridVector1[0]), 0
	));

	// GRID_SAMPLER could be compared to a 3d-texture sampler. 
	// input is a 3d grid,  output is the sampled value from grid
	rpr_material_node gridSampler1 = NULL;
	CHECK(rprMaterialSystemCreateNode(g_matsys, RPR_MATERIAL_NODE_GRID_SAMPLER, &gridSampler1));
	CHECK(rprMaterialNodeSetInputGridDataByKey(gridSampler1, RPR_MATERIAL_INPUT_DATA, rprgrid1));

	// This second grid is a gradient along the Y axis.
	rpr_grid rprgrid2 = 0;
	CHECK(rprContextCreateGrid(g_context, &rprgrid2,
		n, n, n,
		&indicesList[0], indicesList.size() / 3, RPR_GRID_INDICES_TOPOLOGY_XYZ_U32,
		&gridVector2[0], gridVector2.size() * sizeof(gridVector2[0]), 0
	));

	// create grid sample for grid2
	rpr_material_node gridSampler2 = NULL;
	CHECK(rprMaterialSystemCreateNode(g_matsys, RPR_MATERIAL_NODE_GRID_SAMPLER, &gridSampler2));
	CHECK(rprMaterialNodeSetInputGridDataByKey(gridSampler2, RPR_MATERIAL_INPUT_DATA, rprgrid2));

	// create a gradient color texture, here 3 pixels : Red, Green, Blue.
	// will be used as a lookup output 
	float rampData2[] = {
		1.f,0.f,0.f,
		0.f,1.f,0.f,
		0.f,0.f,1.f };
	rpr_image rampimg2 = 0;
	rpr_image_desc rampDesc2;
	rampDesc2.image_width = sizeof(rampData2) / (3 * sizeof(float));
	rampDesc2.image_height = 1;
	rampDesc2.image_depth = 0;
	rampDesc2.image_row_pitch = rampDesc2.image_width * sizeof(rpr_float) * 3;
	rampDesc2.image_slice_pitch = 0;
	CHECK(rprContextCreateImage(g_context, { 3, RPR_COMPONENT_TYPE_FLOAT32 }, &rampDesc2, rampData2, &rampimg2));

	// this texture will be used for the color of the volume material.
	// UV input is the 0->1 gradient created by the scalar grid "rprgrid2".
	// Output is the red,green,blue texture.
	// This demonstrates how we can create a lookup table from scalar grid to vector values.

	// define color using gridSampler2
	rpr_material_node rampSampler2 = NULL;
	CHECK(rprMaterialSystemCreateNode(g_matsys, RPR_MATERIAL_NODE_IMAGE_TEXTURE, &rampSampler2));
	CHECK(rprMaterialNodeSetInputImageDataByKey(rampSampler2, RPR_MATERIAL_INPUT_DATA, rampimg2));
	CHECK(rprMaterialNodeSetInputNByKey(rampSampler2, RPR_MATERIAL_INPUT_UV, gridSampler2));

	// for ramp texture, it's better to clamp it to edges.
	//CHECK(rprMaterialNodeSetInputUByKey(rampSampler2, RPR_MATERIAL_INPUT_WRAP_U, RPR_IMAGE_WRAP_TYPE_CLAMP_TO_EDGE));
	//CHECK(rprMaterialNodeSetInputUByKey(rampSampler2, RPR_MATERIAL_INPUT_WRAP_V, RPR_IMAGE_WRAP_TYPE_CLAMP_TO_EDGE));

	// create the Volume material
	rpr_material_node materialVolume = NULL;
	CHECK(rprMaterialSystemCreateNode(g_matsys, RPR_MATERIAL_NODE_VOLUME, &materialVolume));

	// density is defined by the "cylinder" grid
	CHECK(rprMaterialNodeSetInputNByKey(materialVolume, RPR_MATERIAL_INPUT_DENSITYGRID, gridSampler1));

	// apply the volume material to the shape.
	// Note that here we use   rprShapeSetVolumeMaterial  instead of the classic  rprShapeSetMaterial  call.
	CHECK(rprShapeSetVolumeMaterial(cube, materialVolume));

	// RPR_MATERIAL_INPUT_DENSITY is just a multiplier for DENSITYGRID
	CHECK(rprMaterialNodeSetInputFByKey(materialVolume, RPR_MATERIAL_INPUT_DENSITY, 100.0f, 0.0f, 0.0f, 1.0f));

	// define the color of the volume
	CHECK(rprMaterialNodeSetInputNByKey(materialVolume, RPR_MATERIAL_INPUT_COLOR, rampSampler2));

	// more iterations will increase the light penetration inside the volume.
	//CHECK(rprContextSetParameterByKey1u(g_context, RPR_CONTEXT_MAX_RECURSION, (rpr_uint)5)); // 5

	// when using volumes, we usually need high number of iterations.
	CHECK(rprContextSetParameterByKey1u(g_context, RPR_CONTEXT_ITERATIONS, 3000));

	// set rendering gamma
	CHECK(rprContextSetParameterByKey1f(g_context, RPR_CONTEXT_DISPLAY_GAMMA, 2.2f));


#endif // original


#ifdef original_test
	rpr_mesh_info mesh_properties[16];
	mesh_properties[0] = (rpr_mesh_info)RPR_MESH_VOLUME_FLAG;
	mesh_properties[1] = (rpr_mesh_info)0; // enable the Volume flag for the Mesh
	mesh_properties[2] = (rpr_mesh_info)0;

	// Volume shapes don't need any vertices data: the bounds of volume will only be defined by the grid.
	// Also, make sure to enable the RPR_MESH_VOLUME_FLAG
	rpr_shape cube = 0;
	//CHECK(rprContextCreateMeshEx2(g_context,
	//	nullptr, 0, 0,
	//	nullptr, 0, 0,
	//	nullptr, 0, 0, 0,
	//	nullptr, nullptr, nullptr, nullptr, 0,
	//	nullptr, 0, nullptr, nullptr, nullptr, 0,
	//	mesh_properties,
	//	&cube));

	CHECK(rprContextCreateMesh(g_context,
		nullptr, 0, 0,
		nullptr, 0, 0,
		nullptr, 0, 0, 0,
		nullptr, nullptr, nullptr, nullptr, 0,
		nullptr, 0, nullptr, nullptr, nullptr, 0,
		&cube));


	// bounds of volume will always be a box defined by the rprShapeSetTransform
	RadeonProRender::matrix cubeTransform1 = RadeonProRender::translation(RadeonProRender::float3(0, +0.0f, 0)) * RadeonProRender::rotation_y(0.0f) * RadeonProRender::scale(RadeonProRender::float3(5.0f, 5.0f, 5.0f));
	CHECK(rprShapeSetTransform(cube, true, &cubeTransform1.m00));
	CHECK(rprSceneAttachShape(g_scene, cube));


	// define the grids data used by the Volume material.
	const size_t n = 1;
	size_t ny = 50;
	size_t nz = 50;
	std::vector<unsigned int> indicesList;
	std::vector<float> gridVector1;
	std::vector<float> gridVector2;
	const float radiusFadeoutStart = 10.0f;
	const float radiusFadeoutEnd = 10.0f;
	for (unsigned int x = 0; x < n; x++)
	{
		for (unsigned int y = 0; y < ny; y++)
		{
			for (unsigned int z = 0; z < nz; z++)
			{
				//float radius = sqrtf(((float)x - (float)n / 2.0f) * ((float)x - (float)n / 2.0f) + ((float)z - (float)nz / 2.0f) * ((float)z - (float)nz / 2.0f));

				//if (radius < radiusFadeoutEnd)
				//{
				//	indicesList.push_back(x);
					indicesList.push_back(y);
					indicesList.push_back(z);

					// "gridVector1" is going to be a cylinder
					//if (radius <= radiusFadeoutStart)
					//{
					gridVector1.push_back(1.0f);
					//} else
					//{
					//	gridVector1.push_back(1.0f - (radius - radiusFadeoutStart) / (radiusFadeoutEnd - radiusFadeoutStart));
					//}

					// "gridVector2" will be a 0->1 ramp along Y-axis
					gridVector2.push_back((float)y / (float)ny);
					//gridVector2.push_back(1.0f);
				//}
			}
		}
	}


	// this first grid defines a cylinder
	rpr_grid rprgrid1 = 0;
	CHECK(rprContextCreateGrid(g_context, &rprgrid1,
		n, ny, nz,
		&indicesList[0], indicesList.size() /2, RPR_GRID_INDICES_TOPOLOGY_XYZ_U32,
		&gridVector1[0], gridVector1.size() * sizeof(gridVector1[0]), 0
	));

	// GRID_SAMPLER could be compared to a 3d-texture sampler. 
	// input is a 3d grid,  output is the sampled value from grid
	rpr_material_node gridSampler1 = NULL;
	CHECK(rprMaterialSystemCreateNode(g_matsys, RPR_MATERIAL_NODE_GRID_SAMPLER, &gridSampler1));
	CHECK(rprMaterialNodeSetInputGridDataByKey(gridSampler1, RPR_MATERIAL_INPUT_DATA, rprgrid1));

	// This second grid is a gradient along the Y axis.
	rpr_grid rprgrid2 = 0;
	CHECK(rprContextCreateGrid(g_context, &rprgrid2,
		n, ny, nz,
		&indicesList[0], indicesList.size() /2, RPR_GRID_INDICES_TOPOLOGY_XYZ_U32,
		&gridVector2[0], gridVector2.size() * sizeof(gridVector2[0]), 0
	));

	// create grid sample for grid2
	rpr_material_node gridSampler2 = NULL;
	CHECK(rprMaterialSystemCreateNode(g_matsys, RPR_MATERIAL_NODE_GRID_SAMPLER, &gridSampler2));
	CHECK(rprMaterialNodeSetInputGridDataByKey(gridSampler2, RPR_MATERIAL_INPUT_DATA, rprgrid2));

	// create a gradient color texture, here 3 pixels : Red, Green, Blue.
	// will be used as a lookup output 
	float rampData2[] = {
		1.f,0.f,0.f,
		0.f,1.f,0.f,
		0.f,0.f,1.f };
	rpr_image rampimg2 = 0;
	rpr_image_desc rampDesc2;
	rampDesc2.image_width = sizeof(rampData2) / (3 * sizeof(float));
	rampDesc2.image_height = 1;
	rampDesc2.image_depth = 0;
	rampDesc2.image_row_pitch = rampDesc2.image_width * sizeof(rpr_float) * 3;
	rampDesc2.image_slice_pitch = 0;
	CHECK(rprContextCreateImage(g_context, { 3, RPR_COMPONENT_TYPE_FLOAT32 }, &rampDesc2, rampData2, &rampimg2));

	// this texture will be used for the color of the volume material.
	// UV input is the 0->1 gradient created by the scalar grid "rprgrid2".
	// Output is the red,green,blue texture.
	// This demonstrates how we can create a lookup table from scalar grid to vector values.

	// define color using gridSampler2
	rpr_material_node rampSampler2 = NULL;
	CHECK(rprMaterialSystemCreateNode(g_matsys, RPR_MATERIAL_NODE_IMAGE_TEXTURE, &rampSampler2));
	CHECK(rprMaterialNodeSetInputImageDataByKey(rampSampler2, RPR_MATERIAL_INPUT_DATA, rampimg2));
	CHECK(rprMaterialNodeSetInputNByKey(rampSampler2, RPR_MATERIAL_INPUT_UV, gridSampler2));

	// for ramp texture, it's better to clamp it to edges.
	//CHECK(rprMaterialNodeSetInputUByKey(rampSampler2, RPR_MATERIAL_INPUT_WRAP_U, RPR_IMAGE_WRAP_TYPE_CLAMP_TO_EDGE));
	//CHECK(rprMaterialNodeSetInputUByKey(rampSampler2, RPR_MATERIAL_INPUT_WRAP_V, RPR_IMAGE_WRAP_TYPE_CLAMP_TO_EDGE));

	// create the Volume material
	rpr_material_node materialVolume = NULL;
	CHECK(rprMaterialSystemCreateNode(g_matsys, RPR_MATERIAL_NODE_VOLUME, &materialVolume));

	// density is defined by the "cylinder" grid
	CHECK(rprMaterialNodeSetInputNByKey(materialVolume, RPR_MATERIAL_INPUT_DENSITYGRID, gridSampler1));

	// apply the volume material to the shape.
	// Note that here we use   rprShapeSetVolumeMaterial  instead of the classic  rprShapeSetMaterial  call.
	CHECK(rprShapeSetVolumeMaterial(cube, materialVolume));

	// RPR_MATERIAL_INPUT_DENSITY is just a multiplier for DENSITYGRID
	CHECK(rprMaterialNodeSetInputFByKey(materialVolume, RPR_MATERIAL_INPUT_DENSITY, 100.0f, 0.0f, 0.0f, 1.0f));

	// define the color of the volume
	CHECK(rprMaterialNodeSetInputNByKey(materialVolume, RPR_MATERIAL_INPUT_COLOR, rampSampler2));

	// more iterations will increase the light penetration inside the volume.
	//CHECK(rprContextSetParameterByKey1u(g_context, RPR_CONTEXT_MAX_RECURSION, (rpr_uint)5)); // 5

	// when using volumes, we usually need high number of iterations.
	CHECK(rprContextSetParameterByKey1u(g_context, RPR_CONTEXT_ITERATIONS, 3000));

	// set rendering gamma
	CHECK(rprContextSetParameterByKey1f(g_context, RPR_CONTEXT_DISPLAY_GAMMA, 2.2f));


#endif // original_test




#ifdef shimojima

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
	RadeonProRender::matrix cubeTransform1 = RadeonProRender::translation(RadeonProRender::float3(0, +0.0f, 0)) * RadeonProRender::rotation_y(0.0f) * RadeonProRender::scale(RadeonProRender::float3(5.0f, 2.5f, 2.5f));
	CHECK(rprShapeSetTransform(cube, true, &cubeTransform1.m00));
	CHECK(rprSceneAttachShape(g_scene, cube));


	// define the grids data used by the Volume material.
	//const size_t n = 128;
	std::vector<unsigned int> indicesList;
	std::vector<float> gridVector1;
	std::vector<float> gridVector2;
	//const float radiusFadeoutStart = 100.0f;
	//const float radiusFadeoutEnd = 100.0f;
	//for (unsigned int x = 0; x < n; x++)
	//{
	//	for (unsigned int y = 0; y < n; y++)
	//	{
	//		for (unsigned int z = 0; z < n; z++)
	//		{
	//			float radius = sqrtf(((float)x - (float)n / 2.0f) * ((float)x - (float)n / 2.0f) + ((float)z - (float)n / 2.0f) * ((float)z - (float)n / 2.0f));

	//			if (radius < radiusFadeoutEnd)
	//			{
	//				indicesList.push_back(x);
	//				indicesList.push_back(y);
	//				indicesList.push_back(z);

	//				// "gridVector1" is going to be a cylinder
	//				if (radius <= radiusFadeoutStart)
	//				{
	//					gridVector1.push_back(1.0f);
	//				} else
	//				{
	//					gridVector1.push_back(1.0f - (radius - radiusFadeoutStart) / (radiusFadeoutEnd - radiusFadeoutStart));
	//				}

	//				// "gridVector2" will be a 0->1 ramp along Y-axis
	//				gridVector2.push_back((float)y / (float)n);
	//			}
	//		}
	//	}
	//}


	int xmax, ymax, zmax, xstart, ystart, zstart, xend, yend, zend;
	std::vector<double> xd, yd, zd;
	std::vector<float>bcd;
	std::ifstream mesh;
	std::string line;
	char* dirmesh = "../99_test/gridbc_ascii.dat";
	mesh.open(dirmesh, std::ios::in);
	if (!mesh.is_open()) {
		std::cerr << "Failed to open the file!" << std::endl;
	}
	int idomein = 0;
	int i = 0, index = 0;

	while (getline(mesh, line)) {
		if (i == 0) {
			printf("skip the TITLE line \n");
			std::cout << line.substr() << std::endl;
		} else if (i == 1) {
			printf("skip the VARIABLE line \n");
			std::cout << line.substr() << std::endl;
		} else if (line.substr(0, 4) == "ZONE") {
			/* read the grid dimensions from the ZONE line */

			sscanf(line.c_str(), "ZONE T = \"%*[^\"]\", I=%d, J=%d, K=%d, F=%*s", &xmax, &ymax, &zmax);
			std::cout << line.substr() << std::endl;
			xstart = 1;
			ystart = 1;
			zstart = 0;
			xend = xmax - 2;
			yend = ymax - 2;
			zend = zmax-2;
			printf("xstart, ystart, zstart, xend, yend, zend are %d, %d, %d, %d, %d, %d\n", xstart, ystart, zstart, xend, yend, zend);
			idomein = xmax * ymax * zmax;
			printf("xmax, ymax, zmax, xmax*ymax*zmax are %d, %d, %d, %d\n", xmax, ymax, zmax, idomein);
			/* resize the arrays to the correct size */
			xd.resize(idomein);
			yd.resize(idomein);
			zd.resize(idomein);
			bcd.resize(idomein);
			/*gridVector1.resize(idomein);
			gridVector2.resize(idomein);*/

		} else {
			/* read the x, y, z values from the line*/
			double xi = 0.0, yi = 0.0, zi = 0.0;
			int bcdi = 0;
			sscanf(line.c_str(), "%lf  %lf  %lf  %d", &xi, &yi, &zi, &bcdi);
			/* store the vb*/
			//index = i - 3;
			//xd[index] = xi;
			//yd[index] = yi;
			//zd[index] = zi;
			//indicesList.push_back(float(xi));
			//indicesList.push_back(float(yi));
			//indicesList.push_back(float(zi));
			bcd[index] = float(bcdi);
			/*gridVector1.push_back((float)bcdi);
			gridVector2.push_back((float)bcdi);*/
			gridVector1.push_back(4.0/4.0);
			gridVector2.push_back(bcdi/4.0);
		}
		i++;
	}
	//int j = 0;
	for (unsigned int z = 0; z < zmax; z++)
	{
		for (unsigned int y = 0; y < ymax; y++)
		{
			for (unsigned int x = 0; x < xmax; x++)
			{
				indicesList.push_back(x);
				indicesList.push_back(y);
				indicesList.push_back(z);
				//gridVector1.push_back(bcd[j]/4.0);
				//gridVector2.push_back(bcd[j]/4.0);
				//j++;
			}
		}
	}
	//float max = *max_element(gridVector1.begin(), gridVector1.end());

	//for (int i = 0; i < idomein; i++) {
	//	gridVector1[i] = gridVector1[i] / max;
	//	gridVector2[i] = gridVector2[i] / max;
	//}
	// this first grid defines a cylinder
	rpr_grid rprgrid1 = 0;
	CHECK(rprContextCreateGrid(g_context, &rprgrid1,
		size_t(xmax), size_t(ymax), size_t(zmax),
		&indicesList[0], indicesList.size() / 3, RPR_GRID_INDICES_TOPOLOGY_XYZ_U32,
		&gridVector1[0], gridVector1.size() * sizeof(gridVector1[0]), 0
	));

	// GRID_SAMPLER could be compared to a 3d-texture sampler. 
	// input is a 3d grid,  output is the sampled value from grid
	rpr_material_node gridSampler1 = NULL;
	CHECK(rprMaterialSystemCreateNode(g_matsys, RPR_MATERIAL_NODE_GRID_SAMPLER, &gridSampler1));
	CHECK(rprMaterialNodeSetInputGridDataByKey(gridSampler1, RPR_MATERIAL_INPUT_DATA, rprgrid1));

	// This second grid is a gradient along the Y axis.
	rpr_grid rprgrid2 = 0;
	CHECK(rprContextCreateGrid(g_context, &rprgrid2,
		xmax, ymax, zmax,
		&indicesList[0], indicesList.size() / 3, RPR_GRID_INDICES_TOPOLOGY_XYZ_U32,
		&gridVector2[0], gridVector2.size() * sizeof(gridVector2[0]), 0
	));

	// create grid sample for grid2
	rpr_material_node gridSampler2 = NULL;
	CHECK(rprMaterialSystemCreateNode(g_matsys, RPR_MATERIAL_NODE_GRID_SAMPLER, &gridSampler2));
	CHECK(rprMaterialNodeSetInputGridDataByKey(gridSampler2, RPR_MATERIAL_INPUT_DATA, rprgrid2));

	// create a gradient color texture, here 3 pixels : Red, Green, Blue.
	// will be used as a lookup output 
	float rampData2[] = {
		1.f,0.f,0.f,
		0.f,1.f,0.f,
		0.f,0.f,1.f };
	rpr_image rampimg2 = 0;
	rpr_image_desc rampDesc2;
	rampDesc2.image_width = sizeof(rampData2) / (3 * sizeof(float));
	rampDesc2.image_height = 1;
	rampDesc2.image_depth = 0;
	rampDesc2.image_row_pitch = rampDesc2.image_width * sizeof(rpr_float) * 3;
	rampDesc2.image_slice_pitch = 0;
	CHECK(rprContextCreateImage(g_context, { 3, RPR_COMPONENT_TYPE_FLOAT32 }, &rampDesc2, rampData2, &rampimg2));

	// this texture will be used for the color of the volume material.
	// UV input is the 0->1 gradient created by the scalar grid "rprgrid2".
	// Output is the red,green,blue texture.
	// This demonstrates how we can create a lookup table from scalar grid to vector values.

	// define color using gridSampler2
	rpr_material_node rampSampler2 = NULL;
	CHECK(rprMaterialSystemCreateNode(g_matsys, RPR_MATERIAL_NODE_IMAGE_TEXTURE, &rampSampler2));
	CHECK(rprMaterialNodeSetInputImageDataByKey(rampSampler2, RPR_MATERIAL_INPUT_DATA, rampimg2));
	CHECK(rprMaterialNodeSetInputNByKey(rampSampler2, RPR_MATERIAL_INPUT_UV, gridSampler2));

	// for ramp texture, it's better to clamp it to edges.
	CHECK(rprMaterialNodeSetInputUByKey(rampSampler2, RPR_MATERIAL_INPUT_WRAP_U, RPR_IMAGE_WRAP_TYPE_CLAMP_TO_EDGE));
	CHECK(rprMaterialNodeSetInputUByKey(rampSampler2, RPR_MATERIAL_INPUT_WRAP_V, RPR_IMAGE_WRAP_TYPE_CLAMP_TO_EDGE));

	// create the Volume material
	rpr_material_node materialVolume = NULL;
	CHECK(rprMaterialSystemCreateNode(g_matsys, RPR_MATERIAL_NODE_VOLUME, &materialVolume));

	// density is defined by the "cylinder" grid
	CHECK(rprMaterialNodeSetInputNByKey(materialVolume, RPR_MATERIAL_INPUT_DENSITYGRID, gridSampler1));

	// apply the volume material to the shape.
	// Note that here we use   rprShapeSetVolumeMaterial  instead of the classic  rprShapeSetMaterial  call.
	CHECK(rprShapeSetVolumeMaterial(cube, materialVolume));

	// RPR_MATERIAL_INPUT_DENSITY is just a multiplier for DENSITYGRID
	CHECK(rprMaterialNodeSetInputFByKey(materialVolume, RPR_MATERIAL_INPUT_DENSITY, 100.0f, 0.0f, 0.0f, 1.0f));

	// define the color of the volume
	CHECK(rprMaterialNodeSetInputNByKey(materialVolume, RPR_MATERIAL_INPUT_COLOR, rampSampler2));

	// more iterations will increase the light penetration inside the volume.
	//CHECK(rprContextSetParameterByKey1u(g_context, RPR_CONTEXT_MAX_RECURSION, (rpr_uint)5)); // 5

	// when using volumes, we usually need high number of iterations.
	CHECK(rprContextSetParameterByKey1u(g_context, RPR_CONTEXT_ITERATIONS, 3000));

	// set rendering gamma
	CHECK(rprContextSetParameterByKey1f(g_context, RPR_CONTEXT_DISPLAY_GAMMA, 2.2f));


#endif //shimojima

#ifdef shimojima2

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
	RadeonProRender::matrix cubeTransform1 = RadeonProRender::translation(RadeonProRender::float3(0, +0.0f, 0)) * RadeonProRender::rotation_y(0.0f) * RadeonProRender::scale(RadeonProRender::float3(1.0f, 1.0f, 0.1f));
	CHECK(rprShapeSetTransform(cube, true, &cubeTransform1.m00));
	CHECK(rprSceneAttachShape(g_scene, cube));


	// define the grids data used by the Volume material.
	//const size_t n = 128;
	std::vector<unsigned int> indicesList;
	std::vector<float> gridVector1;
	std::vector<float> gridVector2;

	const char* filename = "../99_test/all-fielda_40000.vtk";
	std::ifstream file(filename);
	std::vector<float> velocities;
	std::vector<float> densities;
	std::vector<float> pressures;
	float velnome;

		if (!file.is_open()) {
			std::cerr << "Failed to open file: " << filename << std::endl;
			return false;
		}

		std::string line;
		bool readingVelocity = false;
		bool readingDensity = false;
		bool readingPressure = false;
		bool readingGridnum = false;

		while (std::getline(file, line)) {
				float vx, vy, vz;
				std::istringstream iss(line);
				iss >> vx >> vy >> vz;
				velnome = sqrt(vx * vx + vy * vy + vz * vz);
				//std::cout << "velnome, vx, vy, vz = " << velnome <<" " << vx << " " << vy << " " << vz << " " << std::endl;
				velocities.push_back(velnome);
				//velocities.push_back(vy);
				//velocities.push_back(vz);
}
		//exit(0);
		unsigned int zmax = 1;
		unsigned int ymax = 151;
		unsigned int xmax = 151;
		float maxvelnome = *max_element(velocities.begin(), velocities.end());
		std::cout << "maxvelnome= " << maxvelnome << std::endl;
	
	//int j = 0;
	for (unsigned int z = 0; z < zmax; z++)
	{
		for (unsigned int y = 0; y < ymax; y++)
		{
			for (unsigned int x = 0; x < xmax; x++)
			{
				indicesList.push_back(x);
				indicesList.push_back(y);
				indicesList.push_back(z);
				//gridVector1.push_back(1.0);
				gridVector2.push_back(velocities[x + y * xmax + z * xmax * ymax]/ maxvelnome);
				gridVector1.push_back(maxvelnome / maxvelnome);
				//gridVector2.push_back(bcd[j]/4.0);
				//j++;
			}
		}
	}
	//float max = *max_element(gridVector1.begin(), gridVector1.end());

	//for (int i = 0; i < idomein; i++) {
	//	gridVector1[i] = gridVector1[i] / max;
	//	gridVector2[i] = gridVector2[i] / max;
	//}
	// this first grid defines a cylinder
	rpr_grid rprgrid1 = 0;
	CHECK(rprContextCreateGrid(g_context, &rprgrid1,
		size_t(xmax), size_t(ymax), size_t(zmax),
		&indicesList[0], indicesList.size() / 3, RPR_GRID_INDICES_TOPOLOGY_XYZ_U32,
		&gridVector1[0], gridVector1.size() * sizeof(gridVector1[0]), 0
	));

	// GRID_SAMPLER could be compared to a 3d-texture sampler. 
	// input is a 3d grid,  output is the sampled value from grid
	rpr_material_node gridSampler1 = NULL;
	CHECK(rprMaterialSystemCreateNode(g_matsys, RPR_MATERIAL_NODE_GRID_SAMPLER, &gridSampler1));
	CHECK(rprMaterialNodeSetInputGridDataByKey(gridSampler1, RPR_MATERIAL_INPUT_DATA, rprgrid1));

	// This second grid is a gradient along the Y axis.
	rpr_grid rprgrid2 = 0;
	CHECK(rprContextCreateGrid(g_context, &rprgrid2,
		xmax, ymax, zmax,
		&indicesList[0], indicesList.size() / 3, RPR_GRID_INDICES_TOPOLOGY_XYZ_U32,
		&gridVector2[0], gridVector2.size() * sizeof(gridVector2[0]), 0
	));

	// create grid sample for grid2
	rpr_material_node gridSampler2 = NULL;
	CHECK(rprMaterialSystemCreateNode(g_matsys, RPR_MATERIAL_NODE_GRID_SAMPLER, &gridSampler2));
	CHECK(rprMaterialNodeSetInputGridDataByKey(gridSampler2, RPR_MATERIAL_INPUT_DATA, rprgrid2));

	// create a gradient color texture, here 3 pixels : Red, Green, Blue.
	// will be used as a lookup output 
	float rampData2[] = {
		1.f,0.f,0.f,
		0.f,1.f,0.f,
		0.f,0.f,1.f };
	rpr_image rampimg2 = 0;
	rpr_image_desc rampDesc2;
	rampDesc2.image_width = sizeof(rampData2) / (3 * sizeof(float));
	rampDesc2.image_height = 1;
	rampDesc2.image_depth = 0;
	rampDesc2.image_row_pitch = rampDesc2.image_width * sizeof(rpr_float) * 3;
	rampDesc2.image_slice_pitch = 0;
	CHECK(rprContextCreateImage(g_context, { 3, RPR_COMPONENT_TYPE_FLOAT32 }, &rampDesc2, rampData2, &rampimg2));

	// this texture will be used for the color of the volume material.
	// UV input is the 0->1 gradient created by the scalar grid "rprgrid2".
	// Output is the red,green,blue texture.
	// This demonstrates how we can create a lookup table from scalar grid to vector values.

	// define color using gridSampler2
	rpr_material_node rampSampler2 = NULL;
	CHECK(rprMaterialSystemCreateNode(g_matsys, RPR_MATERIAL_NODE_IMAGE_TEXTURE, &rampSampler2));
	CHECK(rprMaterialNodeSetInputImageDataByKey(rampSampler2, RPR_MATERIAL_INPUT_DATA, rampimg2));
	CHECK(rprMaterialNodeSetInputNByKey(rampSampler2, RPR_MATERIAL_INPUT_UV, gridSampler2));

	// for ramp texture, it's better to clamp it to edges.
	CHECK(rprMaterialNodeSetInputUByKey(rampSampler2, RPR_MATERIAL_INPUT_WRAP_U, RPR_IMAGE_WRAP_TYPE_CLAMP_TO_EDGE));
	CHECK(rprMaterialNodeSetInputUByKey(rampSampler2, RPR_MATERIAL_INPUT_WRAP_V, RPR_IMAGE_WRAP_TYPE_CLAMP_TO_EDGE));

	// create the Volume material
	rpr_material_node materialVolume = NULL;
	CHECK(rprMaterialSystemCreateNode(g_matsys, RPR_MATERIAL_NODE_VOLUME, &materialVolume));

	// density is defined by the "cylinder" grid
	CHECK(rprMaterialNodeSetInputNByKey(materialVolume, RPR_MATERIAL_INPUT_DENSITYGRID, gridSampler1));

	// apply the volume material to the shape.
	// Note that here we use   rprShapeSetVolumeMaterial  instead of the classic  rprShapeSetMaterial  call.
	CHECK(rprShapeSetVolumeMaterial(cube, materialVolume));

	// RPR_MATERIAL_INPUT_DENSITY is just a multiplier for DENSITYGRID
	CHECK(rprMaterialNodeSetInputFByKey(materialVolume, RPR_MATERIAL_INPUT_DENSITY, 100.0f, 0.0f, 0.0f, 1.0f));

	// define the color of the volume
	CHECK(rprMaterialNodeSetInputNByKey(materialVolume, RPR_MATERIAL_INPUT_COLOR, rampSampler2));

	// more iterations will increase the light penetration inside the volume.
	//CHECK(rprContextSetParameterByKey1u(g_context, RPR_CONTEXT_MAX_RECURSION, (rpr_uint)5)); // 5

	// when using volumes, we usually need high number of iterations.
	CHECK(rprContextSetParameterByKey1u(g_context, RPR_CONTEXT_ITERATIONS, 3000));

	// set rendering gamma
	CHECK(rprContextSetParameterByKey1f(g_context, RPR_CONTEXT_DISPLAY_GAMMA, 2.2f));


#endif //shimojima2
	// Create framebuffer 
	rpr_framebuffer_desc desc = { WINDOW_WIDTH,WINDOW_HEIGHT }; // resolution in pixels
	rpr_framebuffer_format fmt = { 4, RPR_COMPONENT_TYPE_FLOAT32 }; // format: 4 component 32-bit float value each
	//rpr_framebuffer frame_buffer = nullptr;
	//rpr_framebuffer frame_buffer_resolved = nullptr

	// 4 component 32-bit float value each
	//rpr_framebuffer_format fmt = { 4, RPR_`COMPONENT_TYPE_FLOAT32 }; // format: 4 component 32-bit float value each
	CHECK(rprContextCreateFrameBuffer(g_context, fmt, &desc, &g_frame_buffer));
	CHECK(rprContextCreateFrameBuffer(g_context, fmt, &desc, &g_frame_buffer_2));
	// Set framebuffer for the context
	CHECK(rprContextSetAOV(g_context, RPR_AOV_COLOR, g_frame_buffer)); // attach 'frame_buffer' to the Color AOV ( this is the main AOV for final rendering )


	// Define the update callback.
	// During the rprContextRender execution, RPR will call it regularly
	// The 'CALLBACK_DATA' : 'g_update' is not used by RPR. it can be any data structure that the API user wants.
	CHECK(rprContextSetParameterByKeyPtr(g_context, RPR_CONTEXT_RENDER_UPDATE_CALLBACK_FUNC, (void*)GuiRenderImpl::notifyUpdate));
	CHECK(rprContextSetParameterByKeyPtr(g_context, RPR_CONTEXT_RENDER_UPDATE_CALLBACK_DATA, &g_update));


	// Start the rendering. 
	//CHECK(rprContextRender(g_context));
	// do a first rendering iteration, just for to force scene/cache building.
	std::cout << "Cache and scene building... ";
	CHECK(rprContextSetParameterByKey1u(g_context, RPR_CONTEXT_ITERATIONS, 1));
	CHECK(rprContextRender(g_context));
	std::cout << "done\n";


	// each rprContextRender call will do `g_batchSize` iterations. 
	// Note that calling rprContextRender 1 time with RPR_CONTEXT_ITERATIONS = `g_batchSize` is faster than calling rprContextRender `g_batchSize` times with RPR_CONTEXT_ITERATIONS = 1
	CHECK(rprContextSetParameterByKey1u(g_context, RPR_CONTEXT_ITERATIONS, g_batchSize));

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

	// resolve and save the rendering to an image file.
	//CHECK(rprContextResolveFrameBuffer(g_context, g_frame_buffer, g_frame_buffer_2, false));
	//CHECK(rprFrameBufferSaveToFile(frame_buffer_resolved, "../../51_00.png"));


	// Release the stuff we created
	CHECK(rprObjectDelete(cube)); cube = nullptr;
	CHECK(rprObjectDelete(rprgrid1)); rprgrid1 = nullptr;
	CHECK(rprObjectDelete(gridSampler1)); gridSampler1 = nullptr;
	CHECK(rprObjectDelete(rprgrid2)); rprgrid2 = nullptr;
	CHECK(rprObjectDelete(gridSampler2)); gridSampler2 = nullptr;
	CHECK(rprObjectDelete(materialVolume)); materialVolume = nullptr;
	CHECK(rprObjectDelete(rampimg2)); rampimg2 = nullptr;
	CHECK(rprObjectDelete(rampSampler2)); rampSampler2 = nullptr;
	CHECK(rprObjectDelete(g_camera)); g_camera = nullptr;
	CHECK(rprObjectDelete(g_frame_buffer)); g_frame_buffer = nullptr;
	CHECK(rprObjectDelete(g_frame_buffer_2)); g_frame_buffer_2 = nullptr;
	g_gc.GCClean();
	CHECK(rprObjectDelete(g_scene)); g_scene = nullptr;
	CHECK(rprObjectDelete(g_matsys)); g_matsys = nullptr;
	CheckNoLeak(g_context);
	CHECK(rprObjectDelete(g_context)); g_context = nullptr;
	return 0;

}

