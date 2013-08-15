#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include "snes9x.h"
#include <SDL.h>
//#include "allegro.h"
#include <glib.h>
#include <bcm_host.h>

#include "keyconstants.h"
#include "keys.h"

static int game_num_avail=0;
static int last_game_selected=0;

char playgame[255] = "builtinn\0";

static unsigned short *fe_menu_bmp;
//sq static unsigned short *fe_splash_bmp;

unsigned short *fe_screen;

static void initSDL(void);
static void fe_exit(void);
static void frontend_display(void);
static void frontend_deinit(void);
static void frontend_init(void);
static void fe_gamelist_text_out(int x, int y, char *eltexto, int color);
static void fe_gamelist_text_out_fmt(int x, int y, char* fmt, ...);
static void fe_text(unsigned short *screen, int x, int y, char *text, int color);

uint8 *keyssnes;

struct fe_driver {
//sq	char description[128];
	char name[255];
};

struct fe_driver fe_drivers[3000];

#define color16(R,G,B)  ((R >> 3) << 11) | (( G >> 2) << 5 ) | (( B >> 3 ) << 0 )

static void show_bmp_16bpp(unsigned short *out, unsigned short *in)
{
 	int y;

	//Load bitmap, file will be flipped y so invert
	//sq in+=(640*480)-1;
	in+=640*479;
 	for (y=479;y!=-1;y--) {
		memcpy(out, in, 640*2);
		out+=640;
		in-=640;
	}
}

#pragma pack(2) // Align
typedef struct                       /**** BMP file header structure ****/
{
    unsigned short bfType;           /* Magic number for file */
    unsigned int   bfSize;           /* Size of file */
    unsigned short bfReserved1;      /* Reserved */
    unsigned short bfReserved2;      /* ... */
    unsigned int   bfOffBits;        /* Offset to bitmap data */
} BITMAPFILEHEADER;
#pragma pack()

#pragma pack(push, 1)

typedef struct tagBITMAPINFOHEADER
{
    unsigned long biSize;  //specifies the number of bytes required by the struct
    long biWidth;  //specifies width in pixels
    long biHeight;  //species height in pixels
    unsigned short biPlanes; //specifies the number of color planes, must be 1
    unsigned short biBitCount; //specifies the number of bit per pixel
    unsigned long biCompression;//spcifies the type of compression
    unsigned long biSizeImage;  //size of image in bytes
    long biXPelsPerMeter;  //number of pixels per meter in x axis
    long biYPelsPerMeter;  //number of pixels per meter in y axis
    unsigned long biClrUsed;  //number of colors used by th ebitmap
    unsigned long biClrImportant;  //number of colors that are important
} BITMAPINFOHEADER;

#pragma pack(pop)

static unsigned char *LoadBitmapFile(char *filename, BITMAPINFOHEADER *bitmapInfoHeader)
{
    FILE *filePtr; //our file pointer
    BITMAPFILEHEADER bitmapFileHeader; //our bitmap file header
    unsigned char *bitmapImage;  //store image data
    int imageIdx=0;  //image index counter
    unsigned char tempRGB;  //our swap variable

    //open filename in read binary mode
    filePtr = fopen(filename,"rb");
    if (filePtr == NULL)
        return NULL;

    //read the bitmap file header
    fread(&bitmapFileHeader, sizeof(BITMAPFILEHEADER),1,filePtr);

    //verify that this is a bmp file by check bitmap id
    if (bitmapFileHeader.bfType !=0x4D42)
    {
        fclose(filePtr);
        return NULL;
    }

    //read the bitmap info header
    fread(bitmapInfoHeader, sizeof(BITMAPINFOHEADER),1,filePtr);

    //move file point to the begging of bitmap data
    fseek(filePtr, bitmapFileHeader.bfOffBits, SEEK_SET);

    //allocate enough memory for the bitmap image data
    bitmapImage = (unsigned char*)malloc(bitmapInfoHeader->biSizeImage);

    //verify memory allocation
    if (!bitmapImage)
    {
        free(bitmapImage);
        fclose(filePtr);
        return NULL;
    }

    //read in the bitmap image data
    fread(bitmapImage,bitmapInfoHeader->biSizeImage,1,filePtr);

    //make sure bitmap image data was read
    if (bitmapImage == NULL)
    {
        fclose(filePtr);
        return NULL;
    }

//sq     //swap the r and b values to get RGB (bitmap is BGR)
//sq     for (imageIdx = 0;imageIdx < bitmapInfoHeader->biSizeImage;imageIdx+=3)
//sq     {
//sq         tempRGB = bitmapImage[imageIdx];
//sq         bitmapImage[imageIdx] = bitmapImage[imageIdx + 2];
//sq         bitmapImage[imageIdx + 2] = tempRGB;
//sq     }

    //close file and return bitmap iamge data
    fclose(filePtr);
    return bitmapImage;
}

