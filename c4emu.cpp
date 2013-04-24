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
#include "snes9x.h"
#include "memmap.h"
#include "ppu.h"

void S9xInitC4 ()
{
    Memory.C4RAM = &Memory.FillRAM [0x6000];
}

uint8 S9xGetC4 (uint16 Address)
{
    return (Memory.FillRAM [Address]);
}

static uint8 C4TestPattern [12 * 4] =
{
    0x00, 0x00, 0x00, 0xff,
    0xff, 0xff, 0x00, 0xff,
    0x00, 0x00, 0x00, 0xff,
    0xff, 0xff, 0x00, 0x00,
    0xff, 0xff, 0x00, 0x00,
    0x80, 0xff, 0xff, 0x7f,
    0x00, 0x80, 0x00, 0xff,
    0x7f, 0x00, 0xff, 0x7f,
    0xff, 0x7f, 0xff, 0xff,
    0x00, 0x00, 0x01, 0xff,
    0xff, 0xfe, 0x00, 0x01,
    0x00, 0xff, 0xfe, 0x00
};

uint32 C4Timer, C4Timer2;
uint32 C4sprites, C4ObjDisp, C4count;


void S9xC4ConvOAM ()
{
    C4Timer = (C4Timer + 1) & 15;
    C4Timer2 = (C4Timer2 + 1) & 7;
    uint32 count = Memory.FillRAM [0x6620];
    uint32 bgscrollx = READ_WORD (&Memory.FillRAM [0x6621]);
    uint32 bgscrolly = READ_WORD (&Memory.FillRAM [0x6623]);
    uint8 *C4usprptr = Memory.C4RAM + 0x200 + (C4ObjDisp >> 4);
    uint8 *edi = Memory.C4RAM + C4ObjDisp;
    uint8 *esi = Memory.C4RAM + 0x220;
}

void S9xC4ProcessSprites ()
{
    C4sprites = Memory.FillRAM [0x6626];
    C4ObjDisp = C4sprites * 4;
    C4count = 32;
    uint8 *p = Memory.C4RAM + C4ObjDisp + 1;

    int i;

    for (i = 128 - C4sprites; i > 0; p += 4, i--)
	*p = 0xe0;

    S9xC4ConvOAM ();
}

void S9xSetC4 (uint8 byte, uint16 Address)
{
    int i;

    Memory.FillRAM [Address] = byte;
    if (Address == 0x7f4f)
    {
#ifdef DEBUGGER
printf ("C4: %02x\n", byte);
#endif
	switch (byte)
	{
	case 0x00: // Sprite
	{
#ifdef DEBUGGER
printf ("C4 sprite: %02x\n", Memory.FillRAM [0x7f4d]);
#endif
	    switch (Memory.FillRAM [0x7f4d])
	    {
	    case 0x00: // Srpites
		S9xC4ProcessSprites ();
		break;
	    case 0x03: // Scale
	    case 0x05: // Lines
	    case 0x07: // Rotate
	    case 0x08: // Wireframe
	    case 0x0b: // Disintergrate
	    case 0x0c: // Bitmap
		break;
	    default:
#ifdef DEBUGGER
		printf ("Unknown C4 sprite command (%02x)\n", Memory.FillRAM [0x7f4d]);
#endif
		break;
	    }
	    break;
	}
	case 0x01: // Wireframe
	case 0x05: // Propulsion
	case 0x0d: // Equate Velocity
        case 0x10: // Polar to rectangluar
	case 0x13: // ??
	case 0x15: // Calc Distance
	case 0x1f: // Calc Angle
	case 0x22: // Line
	case 0x2d: // Transform
	    break;
	case 0x5c: // Immediate Reg
	    for (i = 0; i < 12 * 4; i++)
		Memory.C4RAM [i] = C4TestPattern [i];
	    break;
	case 0x89: // Immediate ROM
	    Memory.FillRAM [0x7f80] = 0x36;
	    Memory.FillRAM [0x7f81] = 0x43;
	    Memory.FillRAM [0x7f82] = 0x05;
	    break;

	default:
#ifdef DEBUGGER
	    printf ("Unknown C4 command (%02x)\n", byte);
#endif
	    break;
	}
    }
}
