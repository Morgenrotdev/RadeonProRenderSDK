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

#include "RprLoadStore.h"
#include "../common/picojson.h"
#define TINYOBJLOADER_IMPLEMENTATION
#include "../common/tiny_obj_loader.h"


#ifdef __APPLE__
	#ifndef GL_RGBA32F
	#define GL_RGBA32F GL_RGBA32F_ARB
	#endif 
#endif

#include <cassert>
#include <iostream>
#include <thread>
#include <memory>
#include <map>
#include <unordered_set> //202310
#include <chrono> //202310



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

uint32_t			g_max_iterations;
int                 g_ii = 0;
float				g_input_shininess;
float				g_input_roughness;
float				g_input_illum;
float				g_input_dissolve;
float				g_input_ior;
RadeonProRender::float3				g_input_ambient;
RadeonProRender::float3				g_input_diffuse;
RadeonProRender::float3				g_input_specular;
RadeonProRender::float3				g_input_emission;
RadeonProRender::float3				g_input_transmittance;


//--------------------****************---------------
//Inserted from mesh_obj_demo
// Hold objects from different helper functions which are cleaned at end of main
std::vector<void*> garbageCollector;

std::map<std::string, rpr_creation_flags> creationModesMap = {
  {"gpu0", RPR_CREATION_FLAGS_ENABLE_GPU0},
  {"gpu1", RPR_CREATION_FLAGS_ENABLE_GPU1},
  {"gpu2", RPR_CREATION_FLAGS_ENABLE_GPU2},
  {"gpu3", RPR_CREATION_FLAGS_ENABLE_GPU3},
  {"gpu4", RPR_CREATION_FLAGS_ENABLE_GPU4},
  {"gpu5", RPR_CREATION_FLAGS_ENABLE_GPU5},
  {"gpu6", RPR_CREATION_FLAGS_ENABLE_GPU6},
  {"gpu7", RPR_CREATION_FLAGS_ENABLE_GPU7},
  {"gpu8", RPR_CREATION_FLAGS_ENABLE_GPU8},
  {"gpu9", RPR_CREATION_FLAGS_ENABLE_GPU9},
  {"gpu10", RPR_CREATION_FLAGS_ENABLE_GPU10},
  {"gpu11", RPR_CREATION_FLAGS_ENABLE_GPU11},
};

std::map<std::string, rpr_render_mode> renderModesMap = {
{ "gi", RPR_RENDER_MODE_GLOBAL_ILLUMINATION             }  ,
{ "di", RPR_RENDER_MODE_DIRECT_ILLUMINATION             }  ,
{ "di_no_shadow", RPR_RENDER_MODE_DIRECT_ILLUMINATION_NO_SHADOW   }  ,
{ "wire", RPR_RENDER_MODE_WIREFRAME                       }  ,
{ "mi", RPR_RENDER_MODE_MATERIAL_INDEX                  }  ,
{ "pos", RPR_RENDER_MODE_POSITION                        }  ,
{ "normal", RPR_RENDER_MODE_NORMAL                          }  ,
{ "tex", RPR_RENDER_MODE_TEXCOORD                        }  ,
{ "ao", RPR_RENDER_MODE_AMBIENT_OCCLUSION               }  ,
{ "diff", RPR_RENDER_MODE_DIFFUSE                         }  ,
};

std::map<std::string, rpr_camera_mode> cameraModesMap = {
	{"perspective", RPR_CAMERA_MODE_PERSPECTIVE},
	{"orthogrphics", RPR_CAMERA_MODE_ORTHOGRAPHIC},
	{"longlat360", RPR_CAMERA_MODE_LATITUDE_LONGITUDE_360},
	{"longlatstereo", RPR_CAMERA_MODE_LATITUDE_LONGITUDE_STEREO},
	{"cubemap", RPR_CAMERA_MODE_CUBEMAP},
	{"cubemapstereo", RPR_CAMERA_MODE_CUBEMAP_STEREO},
	{"fisheye", RPR_CAMERA_MODE_FISHEYE}
};

template<typename T>
inline
void fill(const picojson::object& v, const char* key, T& dst)
{
	auto c = v.find(key);
	if (c != v.end())
	{
		const picojson::value& d = c->second;
		dst = static_cast<T>(d.get<T>());
	}
}

template<>
inline
void fill(const picojson::object& v, const char* key, RadeonProRender::float3& dst)
{
	auto c = v.find(key);
	if (c != v.end())
	{
		int j = 0;
		const picojson::value& d = c->second;
		const picojson::array& a = d.get<picojson::array>();
		for (picojson::array::const_iterator i = a.begin(); i != a.end(); ++i) {
			dst[j++] = std::stof(i->to_str());
		}

	}
}

template<>
inline
void fill(const picojson::object& v, const char* key, RadeonProRender::float4& dst)
{
	auto c = v.find(key);
	if (c != v.end())
	{
		int j = 0;
		const picojson::value& d = c->second;
		const picojson::array& a = d.get<picojson::array>();
		for (picojson::array::const_iterator i = a.begin(); i != a.end(); ++i) {
			dst[j++] = std::stof(i->to_str());
		}

	}
}

template<>
inline
void fill(const picojson::object& v, const char* key, float& dst)
{
	auto c = v.find(key);
	if (c != v.end())
	{
		const picojson::value& d = c->second;
		dst = static_cast<double>(d.get<double>());
	}
}

struct Context
{
	rpr_context context;
	rpr_scene scene;
	rpr_framebuffer fb;
	rpr_framebuffer fb1;
	uint32_t max_iterations = 1;
	std::string outFileName = "64.png";
};

struct ContextSettings
{
	rpr_render_mode renderMode = RPR_RENDER_MODE_GLOBAL_ILLUMINATION;
	rpr_uint recursion = 10;
	rpr_uint width =  1280;//1280;//80*16
	rpr_uint height =  780;//780;//80*9
	rpr_uint iterations = 64;
	rpr_uint batchSize = 32;
	std::string outImgFile = "test.png";
	std::string outJsonFile = "output.json";
	rpr_uint gui = 0;
	rpr_creation_flags creationFlags = RPR_CREATION_FLAGS_ENABLE_GPU0;
};

struct CameraSettings
{
	RadeonProRender::float3 position = { 0.0, -1.0, 21.0 };
	RadeonProRender::float3 aimed = { 0.0f, 0.0f, 0.0f };
	RadeonProRender::float3 up = { 0.0f, 1.0f, 0.0f };
	rpr_float focalLength = 75.0f;
	RadeonProRender::float3 translation = { 0.0f, 0.0f, 0.0f };
	RadeonProRender::float4 rotation = { 0.0f, 1.0f, 0.0f, 3.14159f };
	rpr_camera_mode cameraMode = RPR_CAMERA_MODE_PERSPECTIVE;
};

struct LightSettings
{
	std::string type = "point";
	RadeonProRender::float3 translation = { 0.0f, 1.0f, 0.0f };
	RadeonProRender::float4 rotation = { 0.0f, 0.0f, 0.0f, 0.0f };
	RadeonProRender::float3 radiantPower = { 55.0f, 45.0f, 45.0f };
	rpr_float intensity = 10.0f;
};

struct EnvLightSettings
{
	//Currently we only specify image path but migh add multiple settings here
	std::string path = "../../Resources/Textures/envLightImage.exr";
};

struct ShapeSettings
{
	std::string name = "cornelBox";
	std::string path = "../../Resources/Meshes/cornellBox.obj";
	RadeonProRender::float3 translation = { 0.0f, 0.0f, 0.0f };
	RadeonProRender::float4 rotation = { 0.0f, 0.0f, 0.0f, 0.0f };
	RadeonProRender::float3 scale = { 0.0f, 0.0f, 0.0f };
};

struct Configuration
{
	ContextSettings contextSettings;
	CameraSettings cameraSettings;
	std::vector<LightSettings> lightSettings;
	std::vector<ShapeSettings> shapeSettings;
	std::vector<EnvLightSettings> envLightSettings;
};

const ContextSettings parseContextSettings(const picojson::value& config)
{
	ContextSettings settings;

	auto context = config.get<picojson::object>();

	std::string renderMode;
	fill(context, "rendermode", renderMode);
	settings.renderMode = renderModesMap[renderMode];

	std::string recursion;
	fill(context, "recursion", recursion);
	settings.recursion = std::stoi(recursion);

	std::string width;
	fill(context, "width", width);
	settings.width = std::stoi(width);

	std::string height;
	fill(context, "height", height);
	settings.height = std::stoi(height);

	std::string iterations;
	fill(context, "iterations", iterations);
	settings.iterations = std::stoi(iterations);

	std::string batchSize;
	fill(context, "batchsize", batchSize);
	settings.batchSize = std::stoi(batchSize);


	fill(context, "output", settings.outImgFile);
	fill(context, "output.json", settings.outJsonFile);

	std::string gui;
	fill(context, "gui", gui);
	settings.gui = std::stoi(gui);

	std::string cpu;
	std::string gpu;

	auto c = context.find("device")->second.get<picojson::object>();

	fill(c, "gpu0", gpu);
	fill(c, "cpu", cpu);

	if (std::stoi(gpu))
	{
		settings.creationFlags = creationModesMap[c.find("gpu0")->first];
	}

	if (std::stoi(cpu))
	{
		settings.creationFlags |= RPR_CREATION_FLAGS_ENABLE_CPU;
	}


#if defined(USING_NORTHSTAR) && defined(__APPLE__) 
	settings.creationFlags |= RPR_CREATION_FLAGS_ENABLE_METAL; // by default always enable Metal for MacOS
#endif

	return settings;
}

const CameraSettings parseCameraSettings(const picojson::value& config)
{
	CameraSettings settings;

	auto camera = config.get<picojson::object>();

	fill(camera, "position", settings.position);
	fill(camera, "aimed", settings.aimed);
	fill(camera, "up", settings.up);
	fill(camera, "focal_length", settings.focalLength);
	fill(camera, "translation", settings.translation);
	fill(camera, "rotation", settings.rotation);

	std::string cameraMode;
	fill(camera, "type", cameraMode);
	settings.cameraMode = cameraModesMap[cameraMode];

	return settings;
}

const LightSettings parseLightSettings(const picojson::value& config)
{
	LightSettings settings;

	auto light = config.get<picojson::object>();

	fill(light, "type", settings.type);
	fill(light, "translation", settings.translation);
	fill(light, "rotation", settings.rotation);
	fill(light, "radiant_power", settings.radiantPower);
	fill(light, "intensity", settings.intensity);

	return settings;
}

const EnvLightSettings parseEnvLightSettings(const picojson::value& config)
{
	EnvLightSettings e;
	std::string path;

	auto ibl = config.get<picojson::object>();
	fill(ibl, "path", path);
	e.path = path;

	return e;
}

const ShapeSettings parseShapeSettings(const picojson::value& config)
{
	ShapeSettings settings;

	auto obj = config.get<picojson::object>();

	fill(obj, "name", settings.name);
	fill(obj, "geomObjFile", settings.path);
	fill(obj, "translation", settings.translation);
	fill(obj, "rotation", settings.rotation);
	fill(obj, "scale", settings.scale);

	return settings;
}