static void load_bitmaps(void) {
	BITMAPINFOHEADER bitmapInfoHeader;

	fe_menu_bmp = (unsigned short*) LoadBitmapFile("skins/rpimenu16.bmp",&bitmapInfoHeader);
	if (!fe_menu_bmp) {
		printf("\nERROR: Menu screen missing from skins directory\n");
		fe_exit();
	}
}

static int strcompare(const struct dirent **left, const struct dirent **right)
{
    return strcasecmp((*left)->d_name, (*right)->d_name);
}

static void game_list_init_nocache(void)
{
	int n,i;
	FILE *f;
	struct dirent **namelist;

	game_num_avail=0;

	n = scandir("roms", &namelist, 0, strcompare);
	if (n > 0)
	{
		for(i=0;i<n;i++)
		{
			if (namelist[i]->d_type != DT_DIR)
			{
				if (strcasecmp(namelist[i]->d_name, "dir.txt") != 0)
					strcpy(fe_drivers[game_num_avail++].name,  namelist[i]->d_name);
			}
			free(namelist[n]);
		}
		free(namelist);
	}
}

static void game_list_view(int *pos) {

	int i;
	int view_pos;
	int aux_pos=0;
	int screen_y = 45;
	int screen_x = 40;

	/* Draw background image */
	show_bmp_16bpp(fe_screen, fe_menu_bmp);

	/* Check Limits */
	if (*pos<0)
		*pos=game_num_avail-1;
	if (*pos>(game_num_avail-1))
		*pos=0;
					   
	/* Set View Pos */
	if (*pos<10) {
		view_pos=0;
	} else {
		if (*pos>game_num_avail-11) {
			view_pos=game_num_avail-21;
			view_pos=(view_pos<0?0:view_pos);
		} else {
			view_pos=*pos-10;
		}
	}

	/* Show List */
	for (i=0;i<game_num_avail;i++) {
			if (aux_pos>=view_pos && aux_pos<=view_pos+28) {

				if (aux_pos==*pos) {
					fe_gamelist_text_out( screen_x, screen_y, fe_drivers[i].name, color16(0,150,255));
					fe_gamelist_text_out( screen_x-5, screen_y,">",color16(255,255,255) );
		//sq			fe_gamelist_text_out( screen_x-7, screen_y-1,"-",color16(255,255,255) );
				}
				else {
					fe_gamelist_text_out( screen_x, screen_y, fe_drivers[i].name, color16(255,255,255));
				}
				
				screen_y+=6;
			}
			aux_pos++;
	}
}

static void game_list_select (int index, char *game) 
{
	int i;
	strcpy(game,fe_drivers[index].name);
}


static void fe_exit(void)
{
	free(fe_menu_bmp);
	frontend_deinit();
	exit(0);
}

uint8 joy_buttons[32];
uint8 joy_axes[8];

int joyaxis_LR, joyaxis_UD;

#define NUMKEYS 256
static Uint16 sfc_key[NUMKEYS];
static Uint16 sfc_joy[NUMKEYS];

static void fe_ProcessEvents (void)
{
	SDL_Event event;
	while(SDL_PollEvent(&event)) {
		switch(event.type) {
		case SDL_JOYBUTTONDOWN:
			joy_buttons[event.jbutton.button] = 1;
			break;
		case SDL_JOYBUTTONUP:
			joy_buttons[event.jbutton.button] = 0;
			break;
		case SDL_JOYAXISMOTION:
			if(event.jaxis.axis == joyaxis_LR) {
				if(event.jaxis.value > -10000 && event.jaxis.value < 10000)
					joy_axes[joyaxis_LR] = CENTER;
				else if(event.jaxis.value > 10000)
					joy_axes[joyaxis_LR] = RIGHT;
				else
					joy_axes[joyaxis_LR] = LEFT;
			}
			if(event.jaxis.axis == joyaxis_UD) {
				if(event.jaxis.value > -10000 && event.jaxis.value < 10000)
					joy_axes[joyaxis_UD] = CENTER;
				else if(event.jaxis.value > 10000)
					joy_axes[joyaxis_UD] = DOWN;
				else
					joy_axes[joyaxis_UD] = UP;
			}
			break;
		case SDL_KEYDOWN:
			keyssnes = SDL_GetKeyState(NULL);
			break;
		case SDL_KEYUP:
			keyssnes = SDL_GetKeyState(NULL);
			break;
		}
	}
}


