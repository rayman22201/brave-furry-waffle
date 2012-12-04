/*==============================================================================
            Copyright (c) 2012 QUALCOMM Austria Research Center GmbH.
            All Rights Reserved.
            Qualcomm Confidential and Proprietary
            
@file 
    ImageTargets.cpp

@brief
    Sample for ImageTargets

==============================================================================*/


#include <jni.h>
#include <android/log.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h> //so I can use rand
#include <sys/time.h>
#include <math.h>
#include "SampleMath.h"

#ifdef USE_OPENGL_ES_1_1
#include <GLES/gl.h>
#include <GLES/glext.h>
#else
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#endif

#include <QCAR/QCAR.h>
#include <QCAR/CameraDevice.h>
#include <QCAR/Renderer.h>
#include <QCAR/VideoBackgroundConfig.h>
#include <QCAR/Trackable.h>
#include <QCAR/Tool.h>
#include <QCAR/Tracker.h>
#include <QCAR/TrackerManager.h>
#include <QCAR/ImageTracker.h>
#include <QCAR/ImageTarget.h>
#include <QCAR/CameraCalibration.h>
#include <QCAR/UpdateCallback.h>
#include <QCAR/DataSet.h>
#include <QCAR/VideoBackgroundTextureInfo.h>

#include "SampleUtils.h"
#include "Texture.h"
#include "CubeShaders.h"
#include "fur_shaders.h"
#include "glow_shaders.h"
#include "sphere.h"

#ifdef __cplusplus
extern "C"
{
#endif

// Textures:
int textureCount                = 0;
Texture** textures              = 0;

// OpenGL ES 2.0 specific:
#ifdef USE_OPENGL_ES_2_0
unsigned int shaderProgramID    = 0;
GLint vertexHandle              = 0;
GLint normalHandle              = 0;
GLint textureCoordHandle        = 0;
GLint mvpMatrixHandle           = 0;
#endif

// Screen dimensions:
unsigned int screenWidth        = 0;
unsigned int screenHeight       = 0;

// Indicates whether screen is in portrait (true) or landscape (false) mode
bool isActivityInPortraitMode   = false;

// The projection matrix used for rendering virtual objects:
QCAR::Matrix44F projectionMatrix;
QCAR::Matrix44F inverseProjMatrix;
QCAR::Matrix44F modelViewMatrix;

// Constants:
static const float kObjectScale = 20.0f;

QCAR::DataSet* dataSetStonesAndChips    = 0;
QCAR::DataSet* dataSetTarmac            = 0;

bool switchDataSetAsap          = false;

//fur stuff
#define FURTEXSIZE 512
#define NUMLAYERS 10
GLuint furTexId;
GLubyte furImg[FURTEXSIZE][FURTEXSIZE][4];
static unsigned int fur_NumLayers = NUMLAYERS;
static float fur_FurLength = 0.5f;

unsigned int fur_shaderProgramID = 0;
GLuint furLength_handle = 0;
GLuint fur_vertexHandle = 0;
GLuint fur_normalHandle = 0;
GLuint fur_textureCoordHandle = 0;
GLuint fur_mvpMatrixHandle = 0;
GLuint fur_layer_handle = 0;
GLuint fur_force_handle = 0;

//glow shader stuff
unsigned int glow_shaderProgramID = 0;
GLuint glow_vertexHandle = 0;
GLuint glow_normalHandle = 0;
GLuint glow_textureCoordHandle = 0;
GLuint glow_mvpMatrixHandle = 0;
GLuint glow_layer_handle = 0;
GLuint glow_radius_handle = 0;

//animation stuff
unsigned int animClock = 0;
float testForce = 0.0;

//transparancy
float   bgOrthoProjMatrix[16];

unsigned int bgShaderProgramID              = 0;
GLuint bgVertexPositionHandle               = 0;
GLuint bgVertexTexCoordHandle               = 0;
GLuint bgTexSamplerVideoHandle              = 0;
GLuint bgProjectionMatrixHandle             = 0;
GLuint bgViewportOriginHandle               = 0;
GLuint bgViewportSizeHandle                 = 0;
GLuint bgTextureRatioHandle                 = 0;
int viewportPositionX           = 0;
int viewportPositionY           = 0;
int viewportSizeX               = 0;
int viewportSizeY               = 0;
float bgOrthoQuadVertices[] =
{
    -1.0f, -1.0f, 0.0f,
     1.0f, -1.0f, 0.0f,
     1.0f,  1.0f, 0.0f,
    -1.0f,  1.0f, 0.0f
};

float bgOrthoQuadTexCoords[] =
{
    0.0,    1.0,
    1.0,    1.0,
    1.0,    0.0,
    0.0,    0.0
};

GLbyte bgOrthoQuadIndices[]=
{
    0, 1, 2, 2, 3, 0
};

// ----------------------------------------------------------------------------
// Time utility
// ----------------------------------------------------------------------------

unsigned long
getCurrentTimeMS() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    unsigned long s = tv.tv_sec * 1000;
    unsigned long us = tv.tv_usec / 1000;
    return s + us;
}
//////////////////////////////////////////////////////////////////////////////

#define MAX_TAP_TIMER 200
#define MAX_TAP_DISTANCE2 400
enum ActionType {
    ACTION_DOWN,
    ACTION_MOVE,
    ACTION_UP,
    ACTION_CANCEL
};
typedef struct _TouchEvent {
    bool isActive;
    int actionType;
    int pointerId;
    float x;
    float y;
    float lastX;
    float lastY;
    float startX;
    float startY;
    float tapX;
    float tapY;
    unsigned long startTime;
    unsigned long dt;
    float dist2;
    bool didTap;
} TouchEvent;

TouchEvent touch1, touch2; //two touch events for phones that support multi-touch

// ----------------------------------------------------------------------------
// Touch projection
// ----------------------------------------------------------------------------
bool
linePlaneIntersection(QCAR::Vec3F lineStart, QCAR::Vec3F lineEnd,
                      QCAR::Vec3F pointOnPlane, QCAR::Vec3F planeNormal,
                      QCAR::Vec3F &intersection)
{
    QCAR::Vec3F lineDir = SampleMath::Vec3FSub(lineEnd, lineStart);
    lineDir = SampleMath::Vec3FNormalize(lineDir);
    
    QCAR::Vec3F planeDir = SampleMath::Vec3FSub(pointOnPlane, lineStart);
    
    float n = SampleMath::Vec3FDot(planeNormal, planeDir);
    float d = SampleMath::Vec3FDot(planeNormal, lineDir);
    
    if (fabs(d) < 0.00001) {
        // Line is parallel to plane
        return false;
    }
    
    float dist = n / d;
    
    QCAR::Vec3F offset = SampleMath::Vec3FScale(lineDir, dist);
    intersection = SampleMath::Vec3FAdd(lineStart, offset);
}