const Configuration loadConfigFile(const char* filepath)
{
	std::ifstream i(filepath, std::ios::binary);
	if (!i.is_open())
	{
		std::cout << "Config file " << filepath << " not found." << '\n';
		std::exit(EXIT_FAILURE);
	}

	std::size_t sizeInByte = 0;
	{
		std::streampos begin = i.tellg();
		i.seekg(0, std::ios::end);
		std::streampos end = i.tellg();
		sizeInByte = end - begin;
	}

	if (sizeInByte == 0)
	{
		std::cout << "Config file" << filepath << "is empty." << '\n';
		std::cout << "Continue with default configuration .... \n";
	}

	i.clear();
	i.seekg(0, std::ios::beg);

	std::vector<char> json;
	json.resize(sizeInByte);
	i.read(json.data(), sizeInByte);
	i.close();

	picojson::value v;
	std::string err;
	const char* json_end = picojson::parse(v, json.data(), json.data() + json.size(), &err);

	Configuration config;
	bool bContext = false;
	bool bLight = false;
	bool bCamera = false;
	bool bIbl = false;
	bool bShape = false;

	try
	{
		if (err.empty())
		{
			const picojson::object& o = v.get<picojson::object>();

			for (picojson::object::const_iterator i = o.begin(); i != o.end(); ++i)
			{
				if (i->first.find("context") != std::string::npos)
				{
					config.contextSettings = parseContextSettings(i->second);
					bContext = true;
				}
				else if (i->first.find("obj") != std::string::npos)
				{
					config.shapeSettings.push_back(parseShapeSettings(i->second));
					bShape = true;
				}
				else if (i->first.find("light") != std::string::npos)
				{
					config.lightSettings.push_back(parseLightSettings(i->second));
					bLight = true;
				}
				else if (i->first.find("camera") != std::string::npos)
				{
					config.cameraSettings = std::move(parseCameraSettings(i->second));
					bCamera = true;
				}
				if (i->first.find("ibl") != std::string::npos)
				{
					config.envLightSettings.push_back(parseEnvLightSettings(i->second));
					bIbl = true;
				}
			}
		}
	}
	catch (std::exception&)
	{
		std::cout << "\nSyntax error with config file!!!\n";
		std::exit(EXIT_FAILURE);
	}

	if (!bContext)
	{
		std::cout << "\nNo context defined in config, Initializing default context : \n";
		std::cout << "Creation Flags     : " << config.contextSettings.creationFlags << '\n';
		std::cout << "Max Iterations     : " << config.contextSettings.iterations << '\n';
		std::cout << "GUI Mode           : " << config.contextSettings.gui << '\n';
		std::cout << "Framebuffer Height : " << config.contextSettings.height << '\n';
		std::cout << "Framebuffer Width  : " << config.contextSettings.width << '\n';
		std::cout << "Rendermode         : " << config.contextSettings.renderMode << '\n';
		std::cout << "Recursion          : " << config.contextSettings.recursion << '\n';
		std::cout << "Batchsize          : " << config.contextSettings.batchSize << '\n';
		std::cout << "outImageFile       : " << config.contextSettings.outImgFile << "\n\n";
	}

	if (!bLight && !bIbl)
	{
		EnvLightSettings settings;
		std::cout << "\nNo lights are defined config, Initializing default Image based lighting :\n";
		std::cout << "Image based light : " << settings.path << '\n';
		config.envLightSettings.push_back(settings);
	}

	if (!bCamera)
	{
		std::cout << "\nNo camera defined in config, Initializing default camera : \n";
		std::cout << "Aimed       :" << config.cameraSettings.aimed << '\n';
		std::cout << "Position    :" << config.cameraSettings.position << '\n';
		std::cout << "Rotation    :" << config.cameraSettings.rotation << '\n';
		std::cout << "Translation :" << config.cameraSettings.translation << '\n';
		std::cout << "Up          :" << config.cameraSettings.up << '\n';
		std::cout << "Focal Length:" << config.cameraSettings.focalLength << '\n';
		std::cout << "Camera Mode :" << config.cameraSettings.cameraMode << "\n\n";
	}

	if (!bShape)
	{
		ShapeSettings settings;
		std::cout << "\nNo shapes defined in config, Initializing default shape : \n";
		std::cout << "Name        : " << settings.name << '\n';
		std::cout << "Path        :" << settings.path << '\n';
		std::cout << "Rotation    : " << settings.rotation << '\n';
		std::cout << "Translation : " << settings.translation << '\n';
		std::cout << "Scale       : " << settings.scale << "\n\n";

		settings.translation = RadeonProRender::float3{ 0.0f, -3.0f, 0.0f };
		settings.scale = RadeonProRender::float3{ 1.0f, 1.0f, 1.0f };
		config.shapeSettings.push_back(settings);
	}

	return config;
}

Context createContext(const ContextSettings settings)
{
	Context outContext;

	rpr_int status = RPR_SUCCESS;

	rpr_int tahoePluginID = rprRegisterPlugin(RPR_PLUGIN_FILE_NAME);
	CHECK_NE(tahoePluginID, -1);

	rpr_int plugins[] = { tahoePluginID };
	size_t pluginCount = sizeof(plugins) / sizeof(plugins[0]);

	// Create context using single GPU 
	CHECK(rprCreateContext(RPR_API_VERSION, plugins, pluginCount, settings.creationFlags, g_contextProperties, NULL, &outContext.context));

	CHECK(rprContextSetActivePlugin(outContext.context, plugins[0]));

	if (status != RPR_SUCCESS)
	{
		std::cout << "RPR Context creation failed.\n";
		std::exit(EXIT_FAILURE);
	}

	CHECK(rprContextCreateScene(outContext.context, &outContext.scene));

	CHECK(rprContextSetScene(outContext.context, outContext.scene));

	rpr_framebuffer_desc desc = { settings.width , settings.height };

	rpr_framebuffer_format fmt = { 4, RPR_COMPONENT_TYPE_FLOAT32 };

	CHECK(rprContextCreateFrameBuffer(outContext.context, fmt, &desc, &outContext.fb));
	//CHECK(rprContextCreateFrameBuffer(g_context, fmt, &desc, &g_frame_buffer_2));

	CHECK(rprFrameBufferClear(outContext.fb));

	CHECK(rprContextCreateFrameBuffer(outContext.context, fmt, &desc, &outContext.fb1));

	CHECK(rprFrameBufferClear(outContext.fb1));

	CHECK(rprContextSetAOV(outContext.context, RPR_AOV_COLOR, outContext.fb));

	CHECK(rprContextSetParameterByKey1u(outContext.context, RPR_CONTEXT_RENDER_MODE, settings.renderMode));

	outContext.max_iterations = settings.iterations;
	outContext.outFileName = settings.outImgFile;
	return outContext;
}


//void createAndAttachCamera(rpr_context& context, rpr_scene& scene, const CameraSettings& settings)
//{
//	// ToDo: add code to create other types of cameras
//	rpr_camera camera = nullptr;
//
//	if (settings.cameraMode & RPR_CAMERA_MODE_PERSPECTIVE)
//	{
//		CHECK(rprContextCreateCamera(context, &camera));
//
//		CHECK(rprCameraLookAt(camera, settings.position.x, settings.position.y, settings.position.z,
//			settings.aimed.x, settings.aimed.y, settings.aimed.z,
//			settings.up.x, settings.up.y, settings.up.z));
//
//		CHECK(rprCameraSetFocalLength(camera, settings.focalLength));
//
//		CHECK(rprCameraSetMode(camera, settings.cameraMode));
//
//		CHECK(rprSceneSetCamera(scene, camera));
//	}
//
//	garbageCollector.push_back(camera);
//}
//202310
void createAndAttachCamera(rpr_context& context, rpr_scene& scene, const CameraSettings& settings)
{
	// ToDo: add code to create other types of cameras
	//rpr_camera camera = nullptr;
	if (settings.cameraMode & RPR_CAMERA_MODE_PERSPECTIVE)
	{
		CHECK(rprContextCreateCamera(context, &g_camera));
		CHECK(rprCameraLookAt(g_camera, settings.position.x, settings.position.y, settings.position.z,
			settings.aimed.x, settings.aimed.y, settings.aimed.z,
			settings.up.x, settings.up.y, settings.up.z));
		CHECK(rprCameraSetFocalLength(g_camera, settings.focalLength));
		CHECK(rprCameraSetMode(g_camera, settings.cameraMode));
		CHECK(rprSceneSetCamera(scene, g_camera));
	}
	garbageCollector.push_back(g_camera);
}
void createAndAttachCamera2(rpr_context& context, rpr_scene& scene, const CameraSettings& settings)
{
	// ToDo: add code to create other types of cameras
	//rpr_camera camera = nullptr;
	if (settings.cameraMode & RPR_CAMERA_MODE_PERSPECTIVE)
	{
		CHECK(rprContextCreateCamera(context, &g_camera));
		CHECK(rprCameraLookAt(g_camera, settings.position.x, settings.position.y, settings.position.z,
			settings.aimed.x, settings.aimed.y, settings.aimed.z,
			settings.up.x, settings.up.y, settings.up.z));

		CHECK(rprCameraSetFocalLength(g_camera, settings.focalLength));
		CHECK(rprCameraSetMode(g_camera, settings.cameraMode));
		CHECK(rprSceneSetCamera(scene, g_camera));
	}
	garbageCollector.push_back(g_camera);
}

void createAndAttachLight(rpr_context& context, rpr_scene& scene, const LightSettings& settings)
{
	// ToDo: add code to create other types of lights
	rpr_light light = nullptr;

	if (settings.type == "point")
	{
		CHECK(rprContextCreatePointLight(context, &light));

		RadeonProRender::matrix lightm = RadeonProRender::translation(settings.translation);

		CHECK(rprLightSetTransform(light, RPR_TRUE, &lightm.m00));

		// Radiant power
		CHECK(rprPointLightSetRadiantPower3f(light, settings.radiantPower.x, settings.radiantPower.x, settings.radiantPower.x));

		CHECK(rprSceneAttachLight(scene, light));
	}

	garbageCollector.push_back(light);
}

void createAndAttachEnvironmentLight(rpr_context& context, rpr_scene& scene, const EnvLightSettings& settings)
{
	if (settings.path.empty())
	{
		return; //No image based lighting
	}

	rpr_int status = RPR_SUCCESS;
	rpr_light light = nullptr;
	rpr_image img = nullptr;
	{
		CHECK(rprContextCreateEnvironmentLight(context, &light));

		CHECK(rprContextCreateImageFromFile(context, settings.path.c_str(), &img));
		if (status == RPR_ERROR_IO_ERROR)
		{
			std::cout << "Error Ibl file : " << settings.path << " not found.\n";
			std::exit(EXIT_FAILURE);
		}

		// Set an image for the light to take the radiance values from
		CHECK(rprEnvironmentLightSetImage(light, img));

		CHECK(rprSceneAttachLight(scene, light));
	}

	garbageCollector.push_back(light);
	garbageCollector.push_back(img);
}

void loadTexture(rpr_context& context, rpr_material_node mat, std::string texture)
{
	rpr_image img;
	rpr_int status;

	status = rprContextCreateImageFromFile(context, texture.c_str(), &img);

	if (status == RPR_ERROR_IO_ERROR)
	{
		std::cout << "Error in loadTexture() : " << texture << " not found.\n";
		std::exit(EXIT_FAILURE);
	}

	CHECK(status);

	CHECK(rprMaterialNodeSetInputImageDataByKey(mat, RPR_MATERIAL_INPUT_DATA, img));

	garbageCollector.push_back(img);
}


