#version 330 core

in vec2 chTex; //koordinate teksture
out vec4 outCol;

uniform sampler2D uTex; //teksturna jedinica
uniform float overlayAlpha; // prozirnost zelene boje

void main()
{
    vec4 texColor = texture(uTex, chTex);
    vec4 greenOverlay = vec4(0.0, 1.0, 0.0, overlayAlpha); // Zelena boja s alpha
    outCol = mix(texColor, greenOverlay, overlayAlpha); // Pomiješaj boje
}