void
projectScreenPointToPlane(QCAR::Vec2F point, QCAR::Vec3F planeCenter, QCAR::Vec3F planeNormal,
                          QCAR::Vec3F &intersection, QCAR::Vec3F &lineStart, QCAR::Vec3F &lineEnd)
{
    // Window Coordinates to Normalized Device Coordinates
    QCAR::VideoBackgroundConfig config = QCAR::Renderer::getInstance().getVideoBackgroundConfig();
    
    float halfScreenWidth = screenWidth / 2.0f;
    float halfScreenHeight = screenHeight / 2.0f;
    
    float halfViewportWidth = config.mSize.data[0] / 2.0f;
    float halfViewportHeight = config.mSize.data[1] / 2.0f;
    
    float x = (point.data[0] - halfScreenWidth) / halfViewportWidth;
    float y = (point.data[1] - halfScreenHeight) / halfViewportHeight * -1;
    
    QCAR::Vec4F ndcNear(x, y, -1, 1);
    QCAR::Vec4F ndcFar(x, y, 1, 1);
    
    // Normalized Device Coordinates to Eye Coordinates
    QCAR::Vec4F pointOnNearPlane = SampleMath::Vec4FTransform(ndcNear, inverseProjMatrix);
    QCAR::Vec4F pointOnFarPlane = SampleMath::Vec4FTransform(ndcFar, inverseProjMatrix);
    pointOnNearPlane = SampleMath::Vec4FDiv(pointOnNearPlane, pointOnNearPlane.data[3]);
    pointOnFarPlane = SampleMath::Vec4FDiv(pointOnFarPlane, pointOnFarPlane.data[3]);
    
    // Eye Coordinates to Object Coordinates
    QCAR::Matrix44F inverseModelViewMatrix = SampleMath::Matrix44FInverse(modelViewMatrix);
    
    QCAR::Vec4F nearWorld = SampleMath::Vec4FTransform(pointOnNearPlane, inverseModelViewMatrix);
    QCAR::Vec4F farWorld = SampleMath::Vec4FTransform(pointOnFarPlane, inverseModelViewMatrix);
    
    lineStart = QCAR::Vec3F(nearWorld.data[0], nearWorld.data[1], nearWorld.data[2]);
    lineEnd = QCAR::Vec3F(farWorld.data[0], farWorld.data[1], farWorld.data[2]);
    linePlaneIntersection(lineStart, lineEnd, planeCenter, planeNormal, intersection);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define FLOOR 20.0
class Glowball {
public:
    float position[3];
    float scale[3];
    int textureIndex;

    //constructor
    Glowball() {
        this->position[0] = 80.0;
        this->position[1] = 0.0;
        this->position[2] = FLOOR;

        this->scale[0] = 9.0;
        this->scale[1] = 9.0;
        this->scale[2] = 9.0;

        textureIndex = 3;
    }

    void move() {
        //handle touch input
        if(touch1.isActive) {
            //@TODO: Find the interection point of the touch with the ground plane
            // Find the interection point of the touch with the ground plane
            QCAR::Vec3F intersection, lineStart, lineEnd;
            projectScreenPointToPlane(QCAR::Vec2F(touch1.x, touch1.y), QCAR::Vec3F(0, 0, 0), QCAR::Vec3F(0, 0, 1), intersection, lineStart, lineEnd);
            this->position[0] = intersection.data[0];
            this->position[1] = intersection.data[1];
        }
    }

    void render(QCAR::Matrix44F modelViewMatrix) {
        const Texture* const thisTexture = textures[this->textureIndex];

        QCAR::Matrix44F modelViewProjection;

        SampleUtils::translatePoseMatrix( this->position[0], 
                                          this->position[1],
                                          this->position[2],
                                            &modelViewMatrix.data[0]);
        SampleUtils::scalePoseMatrix(this->scale[0], this->scale[1], this->scale[2],
                                     &modelViewMatrix.data[0]);

        SampleUtils::multiplyMatrix(&projectionMatrix.data[0],
                                    &modelViewMatrix.data[0] ,
                                    &modelViewProjection.data[0]);

        glUseProgram(shaderProgramID);// cube shader
         
        glVertexAttribPointer(vertexHandle, 3, GL_FLOAT, GL_FALSE, 0,
                              (const GLvoid*) &sphereVerts[0]);
        glVertexAttribPointer(normalHandle, 3, GL_FLOAT, GL_FALSE, 0,
                              (const GLvoid*) &sphereNormals[0]);
        glVertexAttribPointer(textureCoordHandle, 2, GL_FLOAT, GL_FALSE, 0,
                              (const GLvoid*) &sphereTexCoords[0]);
        
        glEnableVertexAttribArray(vertexHandle);
        glEnableVertexAttribArray(normalHandle);
        glEnableVertexAttribArray(textureCoordHandle);
        
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, thisTexture->mTextureID);
        
        glUniformMatrix4fv(mvpMatrixHandle, 1, GL_FALSE,
                           (GLfloat*)&modelViewProjection.data[0] );

        glDrawArrays(GL_TRIANGLES, 0, sphereNumVerts);

        //render the glow
        float layer = 0.7;
        float length = 0.0;
        float glow_NumLayers = 36;

        for(int i=0; i<glow_NumLayers; i+=1)
        {
            layer = layer + 0.03;
            length = length + 0.06;
            //LOG("Layer: %f, Length: %f",layer,length);
            // Set up the Shader
            glUseProgram(glow_shaderProgramID);
         
            glVertexAttribPointer(glow_vertexHandle, 3, GL_FLOAT, GL_FALSE, 0,
                                  (const GLvoid*) &sphereVerts[0]);
            glVertexAttribPointer(glow_normalHandle, 3, GL_FLOAT, GL_FALSE, 0,
                                  (const GLvoid*) &sphereNormals[0]);
            glVertexAttribPointer(glow_textureCoordHandle, 2, GL_FLOAT, GL_FALSE, 0,
                                  (const GLvoid*) &sphereTexCoords[0]);
            
            glEnableVertexAttribArray(glow_vertexHandle);
            glEnableVertexAttribArray(glow_normalHandle);
            glEnableVertexAttribArray(glow_textureCoordHandle);

            // Render Textured glow &
            // Draw our mesh object
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, thisTexture->mTextureID);

            glUniformMatrix4fv(glow_mvpMatrixHandle, 1, GL_FALSE,
                           (GLfloat*)&modelViewProjection.data[0] );

            glUniform1f(glow_radius_handle, length);
            glUniform1f(glow_layer_handle, layer);

            glDrawArrays(GL_TRIANGLES, 0, sphereNumVerts);

            glDisableVertexAttribArray(glow_vertexHandle);
            glDisableVertexAttribArray(glow_normalHandle);
            glDisableVertexAttribArray(glow_textureCoordHandle);

        }//End for loop
    }
};

class Eyeball {
public:
    float parentPosition[3];
    float position[3];
    float rotation[3];
    float initialRotation[3];
    float scale[3];
    int textureIndex;

    //constructor
    Eyeball() {
        this->resetVars();
    }

    Eyeball(float newParent[3], float offsetX, float offsetY, float offsetZ, float offsetAngle) {
        this->resetVars();

        this->move(newParent);
        this->position[0] = offsetX;
        this->position[1] = offsetY;
        this->position[2] = offsetZ;

        this->rotation[2] = offsetAngle;
        this->initialRotation[2] = offsetAngle;
        this->rotate();
    }

    void resetVars() {
        this->parentPosition[0] = 0.0;
        this->parentPosition[1] = 0.0;
        this->parentPosition[2] = 0.0;

        this->position[0] = 0.0;
        this->position[1] = 0.0;
        this->position[2] = 0.0;

        this->rotation[0] = 0.0;
        this->rotation[1] = 0.0;
        this->rotation[2] = 0.0;

        this->initialRotation[0] = 0.0;
        this->initialRotation[1] = 0.0;
        this->initialRotation[2] = 0.0;

        this->scale[0] = 10.0;
        this->scale[1] = 10.0;
        this->scale[2] = 12.0;

        this->textureIndex = 2;
    }

    void move(float newPos[3]) {
        this->parentPosition[0] = newPos[0];
        this->parentPosition[1] = newPos[1];
        this->parentPosition[2] = newPos[2];
    }

    void rotate() {
        this->rotation[2] = fmod(this->rotation[2], 360.0);
        float radiansZ = (this->rotation[2] * M_PI) / 180.0;

        float c = cos(radiansZ);
        float s = sin(radiansZ);

        //calculate the new position..i.e. rotate around the center point
        float dx  = this->position[0];
        float dy = this->position[1];
        float radius = sqrt( (dx*dx) + (dy*dy) );

        this->position[0] = s * radius;
        this->position[1] = c * radius;
    }

