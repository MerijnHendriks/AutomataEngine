#include <stdio.h>
#include <stdlib.h>
#include "../include/tga/tga.h"

 /* OpenGL texture info */
struct gl_texture_t
{
	GLsizei width;
	GLsizei height;

	GLenum format;
	GLint	internalFormat;
	GLuint id;

	GLubyte *texels;
};

#pragma pack(push, 1)
/* TGA header */
struct tga_header_t
{
	GLubyte id_lenght;          /* size of image id */
	GLubyte colormap_type;      /* 1 is has a colormap */
	GLubyte image_type;         /* compression type */

	short	cm_first_entry;       /* colormap origin */
	short	cm_length;            /* colormap length */
	GLubyte cm_size;            /* colormap size */

	short	x_origin;             /* bottom left x coord origin */
	short	y_origin;             /* bottom left y coord origin */

	short	width;                /* picture width (in pixels) */
	short	height;               /* picture height (in pixels) */

	GLubyte pixel_depth;        /* bits per pixel: 8, 16, 24 or 32 */
	GLubyte image_descriptor;   /* 24 bits = 0x00; 32 bits = 0x80 */
};
#pragma pack(pop)

/* Texture id for the demo */
GLuint texId = 0;


static void
GetTextureInfo(const struct tga_header_t *header,
	struct gl_texture_t *texinfo)
{
	texinfo->width = header->width;
	texinfo->height = header->height;

	switch (header->image_type)
	{
	case 3:  /* Grayscale 8 bits */
	case 11: /* Grayscale 8 bits (RLE) */
	{
		if (header->pixel_depth == 8)
		{
			texinfo->format = GL_RED;
			texinfo->internalFormat = 1;
		}
		else /* 16 bits */
		{
			texinfo->format = GL_RG;
			texinfo->internalFormat = 2;
		}

		break;
	}

	case 1:  /* 8 bits color index */
	case 2:  /* BGR 16-24-32 bits */
	case 9:  /* 8 bits color index (RLE) */
	case 10: /* BGR 16-24-32 bits (RLE) */
	{
		/* 8 bits and 16 bits images will be converted to 24 bits */
		if (header->pixel_depth <= 24)
		{
			texinfo->format = GL_RGB;
			texinfo->internalFormat = 3;
		}
		else /* 32 bits */
		{
			texinfo->format = GL_RGBA;
			texinfo->internalFormat = 4;
		}

		break;
	}
	}
}

static void
ReadTGA8bits(FILE *fp, const GLubyte *colormap,
	struct gl_texture_t *texinfo)
{
	int i;
	GLubyte color;

	for (i = 0; i < texinfo->width * texinfo->height; ++i)
	{
		/* Read index color byte */
		color = (GLubyte)fgetc(fp);

		/* Convert to RGB 24 bits */
		texinfo->texels[(i * 3) + 2] = colormap[(color * 3) + 0];
		texinfo->texels[(i * 3) + 1] = colormap[(color * 3) + 1];
		texinfo->texels[(i * 3) + 0] = colormap[(color * 3) + 2];
	}
}

static void
ReadTGA16bits(FILE *fp, struct gl_texture_t *texinfo)
{
	int i;
	unsigned short color;

	for (i = 0; i < texinfo->width * texinfo->height; ++i)
	{
		/* Read color word */
		color = fgetc(fp) + (fgetc(fp) << 8);

		/* Convert BGR to RGB */
		texinfo->texels[(i * 3) + 0] = (GLubyte)(((color & 0x7C00) >> 10) << 3);
		texinfo->texels[(i * 3) + 1] = (GLubyte)(((color & 0x03E0) >> 5) << 3);
		texinfo->texels[(i * 3) + 2] = (GLubyte)(((color & 0x001F) >> 0) << 3);
	}
}

static void
ReadTGA24bits(FILE *fp, struct gl_texture_t *texinfo)
{
	int i;

	for (i = 0; i < texinfo->width * texinfo->height; ++i)
	{
		/* Read and convert BGR to RGB */
		texinfo->texels[(i * 3) + 2] = (GLubyte)fgetc(fp);
		texinfo->texels[(i * 3) + 1] = (GLubyte)fgetc(fp);
		texinfo->texels[(i * 3) + 0] = (GLubyte)fgetc(fp);
	}
}

