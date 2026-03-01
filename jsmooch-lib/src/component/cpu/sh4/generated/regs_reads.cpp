
        case 0xFF000000:  { return regs.PTEH.u; }
        case 0xFF000004:  { return regs.PTEL.u; }
        case 0xFF000008:  { return regs.TTB; }
        case 0xFF00000C:  { return regs.TEA; }
        case 0xFF000010:  { return regs.MMUCR.u; }
        case 0xFF000014:  { return regs.CCN_BASRA; }
        case 0xFF000018:  { return regs.CCN_BASRB; }
        case 0xFF00001C:  { return regs.CCR.u; }
        case 0xFF000020:  { return regs.EXPEVT; }
        case 0xFF000024:  { return regs.TRAPA; }
        case 0xFF000028:  { return regs.INTEVT; }
        case 0xFF000034:  { return regs.PTEA.u; }
        case 0xFF000084:  { return regs.PMCTR1_CTRL; }
        case 0xFF000088:  { return regs.PMCTR2_CTRL; }
        case 0xFF100004: { return (*this->trace.cycles >> 32) & 0xFFFF; }
        case 0xFF100008: { return *this->trace.cycles & 0xFFFFFFFF; }
        case 0xFF10000C: { return (*this->trace.cycles >> 32) & 0xFFFF; }
        case 0xFF100010: { return *this->trace.cycles & 0xFFFFFFFF; }
        case 0xFF200000:  { return regs.UBC_BARA; }
        case 0xFF200004:  { return regs.UBC_BAMRA; }
        case 0xFF200008:  { return regs.UBC_BBRA; }
        case 0xFF20000C:  { return regs.UBC_BARB; }
        case 0xFF200010:  { return regs.UBC_BAMRB; }
        case 0xFF200014:  { return regs.UBC_BBRB; }
        case 0xFF200020:  { return regs.UBC_BRCR; }
        case 0xFF800000:  { return regs.BSCR; }
        case 0xFF800004:  { return regs.BCR2; }
        case 0xFF800008:  { return regs.WCR1; }
        case 0xFF80000C:  { return regs.WCR2; }
        case 0xFF800010:  { return regs.WCR3; }
        case 0xFF800014:  { return regs.MCR; }
        case 0xFF800018:  { return regs.PCR; }
        case 0xFF80001C:  { return regs.RTCSR; }
        case 0xFF800024:  { return regs.RTCOR; }
        case 0xFF800028: { return 0xa400; }
        case 0xFF80002C:  { return regs.PCTRA; }
        case 0xFF800040:  { return regs.PCTRB; }
        case 0xFF800044:  { return regs.PDTRB; }
        case 0xFF800048:  { return regs.GPIOIC; }
        case 0xFF940190:  { return regs.SDMR; }
        case 0xFFA00010:  { return regs.DMAC_SAR1; }
        case 0xFFA00014:  { return regs.DMAC_DAR1; }
        case 0xFFA00018:  { return regs.DMAC_DMATCR1; }
        case 0xFFA0001C:  { return regs.DMAC_CHCR1.u | 0x00000000; }
        case 0xFFA00020:  { return regs.DMAC_SAR2; }
        case 0xFFA00024:  { return regs.DMAC_DAR2; }
        case 0xFFA00028:  { return regs.DMAC_DMATCR2; }
        case 0xFFA0002C:  { return regs.DMAC_CHCR2.u | 0x000042C0; }
        case 0xFFA00030:  { return regs.DMAC_SAR3; }
        case 0xFFA00034:  { return regs.DMAC_DAR3; }
        case 0xFFA00038:  { return regs.DMAC_DMATCR3; }
        case 0xFFA0003C:  { return regs.DMAC_CHCR3.u | 0x00000000; }
        case 0xFFC00004:  { return regs.STBCR | 0x00000003; }
        case 0xFFC0000C:  { return regs.CPG_WTCSR.u | 0x0000A500; }
        case 0xFFC80034:  { return regs.RMONAR; }
        case 0xFFC80038:  { return regs.RCR1.u | 0x00000000; }
        case 0xFFC80040:  { return regs.RCR2.u | 0x00000000; }
        case 0xFFD00000:  { return regs.ICR.u; }
        case 0xFFD00004:  { return regs.IPRA.u; }
        case 0xFFD00008:  { return regs.IPRB.u; }
        case 0xFFD0000C:  { return regs.IPRC.u; }
        case 0xFFE80000:  { return regs.SCIF_SCSMR2; }
        case 0xFFE80004:  { return regs.SCIF_SCBRR2; }
        case 0xFFE80008:  { return regs.SCIF_SCSCR2; }
        case 0xFFE8000C:  { return regs.SCIF_SCFTDR2; }
        case 0xFFE80010: { if (sz==2) return 0x60; return this->regs.SCIF_SCFSR2; }
        case 0xFFE80014:  { return regs.SCIF_SCFRDR2; }
        case 0xFFE80018:  { return regs.SCIF_SCFCR2; }
        case 0xFFE8001C:  { return regs.SCIF_SCFDR2; }
        case 0xFFE80020:  { return regs.SCIF_SCSPTR2; }
        case 0xFFE80024:  { return regs.SCIF_SCLSR2; }