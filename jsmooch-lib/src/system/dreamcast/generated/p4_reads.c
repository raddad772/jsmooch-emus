
        case 0xFF000000:  { return this->io.PTEH.u; }
        case 0xFF000004:  { return this->io.PTEL.u; }
        case 0xFF000008:  { return this->io.TTB; }
        case 0xFF00000C:  { return this->io.TEA; }
        case 0xFF000010:  { return this->io.MMUCR.u; }
        case 0xFF000034:  { return this->io.PTEA.u; }
        case 0xFF200008:  { return this->io.UKNFF200008; }
        case 0xFF200014:  { return this->io.UKNFF200014; }
        case 0xFF200020:  { return this->io.UKNFF200020; }
        case 0xFF800004:  { return this->io.BCR2; }
        case 0xFF800008:  { return this->io.WCR1; }
        case 0xFF80000C:  { return this->io.WCR2; }
        case 0xFF800010:  { return this->io.WCR3; }
        case 0xFF800014:  { return this->io.MCR; }
        case 0xFF800018:  { return this->io.PCR; }
        case 0xFF80001C:  { return this->io.RTCSR; }
        case 0xFF800024:  { return this->io.RTCOR; }
        case 0xFF80002C:  { return this->io.PCTRA; }
        case 0xFF800040:  { return this->io.PCTRB; }
        case 0xFF800044:  { return this->io.PDTRB; }
        case 0xFF800048:  { return this->io.GPIOIC; }
        case 0xFF940190:  { return this->io.SDMR; }
        case 0xFFA00010:  { return this->io.SAR1; }
        case 0xFFA00014:  { return this->io.DAR1; }
        case 0xFFA00018:  { return this->io.DMATCR1; }
        case 0xFFA0001C:  { return this->io.CHCR1.u | 0x00000000; }
        case 0xFFA00020:  { return this->io.SAR2; }
        case 0xFFA00024:  { return this->io.DAR2; }
        case 0xFFA00028:  { return this->io.DMATCR2; }
        case 0xFFA0002C:  { return this->io.CHCR2.u | 0x000042C0; }
        case 0xFFA00030:  { return this->io.SAR3; }
        case 0xFFA00034:  { return this->io.DMAC_DAR3; }
        case 0xFFA00038:  { return this->io.DMAC_DMATCR3; }
        case 0xFFA0003C:  { return this->io.DMAC_CHCR3.u | 0x00000000; }
        case 0xFFC00004:  { return this->io.STBCR | 0x00000003; }
        case 0xFFC80034:  { return this->io.RMONAR; }
        case 0xFFC80038:  { return this->io.RCR1.u | 0x00000000; }
        case 0xFFC80040:  { return this->io.RCR2.u | 0x00000000; }
        case 0xFFD00000:  { return this->io.ICR; }
        case 0xFFD00004:  { return this->io.IPRA.u | 0x00000000; }
        case 0xFFD00008:  { return this->io.IPRB.u | 0x00000000; }
        case 0xFFD0000C:  { return this->io.IPRC.u; }
        case 0xFFD80008:  { return this->io.TCOR0; }
        case 0xFFD8000C:  { return this->io.TCNT0; }
        case 0xFFD80010:  { return this->io.TCR0; }
        case 0xFFD80014:  { return this->io.TCOR1; }
        case 0xFFD80018:  { return this->io.TMU_TCNT1; }
        case 0xFFD8001C:  { return this->io.TMU_TCR1; }
        case 0xFFD80020:  { return this->io.TMU_TCOR2; }
        case 0xFFD80024:  { return this->io.TMU_TCNT2; }
        case 0xFFD80028:  { return this->io.TMU_TCR2; }
        case 0xFFE80000:  { return this->io.SCIF_SCSMR2; }
        case 0xFFE80004:  { return this->io.SCIF_SCBRR2; }
        case 0xFFE80008:  { return this->io.SCIF_SCSCR2; }
        case 0xFFE8000C:  { return this->io.SCIF_SCFTDR2; }
        case 0xFFE80010:  { return this->io.SCIF_SCFSR2; }
        case 0xFFE80014:  { return this->io.SCIF_SCFRDR2; }
        case 0xFFE80018:  { return this->io.SCIF_SCFCR2; }
        case 0xFFE8001C:  { return this->io.SCIF_SCFDR2; }
        case 0xFFE80020:  { return this->io.SCIF_SCSPTR2; }
        case 0xFFE80024:  { return this->io.SCIF_SCLSR2; }