//-------------**************-------------------------
//202310_LAMMPS_GROMACS
struct AtomData {
	int num=0; //LG
	double xave=0, yave=0, zave=0; //LG

	std::vector<int> id; //LG
	std::vector<int> type; //L
	std::vector<std::string> typen; //G
	std::vector<std::string> molname; //G
	std::vector<double> x, y, z;

	AtomData() {}
	
	AtomData(size_t size)
		: id(size), type(size), typen(size), molname(size), x(size), y(size), z(size) {}

	void resize(size_t size) {
		id.resize(size);
		type.resize(size);
		typen.resize(size);
		molname.resize(size);
		x.resize(size);
		y.resize(size);
		z.resize(size);
	}
};

const int AtomDataType = 20; //10:LAMMPS, 20:GROMACS
const int WatVanish = 1; //for GROMACS, 1:ON, 0:OFF

//std::vector<AtomData> extractAtomData() {
AtomData extractAtomData() {

	//const std::string SimFileName = "../89_sample_data/dump.lammpstrj_ionliquid_test10"; 
	//const std::string SimFileName = "../89_sample_data/dump.lammpstrj_ionliquid_27000";
	//const std::string SimFileName = "../89_sample_data/dump.lammpstrj_ionliquid_125000";
	//const std::string SimFileName = "../89_sample_data/dump.lammpstrj_ionliquid_1000";
	//const std::string SimFileName = "../89_sample_data/dump.lammpstrj_Ionic_555";
	//const std::string SimFileName = "../89_sample_data/md_0_1_1M_test100.gro";
	//const std::string SimFileName = "../89_sample_data/md_0_1_1M_test111.gro";
	const std::string SimFileName = "../89_sample_data/md_0_1_1M.gro";

	std::ifstream file(SimFileName);
	if (!file.is_open()) {
		std::cerr << "Failed to open the file." << std::endl;
		exit(1);
	}

	std::string line;
	int numberOfAtoms = 0;
	bool readAtoms = false;

	AtomData data;

	//LAMMPS
	if (AtomDataType == 10) {
		while (std::getline(file, line)) {
			if (line == "ITEM: NUMBER OF ATOMS") {
				file >> numberOfAtoms;
				std::cout << "reading lammps file, num: " << numberOfAtoms << '\n';
				data.resize(numberOfAtoms);
				data.num = numberOfAtoms;
			}
			else if (line == "ITEM: ATOMS id mol type element q xu yu zu") {
				readAtoms = true;
				continue;  // Skip header line
			}
			if (readAtoms && numberOfAtoms > 0) {
				//std::cout << "numberOfAtoms " << numberOfAtoms << '\n';
				int mol;
				std::string element;
				double q;
				std::istringstream iss(line);
				int j = numberOfAtoms - 1;
	
				iss >> data.id[j] >> mol >> data.type[j] >> element >> q >> data.x[j] >> data.y[j] >> data.z[j];

				numberOfAtoms--;
			}
		}
	}
	//GROMACS
	else if (AtomDataType == 20) {
		while (std::getline(file, line)) {
			if (line == "Generic title") {
				std::getline(file, line);  
				std::istringstream iss(line);
				iss >> numberOfAtoms;
				std::cout << "reading gromacs file, num: " << numberOfAtoms << '\n';
				data.resize(numberOfAtoms);
				data.num = numberOfAtoms;
				readAtoms = true;
				continue;  // Skip header line
			}
	
			if (readAtoms && numberOfAtoms > 0) {

				int molid, id;
				double vx, vy, vz;
				std::istringstream iss(line);
				int j = numberOfAtoms - 1;
	
				iss >> molid >> data.molname[j];
				char typen[8];       // 
				iss.read(typen, 7);  // 
				typen[7] = '\0';     // null
				data.typen[j] = typen;
				iss >> id >> data.x[j] >> data.y[j] >> data.z[j] >> vx >> vy >> vz;
	
				numberOfAtoms--;	
			}
		}
	}
	else {
		std::cout << "ERROR : No simulation format is detected." << '\n';
	}


	double sum_x = 0.0, sum_y = 0.0, sum_z = 0.0;
	for (int i = 0; i < (int)data.num ; ++i) {
		sum_x += data.x[i];
		sum_y += data.y[i];
		sum_z += data.z[i];
	}
	data.xave = sum_x / double(data.num);
	data.yave = sum_y / double(data.num);
	data.zave = sum_z / double(data.num);
	
	std::cout << "ave: " << data.xave << " " << data.yave << " " << data.zave << '\n';

	return data;
}


//GROMACS, for charactor finding
std::string trim(const std::string& str) {
	size_t first = str.find_first_not_of(" \t");
	if (std::string::npos == first) {
		return "";
	}
	size_t last = str.find_last_not_of(" \t");
	return str.substr(first, (last - first + 1));
}

//-------------**************-------------------------


