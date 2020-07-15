/*
	이메일: entryswlee@gmail.com

	이 소스 코드는 OpenGLES 3.1 버전을 사용하여 DrawCall을 1로 만드는 방법을 소개합니다.
	보통 여러 텍스처를 한 번에 패스하기 위해 텍스처 아틀라스를 사용합니다. 그러나 이 방식은 몇 가지 단점이 존재합니다.
	예를 들어 2048x32 크기 이미지를 사용한다면 자른 후에 텍스처 아틀라스에 배치를 해야거나 아예 이미지 하나를 따로 처리해야 되는 경우가 생깁니다.
	또 임의의 이미지가 어느 한 텍스처 아틀라스에 배치되어 있다고 하면 그 이미지 하나만 사용하더라도
	그 텍스처 아틀라스에 있는 모든 이미지를 불러오기 때문에 성능에 영향을 미칩니다.
	이 샘플 코드는 이러한 단점들을 모두 해결합니다.

	순수 기술 설명을 위해 C-Style로 코드를 작성했으며 Desktop에서 구동시키 위해 GLFW와 Mali OpenGL ES Emulator를 사용합니다.
	만약 모바일 포팅을 원하는 경우 NDK를 이용하셔야 됩니다.
	모바일로 포팅할 때 안드로이드 스튜디오를 이용하는 방법도 있지만 구글을 통해 비주얼 스튜디오로도 포팅할 수 있는 방법을 찾을 수 있습니다.

	사용한 기술은 Instancing Rendering, Texture Array, Astc Compression 등 이며 이 세 기술을 다 사용하기 위해서는 최소 OpenGLES 3.1 버전 이상을 필요로 합니다.
	즉 옛날 스마트폰에서는 호환이 안 될 수 있기 때문에 유의하셔야 됩니다.
	만약 DirectX 사용자라면 동작 원리만 파악해도 DirectX 11 이상 버전에서는 이 기술을 무난하게 적용할 수 있을 겁니다.

	코드를 분석할 때는 main 함수부터 추적하는 방식을 추천합니다.
	마지막으로 궁금한 점이나 개선점이 있으면 이메일 주세요~
*/

#include <iostream>
#include <string>
#include <cassert>
#include <random>
#include <list>
#include <unordered_map>
#include <chrono>
#include <Windows.h>

#include <GLFW/glfw3.h>
#include <GLES3/gl31.h>
#include <GLES2/gl2ext.h>
#include <GLES3/gl3platform.h>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

/*** Namespaces ***/
using namespace std;
using namespace glm;

/*** Structures ***/
struct Sprite
{
	std::string ImagePath;
	float X;
	float Y;
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

/*** Constant Variables ***/
static constexpr int SCREEN_WIDTH = 1280;
static constexpr int SCREEN_HEIGHT = 720;

// 스프라이트 개수를 적게 설정했을 때 CPU 사용량과 GPU가 90%일 때까지 스프라이트 개수를 설정한 CPU 사용량의 차이를 확인해 보세요
// 혹은 프레임을 통한 방법과 성능 프로파일러를 이용한 방법도 있습니다.
// 여기서 주의할 점은 Release 모드로 설정한 뒤 테스트를 하셔야 됩니다.
static constexpr int SPRITE_COUNT = 1000;

static const mat4 PROJECTION_VIEW = 
	ortho(0.0f, static_cast<float>(SCREEN_WIDTH), 0.0f, static_cast<float>(SCREEN_HEIGHT), 1.0f, -(float)SPRITE_COUNT)
	* translate(mat4(1.0f), vec3(0.0f, 0.0f, 0.0f));

/*** Global Variables ***/
static GLuint ShaderProgram = 0;
static GLuint VAO = 0;
static GLuint VBO = 0;
static GLuint EBO = 0;
static GLuint ProjectionViewWorldVBO = 0;
static GLuint TextureAttributeVBO = 0;
static GLuint TextureArray = 0;

static Sprite Sprites[SPRITE_COUNT]; // 이미지 경로, 위치를 저장합니다.
static unordered_map<string, uvec3> TextureAttributes; // 중복을 제외한 텍스처 속성들을 저장합니다.
static unique_ptr<vec3[]> TextureAttributeBuffer = nullptr; // 셰이더에 사용될 텍스처 속성 버퍼입니다.
static unique_ptr<mat4[]> ProjectionViewWorldBuffer = nullptr; // 셰이더에 사용될 PVW 버퍼입니다.

/*** Global Functions ***/
static void ShowGlfwError(int error, const char* description);
static void Initialize();
static void Update();
static void Shutdown();

static void InitializeTextureAtlas();
static void LoadTexture(const char* fileName, uint32_t* textureOffsetX, size_t* allAstcDataSize, std::list<AstcFile>* astcFiles);
static void CompileShader(GLuint* shader, const GLenum type, const char* shaderFilePath);

/*** Defines ***/
#ifdef _DEBUG // gl 함수를 호출할 때 오류가 있는지 검사합니다. 디버그 모드일 때만 작동합니다.
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