    void render(QCAR::Matrix44F modelViewMatrix) {
        const Texture* const thisTexture = textures[this->textureIndex];

        QCAR::Matrix44F modelViewProjection;

        this->rotate();

        SampleUtils::translatePoseMatrix( (this->parentPosition[0] + this->position[0]), 
                                          (this->parentPosition[1] + this->position[1]),
                                          (this->parentPosition[2] + this->position[2]),
                                            &modelViewMatrix.data[0]);
        SampleUtils::scalePoseMatrix(this->scale[0], this->scale[1], this->scale[2],
                                     &modelViewMatrix.data[0]);

        //rotate around it's center of gravity
        SampleUtils::rotatePoseMatrix( ((this->rotation[2] - this->initialRotation[2]) * -1.0), 0.0, 0.0, 1.0, &modelViewMatrix.data[0]);

        SampleUtils::multiplyMatrix(&projectionMatrix.data[0],
                                    &modelViewMatrix.data[0] ,
                                    &modelViewProjection.data[0]);

        glUseProgram(shaderProgramID);// cube shader
         
        glVertexAttribPointer(vertexHandle, 3, GL_FLOAT, GL_FALSE, 0,
                              (const GLvoid*) &sphereVerts[0]);
        glVertexAttribPointer(normalHandle, 3, GL_FLOAT, GL_FALSE, 0,
                              (const GLvoid*) &sphereNormals[0]);
        glVertexAttribPointer(textureCoordHandle, 2, GL_FLOAT, GL_FALSE, 0,
                              (const GLvoid*) &sphereTexCoords[0]);
        
        glEnableVertexAttribArray(vertexHandle);
        glEnableVertexAttribArray(normalHandle);
        glEnableVertexAttribArray(textureCoordHandle);
        
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, thisTexture->mTextureID);
        
        glUniformMatrix4fv(mvpMatrixHandle, 1, GL_FALSE,
                           (GLfloat*)&modelViewProjection.data[0] );

        glDrawArrays(GL_TRIANGLES, 0, sphereNumVerts);
    }
};

//Lemming class
class Lemming {
public:
    float oldPos[3];
    float position[3];
    float direction[3];
    float rotation[3];
    float scale[3];
    float velocity[3];
    float scaleVelocity[3];
    float rotationVelocity[3];
    float accel[3];
    float force[3];
    float springForce[3]; //technically the position, but w/e
    float springVelocity[3];
    float speed;
    float moveThreshold;

    Eyeball eyes[2];

    int goingUp;
    int textureIndex;


    //constructor
    Lemming() {
        this->position[0] = 0.0;
        this->position[1] = 0.0;
        this->position[2] = FLOOR;

        this->scale[0] = 20.0f;
        this->scale[1] = 20.0f;
        this->scale[2] = 20.0f;

        this->rotation[0] = 0.0;
        this->rotation[1] = 0.0;
        this->rotation[2] = 0.0;

        this->rotationVelocity[0] = 0.0;
        this->rotationVelocity[1] = 0.0;
        this->rotationVelocity[2] = 0.0;

        this->speed = 3.0;
        this->moveThreshold = 32.0;
        this->velocity[0] = 0.0;
        this->velocity[1] = 0.0;
        this->velocity[2] = 1.4;

        this->scaleVelocity[0] = 0.0;
        this->scaleVelocity[1] = 0.0;
        this->scaleVelocity[2] = 0.0;

        this->springForce[0] = 0.0;
        this->springForce[1] = 0.0;
        this->springForce[2] = 0.0;
        this->springVelocity[0] = 0.0;
        this->springVelocity[1] = 0.0;
        this->springVelocity[2] = 0.0;

        this->eyes[0] = Eyeball(this->position, 12.0, 5.0, 5.0, 65.0);
        this->eyes[1] = Eyeball(this->position, 12.0, 5.0, 5.0, 115.0);

        this->goingUp = 1;
        this->textureIndex = 0;


    }

    void rotate(float angle) {
        float eyeAngle[2];

        eyeAngle[0] = angle - 25.0;
        eyeAngle[1] = angle + 25.0;

        this->eyes[0].rotation[2] = eyeAngle[0];
        this->eyes[1].rotation[2] = eyeAngle[1];
    }

    bool collisionCheck(float target[3]) {
        float dir[3];
        dir[0] = target[0] - this->position[0];
        dir[1] = target[1] - this->position[1];
        dir[2] = 0.0;

        float dirDistance = sqrt( (dir[0] * dir[0]) + (dir[1] * dir[1]) );

        if( (dirDistance < (this->moveThreshold - 15) ) ) {
            return true;
        }
        return false;
    }

    void move(float target[3]) {
        float dir[3];
        dir[0] = target[0] - this->position[0];
        dir[1] = target[1] - this->position[1];
        dir[2] = 0.0;

        float dirDistance = sqrt( (dir[0] * dir[0]) + (dir[1] * dir[1]) );
        
        //threshold check
        if( (dirDistance < this->moveThreshold) ) {
            dir[0] = 0.0;
            dir[1] = 0.0;

            this->velocity[0] = dir[0];
            this->velocity[1] = dir[1];
        }
        else {
            //normalize
            dir[0] = dir[0] / dirDistance;
            dir[1] = dir[1] / dirDistance;

            this->velocity[0] = dir[0] * this->speed;
            this->velocity[1] = dir[1] * this->speed;

            float angle = atan( (dir[0] / dir[1]) );
            if( dir[1] < 0 ) {
                angle = (-1.0 * M_PI) - angle; //reflect y
                angle = -1.0 * angle; //reflect x
            }
            float degrees = angle * (180/ M_PI);

            this->rotate(degrees);
        }

        for(int i = 0; i < 3; i++) {
            this->oldPos[i] = this->position[i];
            this->position[i] = this->position[i] + this->velocity[i];
            this->scale[i] = this->scale[i] + this->scaleVelocity[i];
            this->springForce[i] = this->springForce[i] + this->springVelocity[i];
        }
        this->velocity[2] = this->velocity[2] - 0.2; //accel from gravity
        
        if(this->position[2] < FLOOR) {
            this->oldPos[2] = this->position[2];
            this->position[2] = FLOOR;
            this->velocity[2] = 1.4;
            this->scale[2] = 20.0;
            this->scaleVelocity[2] = 0.0;
            this->springForce[2] = 0.0;
            this->springVelocity[2] = 0.0;
        }

        if(this->goingUp == 1) {
            this->springVelocity[2] = this->springVelocity[2] + 0.001;
            this->scaleVelocity[2] = this->scaleVelocity[2] + 0.03;
        }
        else {
            this->springVelocity[2] = this->springVelocity[2] - 0.001;
            this->scaleVelocity[2] = this->scaleVelocity[2] - 0.03;
        }

        if( (this->position[2] * this->oldPos[2]) < 0 ) {
            //we just hit the apex of the jump
            if(this->goingUp == 1) {
                this->goingUp = 0;
            }
            else {
                this->goingUp = 1;
            }
                
        }

        this->eyes[0].move(this->position);
        this->eyes[1].move(this->position);
    }

