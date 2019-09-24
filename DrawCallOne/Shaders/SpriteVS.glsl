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
	
	// �����׸�Ʈ ���̴��� ����� �ؽ�ó ������ x�� �����մϴ�.
	TexCoord.x += _TextureAttribute.z;

	// �ؽ�ó ���� ũ�⸦ 4�� ����� �����մϴ�.
	TextureWidth = ceil(_TextureAttribute.x / 4.0f) * 4.0f;
}