static uint32 fe_ReadJoypad (int which1)
{
	uint32 val=0x80000000;

	if (keyssnes[sfc_key[A_1]] == SDL_PRESSED || joy_buttons[sfc_joy[A_1]])		val |= SNES_A_MASK;
	if (keyssnes[sfc_key[B_1]] == SDL_PRESSED || joy_buttons[sfc_joy[B_1]])		val |= SNES_B_MASK;
	if (keyssnes[sfc_key[START_1]] == SDL_PRESSED || joy_buttons[sfc_joy[START_1]])	val |= SNES_START_MASK;
	if (keyssnes[sfc_key[SELECT_1]] == SDL_PRESSED || joy_buttons[sfc_joy[SELECT_1]])	val |= SNES_SELECT_MASK;
	if (keyssnes[sfc_key[UP_1]] == SDL_PRESSED || joy_axes[joyaxis_UD] == UP)		val |= SNES_UP_MASK;
	if (keyssnes[sfc_key[DOWN_1]] == SDL_PRESSED || joy_axes[joyaxis_UD] == DOWN)	val |= SNES_DOWN_MASK;
	if (keyssnes[sfc_key[LEFT_1]] == SDL_PRESSED || joy_axes[joyaxis_LR] == LEFT)	val |= SNES_LEFT_MASK;
	if (keyssnes[sfc_key[RIGHT_1]] == SDL_PRESSED || joy_axes[joyaxis_LR] == RIGHT)	val |= SNES_RIGHT_MASK;

	if (keyssnes[sfc_key[QUIT]] == SDL_PRESSED || joy_buttons[sfc_joy[QUIT]]) fe_exit();

	if (val&SNES_SELECT_MASK && val&SNES_START_MASK) fe_exit();

	return(val);
}

static GKeyFile *gkeyfile=0;

static void open_config_file(void)
{
	GError *error = NULL;

	gkeyfile = g_key_file_new ();
	if (!(int)g_key_file_load_from_file (gkeyfile, "snes9x.cfg", G_KEY_FILE_NONE, &error))
	{
		gkeyfile=0;
	}
}

static void close_config_file(void)
{
	if(gkeyfile)
    	g_key_file_free(gkeyfile);
}


static int get_keyjoy_conf (char *section, char *option, int defval)
{
	GError *error=NULL;
	int tempint;

	if(!gkeyfile) return defval;

	tempint = g_key_file_get_integer(gkeyfile, section, option, &error);
	if (!error) 
		return tempint;
	else 	
		return defval;

}


