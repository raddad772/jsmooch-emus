
        case 0x005F7404: { this->g1.SB_GDSTAR = (val & 0x1FFFFFE0); return; }
        case 0x005F7408: { this->g1.SB_GDLEN = (val & 0x01FFFFFF); return; }
        case 0x005F740C: { this->g1.SB_GDDIR = (val & 1); return; }
        case 0x005F7414: { this->g1.SB_GDEN = (val & 1); return; }
        case 0x005F7418: { this->g1.SB_GDST = (val & 1); gdrom_dma_start(this); return; }
        case 0x005F7480: { this->g1.SB_G1RRC = val; return; }
        case 0x005F7484: { this->g1.SB_G1RWC = val; return; }
        case 0x005F7488: { this->g1.SB_G1FRC = val; return; }
        case 0x005F748C: { this->g1.SB_G1FWC = val; return; }
        case 0x005F7490: { this->g1.SB_G1CRC = val; return; }
        case 0x005F7494: { this->g1.SB_G1CWC = val; return; }
        case 0x005F74A0: { this->g1.SB_G1GDRC = val; return; }
        case 0x005F74A4: { this->g1.SB_G1GDWC = val; return; }
        case 0x005F74B4: { this->g1.SB_G1CRDYC = val; return; }
        case 0x005F74B8: { if ((val >> 16) == 0x8843) { this->g1.SB_GDAPRO.u = val & 0x00007FFF; } return; }