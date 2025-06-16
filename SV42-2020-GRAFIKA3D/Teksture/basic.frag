// basic.frag
#version 330 core
out vec4 FragColor;
uniform vec3 uColorOverride;
void main() {
    FragColor = vec4(uColorOverride, 1.0);
}