static void fe_S9xInitInputDevices ()
{
	memset(joy_buttons, 0, 32);
	memset(joy_axes, 0, 8);
	memset(sfc_key, 0, NUMKEYS*2);
	memset(sfc_joy, 0, NUMKEYS*2);

	//Open config file to read joystick values below
	open_config_file();

	//Configure keys from config file or defaults
	sfc_key[A_1] = get_keyjoy_conf("Keyboard", "A_1", RPI_KEY_A);
	sfc_key[B_1] = get_keyjoy_conf("Keyboard", "B_1", RPI_KEY_B);
	sfc_key[X_1] = get_keyjoy_conf("Keyboard", "X_1", RPI_KEY_X);
	sfc_key[Y_1] = get_keyjoy_conf("Keyboard", "Y_1", RPI_KEY_Y);
	sfc_key[L_1] = get_keyjoy_conf("Keyboard", "L_1", RPI_KEY_L);
	sfc_key[R_1] = get_keyjoy_conf("Keyboard", "R_1", RPI_KEY_R);
	sfc_key[START_1] = get_keyjoy_conf("Keyboard", "START_1", RPI_KEY_START);
	sfc_key[SELECT_1] = get_keyjoy_conf("Keyboard", "SELECT_1", RPI_KEY_SELECT);
	sfc_key[LEFT_1] = get_keyjoy_conf("Keyboard", "LEFT_1", RPI_KEY_LEFT);
	sfc_key[RIGHT_1] = get_keyjoy_conf("Keyboard", "RIGHT_1", RPI_KEY_RIGHT);
	sfc_key[UP_1] = get_keyjoy_conf("Keyboard", "UP_1", RPI_KEY_UP);
	sfc_key[DOWN_1] = get_keyjoy_conf("Keyboard", "DOWN_1", RPI_KEY_DOWN);

	sfc_key[QUIT] = get_keyjoy_conf("Keyboard", "QUIT", RPI_KEY_QUIT);
	sfc_key[ACCEL] = get_keyjoy_conf("Keyboard", "ACCEL", RPI_KEY_ACCEL);

/*	sfc_key[LEFT_2] = SDLK_4;
	sfc_key[RIGHT_2] = SDLK_6;
	sfc_key[UP_2] = SDLK_8;
	sfc_key[DOWN_2] = SDLK_2;
	sfc_key[LU_2] = SDLK_7;
	sfc_key[LD_2] = SDLK_1;
	sfc_key[RU_2] = SDLK_9;
	sfc_key[RD_2] = SDLK_3; */

	//Configure joysticks from config file or defaults
	sfc_joy[A_1] = get_keyjoy_conf("Joystick", "A_1", RPI_JOY_A);
	sfc_joy[B_1] = get_keyjoy_conf("Joystick", "B_1", RPI_JOY_B);
	sfc_joy[X_1] = get_keyjoy_conf("Joystick", "X_1", RPI_JOY_X);
	sfc_joy[Y_1] = get_keyjoy_conf("Joystick", "Y_1", RPI_JOY_Y);
	sfc_joy[L_1] = get_keyjoy_conf("Joystick", "L_1", RPI_JOY_L);
	sfc_joy[R_1] = get_keyjoy_conf("Joystick", "R_1", RPI_JOY_R);
	sfc_joy[START_1] = get_keyjoy_conf("Joystick", "START_1", RPI_JOY_START);
	sfc_joy[SELECT_1] = get_keyjoy_conf("Joystick", "SELECT_1", RPI_JOY_SELECT);

	sfc_joy[QUIT] = get_keyjoy_conf("Joystick", "QUIT", RPI_JOY_QUIT);
	sfc_joy[ACCEL] = get_keyjoy_conf("Joystick", "ACCEL", RPI_JOY_ACCEL);

	sfc_joy[QLOAD] = get_keyjoy_conf("Joystick", "QLOAD", RPI_JOY_QLOAD);
	sfc_joy[QSAVE] = get_keyjoy_conf("Joystick", "QSAVE", RPI_JOY_QSAVE);

    //Read joystick axis to use, default to 0 & 1
    joyaxis_LR = get_keyjoy_conf("Joystick", "JA_LR", 0);
    joyaxis_UD = get_keyjoy_conf("Joystick", "JA_UD", 1);

	close_config_file();
}

static unsigned long fe_timer_read(void)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC_RAW, &now);

    return ((unsigned long long)now.tv_sec * 1000000LL + (now.tv_nsec / 1000LL));
}


static void select_game(char *game)
{
	extern int kiosk_mode;

	uint32 Joypads;

	unsigned long keytimer=0;
	int keydirection=0, last_keydirection=0;

	/* No Selected game */
	strcpy(game,"builtinn");

	/* Clean screen */
	frontend_display();

	/* Wait until user selects a game */
	while(1)
	{
		game_list_view(&last_game_selected);
		frontend_display();
       	usleep(70000);

		while(1)
		{
            usleep(10000);

			fe_ProcessEvents();
			Joypads = fe_ReadJoypad(0);

			last_keydirection=keydirection;
			keydirection=0;

			//Any keyboard key pressed?
			if(Joypads & SNES_LEFT_MASK || Joypads & SNES_RIGHT_MASK ||
			   Joypads & SNES_UP_MASK || Joypads & SNES_DOWN_MASK)
			{
				keydirection=1;
				break;
			}

			//Game selected
			if(Joypads & SNES_START_MASK || Joypads & SNES_B_MASK) break;

			//Used to delay the initial key press, but 
			//once pressed and held the delay will clear
			keytimer = fe_timer_read() + (1000000/2);

		}

		//Key delay
		if(keydirection && last_keydirection && fe_timer_read() < keytimer) continue;

		int updown=0;
		if(Joypads & SNES_UP_MASK) {last_game_selected--; updown=1;};
		if(Joypads & SNES_DOWN_MASK) {last_game_selected++; updown=1;};

		// Stop diagonals on game selection
		if(!updown) {
			if(Joypads & SNES_LEFT_MASK) last_game_selected-=21;
			if(Joypads & SNES_RIGHT_MASK) last_game_selected+=21;
		}

		if(Joypads & SNES_START_MASK || Joypads & SNES_B_MASK)
		{
			/* Select the game */
			game_list_select(last_game_selected, game);

			break;
		}
	}
}