	// OpenGLES 버전을 3.1로 지정합니다.
	glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
	glfwWindowHint(GLFW_CONTEXT_CREATION_API, GLFW_EGL_CONTEXT_API);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);

	GLFWwindow* window = glfwCreateWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "DrawCallOne", nullptr, nullptr);
	assert(window != nullptr && "Failed to create window");

	glfwMakeContextCurrent(window);
	
	// 오픈지엘 초기화, 텍스처 로드 등을 처리합니다.
	Initialize();

	int frameCount = 0;
	double interval = 0.0;
	std::chrono::system_clock::time_point startTime;
	std::chrono::system_clock::time_point endTime;

	while (glfwWindowShouldClose(window) == false)
	{
		startTime = std::chrono::system_clock::now();

		// 셰이더 업데이트, 렌더링 등을 처리합니다.
		{
			glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			Update();

			glfwPollEvents();
			glfwSwapBuffers(window);
		}

		endTime = std::chrono::system_clock::now();

		++frameCount;

		const long delayTime = 16 - (long)std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

		if (delayTime > 0)
		{
			Sleep(delayTime);
		}
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
	GL_CALL(glEnable(GL_DEPTH_TEST));

	// 셰이더를 초기화합니다.
	{
		GLuint vertexShader = 0;
		CompileShader(&vertexShader, GL_VERTEX_SHADER, "Shaders/SpriteVS.glsl");

		GLuint fragmentShader = 0;
		CompileShader(&fragmentShader, GL_FRAGMENT_SHADER, "Shaders/SpriteFS.glsl");

		ShaderProgram = GL_CALL(glCreateProgram());
		GL_CALL(glAttachShader(ShaderProgram, vertexShader));
		GL_CALL(glAttachShader(ShaderProgram, fragmentShader));
		GL_CALL(glLinkProgram(ShaderProgram));

		// 셰이더를 하나만 사용한다는 전제하에 프로그램 시작할 때만 지정합니다.
		GL_CALL(glUseProgram(ShaderProgram));

		GL_CALL(glDeleteShader(vertexShader));
		GL_CALL(glDeleteShader(fragmentShader));
	}

	// 정점 형태를 초기화합니다.
	{
		const float vertices[] =
		{
			1.0f, 1.0f,
			1.0f, 0.0f,
			0.0f, 0.0f,
			0.0f, 1.0f
		};

		const uint32_t indices[] =
		{
			0, 1, 3,
			1, 2, 3
		};

		GL_CALL(glGenVertexArrays(1, &VAO));
		GL_CALL(glBindVertexArray(VAO));

		GL_CALL(glGenBuffers(1, &VBO));
		GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, VBO));
		GL_CALL(glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW));

		// 정점 데이터가 무엇인지 셰이더에게 알려줍니다.
		GL_CALL(glEnableVertexAttribArray(0));
		GL_CALL(glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr));

		GL_CALL(glGenBuffers(1, &EBO));
		GL_CALL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO));
		GL_CALL(glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW));
	}

	// 스프라이트를 초기화합니다.
	{
		// 각 스프라이트의 위치를 랜덤으로 배치하기 위한 변수입니다.
		default_random_engine randomEngine;
		uniform_int_distribution<int> uidHorizontalRange(0, SCREEN_WIDTH);
		uniform_int_distribution<int> uidVerticalRange(0, SCREEN_HEIGHT);

		// 코드를 간단하게 처리하기 위해 ASTC 파일을 숫자로 지정했습니다.
		// 리소스 폴더에 존재하는 ASTC 파일의 이름을 랜덤으로 선택합니다.
		uniform_int_distribution<int> uidImageKindRange(0, 33);
		
		for (int i = 0; i < SPRITE_COUNT; ++i)
		{
			/*
				ASTC 파일이란 이미지를 압축하는 포맷의 한 종류로 비디오 메모리 사용량을 줄입니다.
				유니티 엔진 또한 이 압축 포맷을 지원하며 높은 OpenGLES 버전을 요구합니다.
				또 압축 시간이 매우 오래 걸리기 때문에 사전에 이미지를 ASTC 파일로 압축하고 사용해야 됩니다.
				압축할 때는 astc-encoder 혹은 Mali Texture Compression Tool을 사용하면 됩니다.
			*/

			Sprites[i] =
			{
				"Resources/" + to_string(uidImageKindRange(randomEngine)) + ".astc"
				, static_cast<float>(uidHorizontalRange(randomEngine))
				, static_cast<float>(uidVerticalRange(randomEngine))
			};
		}

		InitializeTextureAtlas();
	}
}

