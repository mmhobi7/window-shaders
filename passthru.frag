precision highp float;
varying vec2 v_texcoord; // is in 0-1
uniform sampler2D tex;

void main() {
    vec4 color = texture2D(tex, v_texcoord).rgba;
    color.r = 0.5;
    gl_FragColor = color;
}