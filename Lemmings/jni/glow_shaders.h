#define STRINGIFY(x) #x

#ifndef _GLOW_SHADERS_H_
#define _GLOW_SHADERS_H_

static const char* glow_vertex_shader = STRINGIFY(
    precision highp float;
    attribute vec4 vertexPosition;
    attribute vec4 vertexNormal;
    attribute vec2 vertexTexCoord;

    varying vec2 texCoord;
    varying vec4 normal;
    varying float fragLayer;

    uniform mat4 modelViewProjectionMatrix;
    uniform float layer;
    uniform float radius;

    void main()
    {
       vec4 modPos = vec4(vertexPosition);
       modPos[0] = modPos[0] + (vertexNormal[0] * radius);
       modPos[1] = modPos[1] + (vertexNormal[1] * radius);
       modPos[2] = modPos[2] + (vertexNormal[2] * radius);

       gl_Position = modelViewProjectionMatrix * modPos;
       normal = vertexNormal;
       texCoord = vertexTexCoord;
       fragLayer = layer;
    }
);

static const char* glow_fragment_shader = STRINGIFY(
    precision highp float;

    varying vec2 texCoord;
    varying vec4 normal;
    varying float fragLayer;

    uniform sampler2D texSampler2D;

    void main()
    {
      vec4 texColor = texture2D(texSampler2D, texCoord);
      float scale = 1.0 - fragLayer;
      if(scale < 0.0) {
        scale = 0.0;
      }
      texColor[3] = scale;

      gl_FragColor = texColor;
    }
);

#endif