int main (int argc, char **argv)
{
	FILE *f;

	char options[1000];
	int i;

	char gamename[255];
	char *arg1;
	arg1 = "snes9x";

    char abspath[1000];

	//create options string for later passing to runtime
	options[0]=NULL;
	if(argc > 1) {
		for(i=1;i<argc;i++) {
			strcat(options, argv[i]);
			strcat(options, " ");	
		}
	}

	//Set the directory to where the binary is
    realpath(argv[0], abspath);
    char *dirsep = strrchr(abspath, '/');
    if( dirsep != 0 ) *dirsep = 0;
    chdir(abspath);

	frontend_init();

	/* Initialize list of available games */
	game_list_init_nocache();

	/* Load menu baclground */
    load_bitmaps();

	while(1)
	{
		fe_S9xInitInputDevices();

		//Initialise SDL input after each game run
		initSDL();
	
		if (game_num_avail==0)
		{
			/* Draw background image */
	    	show_bmp_16bpp(fe_screen, fe_menu_bmp);
			fe_gamelist_text_out(35, 110, "ERROR: NO AVAILABLE GAMES FOUND",color16(255,255,255));
			frontend_display();
			sleep(5);
			fe_exit();
		}
	
	//sq	/* Read default configuration */
	//sq	f=fopen("frontend/mame.cfg","r");
	//sq	if (f) {
	//sq		fscanf(f,"%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",&gp2x_freq,&gp2x_video_depth,&gp2x_video_aspect,&gp2x_video_sync,
	//sq		&gp2x_frameskip,&gp2x_sound,&gp2x_clock_cpu,&gp2x_clock_sound,&gp2x_cpu_cores,&gp2x_ramtweaks,&last_game_selected,&gp2x_cheat,&gp2x_volume);
	//sq		fclose(f);
		//sq}
		
		/* Select Game */
		select_game(playgame); 
	
		/* Write default configuration */
	//sq 	f=fopen("frontend/mame.cfg","w");
	//sq 	if (f) {
	//sq 		fprintf(f,"%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",gp2x_freq,gp2x_video_depth,gp2x_video_aspect,gp2x_video_sync,
	//sq 		gp2x_frameskip,gp2x_sound,gp2x_clock_cpu,gp2x_clock_sound,gp2x_cpu_cores,gp2x_ramtweaks,last_game_selected,gp2x_cheat,gp2x_volume);
	//sq 		fclose(f);
	//sq 		sync();
	//sq 	}
	
		//Quit SDL input before starting Game
		SDL_Quit();

	
		//Run the actual game
		//Using system seems to work better with snes9x
		sprintf(gamename, "./snes9x %s \"roms/%s\"", options, playgame);
		system(gamename);

		usleep(500000);
	    
	}
	
}

DISPMANX_RESOURCE_HANDLE_T fe_resource;
DISPMANX_ELEMENT_HANDLE_T fe_element;
DISPMANX_DISPLAY_HANDLE_T fe_display;
DISPMANX_UPDATE_HANDLE_T fe_update;

static void initSDL(void)
{
	SDL_Joystick* joy;

	//SDL initialisation
    if (SDL_Init(SDL_INIT_JOYSTICK) < 0 )
    {
        printf("Could not initialize SDL(%s)\n", SDL_GetError());
        fe_exit();
    }
    atexit(SDL_Quit);
    keyssnes = SDL_GetKeyState(NULL);
    SDL_SetVideoMode(0, 0, 16, SDL_SWSURFACE);

    SDL_EventState(SDL_ACTIVEEVENT,SDL_IGNORE);
    SDL_EventState(SDL_SYSWMEVENT,SDL_IGNORE);
    SDL_EventState(SDL_VIDEORESIZE,SDL_IGNORE);
    SDL_EventState(SDL_USEREVENT,SDL_IGNORE);
    SDL_ShowCursor(SDL_DISABLE);

    joy = SDL_JoystickOpen(0);

    if(joy) {
//sq        printf("Opened joystick 0.\n");
        if(SDL_JoystickEventState(SDL_ENABLE) != SDL_ENABLE) {
            printf("Could not set joystick event state\n", SDL_GetError());
            fe_exit();
        }
    } 
}

