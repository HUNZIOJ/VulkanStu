#version 450

layout(location = 0) out vec3 fragColor;

layout(location = 0) in vec2 pos;
layout(location = 1) in vec3 color;


void main()     
{
    gl_Position = vec4(pos, 0.0, 1.0); // 这里是因为传进来的就是一个ndc坐标系的坐标,也就是裁剪空间的坐标,所以不需要进行mvp变换
    fragColor = color;
}