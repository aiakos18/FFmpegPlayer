attribute vec4 vertexIn;
attribute vec2 textureIn_y;
attribute vec2 textureIn_u;
attribute vec2 textureIn_v;
varying vec2 textureOut_y;
varying vec2 textureOut_u;
varying vec2 textureOut_v;

void main(void)
{
    gl_Position = vertexIn;
    textureOut_y = textureIn_y;
    textureOut_u = textureIn_u;
    textureOut_v = textureIn_v;
}
