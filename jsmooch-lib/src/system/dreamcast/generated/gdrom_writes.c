
        case 0x005F7404: { this->gdrom.SB_GDSTAR.u = (val & 0x1FFFFFE0) | 0x00000000; return; }
        case 0x005F7408: { this->gdrom.SB_GDLEN.u = val & 0x01FFFFFF; return; }
        case 0x005F740C: { this->gdrom.SB_GDDIR.u = val & 0x00000001; return; }
        case 0x005F7414: { this->gdrom.SB_GDEN.u = val & 0x00000001; return; }
        case 0x005F7418: { this->gdrom.SB_GDST = (val & 1); gdrom_dma_start(this); return; }