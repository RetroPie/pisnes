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
#include "fxemu.h"
#include "fxinst.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* The FxChip Emulator's internal variables */
struct FxRegs_s GSU = {0};

uint32 (**fx_ppfFunctionTable)(uint32, struct FxRegs_s * gsu) = 0;
void (**fx_ppfPlotTable)(struct FxRegs_s * gsu) = 0;
void (**fx_ppfOpcodeTable)(struct FxRegs_s * gsu) = 0;

#if 0
void fx_setCache()
{
    uint32 c;
    gsu->bCacheActive = TRUE;
    gsu->pvRegisters[0x3e] &= 0xf0;
    c = (uint32)gsu->pvRegisters[0x3e];
    c |= ((uint32)gsu->pvRegisters[0x3f])<<8;
    if(c == gsu->vCacheBaseReg)
	return;
    gsu->vCacheBaseReg = c;
    gsu->vCacheFlags = 0;
    if(c < (0x10000-512))
    {
	uint8 const* t = &ROM(c);
	memcpy(gsu->pvCache,t,512);
    }
    else
    {
	uint8 const* t1;
	uint8 const* t2;
	uint32 i = 0x10000 - c;
	t1 = &ROM(c);
	t2 = &ROM(0);
	memcpy(gsu->pvCache,t1,i);
	memcpy(&gsu->pvCache[i],t2,512-i);
    }
}
#endif

