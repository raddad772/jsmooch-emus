#pragma once

#define iwm_dBase 0xDFE1FF
#define iwm_ph0L (0*512)  // CA0 off (0)
#define iwm_ph0H (1*512)  // CA0 on (1)
#define iwm_ph1L (2*512)  // CA1 off (0)
#define iwm_ph1H (3*512)  // CA1 on (1)
#define iwm_ph2L (4*512)  // CA2 off (0)
#define iwm_ph2H (5*512)  // CA2 on (1)
#define iwm_ph3L (6*512)  // LSTRB off (low)
#define iwm_ph3H (7*512)  // LSTRB on (high)
#define iwm_mtrOff (8*512) // disk enable off
#define iwm_mtrOn (9*512)  // disk enable on
#define iwm_intDrive (10*512) // select internal drive
#define iwm_extDrive (11*512) // select external drive
#define iwm_q6L (12*512) // Q6 off
#define iwm_q6H (13*512) // Q6 on
#define iwm_q7L (14*512) // Q7 off
#define iwm_q7H (15*512) // Q7 on

#define sccRBase  0x9FFFF8
#define sccWBase  0xBFFFF9
#define scc_aData 6
#define scc_aCtl 2
#define scc_bData 4
#define scc_bCtl 0

#define vBase  0xEFE1FE
#define aVBufB vBase    // via register B
#define aVBufA 0xEFFFFE // via register A
#define aVBufM aVBufB    // mouse buffer
#define aVIFR 0xEFFBFE   // interrupt flag
#define aVIER 0xEFFDFE   // interrupt enable

// offsets from vBase
#define vBufB (512*0)  // data reg b
#define vDirB (512*2)  // direction reg b
#define vDirA (512*3)  // direction reg a
#define vT1C  (512*4)  // timer 1 count lo
#define vT1CH (512*5)  // timer 1 count hi, write causes timer start
#define vT1L  (512*6)  // timer 1 latch lo
#define vT1LH (512*7)  // timer 1 latch hi
#define vT2C  (512*8)  // timer 2 count lo
#define vT2CH (512*9)  // timer 2 count hi
#define vSR   (512*10) // shift register, keyboard
#define vACR  (512*11) // aux control register
#define vPCR  (512*12) // peripheral control register
#define vIFR  (512*13) // interrupt flag register
#define vIER  (512*14) // interrupt enable register
#define vBufA (512*15)

