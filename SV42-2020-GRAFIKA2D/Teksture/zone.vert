#version 330 core

layout(location = 0) in vec2 aPos;

uniform vec2 transform;
out vec4 fragColor;

void main()
{

    gl_Position = vec4(aPos + transform, 0.0, 1);
    gl_PointSize = 5;
    fragColor = fragColor; 
}