//void loadAndAttachShapes(rpr_context& context, rpr_scene& scene, rpr_material_system matsys, const ShapeSettings& settings)
//void loadAndAttachShapes(rpr_context& context, rpr_scene& scene, rpr_material_system matsys, const ShapeSettings& settings, const std::vector<AtomData>& atoms)
void loadAndAttachShapes(rpr_context& context, rpr_scene& scene, rpr_material_system matsys, const ShapeSettings& settings, const AtomData& atoms)
{	
	// ToDo : parse more material options
	struct Material
	{
		RadeonProRender::float3 dColor{ 0.0f };
		RadeonProRender::float3 aColor{ 0.0f }; //Not used currently
		RadeonProRender::float3 sColor{ 0.0f };
		RadeonProRender::float3 eColor{ 0.0f };
		RadeonProRender::float3 absorptionColor{ 0.0f };
		rpr_float ior = 0.0f; //Ni
		rpr_float roughness = 0.0f;
		rpr_float shininess = 0.0f;// Ns specular weight
		rpr_float d = 1.0f; //dissolve default : opaque
		rpr_int illum = 2; //default illumination model
		std::string dTexture;
		std::string bTexture;
		std::string alphaTexture;
		std::string name;
	};

	const auto lerp = [](const float a, const float b, const float t)->float {

		return (t * b) + (1 - t) * a;
	};

	const auto inverseLerp = [](const float a, const float b, const float value)->float {
		return (value - a) / (b - a);
	};

	const auto isPresent = [](const RadeonProRender::float3& color)->bool {
		return (color.x != 0.0f) || (color.y != 0.0f) || (color.z != 0.0f);
	};

	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
	std::string err;
	std::string warning;

	std::string mtlBaseDir = settings.path.substr(0, settings.path.rfind("/"));

	if (mtlBaseDir.empty()) {
		mtlBaseDir = ".";
	}

	mtlBaseDir += "/";

	bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warning, &err, settings.path.c_str(), mtlBaseDir.c_str());

	if (!warning.empty()) {
		std::cout << "OBJ Loader WARN : " << warning << '\n';
	}

	if (!err.empty()) {
		std::cout << "OBJ Loader ERROR : " << err << '\n';
	}

	if (!ret) {
		std::cout << "Failed to load obj file\n";
		std::exit(EXIT_FAILURE);
	}

	//Parse materials
	auto convert = [](const tinyobj::real_t c[3])->RadeonProRender::float3 {
		return RadeonProRender::float3{ c[0], c[1], c[2] };
	};

	// Map holds particular material of material_id
	uint32_t matId = 0;
	std::map<uint32_t, Material> matMap;

	for (const auto& mat : materials)
	{
		Material m;

		m.aColor = convert(mat.ambient);
		m.dColor = convert(mat.diffuse);
		m.sColor = convert(mat.specular);
		m.eColor = convert(mat.emission);
		m.absorptionColor = convert(mat.transmittance);
		m.dTexture = mat.diffuse_texname;
		m.bTexture = mat.bump_texname;
		m.alphaTexture = mat.alpha_texname;
		m.ior = mat.ior;
		m.d = mat.dissolve;
		m.illum = mat.illum;
		m.roughness = mat.roughness;
		m.shininess = mat.shininess;
		//-------------**************-------------------------
		m.roughness = g_input_roughness;
		m.shininess = g_input_shininess;
		//-------------**************-------------------------
		m.name = mat.name;
		matMap[matId++] = m;
	}

	// Load geometry
	const rpr_int vertexStride = 3 * sizeof(tinyobj::real_t);
	const rpr_int normalStride = 3 * sizeof(tinyobj::real_t);
	const rpr_int texCoordStride = 2 * sizeof(tinyobj::real_t);;

	for (size_t i = 0; i < shapes.size(); ++i)
	{
		std::vector<rpr_int>faceVert(shapes[i].mesh.num_face_vertices.begin(), shapes[i].mesh.num_face_vertices.end());
		std::map<uint32_t, std::vector<uint32_t>> faceMaterialMap;     //Map holds face indices for particular material_id
		std::vector<uint32_t> vIndices;
		std::vector<uint32_t> nIndices;
		std::vector<uint32_t> tIndices;

		bool isValidTex = true;
		bool isValidNorm = true;

		for (size_t j = 0; j < shapes[i].mesh.indices.size() / 3; j++)
		{
			tinyobj::index_t idx0 = shapes[i].mesh.indices[3 * j + 0];
			tinyobj::index_t idx1 = shapes[i].mesh.indices[3 * j + 1];
			tinyobj::index_t idx2 = shapes[i].mesh.indices[3 * j + 2];

			vIndices.push_back(idx0.vertex_index); vIndices.push_back(idx1.vertex_index); vIndices.push_back(idx2.vertex_index);

			if (idx0.normal_index < 0 || idx1.normal_index < 0 || idx2.normal_index < 0)
			{
				isValidNorm = false;
			}
			else
			{
				nIndices.push_back(idx0.normal_index); nIndices.push_back(idx1.normal_index); nIndices.push_back(idx2.normal_index);
			}

			if (idx0.texcoord_index < 0 || idx1.texcoord_index < 0 || idx2.texcoord_index < 0)
			{
				isValidTex = false;
			}
			else
			{
				tIndices.push_back(idx0.texcoord_index); tIndices.push_back(idx1.texcoord_index); tIndices.push_back(idx2.texcoord_index);
			}
		}

		//Fill material corresponding faces in map
		for (size_t face = 0; face < shapes[i].mesh.num_face_vertices.size(); face++)
		{
			const auto matId = shapes[i].mesh.material_ids[face];
			faceMaterialMap[matId].push_back(face);
		}

		rpr_shape t_shape = nullptr;
		{
			CHECK(rprContextCreateMesh(context,
				(rpr_float const*)attrib.vertices.data(),
				(size_t)attrib.vertices.size() / 3,
				vertexStride,
				(isValidNorm) ? (rpr_float const*)attrib.normals.data() : nullptr,
				(isValidNorm) ? (size_t)attrib.normals.size() / 3 : 0,
				(isValidNorm) ? normalStride : 0,
				(isValidTex) ? (rpr_float const*)attrib.texcoords.data() : nullptr,
				(isValidTex) ? (size_t)attrib.texcoords.size() / 2 : 0,
				(isValidTex) ? texCoordStride : 0,
				(rpr_int const*)vIndices.data(),
				sizeof(int),
				(isValidNorm) ? (rpr_int const*)nIndices.data() : nullptr,
				(isValidNorm) ? sizeof(int) : 0,
				(isValidTex) ? (rpr_int const*)tIndices.data() : nullptr,
				(isValidTex) ? sizeof(int) : 0,
				(rpr_int const*)faceVert.data(),
				(size_t)faceVert.size(),
				&t_shape));

			CHECK(rprSceneAttachShape(scene, t_shape));
			//RadeonProRender::matrix m = RadeonProRender::translation(settings.translation) * RadeonProRender::scale(settings.scale);
			//CHECK(rprShapeSetTransform(t_shape, RPR_TRUE, &m.m00));

			//-------------**************-------------------------
			float atom_shift_x = -(float)atoms.xave / 2.0f; //-2.5f; //lammps
			float atom_shift_y = -(float)atoms.yave / 2.0f; //-2.0f;
			float atom_shift_z = -(float)atoms.zave / 2.0f; // 0.0f;
			float atom_pos_rate;

			//LAMMPS
			if (AtomDataType == 10) {
				atom_pos_rate = 0.1f;
			}
			//GROMACS
			else if (AtomDataType == 20) {
				atom_pos_rate = 0.5f;
			}

			//for the obj sphere
			RadeonProRender::matrix m = RadeonProRender::translation({
							(float)atoms.x[i] + atom_shift_x, (float)atoms.y[i] + atom_shift_y, (float)atoms.z[i] + atom_shift_z })
							* RadeonProRender::scale(settings.scale * 10.0f);
			//Unvisible the obj sphere
			CHECK(rprShapeSetVisibility(t_shape, RPR_FALSE));

			CHECK(rprShapeSetTransform(t_shape, RPR_TRUE, &m.m00));


			//202310_BEGIN
			static int done = 0;

			rpr_material_system materialSystem;
			rprContextCreateMaterialSystem(context, 0, &materialSystem);
			rpr_material_node diffuseMaterial_r, diffuseMaterial_g, diffuseMaterial_b;
			//red
			float color_r[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
			rprMaterialSystemCreateNode(materialSystem, RPR_MATERIAL_NODE_DIFFUSE, &diffuseMaterial_r);
			rprMaterialNodeSetInputFByKey(diffuseMaterial_r, RPR_MATERIAL_INPUT_COLOR, color_r[0], color_r[1], color_r[2], color_r[3]);
			//green
			float color_g[4] = { 0.0f, 1.0f, 0.0f, 1.0f };
			rprMaterialSystemCreateNode(materialSystem, RPR_MATERIAL_NODE_DIFFUSE, &diffuseMaterial_g);
			rprMaterialNodeSetInputFByKey(diffuseMaterial_g, RPR_MATERIAL_INPUT_COLOR, color_g[0], color_g[1], color_g[2], color_g[3]);
			//blue/
			float color_b[4] = { 0.0f, 0.0f, 1.0f, 1.0f };
			rprMaterialSystemCreateNode(materialSystem, RPR_MATERIAL_NODE_DIFFUSE, &diffuseMaterial_b);
			rprMaterialNodeSetInputFByKey(diffuseMaterial_b, RPR_MATERIAL_INPUT_COLOR, color_b[0], color_b[1], color_b[2], color_b[3]);

			rpr_material_node diffuseMaterial_lg, diffuseMaterial_go, diffuseMaterial_gr, diffuseMaterial_bw;
			//lightgreen
			float color_lg[4] = { 0.47f, 1.0f, 0.0f, 1.0f };
			rprMaterialSystemCreateNode(materialSystem, RPR_MATERIAL_NODE_DIFFUSE, &diffuseMaterial_lg);
			rprMaterialNodeSetInputFByKey(diffuseMaterial_lg, RPR_MATERIAL_INPUT_COLOR, color_lg[0], color_lg[1], color_lg[2], color_lg[3]);
			//gold
			float color_go[4] = { 1.0f, 0.85f, 0.0f, 1.0f };
			rprMaterialSystemCreateNode(materialSystem, RPR_MATERIAL_NODE_DIFFUSE, &diffuseMaterial_go);
			rprMaterialNodeSetInputFByKey(diffuseMaterial_go, RPR_MATERIAL_INPUT_COLOR, color_go[0], color_go[1], color_go[2], color_go[3]);
			//gray
			float color_gr[4] = { 0.39f, 0.39f, 0.39f, 1.0f };
			rprMaterialSystemCreateNode(materialSystem, RPR_MATERIAL_NODE_DIFFUSE, &diffuseMaterial_gr);
			rprMaterialNodeSetInputFByKey(diffuseMaterial_gr, RPR_MATERIAL_INPUT_COLOR, color_gr[0], color_gr[1], color_gr[2], color_gr[3]);
			//blue-white
			float color_bw[4] = { 0.94f, 0.97f, 1.00f, 1.0f };
			rprMaterialSystemCreateNode(materialSystem, RPR_MATERIAL_NODE_DIFFUSE, &diffuseMaterial_bw);
			rprMaterialNodeSetInputFByKey(diffuseMaterial_bw, RPR_MATERIAL_INPUT_COLOR, color_bw[0], color_bw[1], color_bw[2], color_bw[3]);


			//std::cout << "atom's size: " << atoms.size() << std::endl;
			//std::cout << "10th atom's num: " << atoms[9].num << std::endl;
			//std::cout << "10th atom's type: " << atoms[9].type << std::endl;
			//std::cout << "10th atom's x: " << atoms[9].x << std::endl;
			//std::cout << "10th atom's y: " << atoms[9].y << std::endl;
			//std::cout << "10th atom's z: " << atoms[9].z << std::endl;

			//LAMMPS, ionic liquid
			//1N 2C 3C 4C 5H 6C 7H 8H 9C 10H 11CL
			std::unordered_set<int> valid_num_H = { 5, 7, 8, 10 };
			std::unordered_set<int> valid_num_C = { 2, 3, 4, 6, 9 };
			std::unordered_set<int> valid_num_N = { 1 };
			std::unordered_set<int> valid_num_CL = { 11 };

			//GROMACS, 1M
			std::unordered_set<std::string> valid_name_H = { "H","H1","H2","H3", "HA", "HA2", "HA3", "HB", "HB1", "HB2", "HB3", "HD1", "HD11", "HD12", "HD13", "HD2", "HD21", "HD22", "HD23", "HD3", "HE", "HE1", "HE2", "HE21", "HE22", "HE3", "HG", "HG1", "HG11", "HG12", "HG13", "HG2", "HG21", "HG22", "HG23", "HG3", "HH", "HH11", "HH12", "HH2", "HH21", "HH22", "HZ", "HZ1", "HZ2", "HZ3"};
			std::unordered_set<std::string> valid_name_C = { "C","CA","CB","CD","CD1","CD2","CE","CE1","CE2","CE3","CG","CG1","CG2","CH2","CZ","CZ2","CZ3" };
			std::unordered_set<std::string> valid_name_N = { "N","ND1","ND2","NE","NE1","NE2","NH1","NH2","NZ" };
			std::unordered_set<std::string> valid_name_O = { "O","OD1","OD2","OE1","OE2","OG","OG1","OH","OXT" };
			std::unordered_set<std::string> valid_name_S = { "SD","SG"};
			std::unordered_set<std::string> valid_name_CL = {"Cl","Cl-" };
			std::unordered_set<std::string> vanish_mol_name = { "WAT" };

			//if (done == 1)
			if (done == 0)
			{
				//20231024def
				//for (int i = 0; i < 100; i++)
				//	for (int j = 0; j < 100; j++)
				//	{
				//		rpr_shape s;
				//		auto e = rprContextCreateInstance(context, t_shape, &s);
				//		RadeonProRender::matrix m = RadeonProRender::translation({ (float)(i - 50) * 0.1f,(float)(j - 50) * 0.1f,0.f })
				//			* RadeonProRender::scale(settings.scale * 0.05f);
				//		rprShapeSetTransform(s, RPR_TRUE, &m.m00);
				//		rprSceneAttachShape(scene, s);
				//	}
				//20231024mod1
				//for (int i = 0; i < 100; i++)
				//	for (int j = 0; j < 100; j++)
				//	{
				//		rpr_shape s;
				//		auto e = rprContextCreateInstance(context, t_shape, &s);
				//		RadeonProRender::matrix m = RadeonProRender::translation({ (float)(i - 50) * 0.1f,(float)(j - 50) * 0.1f,0.f })
				//			* RadeonProRender::scale(settings.scale * 0.05f);
				//		rprShapeSetTransform(s, RPR_TRUE, &m.m00);
				//		rprShapeSetMaterial(s, diffuseMaterial_b);
				//		rprSceneAttachShape(scene, s);
				//	}
				//20231024mod2
				for (int i = 0; i < int(atoms.num); i++)
				{
					float atom_size;

					rpr_shape s;
					auto e = rprContextCreateInstance(context, t_shape, &s);
					//RadeonProRender::matrix m = RadeonProRender::translation({ (float)atoms[i].x * 0.1f, (float)atoms[i].y * 0.1f, (float)atoms[i].z * 0.1f })
					//	* RadeonProRender::scale(settings.scale * 0.05f);
					//rprShapeSetTransform(s, RPR_TRUE, &m.m00);

					//LAMMPS
					if (AtomDataType == 10) {
						if (valid_num_H.find((int)atoms.type[i]) != valid_num_H.end()) {
							rprShapeSetMaterial(s, diffuseMaterial_bw);
							atom_size = 0.04f;
						}
						else if (valid_num_C.find((int)atoms.type[i]) != valid_num_C.end()) {
							rprShapeSetMaterial(s, diffuseMaterial_gr);
							atom_size = 0.08f;
						}
						else if (valid_num_N.find((int)atoms.type[i]) != valid_num_N.end()) {
							rprShapeSetMaterial(s, diffuseMaterial_b);
							atom_size = 0.08f;
						}
						else if (valid_num_CL.find((int)atoms.type[i]) != valid_num_CL.end()) {
							rprShapeSetMaterial(s, diffuseMaterial_lg);
							atom_size = 0.12f;
						}
						else {
							rprShapeSetMaterial(s, diffuseMaterial_r);
							std::cout << "RED atom's type: " << (int)atoms.type[i] << std::endl;
							atom_size = 0.1f;
						}
						//RadeonProRender::matrix m = RadeonProRender::translation({
						//	(float)atoms.x[i] + atom_shift_x, (float)atoms.y[i] + atom_shift_y, (float)atoms.z[i] + atom_shift_z })
						//	* RadeonProRender::scale(settings.scale * atom_size) * 0.1f;


						RadeonProRender::matrix m = RadeonProRender::translation({ \
								((float)atoms.x[i] + atom_shift_x) * atom_pos_rate, \
								((float)atoms.y[i] + atom_shift_y) * atom_pos_rate, \
								((float)atoms.z[i] + atom_shift_z) * atom_pos_rate })\
								* RadeonProRender::scale(settings.scale * atom_size);
						rprShapeSetTransform(s, RPR_TRUE, &m.m00);
						rprSceneAttachShape(scene, s);
					}
					//GROMACS
					else if (AtomDataType == 20) {
						if (((WatVanish == 1)&&(vanish_mol_name.find(atoms.molname[i]) == vanish_mol_name.end()))
							||(WatVanish != 1)) {

							std::string trimmedType = trim(atoms.typen[i]);
							if (valid_name_H.find(trimmedType) != valid_name_H.end()) {
								rprShapeSetMaterial(s, diffuseMaterial_bw);
								atom_size = 0.04f;
							}
							else if (valid_name_C.find(trimmedType) != valid_name_C.end()) {
								rprShapeSetMaterial(s, diffuseMaterial_gr);
								atom_size = 0.08f;
							}
							else if (valid_name_O.find(trimmedType) != valid_name_O.end()) {
								rprShapeSetMaterial(s, diffuseMaterial_r);
								atom_size = 0.08f;
							}
							else if (valid_name_N.find(trimmedType) != valid_name_N.end()) {
								rprShapeSetMaterial(s, diffuseMaterial_b);
								atom_size = 0.08f;
							}
							else if (valid_name_S.find(trimmedType) != valid_name_S.end()) {
								rprShapeSetMaterial(s, diffuseMaterial_go);
								atom_size = 0.08f;
							}
							else if (valid_name_CL.find(trimmedType) != valid_name_CL.end()) {
								rprShapeSetMaterial(s, diffuseMaterial_lg);
								atom_size = 0.12f;
							}
							else {
								rprShapeSetMaterial(s, diffuseMaterial_g);
								std::cout << "GREEN atom's id: " << (int)i << " typen: " << atoms.typen[i] << std::endl;
								atom_size = 0.1f;
							}
							RadeonProRender::matrix m = RadeonProRender::translation({
								(float)atoms.x[i] * 0.5f + atom_shift_x, (float)atoms.y[i] * 0.5f + atom_shift_y, (float)atoms.z[i] * 0.5f + atom_shift_z })
								* RadeonProRender::scale(settings.scale * atom_size);
							rprShapeSetTransform(s, RPR_TRUE, &m.m00);
							rprSceneAttachShape(scene, s);
						}
					}
					else 
					{
						std::cout << "ERROR : No simulation model is detected " << std::endl;
					}
				}
			}
			done++;
			//202310_END

		}

		//-------------**************-------------------------




		// Avoid applying material to per face
		bool bAvoidFaceMat = (faceMaterialMap.size() == 1) ? true : false;

		//Apply materials per face
		for (const auto& fm : faceMaterialMap)
		{
			const Material& m = matMap[fm.first];
			const auto& faceIndices = fm.second;

			rpr_material_node t_uber = nullptr;

			CHECK(rprMaterialSystemCreateNode(matsys, RPR_MATERIAL_NODE_UBERV2, &t_uber));
			CHECK(bAvoidFaceMat ? rprShapeSetMaterial(t_shape, t_uber) :
				rprShapeSetMaterialFaces(t_shape, t_uber, (rpr_int const*)faceIndices.data(), faceIndices.size()));

			// ToDo : Add code to set other types of textures
			rpr_material_node t_diffuse = nullptr;
			if (!m.dTexture.empty())
			{
				const std::string texture = mtlBaseDir + "Textures/" + m.dTexture;

				rpr_material_node t_tex = nullptr;

				CHECK(rprMaterialSystemCreateNode(matsys, RPR_MATERIAL_NODE_DIFFUSE, &t_diffuse));
				CHECK(rprMaterialSystemCreateNode(matsys, RPR_MATERIAL_NODE_IMAGE_TEXTURE, &t_tex));

				loadTexture(context, t_tex, texture);

				CHECK(rprMaterialNodeSetInputNByKey(t_diffuse, RPR_MATERIAL_INPUT_COLOR, t_tex));

				CHECK(bAvoidFaceMat ? rprShapeSetMaterial(t_shape, t_diffuse) :
					rprShapeSetMaterialFaces(t_shape, t_diffuse, (rpr_int const*)faceIndices.data(), faceIndices.size()));

				garbageCollector.push_back(t_tex);
				garbageCollector.push_back(t_diffuse);
			}

			if (!m.bTexture.empty())
			{
				const std::string texture = mtlBaseDir + "Textures/" + m.bTexture;
				rpr_material_node t_micro = nullptr;
				rpr_material_node t_tex = nullptr;
				rpr_material_node t_mat = nullptr;

				rprMaterialSystemCreateNode(matsys, RPR_MATERIAL_NODE_MICROFACET, &t_micro);
				CHECK(rprMaterialNodeSetInputNByKey(t_micro, RPR_MATERIAL_INPUT_COLOR, t_diffuse));

				CHECK(rprMaterialSystemCreateNode(matsys, RPR_MATERIAL_NODE_BUMP_MAP, &t_mat));
				CHECK(rprMaterialSystemCreateNode(matsys, RPR_MATERIAL_NODE_IMAGE_TEXTURE, &t_tex));

				loadTexture(context, t_tex, texture);

				CHECK(rprMaterialNodeSetInputNByKey(t_mat, RPR_MATERIAL_INPUT_COLOR, t_tex));

				CHECK(rprMaterialNodeSetInputNByKey(t_micro, RPR_MATERIAL_INPUT_NORMAL, t_mat));

				CHECK(bAvoidFaceMat ? rprShapeSetMaterial(t_shape, t_micro) :
					rprShapeSetMaterialFaces(t_shape, t_micro, (rpr_int const*)faceIndices.data(), faceIndices.size()));

				garbageCollector.push_back(t_mat);
				garbageCollector.push_back(t_micro);
				garbageCollector.push_back(t_tex);
			}

			if (isPresent(m.eColor))
			{
				rpr_material_node t_emissive = nullptr;

				CHECK(rprMaterialSystemCreateNode(matsys, RPR_MATERIAL_NODE_EMISSIVE, &t_emissive));
				CHECK(rprMaterialNodeSetInputFByKey(t_emissive, RPR_MATERIAL_INPUT_COLOR, m.eColor.x, m.eColor.y, m.eColor.z, 1.f));
				CHECK(bAvoidFaceMat ? rprShapeSetMaterial(t_shape, t_emissive) :
					rprShapeSetMaterialFaces(t_shape, t_emissive, (rpr_int*)faceIndices.data(), faceIndices.size()));

				garbageCollector.push_back(t_emissive);
			}
			else
			{
				CHECK(rprMaterialNodeSetInputFByKey(t_uber, RPR_MATERIAL_INPUT_UBER_EMISSION_COLOR, m.eColor.x, m.eColor.y, m.eColor.z, 1.f));
			}

			CHECK(rprMaterialNodeSetInputFByKey(t_uber, RPR_MATERIAL_INPUT_UBER_DIFFUSE_COLOR, m.dColor.x, m.dColor.y, m.dColor.z, 1.f));

			if (m.illum == 5 || m.illum == 7) //Reflection and ray trace on
			{
				const rpr_float weight = inverseLerp(0.0f, 1000.0f, m.shininess);
				const float ior = (m.ior == 1.0f) ? m.ior * 3.0f : m.ior;

				CHECK(rprMaterialNodeSetInputFByKey(t_uber, RPR_MATERIAL_INPUT_UBER_REFLECTION_COLOR, m.sColor.x, m.sColor.y, m.sColor.z, 1.f));
				CHECK(rprMaterialNodeSetInputFByKey(t_uber, RPR_MATERIAL_INPUT_UBER_REFLECTION_WEIGHT, weight, weight, weight, weight));
				CHECK(rprMaterialNodeSetInputFByKey(t_uber, RPR_MATERIAL_INPUT_UBER_REFLECTION_IOR, ior, ior, ior, ior));
				CHECK(rprMaterialNodeSetInputFByKey(t_uber, RPR_MATERIAL_INPUT_UBER_REFLECTION_ROUGHNESS, m.roughness, m.roughness, m.roughness, m.roughness));
			}

			if (m.illum == 7)//Refraction and ray trace on
			{
				const rpr_float weight = inverseLerp(0.0f, 1000.0f, m.shininess);

				CHECK(rprMaterialNodeSetInputFByKey(t_uber, RPR_MATERIAL_INPUT_UBER_REFRACTION_COLOR, 1.0f - m.absorptionColor.x, 1.0f - m.absorptionColor.y, 1.0f - m.absorptionColor.z, 1.f));
				CHECK(rprMaterialNodeSetInputFByKey(t_uber, RPR_MATERIAL_INPUT_UBER_REFRACTION_WEIGHT, weight, weight, weight, weight));
				CHECK(rprMaterialNodeSetInputFByKey(t_uber, RPR_MATERIAL_INPUT_UBER_REFRACTION_IOR, m.ior, m.ior, m.ior, m.ior));
				CHECK(rprMaterialNodeSetInputFByKey(t_uber, RPR_MATERIAL_INPUT_UBER_REFRACTION_ROUGHNESS, m.roughness, m.roughness, m.roughness, m.roughness));
				CHECK(rprMaterialNodeSetInputFByKey(t_uber, RPR_MATERIAL_INPUT_UBER_REFRACTION_ABSORPTION_COLOR, m.absorptionColor.x, m.absorptionColor.y, m.absorptionColor.z, 1.f));
				CHECK(rprMaterialNodeSetInputFByKey(t_uber, RPR_MATERIAL_INPUT_UBER_REFRACTION_ABSORPTION_DISTANCE, 0.0f, 0.0f, 0.0f, 1.f));
				CHECK(rprMaterialNodeSetInputUByKey(t_uber, RPR_MATERIAL_INPUT_UBER_REFRACTION_CAUSTICS, 1));
				CHECK(rprContextSetParameterByKey1u(context, RPR_CONTEXT_MAX_RECURSION, 10));
			}

			garbageCollector.push_back(t_uber);
		}//Face
		garbageCollector.push_back(t_shape);
	}//Shape
}

