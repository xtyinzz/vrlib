#version 330 core
out vec4 FragColor;

in vec2 texCoord2D;
in vec3 texCoord3D;

uniform sampler3D volume;
uniform sampler1D TF;

void main()
{
    // trying to classify color based on intensity, 
    // not working because alpha channel controls the brilliance,
    // and the fragment still blocks the pixels behind it.
    // need DVR compositing to really see thru the volume

    // vec4 color;
    // float intensity = texture(volume, texCoord3D).x;
    // if (intensity < 0.005) {
    //     color = vec4(1.0f, 0.0f, 0.0f,  1.0f);
    // } else if (intensity < 0.95) {
    //     color = vec4(1.0f, 1.0f, 1.0f,  0.9f);
    // } else if (intensity < 0.9715) {
    //     color = vec4(0.0f, 0.0f, 1.0f,  0.1f);
    // } else {
    //     color = vec4(0.0f, 1.0f, 0.0f,  0.0f);
    // }
    // FragColor = color;

    // FragColor = texture(TF, intensity);

    FragColor = texture(volume, texCoord3D);

}