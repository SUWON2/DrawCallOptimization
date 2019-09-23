/*
	제작자: 이수원, 이메일: entryswlee@gmail.com

	이 소스 코드는 OpenGLES 3.1 버전을 사용하여 DrawCall을 1로 만드는 방법을 소개합니다.
	순수 기술 설명을 위해 C-Style로 코드를 작성했으며 Desktop에서 구동시키 위해 GLFW 라이브러를 사용합니다.

	만약 모바일 포팅을 원하는 경우 NDK를 이용하셔야 됩니다.
	모바일로 포팅할 때는 안드로이드 스튜디오를 이용하는 방법도 있지만 구글을 통해 비주얼 스튜디오로도 포팅할 수 있는 방법을 찾을 수 있습니다.

	사용한 기술은 Instancing Rendering, Texture Array, Astc Compression이며 이 세 기술을 다 사용하기 위해서는 최소 OpenGLES 3.1 버전 이상을 필요로 합니다.
	즉 옛날 스마트폰에서는 호환이 안 될 수 있기 때문에 자세한 내용은 검색해 보세요
	만약 DirectX 사용자라면 이 코드 로직만 이해해도 DirectX 11 이상 버전에서는 이 기술을 적용할 수 있을 겁니다.

	알고리즘 설명은 ~에서 합니다.
	혹시 궁금한 게 있으면 위에 기재된 이메일로 문의주세요!!
*/

#include <iostream>
#include <string>
#include <cassert>
#include <random>
#include <list>
#include <unordered_map>
#include <chrono>

#include <GLFW/glfw3.h>
#include <GLES3/gl31.h>
#include <GLES2/gl2ext.h>
#include <GLES3/gl3platform.h>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

// Namespaces
using namespace std;
using namespace glm;

// Structures
struct TextureInfo
{
	std::string imagePath;
	float x;
	float y;
};

struct AstcFile
{
	size_t size;
	FILE* data;
};

struct AstcHeader
{
	unsigned char magic[4];
	unsigned char blockdim_x;
	unsigned char blockdim_y;
	unsigned char blockdim_z;
	unsigned char xsize[3];
	unsigned char ysize[3];
	unsigned char zsize[3];
};

// Constant Variables
static constexpr int SCREEN_WIDTH = 1280;
static constexpr int SCREEN_HEIGHT = 720;
static constexpr int TEXTURE_COUNT = 30000;

static const mat4 PROJECTION_VIEW = 
	ortho(0.0f, static_cast<float>(SCREEN_WIDTH), 0.0f, static_cast<float>(SCREEN_HEIGHT), 1.0f, -100.0f)
	* translate(mat4(1.0f), vec3(0.0f, 0.0f, 0.0f));

// Global Variables
static GLuint ShaderProgram = 0;
static GLuint VAO = 0;
static GLuint VBO = 0;
static GLuint EBO = 0;
static GLuint ProjectionViewWorldVBO = 0;
static GLuint TextureAttributeVBO = 0;
static GLuint TextureArray = 0;

static TextureInfo TextureInfos[TEXTURE_COUNT];
static unordered_map<string, uvec3> TextureAttributes;
static unique_ptr<vec4[]> TextureAttributeBuffer = nullptr;
static unique_ptr<mat4[]> ProjectionViewWorldBuffer = nullptr;

// Global Functions
static void ShowGlfwError(int error, const char* description);
static void Initialize();
static void Update();
static void Shutdown();

static void InitializeTextureAtlas(const TextureInfo* textureInfos);
static void LoadTexture(const char* fileName, uint32_t* textureOffsetX, size_t* allAstcDataSize, std::list<AstcFile>* astcFiles);
static void CompileShader(GLuint* shader, const GLenum type, const char* shaderFilePath);

// Defines
#ifdef _DEBUG
	#define GL_CALL(x) \
			(x); \
			{ \
				GLenum errorLog; \
				if ((errorLog = glGetError()) != GL_NO_ERROR) \
				{ \
					printf("OpenGL Error: 0x%X\n", errorLog); \
					__debugbreak(); \
				} \
			}
#else
	#define GL_CALL(x) (x);
#endif