void printHelp()
{
	// ToDo : print config specs
	std::cout << "64_rpr_mesh_obj_demo.exe cfg.json [-h | --help]" << '\n';
	return;
}


//-------------------****************----------------


//#include <cuda_runtime.h>
//#include <device_launch_parameters.h>

//const unsigned int WINDOW_WIDTH = 640;
//const unsigned int WINDOW_HEIGHT = 480;
//const unsigned int WINDOW_WIDTH  = 80*10;
//const unsigned int WINDOW_HEIGHT = 60*10;
const unsigned int WINDOW_WIDTH = 1280;
const unsigned int WINDOW_HEIGHT = 720;
//const unsigned int WINDOW_WIDTH = 1920;
//const unsigned int WINDOW_HEIGHT = 1080;


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




////-------------------*****************-----------------------------
//std::string filepath = "sphere.json";
////Load config 
//const Configuration config = loadConfigFile(filepath.c_str());
//rpr_int status = RPR_SUCCESS;
////Create global context holding rpr_context, rpr_scene and rpr_framebuffer
//Context contextSettings = createContext(config.contextSettings);
//
//
//rpr_context context = std::move(contextSettings.context);
//rpr_scene scene = std::move(contextSettings.scene);
//rpr_framebuffer frameBuffer = std::move(contextSettings.fb);
////-------------------*****************-----------------------------



GuiRenderImpl::Update g_update;
const auto g_invalidTime = std::chrono::time_point<std::chrono::high_resolution_clock>::max();
int					g_benchmark_numberOfRenderIteration = 0;
auto g_benchmark_start = g_invalidTime;