static void frontend_init(void)
{
    int ret;

    uint32_t display_width=0, display_height=0;

    VC_RECT_T dst_rect;
    VC_RECT_T src_rect;

    bcm_host_init();

	//initialise dispmanx display and resources
    fe_screen=(unsigned short *) calloc(1, 640*480*2);

    graphics_get_display_size(0 /* LCD */, &display_width, &display_height);

    fe_display = vc_dispmanx_display_open( 0 );

    //Create two surfaces for flipping between
    //Make sure bitmap type matches the source for better performance
    uint32_t crap;
    fe_resource = vc_dispmanx_resource_create(VC_IMAGE_RGB565, 640, 480, &crap);

    vc_dispmanx_rect_set( &dst_rect, 0, 0, display_width, display_height);
    vc_dispmanx_rect_set( &src_rect, 0, 0, 640 << 16, 480 << 16);

    //Make sure mame and background overlay the menu program
    fe_update = vc_dispmanx_update_start( 0 );

    // create the 'window' element - based on the first buffer resource (resource0)
    fe_element = vc_dispmanx_element_add(  fe_update,
           fe_display, 1, &dst_rect, fe_resource, &src_rect,
           DISPMANX_PROTECTION_NONE, 0, 0, (DISPMANX_TRANSFORM_T) 0 );

    ret = vc_dispmanx_update_submit_sync( fe_update );

}

static void frontend_deinit(void)
{
    int ret;

    fe_update = vc_dispmanx_update_start( 0 );
    ret = vc_dispmanx_element_remove( fe_update, fe_element );
    ret = vc_dispmanx_update_submit_sync( fe_update );
    ret = vc_dispmanx_resource_delete( fe_resource );
    ret = vc_dispmanx_display_close( fe_display );

    if(fe_screen) free(fe_screen);
    fe_screen=0;

	bcm_host_deinit();

}

static void frontend_display(void)
{
    VC_RECT_T dst_rect;

    vc_dispmanx_rect_set( &dst_rect, 0, 0, 640, 480 );

    // begin display update
    fe_update = vc_dispmanx_update_start( 0 );

    // blit image to the current resource
    vc_dispmanx_resource_write_data( fe_resource, VC_IMAGE_RGB565, 640*2, fe_screen, &dst_rect );

    vc_dispmanx_update_submit_sync( fe_update );

}


