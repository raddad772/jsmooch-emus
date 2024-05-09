
        case 0xFF000000:  { return this->regs.PTEH.u; }
        case 0xFF000004:  { return this->regs.PTEL.u; }
        case 0xFF000008:  { return this->regs.TTB; }
        case 0xFF00000C:  { return this->regs.TEA; }
        case 0xFF000010:  { return this->regs.MMUCR.u; }
        case 0xFF000014:  { return this->regs.CCN_BASRA; }
        case 0xFF000018:  { return this->regs.CCN_BASRB; }
        case 0xFF00001C:  { return this->regs.CCR.u; }
        case 0xFF000020:  { return this->regs.EXPEVT; }
        case 0xFF000024:  { return this->regs.TRAPA; }
        case 0xFF000028:  { return this->regs.INTEVT; }
        case 0xFF000034:  { return this->regs.PTEA.u; }
        case 0xFF000084:  { return this->regs.PMCTR1_CTRL; }
        case 0xFF000088:  { return this->regs.PMCTR2_CTRL; }
        case 0xFF100004: { return (this->clock.trace_cycles >> 32) & 0xFFFF; }
        case 0xFF100008: { return this->clock.trace_cycles & 0xFFFFFFFF; }
        case 0xFF10000C: { return (this->clock.trace_cycles >> 32) & 0xFFFF; }
        case 0xFF100010: { return this->clock.trace_cycles & 0xFFFFFFFF; }
        case 0xFF200000:  { return this->regs.UBC_BARA; }
        case 0xFF200004:  { return this->regs.UBC_BAMRA; }
        case 0xFF200008:  { return this->regs.UBC_BBRA; }
        case 0xFF20000C:  { return this->regs.UBC_BARB; }
        case 0xFF200010:  { return this->regs.UBC_BAMRB; }
        case 0xFF200014:  { return this->regs.UBC_BBRB; }
        case 0xFF200020:  { return this->regs.UBC_BRCR; }
        case 0xFF800000:  { return this->regs.BSCR; }
        case 0xFF800004:  { return this->regs.BCR2; }
        case 0xFF800008:  { return this->regs.WCR1; }
        case 0xFF80000C:  { return this->regs.WCR2; }
        case 0xFF800010:  { return this->regs.WCR3; }
        case 0xFF800014:  { return this->regs.MCR; }
        case 0xFF800018:  { return this->regs.PCR; }
        case 0xFF80001C:  { return this->regs.RTCSR; }
        case 0xFF800024:  { return this->regs.RTCOR; }
        case 0xFF800028: { return 0xa400; }
        case 0xFF80002C:  { return this->regs.PCTRA; }
        case 0xFF800040:  { return this->regs.PCTRB; }
        case 0xFF800044:  { return this->regs.PDTRB; }
        case 0xFF800048:  { return this->regs.GPIOIC; }
        case 0xFF940190:  { return this->regs.SDMR; }
        case 0xFFA00010:  { return this->regs.DMAC_SAR1; }
        case 0xFFA00014:  { return this->regs.DMAC_DAR1; }
        case 0xFFA00018:  { return this->regs.DMAC_DMATCR1; }
        case 0xFFA0001C:  { return this->regs.DMAC_CHCR1.u | 0x00000000; }
        case 0xFFA00020:  { return this->regs.DMAC_SAR2; }
        case 0xFFA00024:  { return this->regs.DMAC_DAR2; }
        case 0xFFA00028:  { return this->regs.DMAC_DMATCR2; }
        case 0xFFA0002C:  { return this->regs.DMAC_CHCR2.u | 0x000042C0; }
        case 0xFFA00030:  { return this->regs.DMAC_SAR3; }
        case 0xFFA00034:  { return this->regs.DMAC_DAR3; }
        case 0xFFA00038:  { return this->regs.DMAC_DMATCR3; }
        case 0xFFA0003C:  { return this->regs.DMAC_CHCR3.u | 0x00000000; }
        case 0xFFC00004:  { return this->regs.STBCR | 0x00000003; }
        case 0xFFC0000C:  { return this->regs.CPG_WTCSR.u | 0x0000A500; }
        case 0xFFC80034:  { return this->regs.RMONAR; }
        case 0xFFC80038:  { return this->regs.RCR1.u | 0x00000000; }
        case 0xFFC80040:  { return this->regs.RCR2.u | 0x00000000; }
        case 0xFFD00000:  { return this->regs.ICR.u; }
        case 0xFFD00004:  { return this->regs.IPRA.u; }
        case 0xFFD00008:  { return this->regs.IPRB.u; }
        case 0xFFD0000C:  { return this->regs.IPRC.u; }
        case 0xFFE80000:  { return this->regs.SCIF_SCSMR2; }
        case 0xFFE80004:  { return this->regs.SCIF_SCBRR2; }
        case 0xFFE80008:  { return this->regs.SCIF_SCSCR2; }
        case 0xFFE8000C:  { return this->regs.SCIF_SCFTDR2; }
        case 0xFFE80010: { if (sz==DC16) return 0x60; return this->regs.SCIF_SCFSR2; }
        case 0xFFE80014:  { return this->regs.SCIF_SCFRDR2; }
        case 0xFFE80018:  { return this->regs.SCIF_SCFCR2; }
        case 0xFFE8001C:  { return this->regs.SCIF_SCFDR2; }
        case 0xFFE80020:  { return this->regs.SCIF_SCSPTR2; }
        case 0xFFE80024:  { return this->regs.SCIF_SCLSR2; }