void Update()
{
	for (int i = 0; i < SPRITE_COUNT; ++i)
	{
		const Sprite& Sprite = Sprites[i];
		const vec3 spritePosition = { Sprite.X, Sprite.Y, i };
		const uvec3& textureAttribute = TextureAttributes[Sprite.ImagePath];

		ProjectionViewWorldBuffer[i] = translate(PROJECTION_VIEW, spritePosition);
		ProjectionViewWorldBuffer[i] = scale(ProjectionViewWorldBuffer[i], { uvec2(textureAttribute), 0.0f });

		TextureAttributeBuffer[i] = textureAttribute;
	}

	// 비디오 메모리에 ProjectionViewWorldBuffer 데이터를 적용합니다.
	{
		GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, ProjectionViewWorldVBO));

		void* dataPtr = GL_CALL(glMapBufferOES(GL_ARRAY_BUFFER, GL_WRITE_ONLY));
		memcpy(dataPtr, ProjectionViewWorldBuffer.get(), sizeof(mat4) * SPRITE_COUNT);

		GL_CALL(glUnmapBufferOES(GL_ARRAY_BUFFER));
	}

	// 비디오 메모리에 TextureAttributeBuffer 데이터를 적용합니다.
	{
		GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, TextureAttributeVBO));

		void* dataPtr = GL_CALL(glMapBufferOES(GL_ARRAY_BUFFER, GL_WRITE_ONLY));
		memcpy(dataPtr, TextureAttributeBuffer.get(), sizeof(vec3) * SPRITE_COUNT);

		GL_CALL(glUnmapBufferOES(GL_ARRAY_BUFFER));
	}

	GL_CALL(glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0, SPRITE_COUNT));
}

void Shutdown()
{
	GL_CALL(glDeleteBuffers(1, &EBO));
	GL_CALL(glDeleteBuffers(1, &VBO));
	GL_CALL(glDeleteBuffers(1, &VAO));
	GL_CALL(glDeleteProgram(ShaderProgram));
}

