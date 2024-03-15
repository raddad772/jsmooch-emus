
        case 0x005F6C04: { this->maple.SB_MDSTAR = (val & 0x1FFFFFE0); return; }
        case 0x005F6C10: { this->maple.SB_MDTSEL = (val & 1); return; }
        case 0x005F6C14: { this->maple.SB_MDEN = (val & 1); return; }
        case 0x005F6C18: { this->maple.SB_MDST = (val & 1); if (val & 1) { maple_dma_init(this); }; return; }
        case 0x005F6C80: { this->maple.SB_MSYS.u = val & 0xFFFF130F; return; }
        case 0x005F6C8C: { if ((val >> 16) == 0x6155) { this->maple.SB_MDAPRO.u = val & 0x00007F7F; } return; }
        case 0x005F6CE8: { this->maple.SB_MMSEL = (val & 1); return; }