static void
ReadTGA32bits(FILE *fp, struct gl_texture_t *texinfo)
{
	int i;

	for (i = 0; i < texinfo->width * texinfo->height; ++i)
	{
		/* Read and convert BGRA to RGBA */
		texinfo->texels[(i * 4) + 2] = (GLubyte)fgetc(fp);
		texinfo->texels[(i * 4) + 1] = (GLubyte)fgetc(fp);
		texinfo->texels[(i * 4) + 0] = (GLubyte)fgetc(fp);
		texinfo->texels[(i * 4) + 3] = (GLubyte)fgetc(fp);
	}
}

static void
ReadTGAgray8bits(FILE *fp, struct gl_texture_t *texinfo)
{
	int i;

	for (i = 0; i < texinfo->width * texinfo->height; ++i)
	{
		/* Read grayscale color byte */
		texinfo->texels[i] = (GLubyte)fgetc(fp);
	}
}

static void
ReadTGAgray16bits(FILE *fp, struct gl_texture_t *texinfo)
{
	int i;

	for (i = 0; i < texinfo->width * texinfo->height; ++i)
	{
		/* Read grayscale color + alpha channel bytes */
		texinfo->texels[(i * 2) + 0] = (GLubyte)fgetc(fp);
		texinfo->texels[(i * 2) + 1] = (GLubyte)fgetc(fp);
	}
}

static void
ReadTGA8bitsRLE(FILE *fp, const GLubyte *colormap,
	struct gl_texture_t *texinfo)
{
	int i, size;
	GLubyte color;
	GLubyte packet_header;
	GLubyte *ptr = texinfo->texels;

	while (ptr < texinfo->texels + (texinfo->width * texinfo->height) * 3)
	{
		/* Read first byte */
		packet_header = (GLubyte)fgetc(fp);
		size = 1 + (packet_header & 0x7f);

		if (packet_header & 0x80)
		{
			/* Run-length packet */
			color = (GLubyte)fgetc(fp);

			for (i = 0; i < size; ++i, ptr += 3)
			{
				ptr[0] = colormap[(color * 3) + 2];
				ptr[1] = colormap[(color * 3) + 1];
				ptr[2] = colormap[(color * 3) + 0];
			}
		}
		else
		{
			/* Non run-length packet */
			for (i = 0; i < size; ++i, ptr += 3)
			{
				color = (GLubyte)fgetc(fp);

				ptr[0] = colormap[(color * 3) + 2];
				ptr[1] = colormap[(color * 3) + 1];
				ptr[2] = colormap[(color * 3) + 0];
			}
		}
	}
}

static void
ReadTGA16bitsRLE(FILE *fp, struct gl_texture_t *texinfo)
{
	int i, size;
	unsigned short color;
	GLubyte packet_header;
	GLubyte *ptr = texinfo->texels;

	while (ptr < texinfo->texels + (texinfo->width * texinfo->height) * 3)
	{
		/* Read first byte */
		packet_header = fgetc(fp);
		size = 1 + (packet_header & 0x7f);

		if (packet_header & 0x80)
		{
			/* Run-length packet */
			color = fgetc(fp) + (fgetc(fp) << 8);

			for (i = 0; i < size; ++i, ptr += 3)
			{
				ptr[0] = (GLubyte)(((color & 0x7C00) >> 10) << 3);
				ptr[1] = (GLubyte)(((color & 0x03E0) >> 5) << 3);
				ptr[2] = (GLubyte)(((color & 0x001F) >> 0) << 3);
			}
		}
		else
		{
			/* Non run-length packet */
			for (i = 0; i < size; ++i, ptr += 3)
			{
				color = fgetc(fp) + (fgetc(fp) << 8);

				ptr[0] = (GLubyte)(((color & 0x7C00) >> 10) << 3);
				ptr[1] = (GLubyte)(((color & 0x03E0) >> 5) << 3);
				ptr[2] = (GLubyte)(((color & 0x001F) >> 0) << 3);
			}
		}
	}
}