    void render(QCAR::Matrix44F modelViewMatrix) {
        //render the eyeballs
        this->eyes[0].render(modelViewMatrix);
        this->eyes[1].render(modelViewMatrix);

        const Texture* const thisTexture = textures[this->textureIndex];

        QCAR::Matrix44F modelViewProjection;

        SampleUtils::translatePoseMatrix(this->position[0], this->position[1], this->position[2],
                                         &modelViewMatrix.data[0]);
        SampleUtils::scalePoseMatrix(this->scale[0], this->scale[1], this->scale[2],
                                     &modelViewMatrix.data[0]);

        SampleUtils::multiplyMatrix(&projectionMatrix.data[0],
                                    &modelViewMatrix.data[0] ,
                                    &modelViewProjection.data[0]);

        glUseProgram(shaderProgramID);
         
        glVertexAttribPointer(vertexHandle, 3, GL_FLOAT, GL_FALSE, 0,
                              (const GLvoid*) &sphereVerts[0]);
        glVertexAttribPointer(normalHandle, 3, GL_FLOAT, GL_FALSE, 0,
                              (const GLvoid*) &sphereNormals[0]);
        glVertexAttribPointer(textureCoordHandle, 2, GL_FLOAT, GL_FALSE, 0,
                              (const GLvoid*) &sphereTexCoords[0]);
        
        glEnableVertexAttribArray(vertexHandle);
        glEnableVertexAttribArray(normalHandle);
        glEnableVertexAttribArray(textureCoordHandle);
        
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, thisTexture->mTextureID);
        
        glUniformMatrix4fv(mvpMatrixHandle, 1, GL_FALSE,
                           (GLfloat*)&modelViewProjection.data[0] );

        glDrawArrays(GL_TRIANGLES, 0, sphereNumVerts);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, furTexId);
        glDrawArrays(GL_TRIANGLES, 0, sphereNumVerts);

        //Fur Rendering
        ////////////////////////////////////////////////////////////////////////////
        // Render Our Shells!
        
        float layer = 0;
        float length = 0;

        for(int i=0; i<fur_NumLayers; i+=1)
        {
            layer = float(i+1) / fur_NumLayers;
            length = fur_FurLength * layer;
            //LOG("Layer: %f, Length: %f",layer,length);
            // Set up the Shader
            glUseProgram(fur_shaderProgramID);
         
            glVertexAttribPointer(fur_vertexHandle, 3, GL_FLOAT, GL_FALSE, 0,
                                  (const GLvoid*) &sphereVerts[0]);
            glVertexAttribPointer(fur_normalHandle, 3, GL_FLOAT, GL_FALSE, 0,
                                  (const GLvoid*) &sphereNormals[0]);
            glVertexAttribPointer(fur_textureCoordHandle, 2, GL_FLOAT, GL_FALSE, 0,
                                  (const GLvoid*) &sphereTexCoords[0]);
            
            glEnableVertexAttribArray(fur_vertexHandle);
            glEnableVertexAttribArray(fur_normalHandle);
            glEnableVertexAttribArray(fur_textureCoordHandle);

            // Render Textured Fur &
            // Draw our mesh object
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, furTexId);

            glUniformMatrix4fv(fur_mvpMatrixHandle, 1, GL_FALSE,
                           (GLfloat*)&modelViewProjection.data[0] );

            glUniform1f(furLength_handle, length);
            glUniform1f(fur_layer_handle, layer);

            glUniform4f(fur_force_handle, this->springForce[0], this->springForce[1], this->springForce[2], 0.0);

            glDrawArrays(GL_TRIANGLES, 0, sphereNumVerts);

            glDisableVertexAttribArray(fur_vertexHandle);
            glDisableVertexAttribArray(fur_normalHandle);
            glDisableVertexAttribArray(fur_textureCoordHandle);

        }//End for loop
        ////////////////////////////////////////////////////////////////////////////
    }
};

#define NUMLEMMINGS 4
Lemming lemmings[NUMLEMMINGS];
Glowball guideBall;

// Object to receive update callbacks from QCAR SDK
class ImageTargets_UpdateCallback : public QCAR::UpdateCallback
{   
    virtual void QCAR_onUpdate(QCAR::State& /*state*/)
    {
        if (switchDataSetAsap)
        {
            switchDataSetAsap = false;

            // Get the image tracker:
            QCAR::TrackerManager& trackerManager = QCAR::TrackerManager::getInstance();
            QCAR::ImageTracker* imageTracker = static_cast<QCAR::ImageTracker*>(
                trackerManager.getTracker(QCAR::Tracker::IMAGE_TRACKER));
            if (imageTracker == 0 || dataSetStonesAndChips == 0 || dataSetTarmac == 0 ||
                imageTracker->getActiveDataSet() == 0)
            {
                LOG("Failed to switch data set.");
                return;
            }
            
            if (imageTracker->getActiveDataSet() == dataSetStonesAndChips)
            {
                imageTracker->deactivateDataSet(dataSetStonesAndChips);
                imageTracker->activateDataSet(dataSetTarmac);
            }
            else
            {
                imageTracker->deactivateDataSet(dataSetTarmac);
                imageTracker->activateDataSet(dataSetStonesAndChips);
            }
        }
    }
};

ImageTargets_UpdateCallback updateCallback;

JNIEXPORT int JNICALL
Java_com_qualcomm_QCARSamples_ImageTargets_ImageTargets_getOpenGlEsVersionNative(JNIEnv *, jobject)
{
#ifdef USE_OPENGL_ES_1_1        
    return 1;
#else
    return 2;
#endif
}


JNIEXPORT void JNICALL
Java_com_qualcomm_QCARSamples_ImageTargets_ImageTargets_setActivityPortraitMode(JNIEnv *, jobject, jboolean isPortrait)
{
    isActivityInPortraitMode = isPortrait;
}



JNIEXPORT void JNICALL
Java_com_qualcomm_QCARSamples_ImageTargets_ImageTargets_switchDatasetAsap(JNIEnv *, jobject)
{
    switchDataSetAsap = true;
}


JNIEXPORT int JNICALL
Java_com_qualcomm_QCARSamples_ImageTargets_ImageTargets_initTracker(JNIEnv *, jobject)
{
    LOG("Java_com_qualcomm_QCARSamples_ImageTargets_ImageTargets_initTracker");
    
    // Initialize the image tracker:
    QCAR::TrackerManager& trackerManager = QCAR::TrackerManager::getInstance();
    QCAR::Tracker* tracker = trackerManager.initTracker(QCAR::Tracker::IMAGE_TRACKER);
    if (tracker == NULL)
    {
        LOG("Failed to initialize ImageTracker.");
        return 0;
    }

    LOG("Successfully initialized ImageTracker.");
    return 1;
}


JNIEXPORT void JNICALL
Java_com_qualcomm_QCARSamples_ImageTargets_ImageTargets_deinitTracker(JNIEnv *, jobject)
{
    LOG("Java_com_qualcomm_QCARSamples_ImageTargets_ImageTargets_deinitTracker");

    // Deinit the image tracker:
    QCAR::TrackerManager& trackerManager = QCAR::TrackerManager::getInstance();
    trackerManager.deinitTracker(QCAR::Tracker::IMAGE_TRACKER);
}