void FxCacheWriteAccess(uint16 vAddress, struct FxRegs_s * gsu)
{
#if 0
    if(!gsu->bCacheActive)
    {
	uint8 v = gsu->pvCache[gsu->pvCache[vAddress&0x1ff];
	fx_setCache();
	gsu->pvCache[gsu->pvCache[vAddress&0x1ff] = v;
    }
#endif
    if((vAddress & 0x00f) == 0x00f)
	gsu->vCacheFlags |= 1 << ((vAddress&0x1f0) >> 4);
}

void FxFlushCache(struct FxRegs_s * gsu)
{
    gsu->vCacheFlags = 0;
    gsu->vCacheBaseReg = 0;
    gsu->bCacheActive = FALSE;
//    gsu->vPipe = 0x1;
}

static void fx_backupCache()
{
#if 0
    uint32 i;
    uint32 v = gsu->vCacheFlags;
    uint32 c = USEX16(gsu->vCacheBaseReg);
    if(v)
	for(i=0; i<32; i++)
	{
	    if(v&1)
	    {
		if(c < (0x10000-16))
		{
		    uint8 * t = &gsu->pvPrgBank[c];
		    memcpy(&gsu->avCacheBackup[i<<4],t,16);
		    memcpy(t,&gsu->pvCache[i<<4],16);
		}
		else
		{
		    uint8 * t1;
		    uint8 * t2;
		    uint32 a = 0x10000 - c;
		    t1 = &gsu->pvPrgBank[c];
		    t2 = &gsu->pvPrgBank[0];
		    memcpy(&gsu->avCacheBackup[i<<4],t1,a);
		    memcpy(t1,&gsu->pvCache[i<<4],a);
		    memcpy(&gsu->avCacheBackup[(i<<4)+a],t2,16-a);
		    memcpy(t2,&gsu->pvCache[(i<<4)+a],16-a);
		}		
	    }
	    c = USEX16(c+16);
	    v >>= 1;
	}
#endif
}

static void fx_restoreCache()
{
#if 0
    uint32 i;
    uint32 v = gsu->vCacheFlags;
    uint32 c = USEX16(gsu->vCacheBaseReg);
    if(v)
	for(i=0; i<32; i++)
	{
	    if(v&1)
	    {
		if(c < (0x10000-16))
		{
		    uint8 * t = &gsu->pvPrgBank[c];
		    memcpy(t,&gsu->avCacheBackup[i<<4],16);
		    memcpy(&gsu->pvCache[i<<4],t,16);
		}
		else
		{
		    uint8 * t1;
		    uint8 * t2;
		    uint32 a = 0x10000 - c;
		    t1 = &gsu->pvPrgBank[c];
		    t2 = &gsu->pvPrgBank[0];
		    memcpy(t1,&gsu->avCacheBackup[i<<4],a);
		    memcpy(&gsu->pvCache[i<<4],t1,a);
		    memcpy(t2,&gsu->avCacheBackup[(i<<4)+a],16-a);
		    memcpy(&gsu->pvCache[(i<<4)+a],t2,16-a);
		}		
	    }
	    c = USEX16(c+16);
	    v >>= 1;
	}
#endif
}

void fx_flushCache(struct FxRegs_s * gsu)
{
    fx_restoreCache();
    gsu->vCacheFlags = 0;
    gsu->bCacheActive = FALSE;
}

static void fx_readRegisterSpace(struct FxRegs_s * gsu)
{
    int i;
    uint8 *p;
    static uint32 avHeight[] = { 128, 160, 192, 256 };
    static uint32 avMult[] = { 16, 32, 32, 64 };

    gsu->vErrorCode = 0;

    /* Update R0-R15 */
    p = gsu->pvRegisters;
    for(i=0; i<16; i++)
    {
	gsu->avReg[i] = *p++;
	gsu->avReg[i] += ((uint32)(*p++)) << 8;
    }

    /* Update other registers */
    p = gsu->pvRegisters;
    gsu->vStatusReg = (uint32)p[GSU_SFR];
    gsu->vStatusReg |= ((uint32)p[GSU_SFR+1]) << 8;
    gsu->vPrgBankReg = (uint32)p[GSU_PBR];
    gsu->vRomBankReg = (uint32)p[GSU_ROMBR];
    gsu->vRamBankReg = ((uint32)p[GSU_RAMBR]) & (FX_RAM_BANKS-1);
    gsu->vCacheBaseReg = (uint32)p[GSU_CBR];
    gsu->vCacheBaseReg |= ((uint32)p[GSU_CBR+1]) << 8;

    /* Update status register variables */
    gsu->vZero = !(gsu->vStatusReg & FLG_Z);
    gsu->vSign = (gsu->vStatusReg & FLG_S) << 12;
    gsu->vOverflow = (gsu->vStatusReg & FLG_OV) << 16;
    gsu->vCarry = (gsu->vStatusReg & FLG_CY) >> 2;
    
    /* Set bank pointers */
    gsu->pvRamBank = gsu->apvRamBank[gsu->vRamBankReg & 0x3];
    gsu->pvRomBank = gsu->apvRomBank[gsu->vRomBankReg];
    gsu->pvPrgBank = gsu->apvRomBank[gsu->vPrgBankReg];

    /* Set screen pointers */
    gsu->pvScreenBase = &gsu->pvRam[ USEX8(p[GSU_SCBR]) << 10 ];
    i = (int)(!!(p[GSU_SCMR] & 0x04));
    i |= ((int)(!!(p[GSU_SCMR] & 0x20))) << 1;
    gsu->vScreenHeight = gsu->vScreenRealHeight = avHeight[i];
    gsu->vMode = p[GSU_SCMR] & 0x03;
#if 0
    if(gsu->vMode == 2)
	error illegal color depth gsu->vMode;
#endif
    if(i == 3)
	gsu->vScreenSize = (256/8) * (256/8) * 32;
    else
	gsu->vScreenSize = (gsu->vScreenHeight/8) * (256/8) * avMult[gsu->vMode];
    if (gsu->vPlotOptionReg & 0x10)
    {
	/* OBJ Mode (for drawing into sprites) */
	gsu->vScreenHeight = 256;
    }
#if 0
    if(gsu->pvScreenBase + gsu->vScreenSize > gsu->pvRam + (gsu->nRamBanks * 65536))
	error illegal address for screen base register
#else
    if(gsu->pvScreenBase + gsu->vScreenSize > gsu->pvRam + (gsu->nRamBanks * 65536))
	gsu->pvScreenBase =  gsu->pvRam + (gsu->nRamBanks * 65536) - gsu->vScreenSize;
#endif
    gsu->pfPlot = fx_apfPlotTable[gsu->vMode];
    gsu->pfRpix = fx_apfPlotTable[gsu->vMode + 5];

    fx_ppfOpcodeTable[0x04c] = gsu->pfPlot;
    fx_ppfOpcodeTable[0x14c] = gsu->pfRpix;
    fx_ppfOpcodeTable[0x24c] = gsu->pfPlot;
    fx_ppfOpcodeTable[0x34c] = gsu->pfRpix;

    fx_computeScreenPointers (gsu);

    fx_backupCache();
}

void fx_computeScreenPointers (struct FxRegs_s * gsu)
{
    if (gsu->vMode != gsu->vPrevMode || 
	gsu->vPrevScreenHeight != gsu->vScreenHeight)
    {
	int i;

	/* Make a list of pointers to the start of each screen column */
	switch (gsu->vScreenHeight)
	{
	    case 128:
		switch (gsu->vMode)
		{
		    case 0:
			for (i = 0; i < 32; i++)
			{
			    gsu->apvScreen[i] = gsu->pvScreenBase + (i << 4);
			    gsu->x[i] = i << 8;
			}
			break;
		    case 1:
			for (i = 0; i < 32; i++)
			{
			    gsu->apvScreen[i] = gsu->pvScreenBase + (i << 5);
			    gsu->x[i] = i << 9;
			}
			break;
		    case 2:
		    case 3:
			for (i = 0; i < 32; i++)
			{
			    gsu->apvScreen[i] = gsu->pvScreenBase + (i << 6);
			    gsu->x[i] = i << 10;
			}
			break;
		}
		break;
	    case 160:
		switch (gsu->vMode)
		{
		    case 0:
			for (i = 0; i < 32; i++)
			{
			    gsu->apvScreen[i] = gsu->pvScreenBase + (i << 4);
			    gsu->x[i] = (i << 8) + (i << 6);
			}
			break;
		    case 1:
			for (i = 0; i < 32; i++)
			{
			    gsu->apvScreen[i] = gsu->pvScreenBase + (i << 5);
			    gsu->x[i] = (i << 9) + (i << 7);
			}
			break;
		    case 2:
		    case 3:
			for (i = 0; i < 32; i++)
			{
			    gsu->apvScreen[i] = gsu->pvScreenBase + (i << 6);
			    gsu->x[i] = (i << 10) + (i << 8);
			}
			break;
		}
		break;
	    case 192:
		switch (gsu->vMode)
		{
		    case 0:
			for (i = 0; i < 32; i++)
			{
			    gsu->apvScreen[i] = gsu->pvScreenBase + (i << 4);
			    gsu->x[i] = (i << 8) + (i << 7);
			}
			break;
		    case 1:
			for (i = 0; i < 32; i++)
			{
			    gsu->apvScreen[i] = gsu->pvScreenBase + (i << 5);
			    gsu->x[i] = (i << 9) + (i << 8);
			}
			break;
		    case 2:
		    case 3:
			for (i = 0; i < 32; i++)
			{
			    gsu->apvScreen[i] = gsu->pvScreenBase + (i << 6);
			    gsu->x[i] = (i << 10) + (i << 9);
			}
			break;
		}
		break;
	    case 256:
		switch (gsu->vMode)
		{
		    case 0:
			for (i = 0; i < 32; i++)
			{
			    gsu->apvScreen[i] = gsu->pvScreenBase + 
				((i & 0x10) << 9) + ((i & 0xf) << 8);
			    gsu->x[i] = ((i & 0x10) << 8) + ((i & 0xf) << 4);
			}
			break;
		    case 1:
			for (i = 0; i < 32; i++)
			{
			    gsu->apvScreen[i] = gsu->pvScreenBase + 
				((i & 0x10) << 10) + ((i & 0xf) << 9);
			    gsu->x[i] = ((i & 0x10) << 9) + ((i & 0xf) << 5);
			}
			break;
		    case 2:
		    case 3:
			for (i = 0; i < 32; i++)
			{
			    gsu->apvScreen[i] = gsu->pvScreenBase + 
				((i & 0x10) << 11) + ((i & 0xf) << 10);
			    gsu->x[i] = ((i & 0x10) << 10) + ((i & 0xf) << 6);
			}
			break;
		}
		break;
	}
	gsu->vPrevMode = gsu->vMode;
	gsu->vPrevScreenHeight = gsu->vScreenHeight;
    }
}

static void fx_writeRegisterSpace(struct FxRegs_s * gsu)
{
    int i;
    uint8 *p;
    
    p = gsu->pvRegisters;
    for(i=0; i<16; i++)
    {
	*p++ = (uint8)gsu->avReg[i];
	*p++ = (uint8)(gsu->avReg[i] >> 8);
    }

    /* Update status register */
    if( USEX16(gsu->vZero) == 0 ) SF(Z);
    else CF(Z);
    if( gsu->vSign & 0x8000 ) SF(S);
    else CF(S);
    if(gsu->vOverflow >= 0x8000 || gsu->vOverflow < -0x8000) SF(OV);
    else CF(OV);
    if(gsu->vCarry) SF(CY);
    else CF(CY);
    
    p = gsu->pvRegisters;
    p[GSU_SFR] = (uint8)gsu->vStatusReg;
    p[GSU_SFR+1] = (uint8)(gsu->vStatusReg>>8);
    p[GSU_PBR] = (uint8)gsu->vPrgBankReg;
    p[GSU_ROMBR] = (uint8)gsu->vRomBankReg;
    p[GSU_RAMBR] = (uint8)gsu->vRamBankReg;
    p[GSU_CBR] = (uint8)gsu->vCacheBaseReg;
    p[GSU_CBR+1] = (uint8)(gsu->vCacheBaseReg>>8);
    
    fx_restoreCache();
}

/* Reset the FxChip */
void FxReset(struct FxInit_s *psFxInfo)
{
    int i;
    static uint32 (**appfFunction[])(uint32, struct FxRegs_s * gsu) = {
	&fx_apfFunctionTable[0],
#if 0
	&fx_a_apfFunctionTable[0],
	&fx_r_apfFunctionTable[0],
	&fx_ar_apfFunctionTable[0],
#endif	
    };
    static void (**appfPlot[])(struct FxRegs_s * gsu) = {
	&fx_apfPlotTable[0],
#if 0
	&fx_a_apfPlotTable[0],
	&fx_r_apfPlotTable[0],
	&fx_ar_apfPlotTable[0],
#endif	
    };
    static void (**appfOpcode[])(struct FxRegs_s * gsu) = {
	&fx_apfOpcodeTable[0],
#if 0	
	&fx_a_apfOpcodeTable[0],
	&fx_r_apfOpcodeTable[0],
	&fx_ar_apfOpcodeTable[0],
#endif	
    };

    /* Get function pointers for the current emulation mode */
    fx_ppfFunctionTable = appfFunction[psFxInfo->vFlags & 0x3];
    fx_ppfPlotTable = appfPlot[psFxInfo->vFlags & 0x3];
    fx_ppfOpcodeTable = appfOpcode[psFxInfo->vFlags & 0x3];
    
    /* Clear all internal variables */
    memset((uint8*)&GSU,0,sizeof(struct FxRegs_s));
	struct FxRegs_s * gsu = &GSU;
    /* Set default registers */
    gsu->pvSreg = gsu->pvDreg = &R0;

    /* Set RAM and ROM pointers */
    gsu->pvRegisters = psFxInfo->pvRegisters;
    gsu->nRamBanks = psFxInfo->nRamBanks;
    gsu->pvRam = psFxInfo->pvRam;
    gsu->nRomBanks = psFxInfo->nRomBanks;
    gsu->pvRom = psFxInfo->pvRom;
    gsu->vPrevScreenHeight = ~0;
    gsu->vPrevMode = ~0;

    /* The GSU can't access more than 2mb (16mbits) */
    if(gsu->nRomBanks > 0x20)
	gsu->nRomBanks = 0x20;
    
    /* Clear FxChip register space */
    memset(gsu->pvRegisters,0,0x300);

    /* Set FxChip version Number */
    gsu->pvRegisters[0x3b] = 0;

    /* Make ROM bank table */
    for(i=0; i<256; i++)
    {
	uint32 b = i & 0x7f;
	if (b >= 0x40)
	{
	    if (gsu->nRomBanks > 1)
		b %= gsu->nRomBanks;
	    else
		b &= 1;

	    gsu->apvRomBank[i] = &gsu->pvRom[ b << 16 ];
	}
	else
	{
	    b %= gsu->nRomBanks * 2;
	    gsu->apvRomBank[i] = &gsu->pvRom[ (b << 16) + 0x200000];
	}
    }

    /* Make RAM bank table */
    for(i=0; i<4; i++)
    {
	gsu->apvRamBank[i] = &gsu->pvRam[(i % gsu->nRamBanks) << 16];
	gsu->apvRomBank[0x70 + i] = gsu->apvRamBank[i];
    }
    
    /* Start with a nop in the pipe */
    gsu->vPipe = 0x01;

    /* Set pointer to GSU cache */
    gsu->pvCache = &gsu->pvRegisters[0x100];

    fx_readRegisterSpace(gsu);
}

static uint8 fx_checkStartAddress(struct FxRegs_s * gsu)
{
    /* Check if we start inside the cache */
    if(gsu->bCacheActive && R15 >= gsu->vCacheBaseReg && R15 < (gsu->vCacheBaseReg+512))
	return TRUE;
   
    /*  Check if we're in an unused area */
    if(gsu->vPrgBankReg < 0x40 && R15 < 0x8000)
	return FALSE;
    if(gsu->vPrgBankReg >= 0x60 && gsu->vPrgBankReg <= 0x6f)
	return FALSE;
    if(gsu->vPrgBankReg >= 0x74)
	return FALSE;

    /* Check if we're in RAM and the RAN flag is not set */
    if(gsu->vPrgBankReg >= 0x70 && gsu->vPrgBankReg <= 0x73 && !(SCMR&(1<<3)) )
	return FALSE;

    /* If not, we're in ROM, so check if the RON flag is set */
    if(!(SCMR&(1<<4)))
	return FALSE;
    
    return TRUE;
}

/* Execute until the next stop instruction */
int FxEmulate(uint32 nInstructions)
{
	struct FxRegs_s * gsu = &GSU;
    uint32 vCount;

    /* Read registers and initialize GSU session */
    fx_readRegisterSpace(gsu);

    /* Check if the start address is valid */
    if(!fx_checkStartAddress(gsu))
    {
	CF(G);
	fx_writeRegisterSpace(gsu);
#if 0
	gsu->vIllegalAddress = (gsu->vPrgBankReg << 24) | R15;
	return FX_ERROR_ILLEGAL_ADDRESS;
#else
	return 0;
#endif
    }

    /* Execute GSU session */
    CF(IRQ);

    if(gsu->bBreakPoint)
	vCount = fx_ppfFunctionTable[FX_FUNCTION_RUN_TO_BREAKPOINT](nInstructions, gsu);
    else
	vCount = fx_ppfFunctionTable[FX_FUNCTION_RUN](nInstructions, gsu);

    /* Store GSU registers */
    fx_writeRegisterSpace(gsu);

    /* Check for error code */
    if(gsu->vErrorCode)
	return gsu->vErrorCode;
    else
	return vCount;
}

/* Breakpoints */
void FxBreakPointSet(uint32 vAddress, struct FxRegs_s * gsu)
{
    gsu->bBreakPoint = TRUE;
    gsu->vBreakPoint = USEX16(vAddress);
}
void FxBreakPointClear(struct FxRegs_s * gsu)
{
    gsu->bBreakPoint = FALSE;
}

/* Step by step execution */
int FxStepOver(uint32 nInstructions, struct FxRegs_s * gsu)
{
    uint32 vCount;
    fx_readRegisterSpace(gsu);

    /* Check if the start address is valid */
    if(!fx_checkStartAddress(gsu))
    {
	CF(G);
#if 0
	gsu->vIllegalAddress = (gsu->vPrgBankReg << 24) | R15;
	return FX_ERROR_ILLEGAL_ADDRESS;
#else
	return 0;
#endif
    }
    
    if( PIPE >= 0xf0 )
	gsu->vStepPoint = USEX16(R15+3);
    else if( (PIPE >= 0x05 && PIPE <= 0x0f) || (PIPE >= 0xa0 && PIPE <= 0xaf) )
	gsu->vStepPoint = USEX16(R15+2);
    else
	gsu->vStepPoint = USEX16(R15+1);
    vCount = fx_ppfFunctionTable[FX_FUNCTION_STEP_OVER](nInstructions, gsu);
    fx_writeRegisterSpace(gsu);
    if(gsu->vErrorCode)
	return gsu->vErrorCode;
    else
	return vCount;
}

/* Errors */
int FxGetErrorCode(struct FxRegs_s * gsu)
{
    return gsu->vErrorCode;
}

int FxGetIllegalAddress(struct FxRegs_s * gsu)
{
    return gsu->vIllegalAddress;
}

/* Access to internal registers */
uint32 FxGetColorRegister(struct FxRegs_s * gsu)
{
    return gsu->vColorReg & 0xff;
}

uint32 FxGetPlotOptionRegister(struct FxRegs_s * gsu)
{
    return gsu->vPlotOptionReg & 0x1f;
}

uint32 FxGetSourceRegisterIndex(struct FxRegs_s * gsu)
{
    return gsu->pvSreg - gsu->avReg;
}

uint32 FxGetDestinationRegisterIndex(struct FxRegs_s * gsu)
{
    return gsu->pvDreg - gsu->avReg;
}

uint8 FxPipe(struct FxRegs_s * gsu)
{
    return gsu->vPipe;
}
