#define STRINGIFY(x) #x

#ifndef _FUR_SHADERS_H_
#define _FUR_SHADERS_H_

static const char* fur_vertex_shader = STRINGIFY(
    precision highp float;
    attribute vec4 vertexPosition;
    attribute vec4 vertexNormal;
    attribute vec2 vertexTexCoord;

    varying vec2 texCoord;
    varying vec4 normal;
    varying float fragLength;

    uniform mat4 modelViewProjectionMatrix;
    uniform float furLength;
    uniform float layer;
    uniform vec4 force;

    void main()
    {
       vec4 modPos = vec4(vertexPosition);
       modPos[0] = modPos[0] + (vertexNormal[0] * furLength);
       modPos[1] = modPos[1] + (vertexNormal[1] * furLength);
       modPos[2] = modPos[2] + (vertexNormal[2] * furLength);

       vec4 worldForce = force;
       float k = layer * layer * layer;
       modPos[0] = modPos[0] + (worldForce[0] * k);
       modPos[1] = modPos[1] + (worldForce[1] * k);
       modPos[2] = modPos[2] + (worldForce[2] * k);

       gl_Position = modelViewProjectionMatrix * modPos;
       normal = vertexNormal;
       texCoord = vertexTexCoord;
       fragLength = layer;
    }
);

static const char* fur_fragment_shader = STRINGIFY(
    precision highp float;

    varying vec2 texCoord;
    varying vec4 normal;
    varying float fragLength;

    uniform sampler2D texSampler2D;

    void main()
    {
       vec4 texColor = texture2D(texSampler2D, texCoord);
       float scale = 1.0 - fragLength;
       texColor[0] = texColor[0] * fragLength;
       texColor[1] = texColor[1] * fragLength;
       texColor[2] = texColor[2] * fragLength;
       texColor[3] = texColor[3] * scale;

       gl_FragColor = texColor;
    }
);

#endif