JNIEXPORT int JNICALL
Java_com_qualcomm_QCARSamples_ImageTargets_ImageTargets_loadTrackerData(JNIEnv *, jobject)
{
    LOG("Java_com_qualcomm_QCARSamples_ImageTargets_ImageTargets_loadTrackerData");
    
    // Get the image tracker:
    QCAR::TrackerManager& trackerManager = QCAR::TrackerManager::getInstance();
    QCAR::ImageTracker* imageTracker = static_cast<QCAR::ImageTracker*>(
                    trackerManager.getTracker(QCAR::Tracker::IMAGE_TRACKER));
    if (imageTracker == NULL)
    {
        LOG("Failed to load tracking data set because the ImageTracker has not"
            " been initialized.");
        return 0;
    }

    // Create the data sets:
    dataSetStonesAndChips = imageTracker->createDataSet();
    if (dataSetStonesAndChips == 0)
    {
        LOG("Failed to create a new tracking data.");
        return 0;
    }

    dataSetTarmac = imageTracker->createDataSet();
    if (dataSetTarmac == 0)
    {
        LOG("Failed to create a new tracking data.");
        return 0;
    }

    // Load the data sets:
    if (!dataSetStonesAndChips->load("StonesAndChips.xml", QCAR::DataSet::STORAGE_APPRESOURCE))
    {
        LOG("Failed to load data set.");
        return 0;
    }

    if (!dataSetTarmac->load("Tarmac.xml", QCAR::DataSet::STORAGE_APPRESOURCE))
    {
        LOG("Failed to load data set.");
        return 0;
    }

    // Activate the data set:
    if (!imageTracker->activateDataSet(dataSetStonesAndChips))
    {
        LOG("Failed to activate data set.");
        return 0;
    }

    LOG("Successfully loaded and activated data set.");
    return 1;
}


JNIEXPORT int JNICALL
Java_com_qualcomm_QCARSamples_ImageTargets_ImageTargets_destroyTrackerData(JNIEnv *, jobject)
{
    LOG("Java_com_qualcomm_QCARSamples_ImageTargets_ImageTargets_destroyTrackerData");

    // Get the image tracker:
    QCAR::TrackerManager& trackerManager = QCAR::TrackerManager::getInstance();
    QCAR::ImageTracker* imageTracker = static_cast<QCAR::ImageTracker*>(
        trackerManager.getTracker(QCAR::Tracker::IMAGE_TRACKER));
    if (imageTracker == NULL)
    {
        LOG("Failed to destroy the tracking data set because the ImageTracker has not"
            " been initialized.");
        return 0;
    }
    
    if (dataSetStonesAndChips != 0)
    {
        if (imageTracker->getActiveDataSet() == dataSetStonesAndChips &&
            !imageTracker->deactivateDataSet(dataSetStonesAndChips))
        {
            LOG("Failed to destroy the tracking data set StonesAndChips because the data set "
                "could not be deactivated.");
            return 0;
        }

        if (!imageTracker->destroyDataSet(dataSetStonesAndChips))
        {
            LOG("Failed to destroy the tracking data set StonesAndChips.");
            return 0;
        }

        LOG("Successfully destroyed the data set StonesAndChips.");
        dataSetStonesAndChips = 0;
    }

    if (dataSetTarmac != 0)
    {
        if (imageTracker->getActiveDataSet() == dataSetTarmac &&
            !imageTracker->deactivateDataSet(dataSetTarmac))
        {
            LOG("Failed to destroy the tracking data set Tarmac because the data set "
                "could not be deactivated.");
            return 0;
        }

        if (!imageTracker->destroyDataSet(dataSetTarmac))
        {
            LOG("Failed to destroy the tracking data set Tarmac.");
            return 0;
        }

        LOG("Successfully destroyed the data set Tarmac.");
        dataSetTarmac = 0;
    }

    return 1;
}


JNIEXPORT void JNICALL
Java_com_qualcomm_QCARSamples_ImageTargets_ImageTargets_onQCARInitializedNative(JNIEnv *, jobject)
{
    // Register the update callback where we handle the data set swap:
    QCAR::registerCallback(&updateCallback);

    // Comment in to enable tracking of up to 2 targets simultaneously and
    // split the work over multiple frames:
    // QCAR::setHint(QCAR::HINT_MAX_SIMULTANEOUS_IMAGE_TARGETS, 2);
    // QCAR::setHint(QCAR::HINT_IMAGE_TARGET_MULTI_FRAME_ENABLED, 1);
}


JNIEXPORT void JNICALL
Java_com_qualcomm_QCARSamples_ImageTargets_ImageTargetsRenderer_renderFrame(JNIEnv *, jobject)
{
    //update that animation counter
    animClock++;

    LOG("Java_com_qualcomm_QCARSamples_ImageTargets_GLRenderer_renderFrame");

    // Clear color and depth buffer 
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Get the state from QCAR and mark the beginning of a rendering section
    QCAR::State state = QCAR::Renderer::getInstance().begin();
    
    // Explicitly render the Video Background
    //QCAR::Renderer::getInstance().drawVideoBackground();
    const QCAR::VideoBackgroundTextureInfo texInfo =
        QCAR::Renderer::getInstance().getVideoBackgroundTextureInfo();
    float uRatio =
        ((float)texInfo.mImageSize.data[0]/(float)texInfo.mTextureSize.data[0]);
    float vRatio =
        ((float)texInfo.mImageSize.data[1]/(float)texInfo.mTextureSize.data[1]);
    //LOG("imageSize: %f, %f TextureSize:%f, %f",(float)texInfo.mImageSize.data[0],(float)texInfo.mImageSize.data[1],(float)texInfo.mTextureSize.data[0],(float)texInfo.mTextureSize.data[1]);
    bgOrthoQuadTexCoords[1] = vRatio;
    bgOrthoQuadTexCoords[2] = uRatio;
    bgOrthoQuadTexCoords[3] = vRatio;
    bgOrthoQuadTexCoords[4] = uRatio;

    ////////////////////////////////////////////////////////////////////////////
    // This section renders the video background with a
    // a shader defined in Shaders.h, it doesn't apply any effect, it merely
    // renders it
    // 
    QCAR::Renderer::getInstance().bindVideoBackground(0);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    // Load the shader and upload the vertex/texcoord/index data
    glViewport(viewportPositionX, viewportPositionY,
               viewportSizeX, viewportSizeY);

    glUseProgram(bgShaderProgramID);
    glVertexAttribPointer(bgVertexPositionHandle, 3, GL_FLOAT, GL_FALSE, 0,
                                                        bgOrthoQuadVertices);
    glVertexAttribPointer(bgVertexTexCoordHandle, 2, GL_FLOAT, GL_FALSE, 0,
                                                        bgOrthoQuadTexCoords);
    glUniform1i(bgTexSamplerVideoHandle, 0);
    glUniformMatrix4fv(bgProjectionMatrixHandle, 1, GL_FALSE,
                                                        &bgOrthoProjMatrix[0]);

    // Render the video background 
    glEnableVertexAttribArray(bgVertexPositionHandle);
    glEnableVertexAttribArray(bgVertexTexCoordHandle);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, bgOrthoQuadIndices);
    glDisableVertexAttribArray(bgVertexPositionHandle);
    glDisableVertexAttribArray(bgVertexTexCoordHandle);

    // Wrap up this rendering
    glUseProgram(0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);

    SampleUtils::checkGlError("Rendering of the video background");
    //
    ////////////////////////////////////////////////////////////////////////////
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);    

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    // Did we find any trackables this frame?
    for(int tIdx = 0; tIdx < state.getNumActiveTrackables(); tIdx++)
    {
        // Get the trackable:
        const QCAR::Trackable* trackable = state.getActiveTrackable(tIdx);
        modelViewMatrix = QCAR::Tool::convertPose2GLMatrix(trackable->getPose());
            
        guideBall.move();

        for(int i = 0; i < NUMLEMMINGS; i++) {
            float oldPos[3];
            oldPos[0] = lemmings[i].position[0];
            oldPos[1] = lemmings[i].position[1];
            oldPos[2] = lemmings[i].position[2];
            if(i == 0) {
                lemmings[i].move(guideBall.position);
            }
            else {
                lemmings[i].move(lemmings[(i-1)].position);
            }
            
            bool collide = false;
            for(int j = 0; j < NUMLEMMINGS; j++) {
                if(i != j) {
                    if(lemmings[i].collisionCheck(lemmings[j].position) == true) {
                        collide = true;
                    }
                }
            }
            if(collide == true) {
                lemmings[i].position[0] = oldPos[0];
                lemmings[i].position[1] = oldPos[1];
                lemmings[i].position[2] = oldPos[2];
            }
        }
    
        for(int i = (NUMLEMMINGS - 1); i > -1; i--) {
            lemmings[i].render(modelViewMatrix);
        }
        guideBall.render(modelViewMatrix);
        
        /*float zDepth[5];
        float depthPoints[5][3];
        int zIndex[5];
        for(int i = 0; i < NUMLEMMINGS; i++) {
            depthPoints[i][0] = lemmings[i].position[0];
            depthPoints[i][1] = lemmings[i].position[1];
            depthPoints[i][2] = lemmings[i].position[2];
            zIndex[i] = i;
        }
        depthPoints[4][0] = guideBall.position[0];
        depthPoints[4][1] = guideBall.position[1];
        depthPoints[4][2] = guideBall.position[2];
        zIndex[4] = 42;

        for(int i = 0; i < 5; i++) {
            //derive the z-Distance, aka distance from the camera.
            LOG("depthPoints: %f, %f, %f", depthPoints[i][0], depthPoints[i][1], depthPoints[i][2]);
            QCAR::Vec4F worldPos(depthPoints[i][0], depthPoints[i][1], depthPoints[i][2], 1.0);
            LOG("worldPos: %f, %f, %f, %f", worldPos.data[0], worldPos.data[1], worldPos.data[2], worldPos.data[3]);
            QCAR::Vec4F camPoint = SampleMath::Vec4FTransform(worldPos, inverseProjMatrix);
            camPoint = SampleMath::Vec4FDiv(camPoint, camPoint.data[3]);
            LOG("camPoint: %f, %f, %f, %f", camPoint.data[0], camPoint.data[1], camPoint.data[2], camPoint.data[3]);
            float zDistance = sqrt( (camPoint.data[0] * camPoint.data[0]) + (camPoint.data[1] * camPoint.data[1]) + (camPoint.data[2] * camPoint.data[2]) );
            zDepth[i] = zDistance;
        }

        //bubble sort the zDepths
        //@TODO: This is kind of broken. I need to get the camera position from the inversePosMatrix and then get the distance and judge zDepth that way.
        for(int i = 0; i < 5; i++) {
            for(int j = 0, k = 1; k < 5; j++, k++) {
                if( (zDepth[j] > zDepth[k]) ) {
                    //swap
                    float ftemp = zDepth[k];
                    zDepth[k] = zDepth[j];
                    zDepth[j] = ftemp;

                    int itemp = zIndex[k];
                    zIndex[k] = zIndex[j];
                    zIndex[j] = itemp;
                }
            }
        }

        //render in z-order
        for(int i = 0; i < 5; i++) {
            LOG("zDepth:%f, index:%i", zDepth[i],zIndex[i]);
            if(zIndex[i] == 42) {
                guideBall.render(modelViewMatrix);
            }
            else
            {
                lemmings[ zIndex[i] ].render(modelViewMatrix);
            }
        }*/

        SampleUtils::checkGlError("ImageTargets renderFrame");
    }

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    glDisableVertexAttribArray(vertexHandle);
    glDisableVertexAttribArray(normalHandle);
    glDisableVertexAttribArray(textureCoordHandle);

    QCAR::Renderer::getInstance().end();
}


