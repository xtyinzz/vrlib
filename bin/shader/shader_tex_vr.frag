#version 330 core
out vec4 FragColor;

in vec2 texCoord2D;

uniform sampler2D vrTexture;

void main()
{
    FragColor = texture(vrTexture, texCoord2D);
}