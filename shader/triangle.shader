#version 450
#extension GL_ARB_separate_shader_objects : enable

#ifdef MU_VERTEX_SHADER
layout(set = 0, binding = 0) uniform UniformBufferObject
{
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;
//layout(location = 3) in vec2 instancePos;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;


void main()
{
    //gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition + instancePos, 0.0, 1.0);
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0);
    fragColor = inColor; 
    fragTexCoord = inTexCoord;
}
#endif

#ifdef MU_FRAGMENT_SHADER
layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 1) uniform sampler2D texSampler;
void main()
{
    outColor = texture(texSampler, fragTexCoord);
}
#endif