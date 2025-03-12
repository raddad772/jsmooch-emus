//
// Created by . on 1/20/25.
//

#ifndef JSMOOCH_EMUS_NDS_REGS_H
#define JSMOOCH_EMUS_NDS_REGS_H

// Register defs shared

#define R_AUXSPICNT     0x040001A0
#define R_AUXSPIDATA    0x040001A2
#define R_ROMCTRL       0x040001A4
#define R_ROMCMD        0x040001A8
#define R_ROMDATA       0x04100010

#define R_KEYINPUT      0x04000130
#define R_KEYCNT        0x04000132
#define R_EXTKEYIN      0x04000136
#define R_RTC           0x04000138
#define R_IPCSYNC       0x04000180
#define R_IPCFIFOCNT    0x04000184
#define R_IPCFIFOSEND   0x04000188
#define R_POSTFLG       0x04000300
#define R_IPCFIFORECV   0x04100000

#define R_DMA0SAD 0x040000B0 // r/w
#define R_DMA1SAD 0x040000BC
#define R_DMA2SAD 0x040000C8
#define R_DMA3SAD 0x040000D4

#define R_DMA0DAD 0x040000B4 // r/w
#define R_DMA1DAD 0x040000C0
#define R_DMA2DAD 0x040000CC
#define R_DMA3DAD 0x040000D8

#define R_DMA0CNT_L 0x040000B8 // r/w
#define R_DMA1CNT_L 0x040000C4
#define R_DMA2CNT_L 0x040000D0
#define R_DMA3CNT_L 0x040000DC

#define R_IME 0x04000208
#define R_IF  0x04000214
#define R_IE  0x04000210

#define R_DMA0CNT_H 0x040000BA // r/w. lower 5 bits of this now used for word count
#define R_DMA1CNT_H 0x040000C6 // bit27 unused on arm7
#define R_DMA2CNT_H 0x040000D2
#define R_DMA3CNT_H 0x040000DE
#define R_TM0CNT_L  0x04000100
#define R_TM0CNT_H  0x04000102
#define R_TM1CNT_L  0x04000104
#define R_TM1CNT_H  0x04000106
#define R_TM2CNT_L  0x04000108
#define R_TM2CNT_H  0x0400010A
#define R_TM3CNT_L  0x0400010C
#define R_TM3CNT_H  0x0400010E

#define R9_SWAP_BUFFERS     0x04000540
#define R9_DISP3DCNT        0x04000060
#define R9_DISPCAPCNT       0x04000064
#define R9_DISP_MMEM_FIFO   0x04000068
#define R9_MASTER_BRIGHT_A  0x0400006C

#define R_RCNT 0x04000134
#define R_DISPSTAT 0x04000004
#define R9_DISPCNT 0x04000000
#define R_VCOUNT 0x04000006

#define R7_SOUNDCNT  0x04000500
#define R7_SOUNDBIAS 0x04000504

#define R9_BG0CNT   0x04000008
#define R9_BG1CNT   0x0400000A
#define R9_BG2CNT   0x0400000C
#define R9_BG3CNT   0x0400000E
#define R9_BG0HOFS  0x04000010
#define R9_BG0VOFS  0x04000012
#define R9_BG1HOFS  0x04000014
#define R9_BG1VOFS  0x04000016
#define R9_BG2HOFS  0x04000018
#define R9_BG2VOFS  0x0400001A
#define R9_BG3HOFS  0x0400001C
#define R9_BG3VOFS  0x0400001E
#define R9_BG2PA    0x04000020
#define R9_BG2PB    0x04000022
#define R9_BG2PC    0x04000024
#define R9_BG2PD    0x04000026
#define R9_BG2X     0x04000028
#define R9_BG2Y     0x0400002C
#define R9_BG3PA    0x04000030
#define R9_BG3PB    0x04000032
#define R9_BG3PC    0x04000034
#define R9_BG3PD    0x04000036
#define R9_BG3X     0x04000038
#define R9_BG3Y     0x0400003C
#define R9_WIN0H    0x04000040
#define R9_WIN1H    0x04000042
#define R9_WIN0V    0x04000044
#define R9_WIN1V    0x04000046
#define R9_WININ    0x04000048
#define R9_WINOUT   0x0400004A
#define R9_MOSAIC   0x0400004C
#define R9_BLDCNT 0x04000050
#define R9_BLDALPHA 0x04000052
#define R9_BLDY 0x04000054

