attribute vec4 vertexIn;
attribute vec2 textureIn0;
attribute vec2 textureIn1;
attribute vec2 textureIn2;
varying vec2 textureOut0;
varying vec2 textureOut1;
varying vec2 textureOut2;

void main(void)
{
    gl_Position = vertexIn;
    textureOut0 = textureIn0;
    textureOut1 = textureIn1;
    textureOut2 = textureIn2;
}