static unsigned char fontdata8x8[] =
{
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x3C,0x42,0x99,0xBD,0xBD,0x99,0x42,0x3C,0x3C,0x42,0x81,0x81,0x81,0x81,0x42,0x3C,
	0xFE,0x82,0x8A,0xD2,0xA2,0x82,0xFE,0x00,0xFE,0x82,0x82,0x82,0x82,0x82,0xFE,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x38,0x64,0x74,0x7C,0x38,0x00,0x00,
	0x80,0xC0,0xF0,0xFC,0xF0,0xC0,0x80,0x00,0x01,0x03,0x0F,0x3F,0x0F,0x03,0x01,0x00,
	0x18,0x3C,0x7E,0x18,0x7E,0x3C,0x18,0x00,0xEE,0xEE,0xEE,0xCC,0x00,0xCC,0xCC,0x00,
	0x00,0x00,0x30,0x68,0x78,0x30,0x00,0x00,0x00,0x38,0x64,0x74,0x7C,0x38,0x00,0x00,
	0x3C,0x66,0x7A,0x7A,0x7E,0x7E,0x3C,0x00,0x0E,0x3E,0x3A,0x22,0x26,0x6E,0xE4,0x40,
	0x18,0x3C,0x7E,0x3C,0x3C,0x3C,0x3C,0x00,0x3C,0x3C,0x3C,0x3C,0x7E,0x3C,0x18,0x00,
	0x08,0x7C,0x7E,0x7E,0x7C,0x08,0x00,0x00,0x10,0x3E,0x7E,0x7E,0x3E,0x10,0x00,0x00,
	0x58,0x2A,0xDC,0xC8,0xDC,0x2A,0x58,0x00,0x24,0x66,0xFF,0xFF,0x66,0x24,0x00,0x00,
	0x00,0x10,0x10,0x38,0x38,0x7C,0xFE,0x00,0xFE,0x7C,0x38,0x38,0x10,0x10,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x1C,0x1C,0x1C,0x18,0x00,0x18,0x18,0x00,
	0x6C,0x6C,0x24,0x00,0x00,0x00,0x00,0x00,0x00,0x28,0x7C,0x28,0x7C,0x28,0x00,0x00,
	0x10,0x38,0x60,0x38,0x0C,0x78,0x10,0x00,0x40,0xA4,0x48,0x10,0x24,0x4A,0x04,0x00,
	0x18,0x34,0x18,0x3A,0x6C,0x66,0x3A,0x00,0x18,0x18,0x20,0x00,0x00,0x00,0x00,0x00,
	0x30,0x60,0x60,0x60,0x60,0x60,0x30,0x00,0x0C,0x06,0x06,0x06,0x06,0x06,0x0C,0x00,
	0x10,0x54,0x38,0x7C,0x38,0x54,0x10,0x00,0x00,0x18,0x18,0x7E,0x18,0x18,0x00,0x00,
	0x00,0x00,0x00,0x00,0x18,0x18,0x30,0x00,0x00,0x00,0x00,0x00,0x3E,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x18,0x18,0x00,0x00,0x00,0x04,0x08,0x10,0x20,0x40,0x00,0x00,
	0x38,0x4C,0xC6,0xC6,0xC6,0x64,0x38,0x00,0x18,0x38,0x18,0x18,0x18,0x18,0x7E,0x00,
	0x7C,0xC6,0x0E,0x3C,0x78,0xE0,0xFE,0x00,0x7E,0x0C,0x18,0x3C,0x06,0xC6,0x7C,0x00,
	0x1C,0x3C,0x6C,0xCC,0xFE,0x0C,0x0C,0x00,0xFC,0xC0,0xFC,0x06,0x06,0xC6,0x7C,0x00,
	0x3C,0x60,0xC0,0xFC,0xC6,0xC6,0x7C,0x00,0xFE,0xC6,0x0C,0x18,0x30,0x30,0x30,0x00,
	0x78,0xC4,0xE4,0x78,0x86,0x86,0x7C,0x00,0x7C,0xC6,0xC6,0x7E,0x06,0x0C,0x78,0x00,
	0x00,0x00,0x18,0x00,0x00,0x18,0x00,0x00,0x00,0x00,0x18,0x00,0x00,0x18,0x18,0x30,
	0x1C,0x38,0x70,0xE0,0x70,0x38,0x1C,0x00,0x00,0x7C,0x00,0x00,0x7C,0x00,0x00,0x00,
	0x70,0x38,0x1C,0x0E,0x1C,0x38,0x70,0x00,0x7C,0xC6,0xC6,0x1C,0x18,0x00,0x18,0x00,
	0x3C,0x42,0x99,0xA1,0xA5,0x99,0x42,0x3C,0x38,0x6C,0xC6,0xC6,0xFE,0xC6,0xC6,0x00,
	0xFC,0xC6,0xC6,0xFC,0xC6,0xC6,0xFC,0x00,0x3C,0x66,0xC0,0xC0,0xC0,0x66,0x3C,0x00,
	0xF8,0xCC,0xC6,0xC6,0xC6,0xCC,0xF8,0x00,0xFE,0xC0,0xC0,0xFC,0xC0,0xC0,0xFE,0x00,
	0xFE,0xC0,0xC0,0xFC,0xC0,0xC0,0xC0,0x00,0x3E,0x60,0xC0,0xCE,0xC6,0x66,0x3E,0x00,
	0xC6,0xC6,0xC6,0xFE,0xC6,0xC6,0xC6,0x00,0x7E,0x18,0x18,0x18,0x18,0x18,0x7E,0x00,
	0x06,0x06,0x06,0x06,0xC6,0xC6,0x7C,0x00,0xC6,0xCC,0xD8,0xF0,0xF8,0xDC,0xCE,0x00,
	0x60,0x60,0x60,0x60,0x60,0x60,0x7E,0x00,0xC6,0xEE,0xFE,0xFE,0xD6,0xC6,0xC6,0x00,
	0xC6,0xE6,0xF6,0xFE,0xDE,0xCE,0xC6,0x00,0x7C,0xC6,0xC6,0xC6,0xC6,0xC6,0x7C,0x00,
	0xFC,0xC6,0xC6,0xC6,0xFC,0xC0,0xC0,0x00,0x7C,0xC6,0xC6,0xC6,0xDE,0xCC,0x7A,0x00,
	0xFC,0xC6,0xC6,0xCE,0xF8,0xDC,0xCE,0x00,0x78,0xCC,0xC0,0x7C,0x06,0xC6,0x7C,0x00,
	0x7E,0x18,0x18,0x18,0x18,0x18,0x18,0x00,0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0x7C,0x00,
	0xC6,0xC6,0xC6,0xEE,0x7C,0x38,0x10,0x00,0xC6,0xC6,0xD6,0xFE,0xFE,0xEE,0xC6,0x00,
	0xC6,0xEE,0x3C,0x38,0x7C,0xEE,0xC6,0x00,0x66,0x66,0x66,0x3C,0x18,0x18,0x18,0x00,
	0xFE,0x0E,0x1C,0x38,0x70,0xE0,0xFE,0x00,0x3C,0x30,0x30,0x30,0x30,0x30,0x3C,0x00,
	0x60,0x60,0x30,0x18,0x0C,0x06,0x06,0x00,0x3C,0x0C,0x0C,0x0C,0x0C,0x0C,0x3C,0x00,
	0x18,0x3C,0x66,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF,
	0x30,0x30,0x18,0x00,0x00,0x00,0x00,0x00,0x00,0x3C,0x06,0x3E,0x66,0x66,0x3C,0x00,
	0x60,0x7C,0x66,0x66,0x66,0x66,0x7C,0x00,0x00,0x3C,0x66,0x60,0x60,0x66,0x3C,0x00,
	0x06,0x3E,0x66,0x66,0x66,0x66,0x3E,0x00,0x00,0x3C,0x66,0x66,0x7E,0x60,0x3C,0x00,
	0x1C,0x30,0x78,0x30,0x30,0x30,0x30,0x00,0x00,0x3E,0x66,0x66,0x66,0x3E,0x06,0x3C,
	0x60,0x7C,0x76,0x66,0x66,0x66,0x66,0x00,0x18,0x00,0x38,0x18,0x18,0x18,0x18,0x00,
	0x0C,0x00,0x1C,0x0C,0x0C,0x0C,0x0C,0x38,0x60,0x60,0x66,0x6C,0x78,0x6C,0x66,0x00,
	0x38,0x18,0x18,0x18,0x18,0x18,0x18,0x00,0x00,0xEC,0xFE,0xFE,0xFE,0xD6,0xC6,0x00,
	0x00,0x7C,0x76,0x66,0x66,0x66,0x66,0x00,0x00,0x3C,0x66,0x66,0x66,0x66,0x3C,0x00,
	0x00,0x7C,0x66,0x66,0x66,0x7C,0x60,0x60,0x00,0x3E,0x66,0x66,0x66,0x3E,0x06,0x06,
	0x00,0x7E,0x70,0x60,0x60,0x60,0x60,0x00,0x00,0x3C,0x60,0x3C,0x06,0x66,0x3C,0x00,
	0x30,0x78,0x30,0x30,0x30,0x30,0x1C,0x00,0x00,0x66,0x66,0x66,0x66,0x6E,0x3E,0x00,
	0x00,0x66,0x66,0x66,0x66,0x3C,0x18,0x00,0x00,0xC6,0xD6,0xFE,0xFE,0x7C,0x6C,0x00,
	0x00,0x66,0x3C,0x18,0x3C,0x66,0x66,0x00,0x00,0x66,0x66,0x66,0x66,0x3E,0x06,0x3C,
	0x00,0x7E,0x0C,0x18,0x30,0x60,0x7E,0x00,0x0E,0x18,0x0C,0x38,0x0C,0x18,0x0E,0x00,
	0x18,0x18,0x18,0x00,0x18,0x18,0x18,0x00,0x70,0x18,0x30,0x1C,0x30,0x18,0x70,0x00,
	0x00,0x00,0x76,0xDC,0x00,0x00,0x00,0x00,0x10,0x28,0x10,0x54,0xAA,0x44,0x00,0x00,
};

