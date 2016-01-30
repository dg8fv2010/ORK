/*
 * Ork: a small object-oriented OpenGL Rendering Kernel.
 * Copyright (c) 2008-2010 INRIA
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at
 * your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
 */

/*
 * Authors: Eric Bruneton, Antoine Begault, Guillaume Piolat.
 */

#include "ork/render/Texture2D.h"

#include <exception>

#include <GL/glew.h>

#include "ork/render/CPUBuffer.h"
#include "ork/render/FrameBuffer.h"
#include "ork/resource/ResourceTemplate.h"
#include <fstream>
#include "stbi/stb_image.h"
using namespace std;

namespace ork
{

void getParameters(const ptr<ResourceDescriptor> desc, const TiXmlElement *e, TextureInternalFormat &ff, TextureFormat &f, PixelType &t);

void getParameters(const ptr<ResourceDescriptor> desc, const TiXmlElement *e, Texture::Parameters &params);

GLenum getTextureInternalFormat(TextureInternalFormat f);

GLenum getTextureFormat(TextureFormat f);

GLenum getPixelType(PixelType t);

typedef struct tagDDSHeader{
	unsigned int  ddsIdentifier;
	unsigned char junk[8];
	unsigned int  height;
	unsigned int  width;
	unsigned char junk2[8];
	unsigned int  nMipMaps;
	unsigned char junk3[52];
	unsigned int  fourCC;
	unsigned char junk4[40];
} VGE_INNER_S_DDS_HEADER;

Texture2D::Texture2D() : Texture("Texture2D", GL_TEXTURE_2D)
{
}

Texture2D::Texture2D(int w, int h, TextureInternalFormat tf, TextureFormat f, PixelType t,
    const Parameters &params, const Buffer::Parameters &s, const Buffer &pixels) : Texture("Texture2D", GL_TEXTURE_2D)
{
    init(w, h, tf, f, t, params, s, pixels);
}

static unsigned char* load(const string &file, int &size)
{
	ifstream fs(file.c_str(), ios::binary);
	fs.seekg(0, ios::end);
	size = fs.tellg();
	if(size == -1)
		return NULL;
	char *data = new char[size + 1];
	fs.seekg(0);
	fs.read(data, size);
	fs.close();
	data[size] = 0;
	return (unsigned char*) data;
}
Texture2D::Texture2D(char* file, const Parameters &params) : Texture("Texture2D", GL_TEXTURE_2D)
{
	int size;
	int w;
	int h;
	int channels;
	unsigned char* data = load(file, size);
	unsigned char *result = stbi_load_from_memory(data, size, &w, &h, &channels, 0);
	if(!result)
	{
		VGE_INNER_S_DDS_HEADER* pHeader = (VGE_INNER_S_DDS_HEADER*)data;
		if(pHeader)
		{
			w = pHeader->width;
			h = pHeader->height;
		}
		char* bpp = (char*)&pHeader->fourCC;
		TextureInternalFormat tf;
		TextureFormat tof;
		switch(bpp[3])
		{
		case '1':
			tf = COMPRESSED_RGB_S3TC_DXT1_EXT;
			tof = RGB;
			break;
		case  '3':
			tf = COMPRESSED_RGBA_S3TC_DXT3_EXT;
			tof = RGBA;
			break;
		case '5':
			tf = COMPRESSED_RGBA_S3TC_DXT5_EXT;
			tof = RGBA;
			break;
		default:
			tf = COMPRESSED_RGBA_S3TC_DXT3_EXT; 
			tof = RGBA;
		}
		int size = ((w + 3) >> 2) * ((h + 3) >> 2) * ((tf == COMPRESSED_RGB_S3TC_DXT1_EXT)? 8 : 16);	
		unsigned char *pixels = (unsigned char *)(data + sizeof(VGE_INNER_S_DDS_HEADER));
		Buffer::Parameters par = Buffer::Parameters();
		par.compressedSize(size);
		init(w, h, tf, tof, UNSIGNED_BYTE, params, par, CPUBuffer(pixels));
		delete[] data;
	}
	else 
	{
		delete []data;
		int lineSize = w * channels;
		unsigned char* flippedResult = NULL;
		// all formats except 'raw' store the image from top to bottom
		// while OpenGL requires a bottom to top layout; so we revert the
		// order of lines here to get a good orientation in OpenGL
		flippedResult = new unsigned char[lineSize * h];
		int srcOffset = 0;
		int dstOffset = lineSize * (h - 1);
		for (int i = 0; i < h; ++i) {
			memcpy(flippedResult + dstOffset, result + srcOffset, lineSize);
			srcOffset += lineSize;
			dstOffset -= lineSize;
		}
		stbi_image_free(result);
		if(channels == 3)
			init(w, h, RGB8, RGB, UNSIGNED_BYTE, params, Buffer::Parameters(), CPUBuffer(flippedResult));
		else if(channels == 4)
			init(w, h, RGBA8, RGBA, UNSIGNED_BYTE, params, Buffer::Parameters(), CPUBuffer(flippedResult));
		delete []flippedResult;
	}
	
}
void Texture2D::init(int w, int h, TextureInternalFormat tf, TextureFormat f, PixelType t,
    const Parameters &params, const Buffer::Parameters &s, const Buffer &pixels)
{
    Texture::init(tf, params);

    this->w = w;
    this->h = h;

    pixels.bind(GL_PIXEL_UNPACK_BUFFER);

    if (isCompressed() && s.compressedSize() > 0) {
        glCompressedTexImage2D(textureTarget, 0, getTextureInternalFormat(internalFormat), w, h, 0, s.compressedSize(), pixels.data(0));
    } else {
        s.set();
        glTexImage2D(textureTarget, 0, getTextureInternalFormat(internalFormat), w, h, 0, getTextureFormat(f), getPixelType(t), pixels.data(0));
        s.unset();
    }
    pixels.unbind(GL_PIXEL_UNPACK_BUFFER);

    generateMipMap();

    if (FrameBuffer::getError() != 0) {
        throw exception();
    }
}

Texture2D::~Texture2D()
{
}

int Texture2D::getWidth()
{
    return w;
}

int Texture2D::getHeight()
{
    return h;
}

void Texture2D::setImage(int w, int h, TextureFormat f, PixelType t, const Buffer &pixels)
{
    this->w = w;
    this->h = h;
    bindToTextureUnit();
    pixels.bind(GL_PIXEL_UNPACK_BUFFER);
    glTexImage2D(textureTarget, 0, getTextureInternalFormat(internalFormat), w, h, 0, getTextureFormat(f), getPixelType(t), pixels.data(0));
    pixels.unbind(GL_PIXEL_UNPACK_BUFFER);
    generateMipMap();

    assert(FrameBuffer::getError() == GL_NO_ERROR);
}

void Texture2D::setSubImage(int level, int x, int y, int w, int h, TextureFormat f, PixelType t, const Buffer::Parameters &s, const Buffer &pixels)
{
    bindToTextureUnit();
    pixels.bind(GL_PIXEL_UNPACK_BUFFER);
    s.set();
    glTexSubImage2D(textureTarget, level, x, y, w, h, getTextureFormat(f), getPixelType(t), pixels.data(0));
    s.unset();
    pixels.unbind(GL_PIXEL_UNPACK_BUFFER);

    assert(FrameBuffer::getError() == GL_NO_ERROR);
}

void Texture2D::setCompressedSubImage(int level, int x, int y, int w, int h, int s, const Buffer &pixels)
{
    bindToTextureUnit();
    pixels.bind(GL_PIXEL_UNPACK_BUFFER);
    glCompressedTexSubImage2D(textureTarget, level, x, y, w, h, getTextureInternalFormat(internalFormat), s, pixels.data(0));
    pixels.unbind(GL_PIXEL_UNPACK_BUFFER);
    assert(FrameBuffer::getError() == GL_NO_ERROR);
}

void Texture2D::swap(ptr<Texture> t)
{
    Texture::swap(t);
    std::swap(w, t.cast<Texture2D>()->w);
    std::swap(h, t.cast<Texture2D>()->h);
}

/// @cond RESOURCES

class Texture2DResource : public ResourceTemplate<0, Texture2D>
{
public:
    Texture2DResource(ptr<ResourceManager> manager, const string &name, ptr<ResourceDescriptor> desc, const TiXmlElement *e = NULL) :
        ResourceTemplate<0, Texture2D>(manager, name, desc)
    {
        e = e == NULL ? desc->descriptor : e;
        TextureInternalFormat tf;
        TextureFormat f;
        PixelType t;
        Texture::Parameters params;
        Buffer::Parameters s;
        int w;
        int h;
        try {
            checkParameters(desc, e, "name,source,internalformat,format,type,min,mag,wraps,wrapt,maxAniso,width,height,");
            getIntParameter(desc, e, "width", &w);
            getIntParameter(desc, e, "height", &h);
            getParameters(desc, e, tf, f, t);
            getParameters(desc, e, params);
            s.compressedSize(desc->getSize());
            init(w, h, tf, f, t, params, s, CPUBuffer(desc->getData()));
            desc->clearData();
        } catch (...) {
            desc->clearData();
            throw exception();
        }
    }
};

extern const char texture2D[] = "texture2D";

static ResourceFactory::Type<texture2D, Texture2DResource> Texture2DType;

/// @endcond

}
