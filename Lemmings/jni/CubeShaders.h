/*==============================================================================
            Copyright (c) 2012 QUALCOMM Austria Research Center GmbH.
            All Rights Reserved.
            Qualcomm Confidential and Proprietary
            
@file 
    CubeShaders.h

@brief
    Defines OpenGL shaders as char* strings.

==============================================================================*/

#define STRINGIFY(x) #x

#ifndef _QCAR_CUBE_SHADERS_H_
#define _QCAR_CUBE_SHADERS_H_

#ifndef USE_OPENGL_ES_1_1

static const char* cubeMeshVertexShader = STRINGIFY(
    attribute vec4 vertexPosition;
    attribute vec4 vertexNormal;
    attribute vec2 vertexTexCoord;

    varying vec2 texCoord;
    varying vec4 normal;

    uniform mat4 modelViewProjectionMatrix;

    void main()
    {
       gl_Position = modelViewProjectionMatrix * vertexPosition;
       normal = vertexNormal;
       texCoord = vertexTexCoord;
    }
);


static const char* cubeFragmentShader = STRINGIFY(
    precision mediump float;

    varying vec2 texCoord;
    varying vec4 normal;

    uniform sampler2D texSampler2D;

    void main()
    {
       vec4 texColor = texture2D(texSampler2D, texCoord);
       gl_FragColor = texColor;
    }
);

static const char* passThroughVertexShader = STRINGIFY(
    attribute vec4 vertexPosition;
    attribute vec2 vertexTexCoord;
    uniform mat4 modelViewProjectionMatrix;
    varying vec2 texCoord;

    void main()
    {
        gl_Position = modelViewProjectionMatrix * vertexPosition;
        texCoord = vertexTexCoord;
    }
);

static const char* passThroughFragmentShader = STRINGIFY(
    precision mediump float;
    varying vec2 texCoord;
    uniform sampler2D texSamplerVideo;

    void main ()
    {
        gl_FragColor = texture2D(texSamplerVideo, texCoord);
    }
);

#endif

#endif // _QCAR_CUBE_SHADERS_H_
