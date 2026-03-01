
        case 0x005F7404: { g1.SB_GDSTAR = (val & 0x1FFFFFE0); return; }
        case 0x005F7408: { g1.SB_GDLEN = (val & 0x01FFFFFF); return; }
        case 0x005F740C: { g1.SB_GDDIR = (val & 1); return; }
        case 0x005F7414: { g1.SB_GDEN = (val & 1); return; }
        case 0x005F7418: { g1.SB_GDST = (val & 1); gdrom_dma_start(this); return; }
        case 0x005F7480: { g1.SB_G1RRC = val; return; }
        case 0x005F7484: { g1.SB_G1RWC = val; return; }
        case 0x005F7488: { g1.SB_G1FRC = val; return; }
        case 0x005F748C: { g1.SB_G1FWC = val; return; }
        case 0x005F7490: { g1.SB_G1CRC = val; return; }
        case 0x005F7494: { g1.SB_G1CWC = val; return; }
        case 0x005F74A0: { g1.SB_G1GDRC = val; return; }
        case 0x005F74A4: { g1.SB_G1GDWC = val; return; }
        case 0x005F74B4: { g1.SB_G1CRDYC = val; return; }
        case 0x005F74B8: { if ((val >> 16) == 0x8843) { g1.SB_GDAPRO.u = val & 0x00007FFF; } return; }