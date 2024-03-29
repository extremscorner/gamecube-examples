// SD Card Directory Listing Demo
// Updated 12/19/2008 by PunMaster

#include <gccore.h>

#include <fat.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

static void *xfb = NULL;
static GXRModeObj *rmode = NULL;

void dirlist(char* path) {

	DIR* pdir = opendir(path);
	struct dirent* pent;

	if (pdir != NULL) {

		do {
			errno = 0;
			pent = readdir(pdir);

			if(pent != NULL && strcmp(".", pent->d_name) != 0 && strcmp("..", pent->d_name) != 0) {
				char dnbuf[PATH_MAX];
				sprintf(dnbuf, "%s/%s", path, pent->d_name);

				struct stat statbuf;
				stat(dnbuf, &statbuf);

				if(S_ISDIR(statbuf.st_mode)) {
					printf("%s <DIR>\n", dnbuf);
					dirlist(dnbuf);
				} else {
					printf("%s (%lld)\n", dnbuf, statbuf.st_size);
				}
			}
		} while (pent != NULL || errno == EOVERFLOW);

		closedir(pdir);
	} else {
		printf("opendir() failure.\n");
	}
}

int main(int argc, char **argv) {
	// Initialise the video system
	VIDEO_Init();

	// This function initialises the attached controllers
	PAD_Init();

	// Obtain the preferred video mode from the system
	// This will correspond to the settings in the Wii menu
	rmode = VIDEO_GetPreferredMode(NULL);

	// Allocate memory for the display in the uncached region
	xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));

	// Initialise the console, required for printf
	console_init(xfb,0,0,rmode->fbWidth,rmode->xfbHeight,rmode->fbWidth*VI_DISPLAY_PIX_SZ);

	// Set up the video registers with the chosen mode
	VIDEO_Configure(rmode);

	// Tell the video hardware where our display memory is
	VIDEO_SetNextFramebuffer(xfb);

	// Make the display visible
	VIDEO_SetBlack(false);

	// Flush the video register changes to the hardware
	VIDEO_Flush();

	// Wait for Video setup to complete
	VIDEO_WaitVSync();
	if(rmode->viTVMode&VI_NON_INTERLACE) VIDEO_WaitVSync();

	// The console understands VT terminal escape codes
	// This positions the cursor on row 2, column 0
	// we can use variables for this with format codes too
	// e.g. printf ("\x1b[%d;%dH", row, column );
	printf("\x1b[2;0H");

	if(fatInitDefault()) {
		dirlist(".");
	} else {
		printf("fatInitDefault() failure.\n");
	}

	while(1) {
		// Call WPAD_ScanPads each loop, this reads the latest controller states
		PAD_ScanPads();

		// PAD_ButtonsDown tells us which buttons were pressed in this loop
		// this is a "one shot" state which will not fire again until the button has been released
		u32 pressed = PAD_ButtonsDown(0);

		// We return to the launcher application via exit
		if ( pressed & PAD_BUTTON_START ) exit(0);

		// Wait for the next frame
		VIDEO_WaitVSync();
	}

	return 0;
}
