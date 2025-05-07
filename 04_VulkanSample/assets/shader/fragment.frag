#version 450

// 指定输出的texture的索引咧
layout(location = 0) out vec4 outColor;

layout(location = 0) in vec3 fragColor; 
// 这里指定location索引的作用就是显示的关联两个变量，所以现在可以不用保持名字相同

void main() {
    outColor = vec4(fragColor, 1.0);
}