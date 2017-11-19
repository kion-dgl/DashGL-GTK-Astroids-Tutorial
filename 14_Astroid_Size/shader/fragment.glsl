#version 120

varying vec2 texcoord;
uniform sampler2D mytexture;

void main(void) {

    vec2 flipped_texcoord = vec2(texcoord.x, 1.0 - texcoord.y);
    vec4 frag = texture2D(mytexture, flipped_texcoord);
    
    if(frag.z == 0) {
        discard;
    }
    
    gl_FragColor = frag;

}