static void
ReadTGA24bitsRLE(FILE *fp, struct gl_texture_t *texinfo)
{
	int i, size;
	GLubyte rgb[3];
	GLubyte packet_header;
	GLubyte *ptr = texinfo->texels;

	while (ptr < texinfo->texels + (texinfo->width * texinfo->height) * 3)
	{
		/* Read first byte */
		packet_header = (GLubyte)fgetc(fp);
		size = 1 + (packet_header & 0x7f);

		if (packet_header & 0x80)
		{
			/* Run-length packet */
			fread(rgb, sizeof(GLubyte), 3, fp);

			for (i = 0; i < size; ++i, ptr += 3)
			{
				ptr[0] = rgb[2];
				ptr[1] = rgb[1];
				ptr[2] = rgb[0];
			}
		}
		else
		{
			/* Non run-length packet */
			for (i = 0; i < size; ++i, ptr += 3)
			{
				ptr[2] = (GLubyte)fgetc(fp);
				ptr[1] = (GLubyte)fgetc(fp);
				ptr[0] = (GLubyte)fgetc(fp);
			}
		}
	}
}

static void
ReadTGA32bitsRLE(FILE *fp, struct gl_texture_t *texinfo)
{
	int i, size;
	GLubyte rgba[4];
	GLubyte packet_header;
	GLubyte *ptr = texinfo->texels;

	while (ptr < texinfo->texels + (texinfo->width * texinfo->height) * 4)
	{
		/* Read first byte */
		packet_header = (GLubyte)fgetc(fp);
		size = 1 + (packet_header & 0x7f);

		if (packet_header & 0x80)
		{
			/* Run-length packet */
			fread(rgba, sizeof(GLubyte), 4, fp);

			for (i = 0; i < size; ++i, ptr += 4)
			{
				ptr[0] = rgba[2];
				ptr[1] = rgba[1];
				ptr[2] = rgba[0];
				ptr[3] = rgba[3];
			}
		}
		else
		{
			/* Non run-length packet */
			for (i = 0; i < size; ++i, ptr += 4)
			{
				ptr[2] = (GLubyte)fgetc(fp);
				ptr[1] = (GLubyte)fgetc(fp);
				ptr[0] = (GLubyte)fgetc(fp);
				ptr[3] = (GLubyte)fgetc(fp);
			}
		}
	}
}

static void
ReadTGAgray8bitsRLE(FILE *fp, struct gl_texture_t *texinfo)
{
	int i, size;
	GLubyte color;
	GLubyte packet_header;
	GLubyte *ptr = texinfo->texels;

	while (ptr < texinfo->texels + (texinfo->width * texinfo->height))
	{
		/* Read first byte */
		packet_header = (GLubyte)fgetc(fp);
		size = 1 + (packet_header & 0x7f);

		if (packet_header & 0x80)
		{
			/* Run-length packet */
			color = (GLubyte)fgetc(fp);

			for (i = 0; i < size; ++i, ptr++)
				*ptr = color;
		}
		else
		{
			/* Non run-length packet */
			for (i = 0; i < size; ++i, ptr++)
				*ptr = (GLubyte)fgetc(fp);
		}
	}
}

static void
ReadTGAgray16bitsRLE(FILE *fp, struct gl_texture_t *texinfo)
{
	int i, size;
	GLubyte color, alpha;
	GLubyte packet_header;
	GLubyte *ptr = texinfo->texels;

	while (ptr < texinfo->texels + (texinfo->width * texinfo->height) * 2)
	{
		/* Read first byte */
		packet_header = (GLubyte)fgetc(fp);
		size = 1 + (packet_header & 0x7f);

		if (packet_header & 0x80)
		{
			/* Run-length packet */
			color = (GLubyte)fgetc(fp);
			alpha = (GLubyte)fgetc(fp);

			for (i = 0; i < size; ++i, ptr += 2)
			{
				ptr[0] = color;
				ptr[1] = alpha;
			}
		}
		else
		{
			/* Non run-length packet */
			for (i = 0; i < size; ++i, ptr += 2)
			{
				ptr[0] = (GLubyte)fgetc(fp);
				ptr[1] = (GLubyte)fgetc(fp);
			}
		}
	}
}

