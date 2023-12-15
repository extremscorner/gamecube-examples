/* 
 * Copyright (c) 2015-2023, Extrems <extrems@extremscorner.org>
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
#include <ogc/gx.h>
#include <ogc/pad.h>
#include <ogc/system.h>
#include <ogc/tpl.h>
#include <ogc/video.h>
#include <stdlib.h>

#include "textures_tpl.h"
#include "textures.h"

static GXRModeObj rmode;
static void *xfb;

static void *fifo;

static TPLFile tdf;
static GXTexObj texobj;

static u16 indtexdata[][4 * 4] ATTRIBUTE_ALIGN(32) = {
	{
		0x7E7E, 0x817E, 0x7E7E, 0x817E,
		0x7E81, 0x8181, 0x7E81, 0x8181,
		0x7E7E, 0x817E, 0x7E7E, 0x817E,
		0x7E81, 0x8181, 0x7E81, 0x8181,
	}, {
		0x7E80, 0x807E, 0x7E80, 0x807E,
		0x8081, 0x8180, 0x8081, 0x8180,
		0x7E80, 0x807E, 0x7E80, 0x807E,
		0x8081, 0x8180, 0x8081, 0x8180,
	}, {
		0x807E, 0x8180, 0x807E, 0x8180,
		0x7E80, 0x8081, 0x7E80, 0x8081,
		0x807E, 0x8180, 0x807E, 0x8180,
		0x7E80, 0x8081, 0x7E80, 0x8081,
	}
};

static float indtexmtx[][2][3] = {
	{
		{ +.5, +.0, +.0 },
		{ +.0, +.5, +.0 }
	}, {
		{ +.0, +.0, +.0 },
		{ +.0, +.5, +.0 }
	}, {
		{ +.5, +.0, +.0 },
		{ +.0, +.0, +.0 }
	}
};

static GXTexObj indtexobj;

static void init_video(void)
{
	VIDEO_Init();
	PAD_Init();

	VIDEO_GetPreferredMode(&rmode);
	VIDEO_Configure(&rmode);

	xfb = SYS_AllocateFramebuffer(&rmode);
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

	for (int i = 0; i < 10; i++) {
		float s = 1 << (i + 1);

		guMtxScale(m, s, s, s);
		GX_LoadTexMtxImm(m, GX_TEXMTX0 + i * 3, GX_MTX3x4);
	}

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

static void init_scale2x(void)
{
	Mtx44 projection;
	guOrtho(projection, -rmode.efbHeight / 2, rmode.efbHeight / 2, -rmode.fbWidth / 2, rmode.fbWidth / 2, 0., 1.);

	TPL_OpenTPLFromMemory(&tdf, (void *)textures_tpl, textures_tpl_size);
	TPL_GetTexture(&tdf, test, &texobj);
	GX_InitTexObjFilterMode(&texobj, GX_NEAR, GX_NEAR);
	GX_InitTexObjWrapMode(&texobj, GX_CLAMP, GX_CLAMP);

	GX_InitTexObj(&indtexobj, indtexdata[0], 2, 2, GX_TF_IA8, GX_REPEAT, GX_REPEAT, GX_FALSE);
	GX_InitTexObjFilterMode(&indtexobj, GX_NEAR, GX_NEAR);

	GX_SetBlendMode(GX_BM_NONE, GX_BL_ZERO, GX_BL_ZERO, GX_LO_CLEAR);
	GX_SetAlphaCompare(GX_ALWAYS, 0, GX_AOP_AND, GX_ALWAYS, 0);
	GX_SetZMode(GX_FALSE, GX_ALWAYS, GX_FALSE);
	GX_SetZCompLoc(GX_FALSE);

	GX_SetNumChans(0);
	GX_SetNumTexGens(6);
	GX_SetNumIndStages(1);
	GX_SetNumTevStages(7);

	GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);
	GX_SetTexCoordGen2(GX_TEXCOORD1, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY, GX_FALSE, GX_DTTMTX2);
	GX_SetTexCoordGen2(GX_TEXCOORD2, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY, GX_FALSE, GX_DTTMTX8);
	GX_SetTexCoordGen2(GX_TEXCOORD3, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY, GX_FALSE, GX_DTTMTX4);
	GX_SetTexCoordGen2(GX_TEXCOORD4, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY, GX_FALSE, GX_DTTMTX6);
	GX_SetTexCoordGen(GX_TEXCOORD5, GX_TG_MTX2x4, GX_TG_TEX0, GX_TEXMTX0);

	GX_SetIndTexOrder(GX_INDTEXSTAGE0, GX_TEXCOORD5, GX_TEXMAP5);
	GX_SetIndTexMatrix(GX_ITM_1, indtexmtx[1], 0);
	GX_SetIndTexMatrix(GX_ITM_2, indtexmtx[2], 0);

	GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD1, GX_TEXMAP0, GX_COLORNULL);
	GX_SetTevColorIn(GX_TEVSTAGE0, GX_CC_TEXC, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO);
	GX_SetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GX_SetTevDirect(GX_TEVSTAGE0);

	GX_SetTevOrder(GX_TEVSTAGE1, GX_TEXCOORD2, GX_TEXMAP0, GX_COLORNULL);
	GX_SetTevColorIn(GX_TEVSTAGE1, GX_CC_CPREV, GX_CC_TEXC, GX_CC_ZERO, GX_CC_ZERO);
	GX_SetTevColorOp(GX_TEVSTAGE1, GX_TEV_COMP_BGR24_EQ, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GX_SetTevKAlphaSel(GX_TEVSTAGE1, GX_TEV_KASEL_1);
	GX_SetTevAlphaIn(GX_TEVSTAGE1, GX_CA_ZERO, GX_CA_ZERO, GX_CA_KONST, GX_CA_ZERO);
	GX_SetTevAlphaOp(GX_TEVSTAGE1, GX_TEV_COMP_BGR24_EQ, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GX_SetTevDirect(GX_TEVSTAGE1);

	GX_SetTevOrder(GX_TEVSTAGE2, GX_TEXCOORD3, GX_TEXMAP0, GX_COLORNULL);
	GX_SetTevColorIn(GX_TEVSTAGE2, GX_CC_TEXC, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO);
	GX_SetTevColorOp(GX_TEVSTAGE2, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GX_SetTevDirect(GX_TEVSTAGE2);

	GX_SetTevOrder(GX_TEVSTAGE3, GX_TEXCOORD4, GX_TEXMAP0, GX_COLORNULL);
	GX_SetTevColorIn(GX_TEVSTAGE3, GX_CC_CPREV, GX_CC_TEXC, GX_CC_ZERO, GX_CC_ZERO);
	GX_SetTevColorOp(GX_TEVSTAGE3, GX_TEV_COMP_BGR24_EQ, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GX_SetTevKAlphaSel(GX_TEVSTAGE3, GX_TEV_KASEL_1);
	GX_SetTevAlphaIn(GX_TEVSTAGE3, GX_CA_ZERO, GX_CA_ZERO, GX_CA_KONST, GX_CA_APREV);
	GX_SetTevAlphaOp(GX_TEVSTAGE3, GX_TEV_COMP_BGR24_EQ, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GX_SetTevDirect(GX_TEVSTAGE3);

	GX_SetTevOrder(GX_TEVSTAGE4, GX_TEXCOORD0, GX_TEXMAP0, GX_COLORNULL);
	GX_SetTevColorIn(GX_TEVSTAGE4, GX_CC_TEXC, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO);
	GX_SetTevColorOp(GX_TEVSTAGE4, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GX_SetTevAlphaIn(GX_TEVSTAGE4, GX_CA_APREV, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO);
	GX_SetTevAlphaOp(GX_TEVSTAGE4, GX_TEV_SUB, GX_TB_ZERO, GX_CS_SCALE_1, GX_FALSE, GX_TEVPREV);
	GX_SetTevIndWarp(GX_TEVSTAGE4, GX_INDTEXSTAGE0, GX_TRUE, GX_FALSE, GX_ITM_1);

	GX_SetTevOrder(GX_TEVSTAGE5, GX_TEXCOORD0, GX_TEXMAP0, GX_COLORNULL);
	GX_SetTevColorIn(GX_TEVSTAGE5, GX_CC_CPREV, GX_CC_TEXC, GX_CC_CPREV, GX_CC_ZERO);
	GX_SetTevColorOp(GX_TEVSTAGE5, GX_TEV_COMP_BGR24_EQ, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GX_SetTevKAlphaSel(GX_TEVSTAGE5, GX_TEV_KASEL_1);
	GX_SetTevAlphaIn(GX_TEVSTAGE5, GX_CA_ZERO, GX_CA_ZERO, GX_CA_KONST, GX_CA_APREV);
	GX_SetTevAlphaOp(GX_TEVSTAGE5, GX_TEV_COMP_BGR24_EQ, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GX_SetTevIndWarp(GX_TEVSTAGE5, GX_INDTEXSTAGE0, GX_TRUE, GX_FALSE, GX_ITM_2);

	GX_SetTevOrder(GX_TEVSTAGE6, GX_TEXCOORD0, GX_TEXMAP0, GX_COLORNULL);
	GX_SetTevColorIn(GX_TEVSTAGE6, GX_CC_TEXC, GX_CC_CPREV, GX_CC_APREV, GX_CC_ZERO);
	GX_SetTevColorOp(GX_TEVSTAGE6, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GX_SetTevDirect(GX_TEVSTAGE6);

	GX_ClearVtxDesc();
	GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XY, GX_S16, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_S16, 0);

	GX_LoadProjectionMtx(projection, GX_ORTHOGRAPHIC);
	GX_SetCurrentMtx(GX_PNMTX0);
	GX_SetViewport(1./24., 1./24., rmode.fbWidth, rmode.efbHeight, 0., 1.);

	GX_LoadTexObj(&texobj, GX_TEXMAP0);

	GX_LoadTexObj(&indtexobj, GX_TEXMAP5);
}

static void draw_scale2x(void)
{
	void *ptr;
	u16 width, height;
	u8 format, wrap_s, wrap_t, mipmap;

	GX_GetTexObjAll(&texobj, &ptr, &width, &height, &format, &wrap_s, &wrap_t, &mipmap);

	GX_Begin(GX_QUADS, GX_VTXFMT0, 4);

	GX_Position2s16(-width, -height);
	GX_TexCoord2s16(0, 0);

	GX_Position2s16(width, -height);
	GX_TexCoord2s16(width, 0);

	GX_Position2s16(width, height);
	GX_TexCoord2s16(width, height);

	GX_Position2s16(-width, height);
	GX_TexCoord2s16(0, height);

	GX_Flush();
}

int main(int argc, char **argv)
{
	init_video();
	init_graphics();
	init_scale2x();

	while (true) {
		draw_scale2x();

		VIDEO_WaitVSync();
		PAD_ScanPads();

		GX_CopyDisp(xfb, GX_TRUE);
		GX_DrawDone();

		for (int chan = 0; chan < PAD_CHANMAX; chan++) {
			u16 down = PAD_ButtonsDown(chan);
			if (down & PAD_BUTTON_START)
				exit(EXIT_SUCCESS);
		}
	}

	return EXIT_SUCCESS;
}
