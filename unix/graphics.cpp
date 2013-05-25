/*
 * Snes9x - Portable Super Nintendo Entertainment System (TM) emulator.
 *
 * (c) Copyright 1996 - 2001 Gary Henderson (gary.henderson@ntlworld.com) and
 *                           Jerremy Koot (jkoot@snes9x.com)
 *
 * Super FX C emulator code 
 * (c) Copyright 1997 - 1999 Ivar (ivar@snes9x.com) and
 *                           Gary Henderson.
 * Super FX assembler emulator code (c) Copyright 1998 zsKnight and _Demo_.
 *
 * DSP1 emulator code (c) Copyright 1998 Ivar, _Demo_ and Gary Henderson.
 * C4 asm and some C emulation code (c) Copyright 2000 zsKnight and _Demo_.
 * C4 C code (c) Copyright 2001 Gary Henderson (gary.henderson@ntlworld.com).
 *
 * DOS port code contains the works of other authors. See headers in
 * individual files.
 *
 * Snes9x homepage: http://www.snes9x.com
 *
 * Permission to use, copy, modify and distribute Snes9x in both binary and
 * source form, for non-commercial purposes, is hereby granted without fee,
 * providing that this license information and copyright notice appear with
 * all copies and any derived work.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event shall the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Snes9x is freeware for PERSONAL USE only. Commercial users should
 * seek permission of the copyright holders first. Commercial use includes
 * charging money for Snes9x or software derived from Snes9x.
 *
 * The copyright holders request that bug fixes and improvements to the code
 * should be forwarded to them so everyone can benefit from the modifications
 * in future versions.
 *
 * Super NES and Super Nintendo Entertainment System are trademarks of
 * Nintendo Co., Limited and its subsidiary companies.
 */
#ifdef __linux
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <stdlib.h>
#include <signal.h>

#include <SDL/SDL.h>

#include "snes9x.h"
#include "memmap.h"
#include "debug.h"
#include "ppu.h"
#include "snapshot.h"
#include "gfx.h"
#include "display.h"
#include "apu.h"
//sq #include "keydef.h"

#include <bcm_host.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <assert.h>


#define COUNT(a) (sizeof(a) / sizeof(a[0]))

DISPMANX_ELEMENT_HANDLE_T dispman_element;
DISPMANX_ELEMENT_HANDLE_T dispman_element_bg;
DISPMANX_DISPLAY_HANDLE_T dispman_display;
DISPMANX_UPDATE_HANDLE_T dispman_update;
DISPMANX_RESOURCE_HANDLE_T resource_bg;

//sq SDL_Surface *screen, *gfxscreen;
SDL_Surface *gfxscreen;
unsigned short *screen;
SDL_Joystick *joy[2];

uint16 *RGBconvert;
extern uint32 xs, ys, cl, cs;

void gles2_create(int display_width, int display_height, int bitmap_width, int bitmap_height);
void gles2_destroy();
void gles2_draw(short *screen);
void gles2_palette_changed();

EGLDisplay display = NULL;
EGLSurface surface = NULL;
static EGLContext context = NULL;
static EGL_DISPMANX_WINDOW_T nativewindow;

uint32_t display_width, display_height;

#ifndef _ZAURUS
int S9xMinCommandLineArgs ()
{
    return (2);
}

void S9xGraphicsMode ()
{
}

void S9xTextMode ()
{
}
#endif