////202309
//const size_t n = 128;
//float atomposi[] = {
//	20.0f,30.0f,60.0f,
//	20.0f,40.0f,70.0f,
//	40.0f,30.0f,60.0f,
//	50.0f,70.0f,80.0f,
//	50.0f,80.0f,90.0f,
//	70.0f,70.0f,80.0f,
//};
//const int atomnum = sizeof(atomposi) / (3 * sizeof(float));
//const float atomrad = 20.0f;


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

/*
Display()
Update()
*/


// High batch size will increase the RPR performance ( rendering iteration per second ), but lower the render feedback FPS on the OpenGL viewer.
// Note that for better OpenGL FPS (and decreased RPR render quality), API user can also tune the `RPR_CONTEXT_PREVIEW` value.
const int			g_batchSize = 4; //15


// thread rendering 'g_batchSize' iteration(s)
void renderJob( rpr_context ctxt, GuiRenderImpl::Update* update )
{
	CHECK( rprContextRender( ctxt ) );
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
	//std::thread t(&renderJob, context, &g_update);
	// wait the end of the rendering thread
	t.join();

	// wait the rendering thread
	{
		// at each update of the rendering thread
		if( g_update.m_hasUpdate )
		{
			// Read the frame buffer from RPR
			// Note that rprContextResolveFrameBuffer and rprFrameBufferGetInfo(fb,RPR_FRAMEBUFFER_DATA)  can be called asynchronous while rprContextRender is running.

			CHECK( rprContextResolveFrameBuffer( g_context, g_frame_buffer, g_frame_buffer_2, false ) );
			//CHECK(rprContextResolveFrameBuffer(context, g_frame_buffer, g_frame_buffer_2, false));
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
		std::cout << "delaX: " << delaX << " \n";
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

	if (button == GLUT_RIGHT_BUTTON)
	{
		//-------------------------************************--------------------------------------
		//std::cout << "Input number of shininess" << std::endl;
		//std::cin >> g_input_shininess;
		//std::cout << "input_shininess  " << g_input_shininess << std::endl;
		//std::cout << "Input number of roughness" << std::endl;
		//std::cin >> g_input_roughness;
		//std::cout << "input_roughness  " << g_input_roughness << std::endl;

		g_input_shininess = 0.0f;
		g_input_roughness = 10.0f;
		std::cout << "input_shininess  " << g_input_shininess << std::endl;
		std::cout << "input_roughness  " << g_input_roughness << std::endl;


		//std::string filepath = "sphere.json";
		//202310
		std::string filepath = "..\\87_gl_sphere\\sphere_original_camera.json";
		//std::vector<AtomData> atoms = extractAtomData();
		AtomData atoms = extractAtomData();

		//Load config 
		const Configuration config = loadConfigFile(filepath.c_str());

		for (const auto& setting : config.shapeSettings)
		{
			//loadAndAttachShapes(g_context, g_scene, g_matsys, setting, atoms);
			//202310
			loadAndAttachShapes(g_context, g_scene, g_matsys, setting, atoms);
		}

//		CHECK(rprContextSetParameterByKeyPtr(g_context, RPR_CONTEXT_RENDER_UPDATE_CALLBACK_FUNC, (void*)GuiRenderImpl::notifyUpdate));
//		CHECK(rprContextSetParameterByKeyPtr(g_context, RPR_CONTEXT_RENDER_UPDATE_CALLBACK_DATA, &g_update));
//		CHECK(rprContextSetParameterByKey1u(g_context, RPR_CONTEXT_ITERATIONS, 1));

//		CHECK(rprContextSetParameterByKey1u(g_context, RPR_CONTEXT_ITERATIONS, g_max_iterations));
//		CHECK(rprContextRender(g_context));

		CHECK(rprContextSetParameterByKey1u(g_context, RPR_CONTEXT_ITERATIONS, g_batchSize));
		//-------------------------************************--------------------------------------
		CHECK(rprFrameBufferClear(g_frame_buffer));

	}

	return;
}

int movieFlag = 0;

void OnKeyboardEvent(unsigned char key, int xmouse, int ymouse)
{
	bool cameraMoves = false;
	//202311
	bool cameraMoves2 = false;
	//for shiftkey
	bool shiftPressed = (GetKeyState(VK_SHIFT) & 0x8000) != 0;

	switch (key)
	{
	case 'w':
	case 's':
	case 'a':
	case 'd':
	case 'r':
	case 'f':
	//202311 rotation
	case 'z':
	case 'c':
	case 'q':
	case 'e':
	case 't':
	case 'g':

	{
		rprCameraGetInfo(g_camera, RPR_CAMERA_LOOKAT, sizeof(g_mouse_camera.lookat), &g_mouse_camera.lookat, 0);
		rprCameraGetInfo(g_camera, RPR_CAMERA_UP, sizeof(g_mouse_camera.up), &g_mouse_camera.up, 0);
		rprCameraGetInfo(g_camera, RPR_CAMERA_TRANSFORM, sizeof(g_mouse_camera.mat), &g_mouse_camera.mat, 0);
		rprCameraGetInfo(g_camera, RPR_CAMERA_POSITION, sizeof(g_mouse_camera.pos), &g_mouse_camera.pos, 0);
		RadeonProRender::float3 lookAtVec = {0,0,-1};// g_mouse_camera.lookat - g_mouse_camera.pos;
		lookAtVec.normalize();
		RadeonProRender::float3 vecRight = {1,0,0};// RadeonProRender::cross(g_mouse_camera.up, lookAtVec);
		vecRight.normalize();
		RadeonProRender::float3 vecUp = g_mouse_camera.up;
		vecUp.normalize();
		cameraMoves = true;
		const float speed = 0.5f;

		//202311
		RadeonProRender::float3 Del = 
			{ g_mouse_camera.pos.x - g_mouse_camera.lookat.x,
			  g_mouse_camera.pos.y - g_mouse_camera.lookat.y, 
			  g_mouse_camera.pos.z - g_mouse_camera.lookat.z };
		Del.normalize();
		RadeonProRender::float3 DelLeft = RadeonProRender::cross(g_mouse_camera.up, Del);
		DelLeft.normalize();
		RadeonProRender::float3 DelUp = RadeonProRender::cross(Del, DelLeft);
		DelUp.normalize();

		//202311 for cameraMoves2
		std::cout << "g_mouse_camera.lookat: " << g_mouse_camera.lookat << " \n";
		std::cout << "g_mouse_camera.up: " << g_mouse_camera.up << " \n";
		std::cout << "g_mouse_camera.mat: " << sizeof(g_mouse_camera.mat) << " \n";
		std::cout << "g_mouse_camera.pos: " << g_mouse_camera.pos << " \n";
		std::cout << "g_mouse_camera (pos-lookat): " << (g_mouse_camera.pos - g_mouse_camera.lookat) << " \n";
		const float ang = 9.0 * (3.14159265/180.0);
		//RadeonProRender::matrix newMat;

		switch (key)
		{
			case 'w':
				//case 'z': // for azerty keyboard
			{
				//g_mouse_camera.pos += lookAtVec * speed;
				g_mouse_camera.pos += Del * speed;
				break;
			}
			case 's':
			{
				//g_mouse_camera.pos -= lookAtVec * speed;
				g_mouse_camera.pos -= Del * speed;
				break;
			}
			case 'a':
				//case 'q': // for azerty keyboard
			{
				//g_mouse_camera.pos += vecRight * speed;
				g_mouse_camera.pos += DelLeft * speed;
				break;
			}
			case 'd':
			{
				//g_mouse_camera.pos -= vecRight * speed;
				g_mouse_camera.pos -= DelLeft * speed;
				break;
			}
			case 'r':
			{
				//g_mouse_camera.pos += vecUp * speed;
				g_mouse_camera.pos += DelUp * speed;
				break;
			}
			case 'f':
			{
				//g_mouse_camera.pos -= vecUp * speed;
				g_mouse_camera.pos -= DelUp * speed;
				break;
			}
			//202311
			case 'z':
			{
				RadeonProRender::matrix rotZ = RadeonProRender::rotation(g_mouse_camera.up, ang);
				RadeonProRender::float3 lookAtVec = g_mouse_camera.lookat - g_mouse_camera.pos;
				lookAtVec.normalize();
				RadeonProRender::float3 left = RadeonProRender::cross(g_mouse_camera.up, lookAtVec);
				left.normalize();
				RadeonProRender::matrix rotleft = RadeonProRender::rotation(left, 0);
				g_mouse_camera.mat *= rotleft * rotZ;
				cameraMoves2 = true;
				break;
			}
			case 'c':
			{
				RadeonProRender::matrix rotZ = RadeonProRender::rotation(g_mouse_camera.up, -ang);
				RadeonProRender::float3 lookAtVec = g_mouse_camera.lookat - g_mouse_camera.pos;
				lookAtVec.normalize();
				RadeonProRender::float3 left = RadeonProRender::cross(g_mouse_camera.up, lookAtVec);
				left.normalize();
				RadeonProRender::matrix rotleft = RadeonProRender::rotation(left, -0);
				g_mouse_camera.mat *= rotleft * rotZ;
				cameraMoves2 = true;
				break;
			}
			case 'q':
			{
				RadeonProRender::matrix rotZ = RadeonProRender::rotation(g_mouse_camera.up, 0);
				RadeonProRender::float3 lookAtVec = g_mouse_camera.lookat - g_mouse_camera.pos;
				lookAtVec.normalize();
				RadeonProRender::float3 left = RadeonProRender::cross(g_mouse_camera.up, lookAtVec);
				left.normalize();
				RadeonProRender::matrix rotleft = RadeonProRender::rotation(left, ang);
				g_mouse_camera.mat *= rotleft * rotZ;
				cameraMoves2 = true;
				break;
			}
			case 'e':
			{
				RadeonProRender::matrix rotZ = RadeonProRender::rotation(g_mouse_camera.up, -0);
				RadeonProRender::float3 lookAtVec = g_mouse_camera.lookat - g_mouse_camera.pos;
				lookAtVec.normalize();
				RadeonProRender::float3 left = RadeonProRender::cross(g_mouse_camera.up, lookAtVec);
				left.normalize();
				RadeonProRender::matrix rotleft = RadeonProRender::rotation(left, -ang);
				g_mouse_camera.mat *= rotleft * rotZ;
				cameraMoves2 = true;
				break;
			}
			case 't':
			{
				movieFlag = 1; //Run movie
				cameraMoves  = FALSE;
				cameraMoves2 = FALSE;
				break;
			}
			case 'g':
			{
				movieFlag = 0; //Stop movie
				cameraMoves = FALSE;
				cameraMoves2 = FALSE;
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

	//if (cameraMoves)
	if (cameraMoves == TRUE && cameraMoves2 == FALSE)
	{
		g_mouse_camera.mat.m30 = g_mouse_camera.pos.x;
		g_mouse_camera.mat.m31 = g_mouse_camera.pos.y;
		g_mouse_camera.mat.m32 = g_mouse_camera.pos.z;
		rprCameraSetTransform(g_camera, false, &g_mouse_camera.mat.m00);
		// camera moved, so we need to redraw the framebuffer.
		CHECK(rprFrameBufferClear(g_frame_buffer));
	}
	if (cameraMoves == TRUE && cameraMoves2 == TRUE)
	{
		rprCameraSetTransform(g_camera, false, &g_mouse_camera.mat.m00);
		CHECK(rprFrameBufferClear(g_frame_buffer));
	}
}

//202311 time and move camera
int absTime = 0;

double getCurrentTimeInSeconds() {
	static const auto start_time = std::chrono::steady_clock::now();
	auto current_time = std::chrono::steady_clock::now();
	auto duration = current_time - start_time;
	return std::chrono::duration<double>(duration).count();
}

void updateAtoms(rpr_context& context, rpr_scene& scene) {

	double CurrentTime = getCurrentTimeInSeconds();

	if ((int)(CurrentTime / 0.75) > (int)absTime) {
		std::cout << "CurrentTime : " << CurrentTime << " absTime : " << absTime << std::endl;

		rprCameraGetInfo(g_camera, RPR_CAMERA_LOOKAT, sizeof(g_mouse_camera.lookat), &g_mouse_camera.lookat, 0);
		rprCameraGetInfo(g_camera, RPR_CAMERA_UP, sizeof(g_mouse_camera.up), &g_mouse_camera.up, 0);
		rprCameraGetInfo(g_camera, RPR_CAMERA_TRANSFORM, sizeof(g_mouse_camera.mat), &g_mouse_camera.mat, 0);
		rprCameraGetInfo(g_camera, RPR_CAMERA_POSITION, sizeof(g_mouse_camera.pos), &g_mouse_camera.pos, 0);

		//202311
		RadeonProRender::float3 Del =
		{ g_mouse_camera.pos.x - g_mouse_camera.lookat.x,
		  g_mouse_camera.pos.y - g_mouse_camera.lookat.y,
		  g_mouse_camera.pos.z - g_mouse_camera.lookat.z };
		Del.normalize();
		RadeonProRender::float3 DelLeft = RadeonProRender::cross(g_mouse_camera.up, Del);
		DelLeft.normalize();
		RadeonProRender::float3 DelUp = RadeonProRender::cross(Del, DelLeft);
		DelUp.normalize();

		const float ang = 9.0 * (3.14159265 / 180.0);

		RadeonProRender::matrix rotZ = RadeonProRender::rotation(g_mouse_camera.up, ang);
		//RadeonProRender::matrix rotZ = RadeonProRender::rotation(g_mouse_camera.up, 0);
		RadeonProRender::float3 lookAtVec = g_mouse_camera.lookat - g_mouse_camera.pos;
		lookAtVec.normalize();
		RadeonProRender::float3 left = RadeonProRender::cross(g_mouse_camera.up, lookAtVec);
		left.normalize();
		//RadeonProRender::matrix rotleft = RadeonProRender::rotation(left, ang);
		RadeonProRender::matrix rotleft = RadeonProRender::rotation(left, 0);
		g_mouse_camera.mat *= rotleft * rotZ;

		rprCameraSetTransform(g_camera, false, &g_mouse_camera.mat.m00);
		CHECK(rprFrameBufferClear(g_frame_buffer));
	}

	absTime = (double)CurrentTime / (int)1;
}


void Display()
{
	//202311
	//updateAtoms(g_context, g_scene);
	if (movieFlag == 1) {
		updateAtoms(g_context, g_scene);
	}

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
	//CheckNoLeak(context);
	//CHECK(rprObjectDelete(context)); context = nullptr; // Always delete the RPR Context in last.
}


int main(int argc, char** argv)
{
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

	//----------------********************-----------------------
	//202311
	AtomData atoms = extractAtomData();
	std::cout << "obtained atoms.*ave: " << atoms.xave << " " << atoms.yave << " " << atoms.zave << " \n";
	//202309 set json files
	//#ifdef ENABLE_TRACING
	//  	rprContextSetParameterByKey1u(0, RPR_CONTEXT_TRACING_ENABLED, 1);
	//#endif

	//202309 Load config 
	//std::string filepath = "sphere.json";
	std::string filepath = "..\\87_gl_sphere\\sphere_original_camera.json";
	//const Configuration config = loadConfigFile(filepath.c_str());
	Configuration config = loadConfigFile(filepath.c_str());
	//---------------*********************-----------------------

	rpr_int status = RPR_SUCCESS;

	//Create global context holding rpr_context, rpr_scene and rpr_framebuffer
	Context contextSettings = createContext(config.contextSettings);

	rpr_context context = std::move(contextSettings.context);
	rpr_scene scene = std::move(contextSettings.scene);
	rpr_framebuffer frameBuffer = std::move(contextSettings.fb);

	std::cout << "\nContext Successfully create!.\n";

	//Create and attach camerato scene
	//createAndAttachCamera(context, scene, config.cameraSettings);
	//-------------------------************************--------------------------------------
	//202310
	std::cout << "CameraPosi: " << config.cameraSettings.position.x
		<< " " << config.cameraSettings.position.y << " " << config.cameraSettings.position.z << " \n";
	std::cout << "AimedPosi: " << config.cameraSettings.aimed.x
		<< " " << config.cameraSettings.aimed.y << " " << config.cameraSettings.aimed.z << " \n";
	std::cout << "Up: " << config.cameraSettings.up.x
		<< " " << config.cameraSettings.up.y << " " << config.cameraSettings.up.z << " \n";
	std::cout << "focal_length: " << config.cameraSettings.focalLength << " \n";
	config.cameraSettings.position.x = 0;
	config.cameraSettings.position.y = 0;
	config.cameraSettings.position.z = 50;
	config.cameraSettings.aimed.x = 0;
	config.cameraSettings.aimed.y = 0;
	config.cameraSettings.aimed.z = 0;
	config.cameraSettings.up.x = 0; //atoms.xave;
	config.cameraSettings.up.y = 1; //atoms.yave;
	config.cameraSettings.up.z = 0; //atoms.zave;
	config.cameraSettings.focalLength = 50;

	std::cout << "CameraPosi: " << config.cameraSettings.position.x
		<< " " << config.cameraSettings.position.y << " " << config.cameraSettings.position.z << " \n";
	std::cout << "AimedPosi: " << config.cameraSettings.aimed.x
		<< " " << config.cameraSettings.aimed.y << " " << config.cameraSettings.aimed.z << " \n";
	std::cout << "Up: " << config.cameraSettings.up.x
		<< " " << config.cameraSettings.up.y << " " << config.cameraSettings.up.z << " \n";
	std::cout << "focal_length: " << config.cameraSettings.focalLength << " \n";

	createAndAttachCamera2(context, scene, config.cameraSettings);
	//-------------------------************************--------------------------------------

	rpr_material_system matsys = nullptr;
	CHECK(rprContextCreateMaterialSystem(context, 0, &matsys));

	//-------------------------************************--------------------------------------
	//202309 setting inputs
	std::cout << "Input number of shininess" << std::endl;
//	std::cin >> g_input_shininess;
	g_input_shininess = 1000;
	std::cout << "input_shininess  " << g_input_shininess << std::endl;
	std::cout << "Input number of roughness" << std::endl;
//	std::cin >> g_input_roughness;
	std::cout << "input_roughness  " << g_input_roughness << std::endl;
	//-------------------------************************--------------------------------------

	// Load models from config file and attach them to scene
	for (const auto& setting : config.shapeSettings)
	{
		//loadAndAttachShapes(context, scene, matsys, setting);
		//202310
		loadAndAttachShapes(context, scene, matsys, setting, atoms);
	}

	// Create and attach light to scene
	for (const auto& setting : config.lightSettings)
	{
		createAndAttachLight(context, scene, setting);
	}

	// Create and attach environment light
	for (const auto& setting : config.envLightSettings)
	{
		createAndAttachEnvironmentLight(context, scene, setting);
	}

	rpr_post_effect postEffectGamma = 0;
	rpr_post_effect normalizationEff = 0;
	{
		//RPR_CONTEXT_DISPLAY_GAMMA
		CHECK(rprContextSetParameterByKey1f(context, RPR_CONTEXT_DISPLAY_GAMMA, 2.2f));
	}

	// Progressively render an image
	CHECK(rprContextSetParameterByKey1u(context, RPR_CONTEXT_ITERATIONS, contextSettings.max_iterations));
	CHECK(rprContextRender(context));

	//----------------******************---------------------------
	g_context = context;
	g_frame_buffer = contextSettings.fb;
	g_frame_buffer_2 = contextSettings.fb1;
	g_scene = contextSettings.scene;
	g_matsys = matsys;
	g_max_iterations = contextSettings.max_iterations;
	//----------------******************---------------------------

	std::cout << "\nRendering finished.\n";

	//------------------------------------------------------------------------------------
	//In the mesh_obj, "Create Context" function include following without 'Create Camera'

	//// Register the plugin.
	//rpr_int tahoePluginID = rprRegisterPlugin(RPR_PLUGIN_FILE_NAME); 
	//CHECK_NE(tahoePluginID , -1)
	//rpr_int plugins[] = { tahoePluginID };
	//size_t pluginCount = sizeof(plugins) / sizeof(plugins[0]);

	//// Create context using a single GPU 
	//status = rprCreateContext(RPR_API_VERSION, plugins, pluginCount, g_ContextCreationFlags 
	//	#ifndef USING_NORTHSTAR   // note that for Northstar, we don't need the GL_INTEROP flag
	//	| RPR_CREATION_FLAGS_ENABLE_GL_INTEROP
	//	#endif
	//	, g_contextProperties, NULL, &g_context);
	//CHECK(status);

	//// Set active plugin.
	//CHECK( rprContextSetActivePlugin(g_context, plugins[0]) );

	//CHECK( rprContextCreateMaterialSystem(g_context, 0, &g_matsys) );

	//std::cout << "Context successfully created.\n";


    //-------------------------------------------
	// This part is included in "createContext" function
	// Create a scene
	// CHECK( rprContextCreateScene(g_context, &g_scene) );

	//-------------*********************-------------------
	// We need to insert "createContext" here without "Buffer part".
	//-------------*********************-------------------

	//-----------------------------------------------------------------------------------
	// This is not needed in "Mesh obj"
	// 
	// Create an environment light
	//CHECK( CreateNatureEnvLight(g_context, g_scene, g_gc, 0.9f) );
	//CHECK(CreateNatureEnvLightIN(g_context, g_scene, g_gc, 0.9f, "../../Resources/Textures/amd.png")); //"../../Resources/Textures/turning_area_4k.hdr"

	//-----------------------------------------------------------------------------------
	// Camera context is controled in createAttachCamera

	//// Create camera
	//{
	//	CHECK( rprContextCreateCamera(g_context, &g_camera) );

	//	// Position camera in world space: 
	//	CHECK( rprCameraLookAt(g_camera, 0.0f, 5.0f, 20.0f,    0, 1, 0,   0, 1, 0) );

	//	// set camera field of view
	//	CHECK( rprCameraSetFocalLength(g_camera, 30.0f) );

	//	// Set camera for the scene
	//	CHECK( rprSceneSetCamera(g_scene, g_camera) );
	//}

	//-------------*********************-------------------
	// We need to insert "createAttachCamera" here.
	//-------------*********************-------------------

	//-------------*********************-------------------
	// We need to insert other lighting function here.
	// 	// Load models from config file and attach them to scene
	//for (const auto& setting : config.shapeSettings)
	//{
	//	loadAndAttachShapes(context, scene, matsys, setting);
	//}

	//// Create and attach light to scene
	//for (const auto& setting : config.lightSettings)
	//{
	//	createAndAttachLight(context, scene, setting);
	//}

	//// Create and attach environment light
	//for (const auto& setting : config.envLightSettings)
	//{
	//	createAndAttachEnvironmentLight(context, scene, setting);
	//}
	//-------------*********************-------------------


	//// Set scene to render for the context
	//CHECK( rprContextSetScene(g_context, g_scene) );

	//20230920
	//Create material
	//{
	// 	//CHECK(CreateAMDFloor(g_context, g_scene, g_matsys, g_gc, 0.20f, 0.20f, 0.0f, -1.0f, 0.0f));
	//	//char pathImageFileA = "../../Resources/Textures/art.jpg";
	//	CHECK(CreateAMDFloorIN(g_context, g_scene, g_matsys, g_gc, 0.20f, 0.20f, 0.0f, -2.0f, 0.0f, "../../Resources/Textures/amd.png"));

	//	rpr_mesh_info mesh_properties[16];
	//	mesh_properties[0] = (rpr_mesh_info)RPR_MESH_VOLUME_FLAG;
	//	mesh_properties[1] = (rpr_mesh_info)1; // enable the Volume flag for the Mesh
	//	mesh_properties[2] = (rpr_mesh_info)0;

	//	// Volume shapes don't need any vertices data: the bounds of volume will only be defined by the grid.
	//	// Also, make sure to enable the RPR_MESH_VOLUME_FLAG
	//	rpr_shape cube = 0;
	//	CHECK(rprContextCreateMeshEx2(g_context,
	//		nullptr, 0, 0,
	//		nullptr, 0, 0,
	//		nullptr, 0, 0, 0,
	//		nullptr, nullptr, nullptr, nullptr, 0,
	//		nullptr, 0, nullptr, nullptr, nullptr, 0,
	//		mesh_properties,
	//		&cube));

	//	// bounds of volume will always be a box defined by the rprShapeSetTransform
	//	RadeonProRender::matrix cubeTransform1 = RadeonProRender::translation(RadeonProRender::float3(0, +0.0f, 0)) * RadeonProRender::rotation_y(0.0f) * RadeonProRender::scale(RadeonProRender::float3(1.0f, 1.0f, 1.0f));
	//	CHECK(rprShapeSetTransform(cube, true, &cubeTransform1.m00));
	//	CHECK(rprSceneAttachShape(g_scene, cube));
	//	// 
	//	// define the grids data used by the Volume material.
	//	//const size_t n = 128; //128;
	//	std::vector<unsigned int> indicesList;
	//	std::vector<float> gridVector1;
	//	std::vector<float> gridVector2;


	//		for (unsigned int x = 0; x < n; x++)
	//		{
	//			for (unsigned int y = 0; y < n; y++)
	//			{
	//				for (unsigned int z = 0; z < n; z++)
	//				{
	//					for (unsigned int i = 0; i < atomnum; i++)
	//					{
	//					const int j = i * 3;
	//					float radius = sqrtf(((float)x - (float)atomposi[j]) * ((float)x - (float)atomposi[j])
	//								    	+ ((float)y - (float)atomposi[j+1]) * ((float)y - (float)atomposi[j+1])
	//								    	+ ((float)z - (float)atomposi[j+2]) * ((float)z - (float)atomposi[j+2]));
	//					if (radius < atomrad)
	//					{
	//						//std::cout << radius << std::endl;
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
	//					//else 
	//					//{
	//					//	gridVector1.push_back(0.0f);
	//					//	gridVector2.push_back(0.0f);// (float)j / (float)atomnum);
	//					//}
	//				}
	//			}
	//		}
	//	}

	//	// this first grid defines a cylinder
	//	rpr_grid rprgrid1 = 0;
	//	CHECK(rprContextCreateGrid(g_context, &rprgrid1,
	//		n, n, n,
	//		&indicesList[0], indicesList.size() / 3, RPR_GRID_INDICES_TOPOLOGY_XYZ_U32,
	//		&gridVector1[0], gridVector1.size() * sizeof(gridVector1[0]), 0
	//	));

	//	// GRID_SAMPLER could be compared to a 3d-texture sampler. 
	//	// input is a 3d grid,  output is the sampled value from grid
	//	rpr_material_node gridSampler1 = NULL;
	//	CHECK(rprMaterialSystemCreateNode(g_matsys, RPR_MATERIAL_NODE_GRID_SAMPLER, &gridSampler1));
	//	CHECK(rprMaterialNodeSetInputGridDataByKey(gridSampler1, RPR_MATERIAL_INPUT_DATA, rprgrid1));

	//	// This second grid is a gradient along the Y axis.
	//	rpr_grid rprgrid2 = 0;
	//	CHECK(rprContextCreateGrid(g_context, &rprgrid2,
	//		n, n, n,
	//		&indicesList[0], indicesList.size() / 3, RPR_GRID_INDICES_TOPOLOGY_XYZ_U32,
	//		&gridVector2[0], gridVector2.size() * sizeof(gridVector2[0]), 0
	//	));

	//	// create grid sample for grid2
	//	rpr_material_node gridSampler2 = NULL;
	//	CHECK(rprMaterialSystemCreateNode(g_matsys, RPR_MATERIAL_NODE_GRID_SAMPLER, &gridSampler2));
	//	CHECK(rprMaterialNodeSetInputGridDataByKey(gridSampler2, RPR_MATERIAL_INPUT_DATA, rprgrid2));

	//	// create a gradient color texture, here 3 pixels : Red, Green, Blue.
	//	// will be used as a lookup output 
	//	float rampData2[] = {
	//		1.f,0.f,0.f,
	//		0.f,1.f,0.f,
	//		0.f,0.f,1.f 
	//	};
	//	rpr_image rampimg2 = 0;
	//	rpr_image_desc rampDesc2;
	//	rampDesc2.image_width = sizeof(rampData2) / (3 * sizeof(float));
	//	rampDesc2.image_height = 1;
	//	rampDesc2.image_depth = 0;
	//	rampDesc2.image_row_pitch = rampDesc2.image_width * sizeof(rpr_float) * 3;
	//	rampDesc2.image_slice_pitch = 0;
	//	CHECK(rprContextCreateImage(g_context, { 3, RPR_COMPONENT_TYPE_FLOAT32 }, & rampDesc2, rampData2, & rampimg2));

	//	// this texture will be used for the color of the volume material.
	//	// UV input is the 0->1 gradient created by the scalar grid "rprgrid2".
	//	// Output is the red,green,blue texture.
	//	// This demonstrates how we can create a lookup table from scalar grid to vector values.
	//	rpr_material_node rampSampler2 = NULL;
	//	CHECK(rprMaterialSystemCreateNode(g_matsys, RPR_MATERIAL_NODE_IMAGE_TEXTURE, &rampSampler2));
	//	CHECK(rprMaterialNodeSetInputImageDataByKey(rampSampler2, RPR_MATERIAL_INPUT_DATA, rampimg2));
	//	CHECK(rprMaterialNodeSetInputNByKey(rampSampler2, RPR_MATERIAL_INPUT_UV, gridSampler2));

	//	// for ramp texture, it's better to clamp it to edges.
	//	CHECK(rprMaterialNodeSetInputUByKey(rampSampler2, RPR_MATERIAL_INPUT_WRAP_U, RPR_IMAGE_WRAP_TYPE_CLAMP_TO_EDGE));
	//	CHECK(rprMaterialNodeSetInputUByKey(rampSampler2, RPR_MATERIAL_INPUT_WRAP_V, RPR_IMAGE_WRAP_TYPE_CLAMP_TO_EDGE));

	//	// create the Volume material
	//	rpr_material_node materialVolume = NULL;
	//	CHECK(rprMaterialSystemCreateNode(g_matsys, RPR_MATERIAL_NODE_VOLUME, &materialVolume));

	//	// density is defined by the "cylinder" grid
	//	CHECK(rprMaterialNodeSetInputNByKey(materialVolume, RPR_MATERIAL_INPUT_DENSITYGRID, gridSampler1));

	//	// apply the volume material to the shape.
	//	// Note that here we use   rprShapeSetVolumeMaterial  instead of the classic  rprShapeSetMaterial  call.
	//	CHECK(rprShapeSetVolumeMaterial(cube, materialVolume));

	//	// RPR_MATERIAL_INPUT_DENSITY is just a multiplier for DENSITYGRID
	//	CHECK(rprMaterialNodeSetInputFByKey(materialVolume, RPR_MATERIAL_INPUT_DENSITY, 100.0f, 0.0f, 0.0f, 0.0f));

	//	// define the color of the volume
	//	CHECK(rprMaterialNodeSetInputNByKey(materialVolume, RPR_MATERIAL_INPUT_COLOR, rampSampler2));

	//	// more iterations will increase the light penetration inside the volume.
	//	CHECK(rprContextSetParameterByKey1u(g_context, RPR_CONTEXT_MAX_RECURSION, (rpr_uint)2)); // 5

	//	// when using volumes, we usually need high number of iterations.
	//	CHECK(rprContextSetParameterByKey1u(g_context, RPR_CONTEXT_ITERATIONS, 3000));

	//	// set rendering gamma
	//	CHECK(rprContextSetParameterByKey1f(g_context, RPR_CONTEXT_DISPLAY_GAMMA, 2.2f));


	//}

	//// Create framebuffer to store rendering result
	//rpr_framebuffer_desc desc = { WINDOW_WIDTH,WINDOW_HEIGHT };

	// 4 component 32-bit float value each
	//rpr_framebuffer_format fmt = { 4, RPR_COMPONENT_TYPE_FLOAT32 };
	//CHECK(rprContextCreateFrameBuffer(g_context, fmt, &desc, &g_frame_buffer));
	//CHECK(rprContextCreateFrameBuffer(g_context, fmt, &desc, &g_frame_buffer_2));

	// Set framebuffer for the context
	//CHECK(rprContextSetAOV(g_context, RPR_AOV_COLOR, g_frame_buffer));

	// This line can be added for faster RPR rendering.
	// The higher RPR_CONTEXT_PREVIEW, the faster RPR rendering, ( but more pixelated ).
	// 0 = disabled (defaut value)
	//CHECK( rprContextSetParameterByKey1u(g_context,RPR_CONTEXT_PREVIEW, 1u ) );

	// Set framebuffer for the context
	///CHECK(rprContextSetAOV(g_context, RPR_AOV_COLOR, g_frame_buffer));


	//----------------------------------------------------
	// We need following buffer part
	// 
	// Define the update callback.
	// During the rprContextRender execution, RPR will call it regularly
	// The 'CALLBACK_DATA' : 'g_update' is not used by RPR. it can be any data structure that the API user wants.
	CHECK(rprContextSetParameterByKeyPtr(g_context, RPR_CONTEXT_RENDER_UPDATE_CALLBACK_FUNC, (void*)GuiRenderImpl::notifyUpdate));
	CHECK(rprContextSetParameterByKeyPtr(g_context, RPR_CONTEXT_RENDER_UPDATE_CALLBACK_DATA, &g_update));


	// do a first rendering iteration, just for to force scene/cache building.
	std::cout << "Cache and scene building... ";
	CHECK(rprContextSetParameterByKey1u(g_context,RPR_CONTEXT_ITERATIONS,1));
	CHECK(rprContextRender( g_context ) );
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


