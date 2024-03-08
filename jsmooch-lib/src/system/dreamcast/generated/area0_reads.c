
        case 0x00702C00:  { return this->aica.ARMRST; }
        case 0x005F6800:  { return this->io.SB_CD2STAT.u | 0x10000000; }
        case 0x005F6804:  { return this->io.SB_CD2LEN.u; }
        case 0x005F6808:  { return this->io.SB_C2DST; }
        case 0x005F6810:  { return this->io.SB_SDSTAW.u | 0x08000000; }
        case 0x005F6814:  { return this->io.SB_SDBAAW.u | 0x08000000; }
        case 0x005F6818:  { return this->io.SB_SDWLT; }
        case 0x005F681C:  { return this->io.SB_SDLAS; }
        case 0x005F6820:  { return this->io.SB_SDST; }
        case 0x005F6840:  { return this->io.SB_DBREQM; }
        case 0x005F6844:  { return this->io.SB_BAVLWC; }
        case 0x005F6848:  { return this->io.SB_C2DPRYC; }
        case 0x005F684C:  { return this->io.SB_C2DMAXL; }
        case 0x005F6884:  { return this->io.SB_LMMODE0; }
        case 0x005F6888:  { return this->io.SB_LMMODE1; }
        case 0x005F68A0:  { return this->io.SB_RBSPLT; }
        case 0x005F68A4:  { return this->io.SB_UKN5F68A4; }
        case 0x005F68AC:  { return this->io.SB_UKN5F68AC; }
        case 0x005F6914:  { return this->io.SB_IML2EXT.u; }
        case 0x005F6918:  { return this->io.SB_IML2ERR.u; }
        case 0x005F6924:  { return this->io.SB_IML4EXT.u; }
        case 0x005F6928:  { return this->io.SB_IML4ERR.u; }
        case 0x005F6934:  { return this->io.SB_IML6EXT.u; }
        case 0x005F6938:  { return this->io.SB_IML6ERR.u; }
        case 0x005F6940:  { return this->io.SB_PDTNRM; }
        case 0x005F6944:  { return this->io.SB_PDTEXT; }
        case 0x005F6950:  { return this->io.SB_G2DTNRM; }
        case 0x005F6954:  { return this->io.SB_GD2TEXT; }