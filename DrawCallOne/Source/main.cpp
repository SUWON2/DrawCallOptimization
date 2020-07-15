/*
	�̸���: entryswlee@gmail.com

	�� �ҽ� �ڵ�� OpenGLES 3.1 ������ ����Ͽ� DrawCall�� 1�� ����� ����� �Ұ��մϴ�.
	���� ���� �ؽ�ó�� �� ���� �н��ϱ� ���� �ؽ�ó ��Ʋ�󽺸� ����մϴ�. �׷��� �� ����� �� ���� ������ �����մϴ�.
	���� ��� 2048x32 ũ�� �̹����� ����Ѵٸ� �ڸ� �Ŀ� �ؽ�ó ��Ʋ�󽺿� ��ġ�� �ؾ߰ų� �ƿ� �̹��� �ϳ��� ���� ó���ؾ� �Ǵ� ��찡 ����ϴ�.
	�� ������ �̹����� ��� �� �ؽ�ó ��Ʋ�󽺿� ��ġ�Ǿ� �ִٰ� �ϸ� �� �̹��� �ϳ��� ����ϴ���
	�� �ؽ�ó ��Ʋ�󽺿� �ִ� ��� �̹����� �ҷ����� ������ ���ɿ� ������ ��Ĩ�ϴ�.
	�� ���� �ڵ�� �̷��� �������� ��� �ذ��մϴ�.

	���� ��� ������ ���� C-Style�� �ڵ带 �ۼ������� Desktop���� ������Ű ���� GLFW�� Mali OpenGL ES Emulator�� ����մϴ�.
	���� ����� ������ ���ϴ� ��� NDK�� �̿��ϼž� �˴ϴ�.
	����Ϸ� ������ �� �ȵ���̵� ��Ʃ����� �̿��ϴ� ����� ������ ������ ���� ���־� ��Ʃ����ε� ������ �� �ִ� ����� ã�� �� �ֽ��ϴ�.

	����� ����� Instancing Rendering, Texture Array, Astc Compression �� �̸� �� �� ����� �� ����ϱ� ���ؼ��� �ּ� OpenGLES 3.1 ���� �̻��� �ʿ�� �մϴ�.
	�� ���� ����Ʈ�������� ȣȯ�� �� �� �� �ֱ� ������ �����ϼž� �˴ϴ�.
	���� DirectX ����ڶ�� ���� ������ �ľ��ص� DirectX 11 �̻� ���������� �� ����� �����ϰ� ������ �� ���� �̴ϴ�.

	�ڵ带 �м��� ���� main �Լ����� �����ϴ� ����� ��õ�մϴ�.
	���������� �ñ��� ���̳� �������� ������ �̸��� �ּ���~
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

// ��������Ʈ ������ ���� �������� �� CPU ��뷮�� GPU�� 90%�� ������ ��������Ʈ ������ ������ CPU ��뷮�� ���̸� Ȯ���� ������
// Ȥ�� �������� ���� ����� ���� �������Ϸ��� �̿��� ����� �ֽ��ϴ�.
// ���⼭ ������ ���� Release ���� ������ �� �׽�Ʈ�� �ϼž� �˴ϴ�.
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

static Sprite Sprites[SPRITE_COUNT]; // �̹��� ���, ��ġ�� �����մϴ�.
static unordered_map<string, uvec3> TextureAttributes; // �ߺ��� ������ �ؽ�ó �Ӽ����� �����մϴ�.
static unique_ptr<vec3[]> TextureAttributeBuffer = nullptr; // ���̴��� ���� �ؽ�ó �Ӽ� �����Դϴ�.
static unique_ptr<mat4[]> ProjectionViewWorldBuffer = nullptr; // ���̴��� ���� PVW �����Դϴ�.

/*** Global Functions ***/
static void ShowGlfwError(int error, const char* description);
static void Initialize();
static void Update();
static void Shutdown();

static void InitializeTextureAtlas();
static void LoadTexture(const char* fileName, uint32_t* textureOffsetX, size_t* allAstcDataSize, std::list<AstcFile>* astcFiles);
static void CompileShader(GLuint* shader, const GLenum type, const char* shaderFilePath);