void InitializeTextureAtlas()
{
	// ProjectionViewWorldVBO가 무엇인지 셰이더에게 알려줍니다.
	{
		GL_CALL(glGenBuffers(1, &ProjectionViewWorldVBO));
		GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, ProjectionViewWorldVBO));

		for (int i = 1; i <= 4; i++)
		{
			GL_CALL(glEnableVertexAttribArray(i));
			GL_CALL(glVertexAttribPointer(i, 4, GL_FLOAT, GL_FALSE, sizeof(mat4), reinterpret_cast<void*>(sizeof(vec4) * (i - 1))));
			GL_CALL(glVertexAttribDivisor(i, 1));
		}

		GL_CALL(glBufferData(GL_ARRAY_BUFFER, sizeof(mat4) * SPRITE_COUNT, nullptr, GL_DYNAMIC_DRAW));
	}

	// TextureAttributeVBO가 무엇인지 셰이더에게 알려줍니다.
	{
		GL_CALL(glGenBuffers(1, &TextureAttributeVBO));
		GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, TextureAttributeVBO));

		GL_CALL(glEnableVertexAttribArray(5));
		GL_CALL(glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, sizeof(vec3), nullptr));
		GL_CALL(glVertexAttribDivisor(5, 1));

		GL_CALL(glBufferData(GL_ARRAY_BUFFER, sizeof(vec3) * SPRITE_COUNT, nullptr, GL_DYNAMIC_DRAW));
	}

	// 텍스처 아틀라스를 만듭니다.
	{
		/*
			이 코드 구간이 가장 중요합니다.
			우선 모든 ASTC 파일 데이터 모두를 저장하기 위한 큰 버퍼를 하나 할당하고 데이터를 저장시킵니다.
			그리고 512x512 크기를 가진 텍스처를 필요한 만큼 몇 개 만듭니다. 이 방식이 텍스처 어레입니다.
			마지막으로 버퍼를 텍스처 어레이에 저장합니다.

			위 방법이 가능한 이유는 임의의 이미지 하나를 준비해 주세요
			그리고 이미지를 하나 더 복사한 후에 윗 부분을 잘라 밑부분만 남겨주세요 이때 자른 윗 부분에 공백은 없어야 됩니다.
			그럼 원본 이미지와 밑부분만 남겨진 이미지가 준비되는데 이 두 개를 각각 ASTC 파일로 압축해 주세요
			마지막으로 두 압축 파일을 헥스 에디터같은 걸로 비교해 보면 아마 밑부분만 있는 ASTC 파일 내용이 원본 이미지 ASTC 파일 내용에 속해있을 겁니다.
			즉 제 방식은 512x512와 같이 큰 이미지에 모든 이미지를 순차적으로 넣는 개념입니다.
		*/

		size_t allAstcDataSize = 0;
		uint32_t currentTextureArrayOffsetX = 0;
		std::list<AstcFile> astcFiles;

		for (int i = 0; i < SPRITE_COUNT; ++i)
		{
			LoadTexture(Sprites[i].ImagePath.c_str(), &currentTextureArrayOffsetX, &allAstcDataSize, &astcFiles);
		}

		constexpr GLsizei textureArrayWidth = 512;
		constexpr GLsizei textureArrayHeight = 512;
		constexpr GLsizei textureArrayArea = textureArrayWidth * textureArrayHeight;
		const GLsizei textureArrayDepth = static_cast<GLsizei>(ceilf(allAstcDataSize / static_cast<float>(textureArrayArea)));

		// 모든 ASTC파일 데이터를 저장하기 위한 이미지 버퍼입니다.
		size_t dataIndex = 0;
		auto imageDatas = std::make_unique<uint8_t[]>(textureArrayArea * textureArrayDepth);

		// ASTC 파일을 읽어 이미지 버퍼에 저장합니다.
		for (const auto& astcFile : astcFiles)
		{
			fseek(astcFile.data, sizeof(AstcHeader), SEEK_SET);
			fread(imageDatas.get() + dataIndex, astcFile.size, 1, astcFile.data);
			dataIndex += astcFile.size;

			fclose(astcFile.data);
		}

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

		// 텍스처 어레이에 이미지 버퍼를 등록합니다.
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

		// 셰이더에 사용될 두 버퍼를 할당합니다.
		ProjectionViewWorldBuffer = std::make_unique<mat4[]>(SPRITE_COUNT);
		TextureAttributeBuffer = std::make_unique<vec3[]>(SPRITE_COUNT);
	}
}