// ARM7 reg defs
#define R7_EXMEMSTAT 0x04000204
#define R7_VRAMSTAT 0x04000240
#define R7_WRAMSTAT 0x04000241
#define R7_SPICNT   0x040001C0
#define R7_SPIDATA  0x040001C2
#define R7_WIFIWAITCNT 0x04000206
#define R7_HALTCNT  0x04000301
#define R7_POWCNT2  0x04000304
#define R7_BIOSPROT 0x04000308

// ARM9
#define R9_DMAFIL   0x040000E0
#define R9_EXMEMCNT 0x04000204
#define R9_VRAMCNT  0x04000240
#define R9_WRAMCNT  0x04000247
#define R9_POWCNT1  0x04000304
#define R9_DIVCNT 0x04000280
#define R9_DIV_NUMER 0x04000290
#define R9_DIV_DENOM 0x04000298
#define R9_DIV_RESULT 0x040002A0
#define R9_DIVREM_RESULT 0x040002A8
#define R9_SQRTCNT 0x040002B0
#define R9_SQRT_RESULT 0x040002B4
#define R9_SQRT_PARAM 0x040002B8

// rendering engine
#define R9_DISP3DCNT        0x04000060
#define R9_RDLINES_CNT      0x04000320
#define R9_EDGE_COLOR       0x04000330
#define R9_ALPHA_TEST_REF   0x04000340
#define R9_CLEAR_COLOR      0x04000350
#define R9_CLEAR_DEPTH      0x04000354
#define R9_CLRIMG_OFFSET    0x04000356
#define R9_FOG_COLOR        0x04000358
#define R9_FOG_OFFSET       0x0400035C
#define R9_TOON_TABLE       0x04000380

// geometry engine
#define R9_GXFIFO           0x04000400
#define R9_GXSTAT           0x04000600
#define R9_RAM_COUNT        0x04000604
#define R9_DISP_1DOT_DEPTH  0x04000610
#define R9_POS_RESULT       0x04000620
#define R9_VEC_RESULT       0x04000630
#define R9_CLIPMTX_RESULT   0x04000640
#define R9_VECMTX_RESULT    0x04000680

#define R9_G_CMD_MTX_MODE       0x04000440
#define R9_G_CMD_MTX_PUSH       0x04000444
#define R9_G_CMD_MTX_POP        0x04000448
#define R9_G_CMD_MTX_STORE      0x0400044C
#define R9_G_CMD_MTX_RESTORE    0x04000450
#define R9_G_CMD_MTX_IDENTITY   0x04000454
#define R9_G_CMD_MTX_LOAD_4x4   0x04000458
#define R9_G_CMD_MTX_LOAD_4x3   0x0400045C
#define R9_G_CMD_MTX_MULT_4x4   0x04000460
#define R9_G_CMD_MTX_MULT_4x3   0x04000464
#define R9_G_CMD_MTX_MULT_3x3   0x04000468
#define R9_G_CMD_MTX_SCALE      0x0400046C
#define R9_G_CMD_MTX_TRANS      0x04000470
#define R9_G_CMD_COLOR          0x04000480
#define R9_G_CMD_NORMAL         0x04000484
#define R9_G_CMD_TEXCOORD       0x04000488
#define R9_G_CMD_VTX_16         0x0400048C
#define R9_G_CMD_VTX_10         0x04000490
#define R9_G_CMD_VTX_XY         0x04000494
#define R9_G_CMD_VTX_XZ         0x04000498
#define R9_G_CMD_VTX_YZ         0x0400049C
#define R9_G_CMD_VTX_DIFF       0x040004A0
#define R9_G_CMD_POLYGON_ATTR   0x040004A4
#define R9_G_CMD_TEXIMAGE_PARAM 0x040004A8
#define R9_G_CMD_PLTT_BASE      0x040004AC
#define R9_G_CMD_DIFF_AMB       0x040004C0
#define R9_G_CMD_SPE_EMI        0x040004C4
#define R9_G_CMD_LIGHT_VECTOR   0x040004C8
#define R9_G_CMD_LIGHT_COLOR    0x040004CC
#define R9_G_CMD_SHININESS      0x040004D0
#define R9_G_CMD_BEGIN_VTXS     0x04000500
#define R9_G_CMD_END_VTXS       0x04000504
#define R9_G_CMD_SWAP_BUFFERS   0x04000540
#define R9_G_CMD_VIEWPORT       0x04000580
#define R9_G_CMD_BOX_TEST       0x040005C0
#define R9_G_CMD_POS_TEST       0x040005C4
#define R9_G_CMD_VEC_TEST       0x040005C8


#endif //JSMOOCH_EMUS_NDS_REGS_H
