#version 150 core

in vec2 vbo;
out vec2 UV;

void main(void) {

	gl_Position = vec4(2*(vbo - 0.5), 0.0, 1.0);
	UV = vec2(vbo.x, 1-vbo.y);

}