int main()
{
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
	//_CrtSetBreakAlloc(16415);

	glfwSetErrorCallback(ShowGlfwError);

	if (glfwInit() == false)
	{
		assert(false && "Failed to initialize glfw");
	}

	// 오픈지엘ES 버전을 3.1로 지정한다.
	glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
	glfwWindowHint(GLFW_CONTEXT_CREATION_API, GLFW_EGL_CONTEXT_API);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);

	GLFWwindow* window = glfwCreateWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "DrawCallOne", nullptr, nullptr);
	assert(window != nullptr && "Failed to create window");

	glfwMakeContextCurrent(window);
	glfwSwapInterval(0);

	Initialize();

	while (glfwWindowShouldClose(window) == false)
	{
		std::chrono::system_clock::time_point beforeTime = std::chrono::system_clock::now();

		Update();

		glfwPollEvents();
		glfwSwapBuffers(window);

		std::chrono::system_clock::time_point nowTime = std::chrono::system_clock::now();
		const int interval = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(nowTime - beforeTime).count());

		cout << interval << endl;
	}

	Shutdown();

	glfwTerminate();

	return 0;
}

void ShowGlfwError(int error, const char* description)
{
	fputs(description, stderr);
}

void Initialize()
{
	GL_CALL(glViewport(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT));

	// 셰이더를 초기화한다.
	{
		GLuint vertexShader = 0;
		CompileShader(&vertexShader, GL_VERTEX_SHADER, "Shaders/SpriteVS.glsl");

		GLuint fragmentShader = 0;
		CompileShader(&fragmentShader, GL_FRAGMENT_SHADER, "Shaders/SpriteFS.glsl");

		ShaderProgram = GL_CALL(glCreateProgram());
		GL_CALL(glAttachShader(ShaderProgram, vertexShader));
		GL_CALL(glAttachShader(ShaderProgram, fragmentShader));
		GL_CALL(glLinkProgram(ShaderProgram));

		// 셰이더를 하나만 사용한다는 전제하에 프로그램 시작할 때만 지정한다.
		GL_CALL(glUseProgram(ShaderProgram));

		GL_CALL(glDeleteShader(vertexShader));
		GL_CALL(glDeleteShader(fragmentShader));
	}

	// 정점 형태를 초기화한다.
	{
		constexpr float vertices[] =
		{
			1.0f, 1.0f,
			1.0f, 0.0f,
			0.0f, 0.0f,
			0.0f, 1.0f
		};

		constexpr uint32_t indices[] =
		{
			0, 1, 3,
			1, 2, 3
		};

		GL_CALL(glGenVertexArrays(1, &VAO));
		GL_CALL(glBindVertexArray(VAO));

		GL_CALL(glGenBuffers(1, &VBO));
		GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, VBO));
		GL_CALL(glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW));

		// 정점 데이터가 무엇인지 셰이더에게 알려준다.
		GL_CALL(glEnableVertexAttribArray(0));
		GL_CALL(glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr));

		GL_CALL(glGenBuffers(1, &EBO));
		GL_CALL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO));
		GL_CALL(glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW));
	}

	// 텍스처를 초기화한다.
	{
		default_random_engine randomEngine;
		uniform_int_distribution<int> uidHorizontalRange(0, SCREEN_WIDTH);
		uniform_int_distribution<int> uidVerticalRange(0, SCREEN_HEIGHT);
		
		for (int i = 0; i < TEXTURE_COUNT; ++i)
		{
			TextureInfos[i] =
			{
				"Resources/stand.0.astc"
				, static_cast<float>(uidHorizontalRange(randomEngine))
				, static_cast<float>(uidVerticalRange(randomEngine))
			};
		}

		InitializeTextureAtlas(TextureInfos);
	}
}

void Update()
{
	for (int i = 0; i < TEXTURE_COUNT; ++i)
	{
		const TextureInfo& textureInfo = TextureInfos[i];
		const vec2 spritePosition = { textureInfo.x, textureInfo.y };
		const uvec3 textureAttribute = TextureAttributes[textureInfo.imagePath];

		constexpr float depth = 0.0f;
		ProjectionViewWorldBuffer[i] = translate(PROJECTION_VIEW, vec3(spritePosition, depth));
		ProjectionViewWorldBuffer[i] = scale(ProjectionViewWorldBuffer[i], { uvec2(textureAttribute), 0.0f });

		constexpr float alpha = 1.0f;
		TextureAttributeBuffer[i] = { textureAttribute, alpha };
	}

	// 비디오 메모리에 mProjectionViewWorlds의 데이터를 적용합니다.
	{
		GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, ProjectionViewWorldVBO));

		void* dataPtr = GL_CALL(glMapBufferOES(GL_ARRAY_BUFFER, GL_WRITE_ONLY));
		memcpy(dataPtr, ProjectionViewWorldBuffer.get(), sizeof(mat4) * TEXTURE_COUNT);

		GL_CALL(glUnmapBufferOES(GL_ARRAY_BUFFER));
	}

	// 비디오 메모리에 mTextureAttributes의 데이터를 적용합니다.
	{
		GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, TextureAttributeVBO));

		void* dataPtr = GL_CALL(glMapBufferOES(GL_ARRAY_BUFFER, GL_WRITE_ONLY));
		memcpy(dataPtr, TextureAttributeBuffer.get(), sizeof(vec4) * TEXTURE_COUNT);

		GL_CALL(glUnmapBufferOES(GL_ARRAY_BUFFER));
	}

	GL_CALL(glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0, TEXTURE_COUNT));
}

