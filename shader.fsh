varying vec2 textureOut_y;
varying vec2 textureOut_u;
varying vec2 textureOut_v;
uniform sampler2D tex_y;
uniform sampler2D tex_u;
uniform sampler2D tex_v;

void main(void)
{
    vec3 yuv;
    yuv.x = texture2D(tex_y, textureOut_y).x;
    yuv.y = texture2D(tex_u, textureOut_u).x;
    yuv.z = texture2D(tex_v, textureOut_v).x;

    /*
     * YCbCr to RGB
     * R = 1.164*(Y-16)+1.596*(Cr-128)
     * G = 1.164*(Y-16)-0.392*(Cb-128)-0.813*(Cr-128)
     * B = 1.164*(Y-16)+2.017*(Cb-128)
     */
    vec3 rgb = mat3(1.164, 1.164, 1.164,
               0, -0.392, 2.017,
               1.596, -0.813, 0) * vec3(yuv.x-0.0625, yuv.y-0.5, yuv.z-0.5);

    gl_FragColor = vec4(rgb, 1);
}
