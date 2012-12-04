//Utility for dealing with multilayerd fur textures
//
// By: Ray Imber
// 11/29/2012
#ifndef _FUR_TEXTURE_H_
#define _FUR_TEXTURE_H_

// Include files
#include <jni.h>

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <stdlib.h> //so I can use rand


class FurTexture
{
public:
	unsigned int texSize;
	unsigned int numLayers;
	unsigned int density;
	unsigned int rndSeed;
	GLuint *texIDs;
	GLubyte ****texData; //A crazy 4d array. Represents an array of raw image data, [num_images][img_size][img_size][4:rgba]

	/// Constructor
	FurTexture();
    FurTexture(unsigned int size, unsigned int layers, unsigned int density, unsigned int seed);

    /// Destructor.
    ~FurTexture();

    //creates one layer
    void generateLayer(unsigned int layeri);

    //generates the actual pixels
    void generatePattern(unsigned int layeri);

    //creates all the layers
    void generateAllLayers();
};

#endif