void Shutdown()
{
	GL_CALL(glDeleteBuffers(1, &EBO));
	GL_CALL(glDeleteBuffers(1, &VBO));
	GL_CALL(glDeleteBuffers(1, &VAO));
	GL_CALL(glDeleteProgram(ShaderProgram));
}

void InitializeTextureAtlas(const TextureInfo* textureInfos)
{
	// projectionViewWorld가 무엇인지 셰이더에게 알려준다.
	{
		GL_CALL(glGenBuffers(1, &ProjectionViewWorldVBO));
		GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, ProjectionViewWorldVBO));

		for (int i = 1; i <= 4; i++)
		{
			GL_CALL(glEnableVertexAttribArray(i));
			GL_CALL(glVertexAttribPointer(i, 4, GL_FLOAT, GL_FALSE, sizeof(mat4), reinterpret_cast<void*>(sizeof(vec4) * (i - 1))));
			GL_CALL(glVertexAttribDivisor(i, 1));
		}

		GL_CALL(glBufferData(GL_ARRAY_BUFFER, sizeof(mat4) * TEXTURE_COUNT, nullptr, GL_DYNAMIC_DRAW));
	}

	// TextureAttributeVBO가 무엇인지 셰이더에게 알려준다.
	{
		GL_CALL(glGenBuffers(1, &TextureAttributeVBO));
		GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, TextureAttributeVBO));

		GL_CALL(glEnableVertexAttribArray(5));
		GL_CALL(glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, sizeof(vec4), nullptr));
		GL_CALL(glVertexAttribDivisor(5, 1));

		GL_CALL(glBufferData(GL_ARRAY_BUFFER, sizeof(vec4) * TEXTURE_COUNT, nullptr, GL_DYNAMIC_DRAW));
	}

	// 텍스처 아틀라스를 만든다.
	{
		size_t allAstcDataSize = 0;
		uint32_t currentTextureArrayOffsetX = 0;
		std::list<AstcFile> astcFiles;

		for (int i = 0; i < TEXTURE_COUNT; ++i)
		{
			LoadTexture(textureInfos[i].imagePath.c_str(), &currentTextureArrayOffsetX, &allAstcDataSize, &astcFiles);
		}

		constexpr GLsizei textureArrayWidth = 512;
		constexpr GLsizei textureArrayHeight = 512;
		constexpr GLsizei textureArrayArea = textureArrayWidth * textureArrayHeight;
		const GLsizei textureArrayDepth = static_cast<GLsizei>(ceilf(allAstcDataSize / static_cast<float>(textureArrayArea)));

		GL_CALL(glGenTextures(1, &TextureArray));
		GL_CALL(glBindTexture(GL_TEXTURE_2D_ARRAY, TextureArray));

		GL_CALL(glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_COMPRESSED_RGBA_ASTC_4x4_KHR
			, textureArrayWidth, textureArrayHeight, textureArrayDepth));

		GL_CALL(glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT));
		GL_CALL(glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT));
		GL_CALL(glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
		GL_CALL(glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST));

		const GLint uTexSamplerArrayID = GL_CALL(glGetUniformLocation(ShaderProgram, "uTexArraySampler"));
		GL_CALL(glUniform1i(uTexSamplerArrayID, 0));

		size_t dataIndex = 0;
		auto imageDatas = std::make_unique<uint8_t[]>(textureArrayArea * textureArrayDepth);

		// Read astc file to imageDatas
		for (const auto& astcFile : astcFiles)
		{
			fseek(astcFile.data, sizeof(AstcHeader), SEEK_SET);
			fread(imageDatas.get() + dataIndex, astcFile.size, 1, astcFile.data);
			dataIndex += astcFile.size;

			fclose(astcFile.data);
		}

		// Register image data to texture array
		for (int i = 0; i < textureArrayDepth; ++i)
		{
			GL_CALL(glCompressedTexSubImage3D(
				GL_TEXTURE_2D_ARRAY
				, 0
				, 0
				, 0
				, i
				, textureArrayWidth
				, textureArrayHeight
				, 1
				, GL_COMPRESSED_RGBA_ASTC_4x4_KHR
				, textureArrayArea
				, reinterpret_cast<void*>(imageDatas.get() + i * textureArrayArea)
			));
		}

		ProjectionViewWorldBuffer = std::make_unique<mat4[]>(TEXTURE_COUNT);
		TextureAttributeBuffer = std::make_unique<vec4[]>(TEXTURE_COUNT);
	}
}