/*** Defines ***/
#ifdef _DEBUG // gl �Լ��� ȣ���� �� ������ �ִ��� �˻��մϴ�. ����� ����� ���� �۵��մϴ�.
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

	// OpenGLES ������ 3.1�� �����մϴ�.
	glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
	glfwWindowHint(GLFW_CONTEXT_CREATION_API, GLFW_EGL_CONTEXT_API);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);

	GLFWwindow* window = glfwCreateWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "DrawCallOne", nullptr, nullptr);
	assert(window != nullptr && "Failed to create window");

	glfwMakeContextCurrent(window);
	
	// �������� �ʱ�ȭ, �ؽ�ó �ε� ���� ó���մϴ�.
	Initialize();

	int frameCount = 0;
	double interval = 0.0;
	std::chrono::system_clock::time_point startTime;
	std::chrono::system_clock::time_point endTime;

	while (glfwWindowShouldClose(window) == false)
	{
		startTime = std::chrono::system_clock::now();

		// ���̴� ������Ʈ, ������ ���� ó���մϴ�.
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

	// ���̴��� �ʱ�ȭ�մϴ�.
	{
		GLuint vertexShader = 0;
		CompileShader(&vertexShader, GL_VERTEX_SHADER, "Shaders/SpriteVS.glsl");

		GLuint fragmentShader = 0;
		CompileShader(&fragmentShader, GL_FRAGMENT_SHADER, "Shaders/SpriteFS.glsl");

		ShaderProgram = GL_CALL(glCreateProgram());
		GL_CALL(glAttachShader(ShaderProgram, vertexShader));
		GL_CALL(glAttachShader(ShaderProgram, fragmentShader));
		GL_CALL(glLinkProgram(ShaderProgram));

		// ���̴��� �ϳ��� ����Ѵٴ� �����Ͽ� ���α׷� ������ ���� �����մϴ�.
		GL_CALL(glUseProgram(ShaderProgram));

		GL_CALL(glDeleteShader(vertexShader));
		GL_CALL(glDeleteShader(fragmentShader));
	}

	// ���� ���¸� �ʱ�ȭ�մϴ�.
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

		// ���� �����Ͱ� �������� ���̴����� �˷��ݴϴ�.
		GL_CALL(glEnableVertexAttribArray(0));
		GL_CALL(glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr));

		GL_CALL(glGenBuffers(1, &EBO));
		GL_CALL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO));
		GL_CALL(glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW));
	}

	// ��������Ʈ�� �ʱ�ȭ�մϴ�.
	{
		// �� ��������Ʈ�� ��ġ�� �������� ��ġ�ϱ� ���� �����Դϴ�.
		default_random_engine randomEngine;
		uniform_int_distribution<int> uidHorizontalRange(0, SCREEN_WIDTH);
		uniform_int_distribution<int> uidVerticalRange(0, SCREEN_HEIGHT);

		// �ڵ带 �����ϰ� ó���ϱ� ���� ASTC ������ ���ڷ� �����߽��ϴ�.
		// ���ҽ� ������ �����ϴ� ASTC ������ �̸��� �������� �����մϴ�.
		uniform_int_distribution<int> uidImageKindRange(0, 33);
		
		for (int i = 0; i < SPRITE_COUNT; ++i)
		{
			/*
				ASTC �����̶� �̹����� �����ϴ� ������ �� ������ ���� �޸� ��뷮�� ���Դϴ�.
				����Ƽ ���� ���� �� ���� ������ �����ϸ� ���� OpenGLES ������ �䱸�մϴ�.
				�� ���� �ð��� �ſ� ���� �ɸ��� ������ ������ �̹����� ASTC ���Ϸ� �����ϰ� ����ؾ� �˴ϴ�.
				������ ���� astc-encoder Ȥ�� Mali Texture Compression Tool�� ����ϸ� �˴ϴ�.
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

	// ���� �޸𸮿� ProjectionViewWorldBuffer �����͸� �����մϴ�.
	{
		GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, ProjectionViewWorldVBO));

		void* dataPtr = GL_CALL(glMapBufferOES(GL_ARRAY_BUFFER, GL_WRITE_ONLY));
		memcpy(dataPtr, ProjectionViewWorldBuffer.get(), sizeof(mat4) * SPRITE_COUNT);

		GL_CALL(glUnmapBufferOES(GL_ARRAY_BUFFER));
	}

	// ���� �޸𸮿� TextureAttributeBuffer �����͸� �����մϴ�.
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
	// ProjectionViewWorldVBO�� �������� ���̴����� �˷��ݴϴ�.
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

	// TextureAttributeVBO�� �������� ���̴����� �˷��ݴϴ�.
	{
		GL_CALL(glGenBuffers(1, &TextureAttributeVBO));
		GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, TextureAttributeVBO));

		GL_CALL(glEnableVertexAttribArray(5));
		GL_CALL(glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, sizeof(vec3), nullptr));
		GL_CALL(glVertexAttribDivisor(5, 1));

		GL_CALL(glBufferData(GL_ARRAY_BUFFER, sizeof(vec3) * SPRITE_COUNT, nullptr, GL_DYNAMIC_DRAW));
	}

	// �ؽ�ó ��Ʋ�󽺸� ����ϴ�.
	{
		/*
			�� �ڵ� ������ ���� �߿��մϴ�.
			�켱 ��� ASTC ���� ������ ��θ� �����ϱ� ���� ū ���۸� �ϳ� �Ҵ��ϰ� �����͸� �����ŵ�ϴ�.
			�׸��� 512x512 ũ�⸦ ���� �ؽ�ó�� �ʿ��� ��ŭ �� �� ����ϴ�. �� ����� �ؽ�ó ��Դϴ�.
			���������� ���۸� �ؽ�ó ��̿� �����մϴ�.

			�� ����� ������ ������ ������ �̹��� �ϳ��� �غ��� �ּ���
			�׸��� �̹����� �ϳ� �� ������ �Ŀ� �� �κ��� �߶� �غκи� �����ּ��� �̶� �ڸ� �� �κп� ������ ����� �˴ϴ�.
			�׷� ���� �̹����� �غκи� ������ �̹����� �غ�Ǵµ� �� �� ���� ���� ASTC ���Ϸ� ������ �ּ���
			���������� �� ���� ������ �� �����Ͱ��� �ɷ� ���� ���� �Ƹ� �غκи� �ִ� ASTC ���� ������ ���� �̹��� ASTC ���� ���뿡 �������� �̴ϴ�.
			�� �� ����� 512x512�� ���� ū �̹����� ��� �̹����� ���������� �ִ� �����Դϴ�.
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

		// ��� ASTC���� �����͸� �����ϱ� ���� �̹��� �����Դϴ�.
		size_t dataIndex = 0;
		auto imageDatas = std::make_unique<uint8_t[]>(textureArrayArea * textureArrayDepth);

		// ASTC ������ �о� �̹��� ���ۿ� �����մϴ�.
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

		// �ؽ�ó ��̿� �̹��� ���۸� ����մϴ�.
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

		// ���̴��� ���� �� ���۸� �Ҵ��մϴ�.
		ProjectionViewWorldBuffer = std::make_unique<mat4[]>(SPRITE_COUNT);
		TextureAttributeBuffer = std::make_unique<vec3[]>(SPRITE_COUNT);
	}
}

void LoadTexture(const char* fileName, uint32_t* textureOffsetX, size_t* allAstcDataSize, std::list<AstcFile>* astcFiles)
{
	const auto& foundTextureAttribute = TextureAttributes.find(fileName);

	// �̹� ��ϵ� �ؽ�ó�� �����ϰ� ������ �ִ� �� ����ϴ� ������� ó���Ͽ� �޸� ���� ���Դϴ�.
	if (foundTextureAttribute != TextureAttributes.end())
	{
		return;
	}

	FILE* astcData = fopen(fileName, "rb");
	assert(astcData != nullptr && "Could not open a astc file");

	uint32_t imageWidth = 0;
	uint32_t imageHeight = 0;
	size_t astcDataSize = 0;

	// �̹����� ���� ���� ũ��� ���� ũ�⸦ �����ɴϴ�.
	{
		/*
			ASTC�� ���� �� �ڼ��� �˰� ������ �Ʒ� ��ũ�� �����ϼ���
			astc ����: https://arm-software.github.io/opengl-es-sdk-for-android/astc_textures.html
			���⼭ �߿��� �� ���� ��� ũ�⸦ 4x4�� �����ߴٴ� ���Դϴ�.
			���� �ٸ� ��� ũ�⸦ ���ϽŴٸ� main.cpp�� ���̴� �ڵ带 �����ϸ� �˴ϴ�.
		*/

		AstcHeader astcHeader;
		fread(&astcHeader, sizeof(AstcHeader), 1, astcData);

		imageWidth = astcHeader.xsize[0] + (astcHeader.xsize[1] << 8) + (astcHeader.xsize[2] << 16);
		imageHeight = astcHeader.ysize[0] + (astcHeader.ysize[1] << 8) + (astcHeader.ysize[2] << 16);

		const int xBlocks = (imageWidth + astcHeader.blockdim_x - 1) / astcHeader.blockdim_x;
		const int yBlocks = (imageHeight + astcHeader.blockdim_y - 1) / astcHeader.blockdim_y;

		astcDataSize = xBlocks * yBlocks << 4;
	}

	// ���̴��� ���� �ؽ�ó �Ӽ��� �����մϴ�.
	TextureAttributes.insert(std::make_pair(fileName, uvec3{ imageWidth, imageHeight, *textureOffsetX }));

	// ���θ� 4�ȼ��� �����Ͽ� �� �������� �������� �� �� �ؽ�ó�� �ؽ�ó ��� ������ ���� �������ϴ�.
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

	// ���̴� ���� ũ�⸦ �����ɴϴ�.
	fseek(shaderFile, 0, SEEK_END);
	const uint32_t shaderFileSize = static_cast<uint32_t>(ftell(shaderFile));
	rewind(shaderFile);

	auto shaderSource = std::make_unique<char[]>(shaderFileSize);
	fread(shaderSource.get(), shaderFileSize, 1, shaderFile);

	// ����ũ �����ͷ� �ѱ�� ���� �Ұ����ϱ� ������ ���� ������ ����� �ݴϴ�.
	const char* data = shaderSource.get();

	fclose(shaderFile);

	*shader = GL_CALL(glCreateShader(type));
	GL_CALL(glShaderSource(*shader, 1, &data, nullptr));
	GL_CALL(glCompileShader(*shader));

#ifdef _DEBUG // ����� ����� �� ���̴� ������ �˻��մϴ�.
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
