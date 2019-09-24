#version 310 es

layout (location = 0) in vec2 _PosOrTexCoord;
layout (location = 1) in mat4 _ProjectionViewWorld;
layout (location = 5) in vec3 _TextureAttribute; // width, height, offset x

out vec2 TexCoord;
out flat float TextureWidth;

void main()
{
	gl_Position = _ProjectionViewWorld * vec4(_PosOrTexCoord, 0.0f, 1.0f);

	TexCoord.xy = _PosOrTexCoord * _TextureAttribute.xy;
	
	// 프래그먼트 셰이더가 사용할 텍스처 오프셋 x를 저장합니다.
	TexCoord.x += _TextureAttribute.z;

	// 텍스처 가로 크기를 4의 배수로 조정합니다.
	TextureWidth = ceil(_TextureAttribute.x / 4.0f) * 4.0f;
}