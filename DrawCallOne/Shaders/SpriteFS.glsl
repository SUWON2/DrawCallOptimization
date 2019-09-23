#version 310 es

precision mediump float;
precision mediump sampler2DArray;

uniform sampler2DArray uTexArraySampler;

in vec2 TexCoord;
in flat float TextureWidth;
in flat float Alpha;

out vec4 _Color;

void main()
{
	vec3 atlasOffset = vec3(TexCoord, 0.0f);

	float overTexCoordY = floor(TexCoord.y / 4.0f);
	atlasOffset.x += overTexCoordY * TextureWidth;
	atlasOffset.y -= overTexCoordY * 4.0f;

	float overOffsetX = floor(atlasOffset.x / 512.0f);
	atlasOffset.x -= overOffsetX * 512.0f;
	atlasOffset.y += overOffsetX * 4.0f;

	float overOffsetY = floor(atlasOffset.y / 512.0f);
	atlasOffset.y -= overOffsetY * 512.0f;
	atlasOffset.z += overOffsetY;

	_Color = texture(uTexArraySampler, vec3(atlasOffset.xy / 512.0f, atlasOffset.z));
	_Color.a *= Alpha;

	// if texture atals mode, you must rede this code
	if (_Color.a < 0.05f)
	{
		discard;
	}

	// HACK: texture atlas mode
	//_Color = texture(uTexArraySampler, vec3(TexCoord, 0));
}