static void fe_text(unsigned short *screen, int x, int y, char *text, int color)
{
	unsigned int i,l;
	screen=screen+(x*2)+(y*2)*640;

	for (i=0;i<strlen(text);i++) {
		
		for (l=0;l<8;l++) {
			screen[l*640+0]=(fontdata8x8[((text[i])*8)+l]&0x80)?color:screen[l*640+0];
			screen[l*640+1]=(fontdata8x8[((text[i])*8)+l]&0x40)?color:screen[l*640+1];
			screen[l*640+2]=(fontdata8x8[((text[i])*8)+l]&0x20)?color:screen[l*640+2];
			screen[l*640+3]=(fontdata8x8[((text[i])*8)+l]&0x10)?color:screen[l*640+3];
			screen[l*640+4]=(fontdata8x8[((text[i])*8)+l]&0x08)?color:screen[l*640+4];
			screen[l*640+5]=(fontdata8x8[((text[i])*8)+l]&0x04)?color:screen[l*640+5];
			screen[l*640+6]=(fontdata8x8[((text[i])*8)+l]&0x02)?color:screen[l*640+6];
			screen[l*640+7]=(fontdata8x8[((text[i])*8)+l]&0x01)?color:screen[l*640+7];
		}
		screen+=8;
	} 
}

static void fe_gamelist_text_out(int x, int y, char *eltexto, int color)
{
	char texto[36];
	strncpy(texto,eltexto,35);
	texto[35]=0;
	fe_text(fe_screen,x,y,texto,color);
}

static void fe_gamelist_text_out_fmt(int x, int y, char* fmt, ...)
{
	char strOut[128];
	va_list marker;
	
	va_start(marker, fmt);
	vsprintf(strOut, fmt, marker);
	va_end(marker);	

	fe_gamelist_text_out(x, y, strOut, 255);
}
