#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord2D;
layout (location = 2) in vec3 aTexCoord3D;

uniform mat4 mvp;

out vec2 texCoord2D;
out vec3 texCoord3D;

void main()
{
    gl_Position = mvp * vec4(aPos, 1.0f);
    //gl_PointSize = 0.5f;
    texCoord2D = aTexCoord2D;
    texCoord3D = aTexCoord3D;
}