static struct gl_texture_t *
ReadTGAFile(const char *filename)
{
	FILE *fp;
	struct gl_texture_t *texinfo;
	struct tga_header_t header;
	GLubyte *colormap = NULL;

	fp = fopen(filename, "rb");
	if (!fp)
	{
		fprintf(stderr, "error: couldn't open \"%s\"!\n", filename);
		return NULL;
	}

	/* Read header */
	fread(&header, sizeof(struct tga_header_t), 1, fp);

	texinfo = (struct gl_texture_t *)
		malloc(sizeof(struct gl_texture_t));
	GetTextureInfo(&header, texinfo);
	fseek(fp, header.id_lenght, SEEK_CUR);

	/* Memory allocation */
	texinfo->texels = (GLubyte *)malloc(sizeof(GLubyte) *
		texinfo->width * texinfo->height * texinfo->internalFormat);
	if (!texinfo->texels)
	{
		free(texinfo);
		return NULL;
	}

	/* Read color map */
	if (header.colormap_type)
	{
		/* NOTE: color map is stored in BGR format */
		colormap = (GLubyte *)malloc(sizeof(GLubyte)
			* header.cm_length * (header.cm_size >> 3));
		fread(colormap, sizeof(GLubyte), header.cm_length
			* (header.cm_size >> 3), fp);
	}

	/* Read image data */
	switch (header.image_type)
	{
	case 0:
		/* No data */
		break;

	case 1:
		/* Uncompressed 8 bits color index */
		ReadTGA8bits(fp, colormap, texinfo);
		break;

	case 2:
		/* Uncompressed 16-24-32 bits */
		switch (header.pixel_depth)
		{
		case 16:
			ReadTGA16bits(fp, texinfo);
			break;

		case 24:
			ReadTGA24bits(fp, texinfo);
			break;

		case 32:
			ReadTGA32bits(fp, texinfo);
			break;
		}

		break;

	case 3:
		/* Uncompressed 8 or 16 bits grayscale */
		if (header.pixel_depth == 8)
			ReadTGAgray8bits(fp, texinfo);
		else /* 16 */
			ReadTGAgray16bits(fp, texinfo);

		break;

	case 9:
		/* RLE compressed 8 bits color index */
		ReadTGA8bitsRLE(fp, colormap, texinfo);
		break;

	case 10:
		/* RLE compressed 16-24-32 bits */
		switch (header.pixel_depth)
		{
		case 16:
			ReadTGA16bitsRLE(fp, texinfo);
			break;

		case 24:
			ReadTGA24bitsRLE(fp, texinfo);
			break;

		case 32:
			ReadTGA32bitsRLE(fp, texinfo);
			break;
		}

		break;

	case 11:
		/* RLE compressed 8 or 16 bits grayscale */
		if (header.pixel_depth == 8)
			ReadTGAgray8bitsRLE(fp, texinfo);
		else /* 16 */
			ReadTGAgray16bitsRLE(fp, texinfo);

		break;

	default:
		/* Image type is not correct */
		fprintf(stderr, "error: unknown TGA image type %i!\n", header.image_type);
		free(texinfo->texels);
		free(texinfo);
		texinfo = NULL;
		break;
	}

	/* No longer need colormap data */
	if (colormap)
		free(colormap);

	fclose(fp);
	return texinfo;
}

GLuint
loadTGATexture(const char *filename)
{
	struct gl_texture_t *tga_tex = NULL;
	GLuint tex_id = 0;
	GLint alignment;

	tga_tex = ReadTGAFile(filename);

	if (tga_tex && tga_tex->texels)
	{
		/* Generate texture */
		glGenTextures(1, &tga_tex->id);
		glBindTexture(GL_TEXTURE_2D, tga_tex->id);

		/* Setup some parameters for texture filters and mipmapping */
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		glGetIntegerv(GL_UNPACK_ALIGNMENT, &alignment);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

		glTexImage2D(GL_TEXTURE_2D, 0, tga_tex->internalFormat,
		tga_tex->width, tga_tex->height, 0, tga_tex->format,
		GL_UNSIGNED_BYTE, tga_tex->texels);
		glGenerateMipmap(GL_TEXTURE_2D);

		glPixelStorei(GL_UNPACK_ALIGNMENT, alignment);

		tex_id = tga_tex->id;

		/* OpenGL has its own copy of texture data */
		free(tga_tex->texels);
		free(tga_tex);
	}

	return tex_id;
}
