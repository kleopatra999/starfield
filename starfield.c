/*
 * starfield.c
 *
 * Starfield
 *
 * Copyright (C) 2004 Mark Probst
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifdef QUICKTIME
#include <QuickTime/QuickTime.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>

#include "api.h"

// Size of the output images/movie in pixels
#define OUTPUT_WIDTH              720
#define OUTPUT_HEIGHT             576

// The number of frames per second for QuickTime movies
#define FPS                       25

// The number of frames to calculate
#define NUM_FRAMES                (FPS * 5)

// The codec to use for the QuickTime movie
#define QUICKTIME_CODEC           kDVCPALCodecType

// The number of stars in the star cylinder (see below for details on
// the latter).
#define NUM_STARS                 500

// The speed with which the viewer travels though the star field.
// Higher is faster.
#define SPEED                     0.5

// Internally, a bigger image is calculated and then scaled down to
// the actual output size.  This first makes it possible to assume
// square pixels while the output may still have a non-square aspect
// ratio, and it gives us subpixel accuracy without really having to
// implement it.
//
// These two define the size of this intermediate, internal image.  If
// your output format has non-square pixels (like NTSC or PAL), you'll
// have to use a multiple of the squared size here.
#define FULL_WIDTH                (768 * 4)
#define FULL_HEIGHT               (576 * 4)

// The maximum size of a star in the intermediate image in pixels.
// This must be less than the circle size below (32 by default).
#define MAX_STAR_SIZE             20

// The stars are confined to a cylindrical area and they travel along
// the length of it.  The stars that travel out of the cylinder on the
// far side come in again at the near side.  (You can either think of
// the movement as the viewer travelling along an infinte cylinder
// with a repeated pattern of stars and a finite drawing distance or
// as the viewer staying put and the stars moving across the
// cylinder).
//
// These two define the diameter and the length of the cylinder.
#define CYLINDER_DIAMETER         1000
#define CYLINDER_LENGTH           1500

// Actually, the stars are not everywhere in the cylinder.  There is a
// smaller cylindrical area along the whole length of the cylinder
// where there are no stars, to prevent the stars from coming too
// close to the viewer.  This defines its diameter.  It must of course
// be smaller than the diameter of the star field cylinder above.
#define CORRIDOR_DIAMETER         100

// Optical parameters.  Experiment if you like.
#define VIEWPORT_DISTANCE         10.0
#define VIEWPORT_WIDTH            10.0

// We don't draw circles with antialiasing.  Instead we draw one big
// circle and then scale it down to get the antialiasing.  This gives
// the size of that circle and also the size of the image it is drawn
// in.  The bigger this image, the more accuracy we have in scaling
// down the circle.  Making it more than about 8 times as big as the
// circle is probably excessive overkill, though.
#define CIRCLE_CANVAS_SIZE        128
#define CIRCLE_SIZE               32

// This assumes that the intermediate image is wider than it is high.
// If this is not the case, exchange WIDTH and HEIGHT in all three
// names on the right side.
#define VIEWPORT_HEIGHT           (VIEWPORT_WIDTH * FULL_HEIGHT / FULL_WIDTH)

bitmap_t *circle;

typedef struct
{
    float x;
    float y;
    float z;
    float size;
} star_t;

star_t stars[NUM_STARS];

void
bitmap_set_pixel (bitmap_t *bitmap, unsigned int x, unsigned int y, unsigned char r, unsigned char g, unsigned char b)
{
    assert(bitmap->data != 0);
    assert(x < bitmap->width && y < bitmap->height);

    bitmap->data[y * bitmap->row_stride + x * bitmap->pixel_stride + 0] = r;
    bitmap->data[y * bitmap->row_stride + x * bitmap->pixel_stride + 1] = g;
    bitmap->data[y * bitmap->row_stride + x * bitmap->pixel_stride + 2] = b;
}

void
bitmap_add (bitmap_t *dest, bitmap_t *src)
{
    unsigned int x, y, i;

    assert(dest->width == src->width && dest->height == src->height);

    for (y = 0; y < dest->height; ++y)
	for (x = 0; x < dest->width; ++x)
	    for (i = 0; i < 3; ++i)
	    {
		unsigned int v1 = dest->data[y * dest->row_stride + x * dest->pixel_stride + i];
		unsigned int v2 = src->data[y * src->row_stride + x * src->pixel_stride + i];
		unsigned int v = v1 + v2;

		if (v > 255)
		    v = 255;

		dest->data[y * dest->row_stride + x * dest->pixel_stride + i] = v;
	    }
}

void
bitmap_add_with_crop (bitmap_t *dest, bitmap_t *src, int x, int y)
{
    unsigned int sub_x, sub_y;
    unsigned int sub_width, sub_height;
    bitmap_t *cropped_src, *cropped_dest;

    if (x + src->width <= 0 || y + src->height <= 0
	|| x >= dest->width || y >= dest->height)
	return;

    if (x >= 0)
	sub_x = 0;
    else
	sub_x = -x;
    if (y >= 0)
	sub_y = 0;
    else
	sub_y = -y;
    if (x + src->width < dest->width)
	sub_width = src->width - sub_x;
    else
	sub_width = dest->width - x - sub_x;
    if (y + src->height < dest->height)
	sub_height = src->height - sub_y;
    else
	sub_height = dest->height - y - sub_y;

    cropped_src = bitmap_sub(src, sub_x, sub_y, sub_width, sub_height);
    cropped_dest = bitmap_sub(dest, x + sub_x, y + sub_y, sub_width, sub_height);

    bitmap_add(cropped_dest, cropped_src);

    bitmap_free(cropped_src);
    bitmap_free(cropped_dest);
}

void
init_circle (void)
{
    int x, y;

    circle = bitmap_new_empty(COLOR_RGB_8, CIRCLE_CANVAS_SIZE, CIRCLE_CANVAS_SIZE);
    memset(circle->data, 0, circle->row_stride * CIRCLE_CANVAS_SIZE);

    for (y = 0; y < CIRCLE_SIZE; ++y)
	for (x = 0; x < CIRCLE_SIZE; ++x)
	{
	    float circle_x = ((float)x + 0.5) / (float)CIRCLE_SIZE * 2.0 - 1.0;
	    float circle_y = ((float)y + 0.5) / (float)CIRCLE_SIZE * 2.0 - 1.0;
	    unsigned int bitmap_x = CIRCLE_CANVAS_SIZE / 2 - CIRCLE_SIZE / 2 + x;
	    unsigned int bitmap_y = CIRCLE_CANVAS_SIZE / 2 - CIRCLE_SIZE / 2 + y;
	    unsigned char gray;

	    assert(circle_x > -1.0 && circle_x < 1.0);
	    assert(circle_y > -1.0 && circle_y < 1.0);

	    if (circle_x * circle_x + circle_y * circle_y <= 1.0)
		gray = 255;
	    else
		gray = 0;

	    bitmap_set_pixel(circle, bitmap_x, bitmap_y, gray, gray, gray);
	}
}

bitmap_t*
scale_circle (float size)
{
    bitmap_t *scaled;

    if (size <= 1.0)
    {
	unsigned char gray = size * 255.0;

	scaled = bitmap_new_empty(COLOR_RGB_8, 1, 1);
	bitmap_set_pixel(scaled, 0, 0, gray, gray, gray);
    }
    else
    {
	float canvas_ideal_pixel_size = CIRCLE_CANVAS_SIZE * (size / CIRCLE_SIZE);
	unsigned int canvas_real_pixel_size = floorf(canvas_ideal_pixel_size);
	unsigned int canvas_sub_size = CIRCLE_CANVAS_SIZE * ((float)canvas_real_pixel_size / canvas_ideal_pixel_size);
	unsigned int canvas_border = (CIRCLE_CANVAS_SIZE - canvas_sub_size) / 2;
	bitmap_t *canvas_sub;

	assert(canvas_sub_size >= CIRCLE_SIZE);

	canvas_sub = bitmap_sub(circle, canvas_border, canvas_border, canvas_sub_size, canvas_sub_size);

	scaled = bitmap_scale(canvas_sub, canvas_real_pixel_size, canvas_real_pixel_size, FILTER_TRIANGLE);

	bitmap_free(canvas_sub);
    }

    return scaled;
}

void
init_stars (void)
{
    int i;

    for (i = 0; i < NUM_STARS; ++i)
    {
	float distance = (float)(random() % ((CYLINDER_DIAMETER - CORRIDOR_DIAMETER) * 500)
				 + CORRIDOR_DIAMETER * 500) / 1000.0;
	float angle = (float)(random() % (360 * 1000)) / 1000.0 / 180.0 * M_PI;
	float z = (float)(random() % (CYLINDER_LENGTH * 1000)) / 1000.0;
	float size = (float)(random() % (MAX_STAR_SIZE * 1000)) / 1000.0;

	stars[i].x = distance * cosf(angle);
	stars[i].y = distance * sinf(angle);
	stars[i].z = z;
	stars[i].size = size;
    }
}

bitmap_t*
render_frame (int frame)
{
    float ndz = VIEWPORT_DISTANCE * CORRIDOR_DIAMETER / VIEWPORT_HEIGHT;
    float nd = sqrtf(ndz * ndz + CORRIDOR_DIAMETER * CORRIDOR_DIAMETER / 4.0);
    float a = MAX_STAR_SIZE * nd;
    float pos = fmodf(frame * SPEED, CYLINDER_LENGTH);
    bitmap_t *canvas = bitmap_new_empty(COLOR_RGB_8, FULL_WIDTH, FULL_HEIGHT);
    bitmap_t *out;
    int i;

    memset(canvas->data, 0, canvas->row_stride * FULL_HEIGHT);

    for (i = 0; i < NUM_STARS; ++i)
    {
	float x = stars[i].x, y = stars[i].y;
	float z = (stars[i].z >= pos) ? (stars[i].z - CYLINDER_LENGTH) : stars[i].z;
	float dz = pos - z + VIEWPORT_DISTANCE;
	float rx = x * VIEWPORT_DISTANCE / dz;
	float ry = y * VIEWPORT_DISTANCE / dz;
	float d = sqrtf(x * x + y * y + dz * dz);
	float vx = rx / VIEWPORT_WIDTH * FULL_WIDTH / 2.0 + FULL_WIDTH / 2.0;
	float vy = ry / VIEWPORT_HEIGHT * FULL_HEIGHT / 2.0 + FULL_HEIGHT / 2.0;

	if (vx >= -MAX_STAR_SIZE / 2.0 && vy >= -MAX_STAR_SIZE / 2.0
	    && vx <= FULL_WIDTH + MAX_STAR_SIZE / 2.0
	    && vy <= FULL_HEIGHT + MAX_STAR_SIZE / 2.0)
	{
	    float size = a / d;
	    bitmap_t *star = scale_circle(size);

	    bitmap_add_with_crop(canvas, star, vx - star->width / 2.0, vy - star->width / 2.0);

	    bitmap_free(star);
	}
    }

    out = bitmap_scale(canvas, OUTPUT_WIDTH, OUTPUT_HEIGHT, FILTER_TRIANGLE);

    bitmap_free(canvas);

    return out;
}

#ifdef QUICKTIME
int
make_fsspec_from_filename (char *name, FSSpec *spec)
{
    CFURLRef urlref;
    FSRef fsref;
    int fd;

    fd = open(name, O_WRONLY | O_CREAT, 0666);
    if (fd == -1)
        return 0;
    close(fd);

    urlref = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, CFStringCreateWithCString(NULL, name, CFStringGetSystemEncoding()), kCFURLPOSIXPathStyle, FALSE);
    if (urlref == NULL)
        return 0;
    //printf("%s\n", CFStringGetCStringPtr(CFURLGetString(urlref), CFStringGetSystemEncoding()));
    if (!CFURLGetFSRef(urlref, &fsref))
        return 0;
    FSGetCatalogInfo(&fsref, kFSCatInfoNone, NULL, NULL, spec, NULL);

    unlink(name);

    return 1;
}

static StringPtr QTUtils_ConvertCToPascalString (char *theString)
{
	StringPtr	myString = malloc(strlen(theString) + 1);
	short		myIndex = 0;

	while (theString[myIndex] != '\0') {
		myString[myIndex + 1] = theString[myIndex];
		myIndex++;
	}
	
	myString[0] = (unsigned char)myIndex;
	
	return(myString);
}

void
run (void)
{
    Rect trackFrame = { 0, 0, OUTPUT_HEIGHT, OUTPUT_WIDTH };
    Movie theMovie = nil;
    FSSpec mySpec;
    short resRefNum = 0;
    short resId = movieInDataForkResID;
    OSErr err = noErr;

    EnterMovies();

    if (!make_fsspec_from_filename("out.mov", &mySpec))
        return;

    err = CreateMovieFile (&mySpec, 
			   FOUR_CHAR_CODE('TVOD'),
			   smCurrentScript, 
			   createMovieFileDeleteCurFile | createMovieFileDontCreateResFile,
			   &resRefNum, 
			   &theMovie );
    if (err != noErr || theMovie == nil)
        return;

    {
	Track theTrack;
	Media theMedia;

	// 1. Create the track
	theTrack = NewMovieTrack (theMovie, 		/* movie specifier */
				  FixRatio(OUTPUT_WIDTH,1),  /* width */
				  FixRatio(OUTPUT_HEIGHT,1), /* height */
				  kNoVolume);  /* trackVolume */
	if (GetMoviesError() != noErr)
            return;

	// 2. Create the media for the track
	theMedia = NewTrackMedia (theTrack,		/* track identifier */
				  VideoMediaType,		/* type of media */
				  FPS, 	/* time coordinate system */
				  nil,	/* data reference - use the file that is associated with the movie  */
				  0);		/* data reference type */
	if (GetMoviesError() != noErr)
            return;

	// 3. Establish a media-editing session
	err = BeginMediaEdits (theMedia);
	if (err != noErr)
            return;

	// 3a. Add Samples to the media
	{
	    GWorldPtr theGWorld = nil;
	    long maxCompressedSize;
	    long frame;
	    Handle compressedData = nil;
	    Ptr compressedDataPtr;
	    ImageDescriptionHandle imageDesc = nil;
	    CGrafPtr oldPort;
	    GDHandle oldGDeviceH;
            
	    // Create a graphics world
	    err = NewGWorld (&theGWorld,	/* pointer to created gworld */	
			     32,		/* pixel depth */
			     &trackFrame, 		/* bounds */
			     nil, 			/* color table */
			     nil,			/* handle to GDevice */ 
			     (GWorldFlags)0);	/* flags */
	    if (err != noErr)
                return;
        
	    // Lock the pixels
	    LockPixels (GetGWorldPixMap(theGWorld)/*GetPortPixMap(theGWorld)*/);
        
	    // Determine the maximum size the image will be after compression.
	    // Specify the compression characteristics, along with the image.
	    err = GetMaxCompressionSize(GetGWorldPixMap(theGWorld),		/* Handle to the source image */
					&trackFrame, 				/* bounds */
					32, 					/* let ICM choose depth */
					codecNormalQuality,				/* desired image quality */ 
					kAnimationCodecType,			/* compressor type */ 
					(CompressorComponent)anyCodec,  		/* compressor identifier */
					&maxCompressedSize);		    	/* returned size */
	    if (err != noErr)
                return;
        
	    // Create a new handle of the right size for our compressed image data
	    compressedData = NewHandle(maxCompressedSize);
	    if (MemError() != noErr)
                return;
        
	    MoveHHi( compressedData );
	    HLock( compressedData );
	    compressedDataPtr = *compressedData;
        
	    // Create a handle for the Image Description Structure
	    imageDesc = (ImageDescriptionHandle)NewHandle(4);
	    if (MemError() != noErr)
                return;
        
	    // Change the current graphics port to the GWorld
	    GetGWorld(&oldPort, &oldGDeviceH);
	    SetGWorld(theGWorld, nil);
        
	    // For each sample...
	    for (frame = 0; frame < NUM_FRAMES; ++frame)
	    {
		bitmap_t *image = render_frame(frame);

		// do the drawing of our image
		{
		    PixMapHandle 	pixMapHandle;
		    Ptr 		pixBaseAddr;
                    
		    // Lock the pixels
		    pixMapHandle = GetGWorldPixMap(theGWorld);
		    LockPixels (pixMapHandle);
		    pixBaseAddr = GetPixBaseAddr(pixMapHandle);

		    assert(pixBaseAddr != nil);

		    {
			int i,j;
			int pixmapRowBytes = GetPixRowBytes(pixMapHandle);
                            
			for (i=0; i< OUTPUT_HEIGHT; i++)
			{
			    unsigned char *src = image->data + i * image->row_stride;
			    unsigned char *dst = pixBaseAddr + i * pixmapRowBytes;
			    for (j = 0; j < OUTPUT_WIDTH; j++)
			    {
				*dst++ = 0;		// X - our src is 24-bit only
				*dst++ = src[0];	// Red component
				*dst++ = src[1];	// Green component
				*dst++ = src[2];	// Blue component

				src += image->pixel_stride;
			    }
			}
		    }

		    UnlockPixels(pixMapHandle);
		}

		bitmap_free(image);
        
		// Use the ICM to compress the image 
		err = CompressImage(GetGWorldPixMap(theGWorld),	/* source image to compress */
				    &trackFrame, 		/* bounds */
				    codecMaxQuality,	/* desired image quality */
				    QUICKTIME_CODEC,	/* compressor identifier */
				    imageDesc, 		/* handle to Image Description Structure; will be resized by call */
				    compressedDataPtr);	/* pointer to a location to recieve the compressed image data */
		if (err != noErr)
                    return;
        
		// Add sample data and a description to a media
		err = AddMediaSample(theMedia,	/* media specifier */ 
				     compressedData,	/* handle to sample data - dataIn */
				     0,		/* specifies offset into data reffered to by dataIn handle */
				     (**imageDesc).dataSize, /* number of bytes of sample data to be added */ 
				     1,		 /* frame duration = 1/10 sec */
				     (SampleDescriptionHandle)imageDesc,	/* sample description handle */ 
				     1,	/* number of samples */
				     0,	/* control flag indicating self-contained samples */
				     nil);		/* returns a time value where sample was insterted */
		if (err != noErr)
                    return;

		printf("frame %ld\n", frame);
	    } // for loop
                        
	    UnlockPixels (GetGWorldPixMap(theGWorld)/*GetPortPixMap(theGWorld)*/);
        
	    SetGWorld (oldPort, oldGDeviceH);
        
	    // Dealocate our previously alocated handles and GWorld
	    if (imageDesc)
	    {
		DisposeHandle ((Handle)imageDesc);
	    }
                
	    if (compressedData)
	    {
		DisposeHandle (compressedData);
	    }
                
	    if (theGWorld)
	    {
		DisposeGWorld (theGWorld);
	    }
	}

	// 3b. End media-editing session
	err = EndMediaEdits (theMedia);
	if (err != noErr)
            return;

	// 4. Insert a reference to a media segment into the track
	err = InsertMediaIntoTrack (theTrack,		/* track specifier */
                                    0,	/* track start time */
                                    0, 	/* media start time */
                                    GetMediaDuration(theMedia), /* media duration */
                                    fixed1);		/* media rate ((Fixed) 0x00010000L) */
	if (err != noErr)
            return;
    }

    err = AddMovieResource (theMovie, resRefNum, &resId, QTUtils_ConvertCToPascalString ("StarField"));
    if (resRefNum)
    {
	CloseMovieFile (resRefNum);
//	NSLog(@"closing");
    }
    
    DisposeMovie(theMovie);
}
#else
void
run (void)
{
    int frame;

    for (frame = 0; frame < NUM_FRAMES; ++frame)
    {
	char filename[64];
	bitmap_t *image = render_frame(frame);

	sprintf(filename, "out%04d.png", frame);
	bitmap_write(image, filename);

	printf("frame %d\n", frame);

	bitmap_free(image);
    }
}
#endif

int
main (void)
{
    init_circle();
    init_stars();

    run();

    return 0;
}