void
configureVideoBackground()
{
    // Get the default video mode:
    QCAR::CameraDevice& cameraDevice = QCAR::CameraDevice::getInstance();
    QCAR::VideoMode videoMode = cameraDevice.
                                getVideoMode(QCAR::CameraDevice::MODE_DEFAULT);


    // Configure the video background
    QCAR::VideoBackgroundConfig config;
    config.mEnabled = true;
    config.mSynchronous = true;
    config.mPosition.data[0] = 0.0f;
    config.mPosition.data[1] = 0.0f;
    
    if (isActivityInPortraitMode)
    {
        //LOG("configureVideoBackground PORTRAIT");
        config.mSize.data[0] = videoMode.mHeight
                                * (screenHeight / (float)videoMode.mWidth);
        config.mSize.data[1] = screenHeight;

        if(config.mSize.data[0] < screenWidth)
        {
            LOG("Correcting rendering background size to handle missmatch between screen and video aspect ratios.");
            config.mSize.data[0] = screenWidth;
            config.mSize.data[1] = screenWidth * 
                              (videoMode.mWidth / (float)videoMode.mHeight);
        }
    }
    else
    {
        //LOG("configureVideoBackground LANDSCAPE");
        config.mSize.data[0] = screenWidth;
        config.mSize.data[1] = videoMode.mHeight
                            * (screenWidth / (float)videoMode.mWidth);

        if(config.mSize.data[1] < screenHeight)
        {
            LOG("Correcting rendering background size to handle missmatch between screen and video aspect ratios.");
            config.mSize.data[0] = screenHeight
                                * (videoMode.mWidth / (float)videoMode.mHeight);
            config.mSize.data[1] = screenHeight;
        }
    }

    LOG("Configure Video Background : Video (%d,%d), Screen (%d,%d), mSize (%d,%d)", videoMode.mWidth, videoMode.mHeight, screenWidth, screenHeight, config.mSize.data[0], config.mSize.data[1]);

    viewportPositionX = ((screenWidth - config.mSize.data[0]) / 2) +
                                                    config.mPosition.data[0];
    viewportPositionY =  (((int)(screenHeight - config.mSize.data[1])) / (int) 2) +
                                                    config.mPosition.data[1];
    viewportSizeX = config.mSize.data[0];
    viewportSizeY = config.mSize.data[1];

    // Set the config:
    QCAR::Renderer::getInstance().setVideoBackgroundConfig(config);
}


JNIEXPORT void JNICALL
Java_com_qualcomm_QCARSamples_ImageTargets_ImageTargets_initApplicationNative(
                            JNIEnv* env, jobject obj, jint width, jint height)
{
    LOG("Java_com_qualcomm_QCARSamples_ImageTargets_ImageTargets_initApplicationNative");
    
    // Store screen dimensions
    screenWidth = width;
    screenHeight = height;
        
    // Handle to the activity class:
    jclass activityClass = env->GetObjectClass(obj);

    jmethodID getTextureCountMethodID = env->GetMethodID(activityClass,
                                                    "getTextureCount", "()I");
    if (getTextureCountMethodID == 0)
    {
        LOG("Function getTextureCount() not found.");
        return;
    }

    textureCount = env->CallIntMethod(obj, getTextureCountMethodID);    
    if (!textureCount)
    {
        LOG("getTextureCount() returned zero.");
        return;
    }

    textures = new Texture*[textureCount];

    jmethodID getTextureMethodID = env->GetMethodID(activityClass,
        "getTexture", "(I)Lcom/qualcomm/QCARSamples/ImageTargets/Texture;");

    if (getTextureMethodID == 0)
    {
        LOG("Function getTexture() not found.");
        return;
    }

    // Register the textures
    for (int i = 0; i < textureCount; ++i)
    {

        jobject textureObject = env->CallObjectMethod(obj, getTextureMethodID, i); 
        if (textureObject == NULL)
        {
            LOG("GetTexture() returned zero pointer");
            return;
        }

        textures[i] = Texture::create(env, textureObject);
    }
    LOG("Java_com_qualcomm_QCARSamples_ImageTargets_ImageTargets_initApplicationNative finished");
}


