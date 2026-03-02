
        case 0x005F6C04: { SB_MDSTAR = (val & 0x1FFFFFE0); return; }
        case 0x005F6C10: { SB_MDTSEL = (val & 1); return; }
        case 0x005F6C14: { SB_MDEN = (val & 1); return; }
        case 0x005F6C18: { SB_MDST = (val & 1); if (val & 1) { dma_init(); }; return; }
        case 0x005F6C80: { SB_MSYS.u = val & 0xFFFF130F; return; }
        case 0x005F6C8C: { if ((val >> 16) == 0x6155) { SB_MDAPRO.u = val & 0x00007F7F; } return; }
        case 0x005F6CE8: { SB_MMSEL = (val & 1); return; }