void LoadTexture(const char* fileName, uint32_t* textureOffsetX, size_t* allAstcDataSize, std::list<AstcFile>* astcFiles)
{
	const auto& foundTextureAttribute = TextureAttributes.find(fileName);

	// 이미 등록된 텍스처는 제외하고 기존에 있는 걸 사용하는 방식으로 처리하여 메모리 낭비를 줄입니다.
	if (foundTextureAttribute != TextureAttributes.end())
	{
		return;
	}

	FILE* astcData = fopen(fileName, "rb");
	assert(astcData != nullptr && "Could not open a astc file");

	uint32_t imageWidth = 0;
	uint32_t imageHeight = 0;
	size_t astcDataSize = 0;

	// 이미지의 가로 세로 크기와 파일 크기를 가져옵니다.
	{
		/*
			ASTC에 대해 더 자세히 알고 싶으면 아래 링크를 참고하세요
			astc 참고: https://arm-software.github.io/opengl-es-sdk-for-android/astc_textures.html
			여기서 중요한 건 저는 블록 크기를 4x4로 지정했다는 것입니다.
			만약 다른 블록 크기를 원하신다면 main.cpp과 셰이더 코드를 수정하면 됩니다.
		*/

		AstcHeader astcHeader;
		fread(&astcHeader, sizeof(AstcHeader), 1, astcData);

		imageWidth = astcHeader.xsize[0] + (astcHeader.xsize[1] << 8) + (astcHeader.xsize[2] << 16);
		imageHeight = astcHeader.ysize[0] + (astcHeader.ysize[1] << 8) + (astcHeader.ysize[2] << 16);

		const int xBlocks = (imageWidth + astcHeader.blockdim_x - 1) / astcHeader.blockdim_x;
		const int yBlocks = (imageHeight + astcHeader.blockdim_y - 1) / astcHeader.blockdim_y;

		astcDataSize = xBlocks * yBlocks << 4;
	}

	// 셰이더에 사용될 텍스처 속성을 저장합니다.
	TextureAttributes.insert(std::make_pair(fileName, uvec3{ imageWidth, imageHeight, *textureOffsetX }));

	// 세로를 4픽셀을 기준하여 한 라인으로 나열했을 때 각 텍스처의 텍스처 어레이 오프셋 값이 정해집니다.
	imageWidth = static_cast<int>(ceilf(imageWidth / 4.0f)) * 4;
	imageHeight = static_cast<int>(ceilf(imageHeight / 4.0f));

	*textureOffsetX += imageWidth * imageHeight;
	*allAstcDataSize += astcDataSize;
	astcFiles->push_back({ astcDataSize, astcData });
}

void CompileShader(GLuint* shader, const GLenum type, const char* shaderFilePath)
{
	assert(shader != nullptr && "the shader must not be null");

	FILE* shaderFile = fopen(shaderFilePath, "r");
	assert(shaderFile != nullptr && "Failed to load shader");

	// 셰이더 파일 크기를 가져옵니다.
	fseek(shaderFile, 0, SEEK_END);
	const uint32_t shaderFileSize = static_cast<uint32_t>(ftell(shaderFile));
	rewind(shaderFile);

	auto shaderSource = std::make_unique<char[]>(shaderFileSize);
	fread(shaderSource.get(), shaderFileSize, 1, shaderFile);

	// 유니크 포인터로 넘기는 것은 불가능하기 때문에 따로 변수를 만들어 줍니다.
	const char* data = shaderSource.get();

	fclose(shaderFile);

	*shader = GL_CALL(glCreateShader(type));
	GL_CALL(glShaderSource(*shader, 1, &data, nullptr));
	GL_CALL(glCompileShader(*shader));

#ifdef _DEBUG // 디버그 모드일 때 셰이더 오류를 검사합니다.
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

			fprintf(stderr, "%s", log.get());
			__debugbreak();
		}
	}
#endif
}