JNIEXPORT void JNICALL
Java_com_qualcomm_QCARSamples_ImageTargets_ImageTargets_deinitApplicationNative(
                                                        JNIEnv* env, jobject obj)
{
    LOG("Java_com_qualcomm_QCARSamples_ImageTargets_ImageTargets_deinitApplicationNative");

    // Release texture resources
    if (textures != 0)
    {    
        for (int i = 0; i < textureCount; ++i)
        {
            delete textures[i];
            textures[i] = NULL;
        }
    
        delete[]textures;
        textures = NULL;
        
        textureCount = 0;
    }
}


JNIEXPORT void JNICALL
Java_com_qualcomm_QCARSamples_ImageTargets_ImageTargets_startCamera(JNIEnv *,
                                                                         jobject)
{
    LOG("Java_com_qualcomm_QCARSamples_ImageTargets_ImageTargets_startCamera");

    // Initialize the camera:
    if (!QCAR::CameraDevice::getInstance().init())
        return;

    // Configure the video background
    configureVideoBackground();

    // Select the default mode:
    if (!QCAR::CameraDevice::getInstance().selectVideoMode(
                                QCAR::CameraDevice::MODE_DEFAULT))
        return;

    // Start the camera:
    if (!QCAR::CameraDevice::getInstance().start())
        return;

    // Uncomment to enable flash
    //if(QCAR::CameraDevice::getInstance().setFlashTorchMode(true))
    //	LOG("IMAGE TARGETS : enabled torch");

    // Uncomment to enable infinity focus mode, or any other supported focus mode
    // See CameraDevice.h for supported focus modes
    //if(QCAR::CameraDevice::getInstance().setFocusMode(QCAR::CameraDevice::FOCUS_MODE_INFINITY))
    //	LOG("IMAGE TARGETS : enabled infinity focus");

    // Start the tracker:
    QCAR::TrackerManager& trackerManager = QCAR::TrackerManager::getInstance();
    QCAR::Tracker* imageTracker = trackerManager.getTracker(QCAR::Tracker::IMAGE_TRACKER);
    if(imageTracker != 0)
        imageTracker->start();
}


JNIEXPORT void JNICALL
Java_com_qualcomm_QCARSamples_ImageTargets_ImageTargets_stopCamera(JNIEnv *, jobject)
{
    LOG("Java_com_qualcomm_QCARSamples_ImageTargets_ImageTargets_stopCamera");

    // Stop the tracker:
    QCAR::TrackerManager& trackerManager = QCAR::TrackerManager::getInstance();
    QCAR::Tracker* imageTracker = trackerManager.getTracker(QCAR::Tracker::IMAGE_TRACKER);
    if(imageTracker != 0)
        imageTracker->stop();
    
    QCAR::CameraDevice::getInstance().stop();
    QCAR::CameraDevice::getInstance().deinit();
}


JNIEXPORT void JNICALL
Java_com_qualcomm_QCARSamples_ImageTargets_ImageTargets_setProjectionMatrix(JNIEnv *, jobject)
{
    LOG("Java_com_qualcomm_QCARSamples_ImageTargets_ImageTargets_setProjectionMatrix");

    // Cache the projection matrix:
    const QCAR::CameraCalibration& cameraCalibration =
                                QCAR::CameraDevice::getInstance().getCameraCalibration();
    projectionMatrix = QCAR::Tool::getProjectionGL(cameraCalibration, 2.0f,
                                            2000.0f);
    // Cache the inverse projection matrix:
    inverseProjMatrix = SampleMath::Matrix44FInverse(projectionMatrix);
}


JNIEXPORT jboolean JNICALL
Java_com_qualcomm_QCARSamples_ImageTargets_ImageTargets_activateFlash(JNIEnv*, jobject, jboolean flash)
{
    return QCAR::CameraDevice::getInstance().setFlashTorchMode((flash==JNI_TRUE)) ? JNI_TRUE : JNI_FALSE;
}


JNIEXPORT jboolean JNICALL
Java_com_qualcomm_QCARSamples_ImageTargets_ImageTargets_autofocus(JNIEnv*, jobject)
{
    return QCAR::CameraDevice::getInstance().setFocusMode(QCAR::CameraDevice::FOCUS_MODE_TRIGGERAUTO) ? JNI_TRUE : JNI_FALSE;
}


JNIEXPORT jboolean JNICALL
Java_com_qualcomm_QCARSamples_ImageTargets_ImageTargets_setFocusMode(JNIEnv*, jobject, jint mode)
{
    int qcarFocusMode;

    switch ((int)mode)
    {
        case 0:
            qcarFocusMode = QCAR::CameraDevice::FOCUS_MODE_NORMAL;
            break;
        
        case 1:
            qcarFocusMode = QCAR::CameraDevice::FOCUS_MODE_CONTINUOUSAUTO;
            break;
            
        case 2:
            qcarFocusMode = QCAR::CameraDevice::FOCUS_MODE_INFINITY;
            break;
            
        case 3:
            qcarFocusMode = QCAR::CameraDevice::FOCUS_MODE_MACRO;
            break;
    
        default:
            return JNI_FALSE;
    }
    
    return QCAR::CameraDevice::getInstance().setFocusMode(qcarFocusMode) ? JNI_TRUE : JNI_FALSE;
}

//function to create a random texture for use with the fur
//furImg is a 3D array that represents the image. It is of dimensions, [width][height][4]
//The third dimension is the color value including alpha, RGBA
bool createFurTexture(GLuint mySize, GLuint idensity, unsigned int seed)
{
    srand(seed);

    for (GLuint x = 0; x < mySize; x++) {
        for (GLuint y = 0; y < mySize; y++) {
            //set every pixel to black and transparant
            furImg[x][y][0] = 0;
            furImg[x][y][1] = 0;
            furImg[x][y][2] = 0;
            furImg[x][y][3] = 0;
        }
    }

    for (GLuint myDensity = 0; myDensity < idensity; myDensity++) {
        int xrand = (rand() % mySize);
        int yrand = (rand() % mySize);
        
        furImg[xrand][yrand][0] = 0;
        furImg[xrand][yrand][1] = 0;
        furImg[xrand][yrand][2] = 255;
        furImg[xrand][yrand][3] = 255;
    }

    return true;
}