void LoadTexture(const char* fileName, uint32_t* textureOffsetX, size_t* allAstcDataSize, std::list<AstcFile>* astcFiles)
{
	const auto& findedTextureAttribute = TextureAttributes.find(fileName);

	// Exclude texture that have already been added.
	if (findedTextureAttribute != TextureAttributes.end())
	{
		return;
	}

	FILE* astcData = fopen(fileName, "rb");
	assert(astcData != nullptr && "Could not open a astc file");

	uint32_t imageWidth = 0;
	uint32_t imageHeight = 0;
	size_t astcDataSize = 0;

	// astc 자료: https://arm-software.github.io/opengl-es-sdk-for-android/astc_textures.html
	{
		AstcHeader astcHeader;
		fread(&astcHeader, sizeof(AstcHeader), 1, astcData);

		imageWidth = astcHeader.xsize[0] + (astcHeader.xsize[1] << 8) + (astcHeader.xsize[2] << 16);
		imageHeight = astcHeader.ysize[0] + (astcHeader.ysize[1] << 8) + (astcHeader.ysize[2] << 16);

		// Compute number of blocks in each direction.
		const int xBlocks = (imageWidth + astcHeader.blockdim_x - 1) / astcHeader.blockdim_x;
		const int yBlocks = (imageHeight + astcHeader.blockdim_y - 1) / astcHeader.blockdim_y;

		astcDataSize = xBlocks * yBlocks << 4;
	}

	// Store texture attribute that shader use
	TextureAttributes.insert(std::make_pair(fileName, uvec3{ imageWidth, imageHeight, *textureOffsetX }));

	// 세로를 4픽셀을 기준하여 한 라인으로 나열했을 때 각 텍스처의 텍스처 어레이 오프셋 값이 정해진다.
	imageWidth = static_cast<int>(ceilf(imageWidth / 4.0f)) * 4;
	imageHeight = static_cast<int>(ceilf(imageHeight / 4.0f));
	*textureOffsetX += imageWidth * imageHeight;

	astcFiles->push_back({ astcDataSize, astcData });

	*allAstcDataSize += astcDataSize;
}

void CompileShader(GLuint* shader, const GLenum type, const char* shaderFilePath)
{
	assert(shader != nullptr && "the shader must not be null");

	FILE* shaderFile = fopen(shaderFilePath, "r");
	assert(shaderFile != nullptr && "Failed to load shader");

	// 셰이더 파일 사이즈 크기를 가져온다.
	fseek(shaderFile, 0, SEEK_END);
	const uint32_t shaderFileSize = static_cast<uint32_t>(ftell(shaderFile));
	rewind(shaderFile);

	auto shaderSource = std::make_unique<char[]>(shaderFileSize);
	fread(shaderSource.get(), shaderFileSize, 1, shaderFile);

	// 유니크 포인터로 넘기는 것은 불가능하기 때문에 따로 변수를 만들어 준다.
	const char* data = shaderSource.get();

	fclose(shaderFile);

	*shader = GL_CALL(glCreateShader(type));
	GL_CALL(glShaderSource(*shader, 1, &data, nullptr));
	GL_CALL(glCompileShader(*shader));

#ifdef _DEBUG // 디버그 모드일 때 셰이더 오류를 검사한다.
	GLint bSuccess = 0;
	GL_CALL(glGetShaderiv(*shader, GL_COMPILE_STATUS, &bSuccess));

	if (bSuccess == 0)
	{
		GLint logLength = 0;
		GL_CALL(glGetShaderiv(*shader, GL_INFO_LOG_LENGTH, &logLength));

		if (logLength > 0)
		{
			auto log = std::make_unique<char[]>(logLength);
			GL_CALL(glGetShaderInfoLog(*shader, logLength, nullptr, log.get()));

			printf(log.get());
			__debugbreak();
		}
	}
#endif
}
