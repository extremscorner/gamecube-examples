#include <gccore.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ogcsys.h>
#include <time.h>

#include "input.h"

#define DemoFileName "MemCardDemo.dat"

static void *xfb = NULL;

static u8 SysArea[CARD_WORKAREA] ATTRIBUTE_ALIGN(32);

u32 first_frame = 1;
GXRModeObj *rmode;
void (*PSOreload)() = (void(*)())0x80001800;


/*---------------------------------------------------------------------------------
	This function is called if a card is physically removed
---------------------------------------------------------------------------------*/
void card_removed(s32 chn,s32 result) {
//---------------------------------------------------------------------------------
	printf("card was removed from slot %c\n",(chn==0)?'A':'B');
	CARD_Unmount(chn);
}

//---------------------------------------------------------------------------------
int main() {
//---------------------------------------------------------------------------------
	VIDEO_Init();
	
	switch(VIDEO_GetCurrentTvMode())
	{
		case VI_NTSC:
			rmode = &TVNtsc480IntDf;
			break;
		case VI_PAL:
			rmode = &TVPal528IntDf;
			break;
		case VI_MPAL:
			rmode = &TVMpal480IntDf;
			break;
		default:
			rmode = &TVNtsc480IntDf;
			break;
	}

	PAD_Init();
	
	xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
		
	VIDEO_Configure(rmode);
		
	VIDEO_SetNextFramebuffer(xfb);
	
	VIDEO_SetBlack(FALSE);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	if(rmode->viTVMode&VI_NON_INTERLACE) VIDEO_WaitVSync();
	VIDEO_SetPreRetraceCallback(ScanPads);
	console_init(xfb,20,64,rmode->fbWidth,rmode->xfbHeight,rmode->fbWidth*2);

	printf("Memory Card Demo\n\n");

	while (1) {
		printf("Insert A card in slot B and press A\n");


		while ( !(ButtonsDown(0) & PAD_BUTTON_A)) {
			if (ButtonsDown(0) & PAD_BUTTON_START) PSOreload();
			VIDEO_WaitVSync();
		}

		printf("Mounting card ...\n");

		CARD_Init("\xffDEMO","\xff00");
		int SlotB_error = CARD_Mount(CARD_SLOTB, SysArea, card_removed);
	
		printf("slot B code %d\n",SlotB_error);

		if (SlotB_error > 0) {

			int SectorSize = 0;
			CARD_GetSectorSize(CARD_SLOTB,&SectorSize);

			printf("Sector size is %d bytes.\n\n",SectorSize);

			char *CardBuffer = (char *)memalign(32,SectorSize);
			
			printf("Starting directory\n");

			card_dir CardDir;
			card_file CardFile;
			
			int CardError = CARD_FindFirst(CARD_SLOTB, &CardDir);

			bool found = false;
			
			while ( CARD_ERROR_NOFILE != CardError ) {
				printf("%s  %s  %s\n",CardDir.filename,CardDir.gamecode,CardDir.company);
				CardError = CARD_FindNext(&CardDir);
				if ( 0 == strcmp (DemoFileName, CardDir.filename)) found = true; 
			};

			printf("Finished directory\n\n");
			
			if (found) {
				printf("Test file contains :- \n");
				//CardError = CARD_Open(CARD_SLOTB ,"MemCardDemo.bin",&CardFile);
				//CARD_Read(&CardFile,CardBuffer,SectorSize,0);
				//printf(CardBuffer);
				
				//CARD_Close(&CardFile);
			
			} else {
			
				printf("writing test file ...\n");

				CARD_Unmount(CARD_SLOTB);
				CARD_Init("DEMO","00");
				int SlotB_error = CARD_Mount(CARD_SLOTB, SysArea, card_removed);

				printf("mount code %d\n",SlotB_error);

				CardError = CARD_Create(CARD_SLOTB ,DemoFileName,SectorSize,&CardFile);
				
				printf("error code %d\n",CardError);

				if (0 == CardError) {
					time_t gc_time;
					gc_time = time(NULL);

					sprintf(CardBuffer," This text was written by MemCardDemo at %s\n\0",ctime(&gc_time));

					CardError = CARD_Write(&CardFile,CardBuffer,SectorSize,0);
					printf("error code %d\n",CardError);
					CardError = CARD_Close(&CardFile);
					printf("error code %d\n",CardError);
					}

			}

			CARD_Unmount(CARD_SLOTB);
			free(CardBuffer);
			
		}
	}

}