JNIEXPORT void JNICALL
Java_com_qualcomm_QCARSamples_ImageTargets_ImageTargetsRenderer_initRendering(
                                                    JNIEnv* env, jobject obj)
{
    LOG("Java_com_qualcomm_QCARSamples_ImageTargets_ImageTargetsRenderer_initRendering");

    //init the Lemming objects
    for(int i = 0; i < NUMLEMMINGS; i++) {
        lemmings[i] = Lemming();
    }
    for(int i = 1; i < NUMLEMMINGS; i++) {
        lemmings[i].position[0] = i * -50.0;
        lemmings[i].moveThreshold = 45.0;
        /*lemmings[i].scale[0] = 20.0f;
        lemmings[i].scale[1] = 20.0f;
        lemmings[i].scale[2] = 20.0f;
        lemmings[i].eyes[0].scale[0] = lemmings[i].eyes[0].scale[0] * 0.8;
        lemmings[i].eyes[0].scale[1] = lemmings[i].eyes[0].scale[0] * 0.8;
        lemmings[i].eyes[0].scale[2] = lemmings[i].eyes[0].scale[0] * 0.8;*/
    }
    lemmings[0].moveThreshold = 38.0;
    lemmings[0].scale[0] = lemmings[0].scale[0] * 1.4;
    lemmings[0].scale[1] = lemmings[0].scale[1] * 1.5;
    lemmings[0].scale[2] = lemmings[0].scale[2] * 1.4;
    lemmings[0].eyes[0].scale[0] = lemmings[0].eyes[0].scale[0] * 1.2;
    lemmings[0].eyes[0].scale[1] = lemmings[0].eyes[0].scale[0] * 1.2;
    lemmings[0].eyes[0].scale[2] = lemmings[0].eyes[0].scale[0] * 1.2;
    lemmings[0].eyes[1].scale[0] = lemmings[0].eyes[1].scale[0] * 1.2;
    lemmings[0].eyes[1].scale[1] = lemmings[0].eyes[1].scale[0] * 1.2;
    lemmings[0].eyes[1].scale[2] = lemmings[0].eyes[1].scale[0] * 1.2;
    lemmings[0].eyes[0].position[0] = lemmings[0].eyes[0].position[0] * 1.2;
    lemmings[0].eyes[0].position[1] = lemmings[0].eyes[0].position[1] * 1.2;
    lemmings[0].eyes[0].position[2] = lemmings[0].eyes[0].position[2] * 1.2;
    lemmings[0].eyes[1].position[0] = lemmings[0].eyes[1].position[0] * 1.2;
    lemmings[0].eyes[1].position[1] = lemmings[0].eyes[1].position[1] * 1.2;
    lemmings[0].eyes[1].position[2] = lemmings[0].eyes[1].position[2] * 1.2;


    //init the glowing leader ball
    guideBall = Glowball();

    // Define clear color
    glClearColor(0.0f, 0.0f, 0.0f, QCAR::requiresAlpha() ? 0.0f : 1.0f);
    
    // Now generate the OpenGL texture objects and add settings
    for (int i = 0; i < textureCount; ++i)
    {
        glGenTextures(1, &(textures[i]->mTextureID));
        glBindTexture(GL_TEXTURE_2D, textures[i]->mTextureID);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, textures[i]->mWidth,
                textures[i]->mHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                (GLvoid*)  textures[i]->mData);
    }

    //generate the random fur texture
    textureCount++;

    glGenTextures(1, &furTexId);
    glBindTexture(GL_TEXTURE_2D, furTexId);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    createFurTexture(FURTEXSIZE, 300000, 383832);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D( GL_TEXTURE_2D,
        0,
        GL_RGBA,
        FURTEXSIZE,
        FURTEXSIZE,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        (&furImg)
        );
  
    //default cube shader
    shaderProgramID     = SampleUtils::createProgramFromBuffer(cubeMeshVertexShader,
                                                            cubeFragmentShader);

    vertexHandle        = glGetAttribLocation(shaderProgramID,
                                                "vertexPosition");
    normalHandle        = glGetAttribLocation(shaderProgramID,
                                                "vertexNormal");
    textureCoordHandle  = glGetAttribLocation(shaderProgramID,
                                                "vertexTexCoord");
    mvpMatrixHandle     = glGetUniformLocation(shaderProgramID,
                                                "modelViewProjectionMatrix");


    //Background Shader for transparancy
    bgShaderProgramID        = SampleUtils::createProgramFromBuffer(
                            passThroughVertexShader, passThroughFragmentShader);
    bgVertexPositionHandle              =
            glGetAttribLocation(bgShaderProgramID, "vertexPosition");
    bgVertexTexCoordHandle              =
            glGetAttribLocation(bgShaderProgramID, "vertexTexCoord");
    bgProjectionMatrixHandle            =
            glGetUniformLocation(bgShaderProgramID, "modelViewProjectionMatrix");
    bgTexSamplerVideoHandle             =
            glGetUniformLocation(bgShaderProgramID, "texSamplerVideo");
    SampleUtils::setOrthoMatrix(-1.0, 1.0, -1.0, 1.0, -1.0, 1.0,
                                                            bgOrthoProjMatrix);

    //Fur Shader
    fur_shaderProgramID = SampleUtils::createProgramFromBuffer(fur_vertex_shader, fur_fragment_shader);
    furLength_handle = glGetUniformLocation(fur_shaderProgramID, "furLength");
    fur_vertexHandle = glGetAttribLocation(fur_shaderProgramID, "vertexPosition");
    fur_normalHandle = glGetAttribLocation(fur_shaderProgramID, "vertexNormal");
    fur_textureCoordHandle = glGetAttribLocation(fur_shaderProgramID, "vertexTexCoord");
    fur_mvpMatrixHandle = glGetUniformLocation(fur_shaderProgramID, "modelViewProjectionMatrix");
    fur_layer_handle = glGetUniformLocation(fur_shaderProgramID, "layer");
    fur_force_handle = glGetUniformLocation(fur_shaderProgramID, "force");

    //glow shader
    glow_shaderProgramID = SampleUtils::createProgramFromBuffer(glow_vertex_shader, glow_fragment_shader);
    glow_vertexHandle = glGetAttribLocation(glow_shaderProgramID, "vertexPosition");
    glow_normalHandle = glGetAttribLocation(glow_shaderProgramID, "vertexNormal");
    glow_textureCoordHandle = glGetAttribLocation(glow_shaderProgramID, "vertexTexCoord");
    glow_mvpMatrixHandle = glGetUniformLocation(glow_shaderProgramID, "modelViewProjectionMatrix");
    glow_layer_handle = glGetUniformLocation(glow_shaderProgramID, "layer");
    glow_radius_handle = glGetUniformLocation(glow_shaderProgramID, "radius");
    
}


JNIEXPORT void JNICALL
Java_com_qualcomm_QCARSamples_ImageTargets_ImageTargetsRenderer_updateRendering(
                        JNIEnv* env, jobject obj, jint width, jint height)
{
    LOG("Java_com_qualcomm_QCARSamples_ImageTargets_ImageTargetsRenderer_updateRendering");

    // Update screen dimensions
    screenWidth = width;
    screenHeight = height;

    // Reconfigure the video background
    configureVideoBackground();
}

JNIEXPORT void JNICALL
Java_com_qualcomm_QCARSamples_ImageTargets_ImageTargets_nativeTouchEvent(JNIEnv* , jobject, jint actionType, jint pointerId, jfloat x, jfloat y)
{
    TouchEvent* touchEvent;
    
    // Determine which finger this event represents
    if (pointerId == 0) {
        touchEvent = &touch1;
    } else if (pointerId == 1) {
        touchEvent = &touch2;
    } else {
        return;
    }
    
    if (actionType == ACTION_DOWN) {
        // On touch down, reset the following:
        touchEvent->lastX = x;
        touchEvent->lastY = y;
        touchEvent->startX = x;
        touchEvent->startY = y;
        touchEvent->startTime = getCurrentTimeMS();
        touchEvent->didTap = false;
    } else {
        // Store the last event's position
        touchEvent->lastX = touchEvent->x;
        touchEvent->lastY = touchEvent->y;
    }
    
    // Store the lifetime of the touch, used for tap recognition
    unsigned long time = getCurrentTimeMS();
    touchEvent->dt = time - touchEvent->startTime;
    
    // Store the distance squared from the initial point, for tap recognition
    float dx = touchEvent->lastX - touchEvent->startX;
    float dy = touchEvent->lastY - touchEvent->startY;
    touchEvent->dist2 = dx * dx + dy * dy;
    
    if (actionType == ACTION_UP) {
        // On touch up, this touch is no longer active
        touchEvent->isActive = false;
        
        // Determine if this touch up ends a tap gesture
        // The tap must be quick and localized
        if (touchEvent->dt < MAX_TAP_TIMER && touchEvent->dist2 < MAX_TAP_DISTANCE2) {
            touchEvent->didTap = true;
            touchEvent->tapX = touchEvent->startX;
            touchEvent->tapY = touchEvent->startY;
        }
    } else {
        // On touch down or move, this touch is active
        touchEvent->isActive = true;
    }
    
    // Set the touch information for this event
    touchEvent->actionType = actionType;
    touchEvent->pointerId = pointerId;
    touchEvent->x = x;
    touchEvent->y = y;
}


#ifdef __cplusplus
}
#endif