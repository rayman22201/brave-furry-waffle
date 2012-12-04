//Implementation of class FurTexture
//Ray Imber

// Include files
#include "FurTexture.h"
#include "SampleUtils.h"

#include <string.h>

/// Constructor
FurTexture::FurTexture() {
	this->texSize = 0;
	this->numLayers = 0;
	this->density = 0;
	this->rndSeed = 0;
}

FurTexture::FurTexture(unsigned int size, unsigned int layers, unsigned int density, unsigned int seed) {
	this->texSize = size;
	this->numLayers = layers;
	this->density = density;
	this->rndSeed = seed;

	//allocate space for the gl texture ids
	this->texIDs = new GLuint [this->numLayers];
	//allocated space for the texture data
	//unsigned int arrSize = this->numLayers * this->texSize * this->texSize * 4;
	this->texData = new GLubyte***[this->numLayers];
	for(int i = 0; i < this->numLayers; i++) {
		this->texData[i] = new GLubyte**[this->texSize];
		for(int j = 0 ; j < this->texSize; j++) {
			this->texData[i][j] = new GLubyte*[this->texSize];
			for(int k = 0; k < this->texSize; k++) {
				this->texData[i][j][k] = new GLubyte[4];
				/*for (int l = 0; l < 4; l++) {
					this->texData[i][j][k][l] = 0;
				}*/
			}
		}
	}

	//generate the layers
	this->generateAllLayers();
}

/// Destructor.
FurTexture::~FurTexture() {
	//delete the ids
	delete [] this->texIDs;

	//delete the texture data
	for(int i = 0; i < this->numLayers; i++) {
		for(int j = 0 ; j < this->texSize; j++) {
			for(int k = 0; k < this->texSize; k++) {
				delete [] this->texData[i][j][k];
			}
			delete [] this->texData[i][j];
		}
		delete [] this->texData[i];
	}
	delete [] this->texData;
}

//generate the actual pixel data
void FurTexture::generatePattern(unsigned int layeri) {
    srand(this->rndSeed);

    for (GLuint x = 0; x < this->texSize; x++) {
        for (GLuint y = 0; y < this->texSize; y++) {
            //set every pixel to black and transparant
            this->texData[layeri][x][y][0] = 0;
            this->texData[layeri][y][1] = 0;
            this->texData[layeri][x][y][2] = 0;
            this->texData[layeri][x][y][3] = 0;
        }
    }

    float length = float(layeri)/this->numLayers;
    float scale = length;
    if(scale < 0.1)
    	scale = 0.1;
	
	length = 1 - length; // 1 to 0
	GLuint layerDensity = this->density * length * 3;

    for (GLuint myDensity = 0; myDensity < layerDensity; myDensity++) {
        int xrand = (rand() % this->texSize);
        int yrand = (rand() % this->texSize);
        
        this->texData[layeri][xrand][yrand][0] = 0;
        this->texData[layeri][xrand][yrand][1] = 0;
        this->texData[layeri][xrand][yrand][2] = int( (scale * 255.0) );
        this->texData[layeri][xrand][yrand][3] = int( (length * 255.0) );
    }
}

//creates one layer
void FurTexture::generateLayer(unsigned int layeri) {
	//generate the gl texture id
	glGenTextures(1, &(this->texIDs[layeri]) );
    glBindTexture(GL_TEXTURE_2D, this->texIDs[layeri]);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	
	//generate the actual texture data
	this->generatePattern(layeri);

	//bind the texture to the buffer
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D,
        0,
        GL_RGBA,
        this->texSize,
        this->texSize,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        (&(this->texData[layeri]) )
        );
}

//creates all the layers
void FurTexture::generateAllLayers() {
	for(unsigned int i = 0; i < this->numLayers; i++) {
		this->generateLayer(i);
	}
}