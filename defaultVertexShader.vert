#version 460 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aOffset;

out vec3 localPos;

uniform mat4 modelToWorld;
uniform mat4 view;
uniform mat4 projection;

//Remember to multiply in reverse order
void main()
{
	localPos = aPos;
    gl_Position = projection * view * modelToWorld * (vec4(aPos + aOffset, 1.0));
}
