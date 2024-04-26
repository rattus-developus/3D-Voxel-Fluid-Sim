#version 460 core

in vec3 localPos;
out vec4 FragColor;

void main()
{
   FragColor = vec4(localPos, 1.0);
}