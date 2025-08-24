/* 
 * Copyright (c) 2023-2025, Extrems <extrems@extremscorner.org>
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <malloc.h>
#include <ogc/color.h>
#include <ogc/gx.h>
#include <ogc/pad.h>
#include <ogc/system.h>
#include <ogc/timesupp.h>
#include <ogc/video.h>
#include <stdlib.h>

static GXRModeObj rmode;
static void *xfb;

static void *fifo;

static void *texdata;
static GXTexObj texobj;

static void init_video(void)
{
	VIDEO_Init();
	PAD_Init();

	VIDEO_GetPreferredMode(&rmode);
	VIDEO_Configure(&rmode);

	xfb = SYS_AllocateFramebuffer(&rmode);
	VIDEO_ClearFrameBuffer(&rmode, xfb, COLOR_BLACK);
	VIDEO_SetNextFramebuffer(xfb);

	VIDEO_SetBlack(false);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	VIDEO_WaitVSync();
}

static void init_graphics(void)
{
	Mtx m;

	fifo = memalign(32, GX_FIFO_MINSIZE);
	GX_Init(fifo, GX_FIFO_MINSIZE);

	for (int i = GX_TEXCOORD0; i < GX_MAXCOORD; i++)
		GX_SetTexCoordScaleManually(i, GX_TRUE, 1, 1);

	guMtxTrans(m, 1./64., 1./64., 0);
	GX_LoadTexMtxImm(m, GX_DTTIDENTITY, GX_MTX3x4);

	for (int i = 0; i < 9; i++) {
		float x = i % 3 - 1;
		float y = i / 3 - 1;

		x += 1./64.;
		y += 1./64.;

		guMtxTrans(m, x, y, 0);
		GX_LoadTexMtxImm(m, GX_DTTMTX1 + i * 3, GX_MTX3x4);
	}

	GX_SetCopyClear((GXColor){0, 0, 0, 0}, GX_MAX_Z24);
	GX_SetCopyFilter(GX_FALSE, NULL, GX_FALSE, NULL);

	GX_SetScissor(0, 0, rmode.fbWidth, rmode.efbHeight);
	GX_SetScissorBoxOffset(0, 0);
	GX_ClearBoundingBox();

	GX_SetDispCopySrc(0, 0, rmode.fbWidth, rmode.efbHeight);
	GX_SetDispCopyYScale(GX_GetYScaleFactor(rmode.efbHeight, rmode.xfbHeight));
	GX_SetDispCopyDst(rmode.fbWidth, rmode.xfbHeight);

	GX_CopyDisp(xfb, GX_TRUE);
	GX_DrawDone();
}

static void init_conway(void)
{
	Mtx44 projection;
	guOrtho(projection, 0., 1024., 0., 1024., 0., 1.);

	texdata = memalign(32, GX_GetTexBufferSize(rmode.fbWidth, rmode.efbHeight, GX_TF_I4, GX_FALSE, 0));
	GX_InitTexObj(&texobj, texdata, rmode.fbWidth, rmode.efbHeight, GX_TF_I4, GX_CLAMP, GX_CLAMP, GX_FALSE);
	GX_InitTexObjFilterMode(&texobj, GX_NEAR, GX_NEAR);

	GX_SetBlendMode(GX_BM_NONE, GX_BL_ZERO, GX_BL_ZERO, GX_LO_CLEAR);
	GX_SetAlphaCompare(GX_NEQUAL, 64, GX_AOP_AND, GX_NEQUAL, 64);
	GX_SetZMode(GX_FALSE, GX_ALWAYS, GX_FALSE);
	GX_SetZCompLoc(GX_FALSE);

	GX_SetNumChans(0);
	GX_SetNumTexGens(8);
	GX_SetNumIndStages(0);
	GX_SetNumTevStages(9);

	GX_SetTexCoordGen2(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_POS, GX_IDENTITY, GX_FALSE, GX_DTTMTX1);
	GX_SetTexCoordGen2(GX_TEXCOORD1, GX_TG_MTX2x4, GX_TG_POS, GX_IDENTITY, GX_FALSE, GX_DTTMTX2);
	GX_SetTexCoordGen2(GX_TEXCOORD2, GX_TG_MTX2x4, GX_TG_POS, GX_IDENTITY, GX_FALSE, GX_DTTMTX3);
	GX_SetTexCoordGen2(GX_TEXCOORD3, GX_TG_MTX2x4, GX_TG_POS, GX_IDENTITY, GX_FALSE, GX_DTTMTX4);
	GX_SetTexCoordGen2(GX_TEXCOORD4, GX_TG_MTX2x4, GX_TG_POS, GX_IDENTITY, GX_FALSE, GX_DTTMTX6);
	GX_SetTexCoordGen2(GX_TEXCOORD5, GX_TG_MTX2x4, GX_TG_POS, GX_IDENTITY, GX_FALSE, GX_DTTMTX7);
	GX_SetTexCoordGen2(GX_TEXCOORD6, GX_TG_MTX2x4, GX_TG_POS, GX_IDENTITY, GX_FALSE, GX_DTTMTX8);
	GX_SetTexCoordGen2(GX_TEXCOORD7, GX_TG_MTX2x4, GX_TG_POS, GX_IDENTITY, GX_FALSE, GX_DTTMTX9);

	for (int i = 0; i < 8; i++) {
		GX_SetTevOrder(GX_TEVSTAGE0 + i, GX_TEXCOORD0 + i, GX_TEXMAP0, GX_COLORNULL);
		GX_SetTevKColorSel(GX_TEVSTAGE0 + i, GX_TEV_KCSEL_1_8);
		GX_SetTevColorIn(GX_TEVSTAGE0 + i, GX_CC_ZERO, GX_CC_TEXC, GX_CC_KONST, i == 0 ? GX_CC_ZERO : GX_CC_CPREV);
		GX_SetTevColorOp(GX_TEVSTAGE0 + i, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
		GX_SetTevKAlphaSel(GX_TEVSTAGE0 + i, GX_TEV_KASEL_1_8);
		GX_SetTevAlphaIn(GX_TEVSTAGE0 + i, GX_CA_ZERO, GX_CA_TEXA, GX_CA_KONST, i == 0 ? GX_CA_ZERO : GX_CA_APREV);
		GX_SetTevAlphaOp(GX_TEVSTAGE0 + i, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
		GX_SetTevDirect(GX_TEVSTAGE0 + i);
	}

	GX_SetTevOrder(GX_TEVSTAGE8, GX_TEXCOORDNULL, GX_TEXMAP_NULL, GX_COLORNULL);
	GX_SetTevKColorSel(GX_TEVSTAGE8, GX_TEV_KCSEL_3_8);
	GX_SetTevColorIn(GX_TEVSTAGE8, GX_CC_CPREV, GX_CC_KONST, GX_CC_ONE, GX_CC_ZERO);
	GX_SetTevColorOp(GX_TEVSTAGE8, GX_TEV_COMP_R8_EQ, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GX_SetTevAlphaIn(GX_TEVSTAGE8, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_APREV);
	GX_SetTevAlphaOp(GX_TEVSTAGE8, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GX_SetTevDirect(GX_TEVSTAGE8);

	GX_ClearVtxDesc();
	GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XY, GX_U16, 0);

	GX_LoadProjectionMtx(projection, GX_ORTHOGRAPHIC);
	GX_SetCurrentMtx(GX_PNMTX0);
	GX_SetViewport(0., 0., 1024., 1024., 0., 1.);

	GX_LoadTexObj(&texobj, GX_TEXMAP0);

	GX_SetTexCopySrc(0, 0, rmode.fbWidth, rmode.efbHeight);
	GX_SetTexCopyDst(rmode.fbWidth, rmode.efbHeight, GX_CTF_R4, GX_FALSE);
}

static void draw_conway(void)
{
	GX_CopyTex(texdata, GX_FALSE);
	GX_InvalidateTexAll();

	GX_Begin(GX_QUADS, GX_VTXFMT0, 4);

	GX_Position2u16(1, rmode.efbHeight - 1);
	GX_Position2u16(1, 1);
	GX_Position2u16(rmode.fbWidth - 1, 1);
	GX_Position2u16(rmode.fbWidth - 1, rmode.efbHeight - 1);

	GX_Flush();
}

static void randomize(void)
{
	srand48(gettick());

	for (int y = 1; y < rmode.efbHeight - 1; y++)
		for (int x = 1; x < rmode.fbWidth - 1; x++)
			if (mrand48() % 2)
				GX_PokeARGB(x, y, (GXColor){255, 255, 255, 255});
}

int main(int argc, char **argv)
{
	init_video();
	init_graphics();
	init_conway();
	randomize();

	while (true) {
		draw_conway();

		VIDEO_WaitVSync();
		PAD_ScanPads();

		GX_CopyDisp(xfb, GX_FALSE);
		GX_DrawDone();

		for (int chan = 0; chan < PAD_CHANMAX; chan++) {
			u16 down = PAD_ButtonsDown(chan);
			if (down & PAD_BUTTON_A)
				randomize();
			if (down & PAD_BUTTON_START)
				exit(EXIT_SUCCESS);
		}
	}

	return EXIT_SUCCESS;
}
