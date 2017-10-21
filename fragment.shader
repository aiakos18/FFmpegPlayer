varying vec2 textureOut0;
varying vec2 textureOut1;
varying vec2 textureOut2;
uniform sampler2D tex0;
uniform sampler2D tex1;
uniform sampler2D tex2;

uniform int tex_type; // 0: ycbcr, 1: rgb

void main(void)
{
    if (tex_type == 0) {
        vec3 yuv;
        yuv.x = texture2D(tex0, textureOut0).x;
        yuv.y = texture2D(tex1, textureOut1).x;
        yuv.z = texture2D(tex2, textureOut2).x;
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
    else if (tex_type == 1) {
        gl_FragColor = vec4(texture2D(tex0, textureOut0).rgb, 1);
    }
    else {
        gl_FragColor = vec4(0, 0, 0, 1);
    }
}
