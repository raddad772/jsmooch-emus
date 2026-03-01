
        case 0x005F6C04: { maple.SB_MDSTAR = (val & 0x1FFFFFE0); return; }
        case 0x005F6C10: { maple.SB_MDTSEL = (val & 1); return; }
        case 0x005F6C14: { maple.SB_MDEN = (val & 1); return; }
        case 0x005F6C18: { maple.SB_MDST = (val & 1); if (val & 1) { maple_dma_init(this); }; return; }
        case 0x005F6C80: { maple.SB_MSYS.u = val & 0xFFFF130F; return; }
        case 0x005F6C8C: { if ((val >> 16) == 0x6155) { maple.SB_MDAPRO.u = val & 0x00007F7F; } return; }
        case 0x005F6CE8: { maple.SB_MMSEL = (val & 1); return; }