
        case 0x00702C00: { this->aica.ARMRST = val; return; }
        case 0x005F6800: { this->io.SB_C2DSTAT = (val & 0x03FFFFE0); /*printf("\nSB-C2DSTAT WRITE %08llx cyc:%llu", val, this->sh4.clock.trace_cycles)*/(void)0; return; }
        case 0x005F6804: { this->io.SB_C2DLEN = (val & 0x00FFFFE0); return; }
        case 0x005F6808: { this->io.SB_C2DST = (val & 1); DC_write_C2DST(this, val); return; }
        case 0x005F6810: { this->io.SB_SDSTAW = (val & 0x07FFFFE0); return; }
        case 0x005F6814: { this->io.SB_SDBAAW = (val & 0x07FFFFE0); return; }
        case 0x005F6818: { this->io.SB_SDWLT = (val & 1); return; }
        case 0x005F681C: { this->io.SB_SDLAS = (val & 1); return; }
        case 0x005F6820: { this->io.SB_SDST = (val & 1); DC_write_SDST(this, val); return; }
        case 0x005F6840: { this->io.SB_DBREQM = (val & 1); return; }
        case 0x005F6844: { this->io.SB_BAVLWC = val; return; }
        case 0x005F6848: { this->io.SB_C2DPRYC = val; return; }
        case 0x005F684C: { this->io.SB_C2DMAXL = val; return; }
        case 0x005F6884: { this->io.SB_LMMODE0 = (val & 1); return; }
        case 0x005F6888: { this->io.SB_LMMODE1 = (val & 1); return; }
        case 0x005F68A0: { this->io.SB_RBSPLT = val; return; }
        case 0x005F68A4: { this->io.SB_UKN5F68A4 = val; return; }
        case 0x005F68AC: { this->io.SB_UKN5F68AC = val; return; }
        case 0x005F6900: { this->io.SB_ISTNRM.u &= ~val; holly_recalc_interrupts(this)/*; printf("\nSB_ISTNRM wrote: %08llx cyc:%llu", val, this->sh4.clock.trace_cycles);*/; return; }
        case 0x005F6908: { this->io.SB_ISTERR.u &= ~val; holly_recalc_interrupts(this)/*; printf("\nSB_ISTERR write: %08llx cyc:%llu", val, this->sh4.clock.trace_cycles)*/; return; }
        case 0x005F6910: { this->io.SB_IML2NRM = val; holly_recalc_interrupts(this); /*printf("\nSB_IML2NRM wrote: %08llu cyc: %llx", val, this->sh4.clock.trace_cycles)*/; return; }
        case 0x005F6914: { this->io.SB_IML2EXT.u = val & 0x0000000F; holly_recalc_interrupts(this)/*; printf("\nSB_IML2EXT write: %08llx cyc:%llu", val, this->sh4.clock.trace_cycles)*/; return; }
        case 0x005F6918: { this->io.SB_IML2ERR.u = val & 0x9FFFFFFF; holly_recalc_interrupts(this)/*; printf("\nSB_IML2ERR write: %08llx cyc:%llu", val, this->sh4.clock.trace_cycles)*/; return; }
        case 0x005F6920: { this->io.SB_IML4NRM = val; holly_recalc_interrupts(this); /*printf("\nSB_IML4NRM wrote: %08llx cyc: %llu", val, this->sh4.clock.trace_cycles)*/; return; }
        case 0x005F6924: { this->io.SB_IML4EXT.u = val & 0x0000000F; holly_recalc_interrupts(this)/*; printf("\nSB_IML4EXT write: %08llx cyc:%llu", val, this->sh4.clock.trace_cycles)*/; return; }
        case 0x005F6928: { this->io.SB_IML4ERR.u = val & 0x9FFFFFFF; holly_recalc_interrupts(this)/*; printf("\nSB_IML4ERR write: %08llx cyc:%llu", val, this->sh4.clock.trace_cycles)*/; return; }
        case 0x005F6930: { this->io.SB_IML6NRM = val; holly_recalc_interrupts(this); /*printf("\nSB_IML6NRM wrote: %08llx cyc: %llu", val, this->sh4.clock.trace_cycles)*/; return; }
        case 0x005F6934: { this->io.SB_IML6EXT.u = val & 0x0000000F; holly_recalc_interrupts(this)/*; printf("\nSB_IML6EXT write: %08llx cyc:%llu", val, this->sh4.clock.trace_cycles)*/; return; }
        case 0x005F6938: { this->io.SB_IML6ERR.u = val & 0x9FFFFFFF; holly_recalc_interrupts(this)/*; printf("\nSB_IML6ERR write: %08llx cyc:%llu", val, this->sh4.clock.trace_cycles)*/; return; }
        case 0x005F6940: { this->io.SB_PDTNRM = val; return; }
        case 0x005F6944: { this->io.SB_PDTEXT = val; return; }
        case 0x005F6950: { this->io.SB_G2DTNRM = val; return; }
        case 0x005F6954: { this->io.SB_GD2TEXT = val; return; }