extern uint8 *keyssnes;
void S9xInitDisplay (int height)
{
	int width=256;

	if (SDL_Init(SDL_INIT_JOYSTICK) < 0 ) 
	{
		printf("Could not initialize SDL(%s)\n", SDL_GetError());
		S9xExit();
	}
	atexit(SDL_Quit);
	keyssnes = SDL_GetKeyState(NULL);
	SDL_SetVideoMode(0, 0, 16, SDL_SWSURFACE);

    SDL_EventState(SDL_ACTIVEEVENT,SDL_IGNORE);
    SDL_EventState(SDL_SYSWMEVENT,SDL_IGNORE);
    SDL_EventState(SDL_VIDEORESIZE,SDL_IGNORE);
    SDL_EventState(SDL_USEREVENT,SDL_IGNORE);
    SDL_ShowCursor(SDL_DISABLE);

	screen = (unsigned short *) calloc(1, 256*240*2);

	bcm_host_init();

    //We handle up to four joysticks
    if(SDL_NumJoysticks())
    {
        int i;
        SDL_JoystickEventState(SDL_ENABLE);

        for(i=0;i<SDL_NumJoysticks();i++) {
            joy[i]=SDL_JoystickOpen(i);

            //Check for valid joystick, some keyboards
            //aren't SDL compatible
            if(joy[i])
            {
                if (SDL_JoystickNumAxes(joy[i]) > 6)
                {
                    SDL_JoystickClose(joy[i]);
                    joy[i]=0;
                    printf("Error detected invalid joystick/keyboard\n");
                    break;
                }
            }
			if(i==1) break;		//Only need two joysticks
        }
    }


	if (screen == NULL)
	{
		printf("Couldn't set video mode: %s\n", SDL_GetError());
		S9xExit();
	}
	if (Settings.SupportHiRes) {
		gfxscreen = SDL_CreateRGBSurface(SDL_SWSURFACE, 512, 480, 16, 0, 0, 0, 0);
		GFX.Screen = (uint8 *)gfxscreen->pixels;
		GFX.Pitch = 512 * 2;
	} else {
		GFX.Screen = (uint8 *)screen;
		GFX.Pitch = 256 * 2;
	}
	GFX.SubScreen = (uint8 *)malloc(512 * 480 * 2);
	GFX.ZBuffer = (uint8 *)malloc(512 * 480 * 2);
	GFX.SubZBuffer = (uint8 *)malloc(512 * 480 * 2);

	RGBconvert = (uint16 *)malloc(65536 * 2);
	if (!RGBconvert)
	{
//		OutOfMemory();
		S9xExit();
	}
	for (uint32 i = 0; i < 65536; i++) 
		((uint16 *)(RGBconvert))[i] = ((i >> 11) << 10) | ((((i >> 5) & 63) >> 1) << 5) | (i & 31);


{
	int ret;

	VC_RECT_T dst_rect;
	VC_RECT_T src_rect;

	// get an EGL display connection
	display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	assert(display != EGL_NO_DISPLAY);

	// initialize the EGL display connection
	EGLBoolean result = eglInitialize(display, NULL, NULL);
	assert(EGL_FALSE != result);

	// get an appropriate EGL frame buffer configuration
	EGLint num_config;
	EGLConfig config;
	static const EGLint attribute_list[] =
	{
	    EGL_RED_SIZE, 8,
	    EGL_GREEN_SIZE, 8,
	    EGL_BLUE_SIZE, 8,
	    EGL_ALPHA_SIZE, 8,
	    EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
	    EGL_NONE
	};
	result = eglChooseConfig(display, attribute_list, &config, 1, &num_config);
	assert(EGL_FALSE != result);

	result = eglBindAPI(EGL_OPENGL_ES_API);
	assert(EGL_FALSE != result);

	// create an EGL rendering context
	static const EGLint context_attributes[] =
	{
	    EGL_CONTEXT_CLIENT_VERSION, 2,
	    EGL_NONE
	};
	context = eglCreateContext(display, config, EGL_NO_CONTEXT, context_attributes);
	assert(context != EGL_NO_CONTEXT);

	// create an EGL window surface
	int32_t success = graphics_get_display_size(0, &display_width, &display_height);
	assert(success >= 0);

	vc_dispmanx_rect_set( &dst_rect, 0, 0, display_width, display_height);
	vc_dispmanx_rect_set( &src_rect, 0, 0, display_width << 16, display_height << 16);

	dispman_display = vc_dispmanx_display_open(0);
	dispman_update = vc_dispmanx_update_start(0);
	dispman_element = vc_dispmanx_element_add(dispman_update, dispman_display,
	  				10, &dst_rect, 0, &src_rect, 
					DISPMANX_PROTECTION_NONE, NULL, NULL, DISPMANX_NO_ROTATE);

	vc_dispmanx_rect_set( &dst_rect, 0, 0, display_width, display_height );
	vc_dispmanx_rect_set( &src_rect, 0, 0, 128 << 16, 128 << 16);
 
	//Create a blank background for the whole screen, make sure width is divisible by 32!
	uint32_t crap;
	resource_bg = vc_dispmanx_resource_create(VC_IMAGE_RGB565, 128, 128, &crap);
	dispman_element_bg = vc_dispmanx_element_add(  dispman_update,
	                                      dispman_display,
	                                      9,
	                                      &dst_rect,
	                                      resource_bg,
	                                      &src_rect,
	                                      DISPMANX_PROTECTION_NONE,
	                                      0,
	                                      0,
	                                      (DISPMANX_TRANSFORM_T) 0 );

	nativewindow.element = dispman_element;
	nativewindow.width = display_width;
	nativewindow.height = display_height;
	vc_dispmanx_update_submit_sync(dispman_update);

	surface = eglCreateWindowSurface(display, config, &nativewindow, NULL);
	assert(surface != EGL_NO_SURFACE);

	// connect the context to the surface
	result = eglMakeCurrent(display, surface, surface, context);
	assert(EGL_FALSE != result);

//sq	eglSwapInterval(display, 0);

	gles2_create(display_width, display_height, width, height);

}


}

void S9xDeinitDisplay ()
{
	int ret;
	int i;
	free(screen);

	for(i=0;i<2;i++)
	{
		if(SDL_JoystickOpened(i))
			SDL_JoystickClose(joy[i]); // Should this go here? WHO KNOWS
	}

	SDL_Quit();

    gles2_destroy();
    // Release OpenGL resources
    eglMakeCurrent( display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT );
    eglDestroySurface( display, surface );
    eglDestroyContext( display, context );
    eglTerminate( display );

	dispman_update = vc_dispmanx_update_start( 0 );
	ret = vc_dispmanx_element_remove( dispman_update, dispman_element );
	ret = vc_dispmanx_element_remove( dispman_update, dispman_element_bg );
	ret = vc_dispmanx_update_submit_sync( dispman_update );
	ret = vc_dispmanx_resource_delete( resource_bg );
	ret = vc_dispmanx_display_close( dispman_display );

    bcm_host_deinit();

	free(GFX.SubScreen);
	free(GFX.ZBuffer);
	free(GFX.SubZBuffer);
}

void S9xSetPalette ()
{
}

void S9xSetTitle (const char * /*title*/)
{
}

#ifndef _ZAURUS
const char *S9xSelectFilename (const char *def, const char *dir1,
			    const char *ext1, const char *title)
{
    static char path [PATH_MAX];
    char buffer [PATH_MAX];
    
    S9xTextMode ();
    printf ("\n%s (default: %s): ", title, def);
    fflush (stdout);
    if (fgets (buffer, sizeof (buffer) - 1, stdin))
    {
	char *p = buffer;
	while (isspace (*p) || *p == '\n')
	    p++;
	if (!*p)
	{
	    strcpy (buffer, def);
	    p = buffer;
	}

	char *q = strrchr (p, '\n');
	if (q)
	    *q = 0;

	char fname [PATH_MAX];
	char drive [_MAX_DRIVE];
	char dir [_MAX_DIR];
	char ext [_MAX_EXT];

	_splitpath (p, drive, dir, fname, ext);
	_makepath (path, drive, *dir ? dir : dir1, fname, *ext ? ext : ext1);
	S9xGraphicsMode ();
	return (path);
    }
    S9xGraphicsMode ();
    return (NULL);
}

void S9xParseDisplayArg (char **argv, int &ind, int)
{
    if (strcasecmp (argv [ind], "-scale") == 0 ||
	strcasecmp (argv [ind], "-sc") == 0)
	stretch = TRUE;
    else
    if (strcasecmp (argv [ind], "-y") == 0 ||
	strcasecmp (argv [ind], "-interpolation") == 0)
    {
	interpolation = TRUE;
	Settings.SixteenBit = TRUE;
	Settings.SupportHiRes = TRUE;
	Settings.Transparency = TRUE;
    }
    else
	S9xUsage ();
}

void S9xExtraUsage ()
{
}

bool8 S9xReadMousePosition (int /* which1 */, int &/* x */, int & /* y */,
			    uint32 & /* buttons */)
{
    return (FALSE);
}

bool8 S9xReadSuperScopePosition (int & /* x */, int & /* y */, 
				 uint32 & /* buttons */)
{
    return (FALSE);
}
#endif

void S9xMessage (int /* type */, int /* number */, const char *message)
{
    fprintf (stderr, "%s\n", message);
}
#endif
