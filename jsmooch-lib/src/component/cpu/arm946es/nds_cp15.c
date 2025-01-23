//
// Created by . on 1/19/25.
//

#include <stdio.h>
#include "arm946es.h"

enum CP15_regs {
    CP15r_unknown,
    CP15r_main_id,
    CP15r_cache_type_and_size,
    CP15r_TCM_physical_size,
    CP15r_control_register,
    CP15r_PU_cacheability_data_unified_PR,
    CP15r_PU_cacheability_instruction_PR,
    CP15r_PU_cache_write_buffer_ability_data_PR,
    CP15r_PU_access_permission_data_unified_PR,
    CP15r_PU_access_permission_instruction_PR,
    CP15r_PU_extended_access_permission_data_unified_PR,
    CP15r_PU_extended_access_permission_instruction_PTR,
    CP15r_PU_data_unified_0_7,
    CP15r_PU_instruction_0_7,
    CP15r_cache_commands_and_halt,
    CP15r_cache_data_lockdown,
    CP15r_cache_instruction_lockdown,
    CP15r_TCM_data_TCM_base_and_virtual_size,
    CP15r_TCM_instruction_TCM_base_and_virtual_size,
    CP15r_misc_process_ID,
    CP15r_misc_implementation_defined
};

static enum CP15_regs get_register(u32 opcode, u32 cn, u32 cm, u32 cp)
{
#define CP(name, opce, cne, cme, cpe) if ((opcode == opce) && (cn == cne) && (cm == cme) && (cp == cpe)) return name
    CP(CP15r_main_id, 0, 0, 0, 0);
    CP(CP15r_cache_type_and_size, 0, 0, 0, 1);
    CP(CP15r_TCM_physical_size, 0, 0, 0, 2);
    CP(CP15r_control_register, 0, 1, 0, 0);
    CP(CP15r_PU_cacheability_data_unified_PR, 0, 2, 0, 0);
    CP(CP15r_PU_cacheability_instruction_PR, 0, 2, 0, 1);
    CP(CP15r_PU_cache_write_buffer_ability_data_PR, 0, 3, 0, 0);
    CP(CP15r_PU_access_permission_data_unified_PR, 0, 5, 0, 0);
    CP(CP15r_PU_access_permission_instruction_PR, 0, 5, 0, 1);
    CP(CP15r_PU_extended_access_permission_data_unified_PR, 0, 5, 0, 2);
    CP(CP15r_PU_extended_access_permission_instruction_PTR, 0, 5, 0, 3);
    if ((opcode == 0) && (cn == 6) && (cm < 7) && (cp == 0)) return CP15r_PU_data_unified_0_7;
    if ((opcode == 0) && (cn == 6) && (cm < 7) && (cp == 1)) return CP15r_PU_instruction_0_7;
    if ((opcode == 0) && (cn == 7)) return CP15r_cache_commands_and_halt;
    CP(CP15r_cache_data_lockdown, 0, 9, 0, 0);
    CP(CP15r_cache_instruction_lockdown, 0, 9, 0, 1);
    CP(CP15r_TCM_data_TCM_base_and_virtual_size, 0, 9, 1, 0);
    CP(CP15r_TCM_instruction_TCM_base_and_virtual_size, 0, 9, 1, 1);
    if ((opcode == 0) && (cn == 13)) return CP15r_misc_process_ID;
    if ((opcode == 0) && (cn == 15)) return CP15r_misc_implementation_defined;
#undef CP
    return CP15r_unknown;
}

u32 NDS_CP_read(struct ARM946ES *this, u32 num, u32 opcode, u32 Cn, u32 Cm, u32 CP)
{
    if (num != 15) {
        printf("\n!BAD! CP.r:%d opcode:%d Cn:%d Cm:%d CP:%d", num, opcode, Cn, Cm, CP);
        return 0;
    }

    // Register is selected by <cpopc>,Cn,Cm,<cp>

    enum CP15_regs cpreg = get_register(opcode, Cn, Cm, CP);

    u32 v = 0;
    switch(cpreg) {
        case CP15r_main_id:
            return 0x41059461; // ARM946ES
        case CP15r_cache_type_and_size:
            return 0x0F0D2112;
        case CP15r_TCM_physical_size:
            return 0x00140180;
        case CP15r_control_register:
            return this->cp15.regs.control.u;
        case CP15r_PU_cacheability_data_unified_PR:
            return this->cp15.regs.pu_data_cacheable;
        case CP15r_PU_cacheability_instruction_PR:
            return this->cp15.regs.pu_instruction_cacheable;
        case CP15r_PU_cache_write_buffer_ability_data_PR:
            return this->cp15.regs.pu_data_cached_write;
        case CP15r_PU_access_permission_data_unified_PR:
            v = this->cp15.regs.pu_data_rw & 0x00000003;
            v |= (this->cp15.regs.pu_data_rw & 0x00000030) >> 2;
            v |= (this->cp15.regs.pu_data_rw & 0x00000300) >> 4;
            v |= (this->cp15.regs.pu_data_rw & 0x00003000) >> 6;
            v |= (this->cp15.regs.pu_data_rw & 0x00030000) >> 8;
            v |= (this->cp15.regs.pu_data_rw & 0x00300000) >> 10;
            v |= (this->cp15.regs.pu_data_rw & 0x03000000) >> 12;
            v |= (this->cp15.regs.pu_data_rw & 0x30000000) >> 14;
            return v;
        case CP15r_PU_access_permission_instruction_PR:
            v = this->cp15.regs.pu_code_rw & 0x00000003;
            v |= (this->cp15.regs.pu_code_rw & 0x00000030) >> 2;
            v |= (this->cp15.regs.pu_code_rw & 0x00000300) >> 4;
            v |= (this->cp15.regs.pu_code_rw & 0x00003000) >> 6;
            v |= (this->cp15.regs.pu_code_rw & 0x00030000) >> 8;
            v |= (this->cp15.regs.pu_code_rw & 0x00300000) >> 10;
            v |= (this->cp15.regs.pu_code_rw & 0x03000000) >> 12;
            v |= (this->cp15.regs.pu_code_rw & 0x30000000) >> 14;
            return v;
        case CP15r_PU_extended_access_permission_data_unified_PR:
            return this->cp15.regs.pu_data_rw;
        case CP15r_PU_extended_access_permission_instruction_PTR:
            return this->cp15.regs.pu_code_rw;
        case CP15r_PU_instruction_0_7:
        case CP15r_PU_data_unified_0_7:
            return this->cp15.regs.pu_region[Cm];
        case CP15r_TCM_data_TCM_base_and_virtual_size:
            return this->cp15.regs.dtcm_setting;
        case CP15r_TCM_instruction_TCM_base_and_virtual_size:
            return this->cp15.regs.itcm_setting;
        case CP15r_misc_process_ID:
            return this->cp15.regs.trace_process_id;
        case CP15r_misc_implementation_defined:
            return 0;

        default:
            break;
    }

    printf("\nUNHANDLED CP.r:15 opcode:%d Cn:%d Cm:%d CP:%d", opcode, Cn, Cm, CP);
    return 0;
}

static void update_dtcm(struct ARM946ES *this)
{
    if (!this->cp15.regs.control.dtcm_enable) {
        this->cp15.dtcm.size = this->cp15.dtcm.mask = 0;
        this->cp15.dtcm.base_addr = 0xFFFFFFFF;
        this->cp15.dtcm.end_addr = 0xFFFFFFFF;
    }
    else {
        this->cp15.dtcm.size = 0x200 << ((this->cp15.regs.dtcm_setting >> 1) & 0x1F);
        if (this->cp15.dtcm.size < 0x1000) this->cp15.dtcm.size = 0x1000;
        this->cp15.dtcm.mask = 0xFFFFF000 & ((this->cp15.dtcm.size - 1) ^ 0xFFFFFFFF);
        this->cp15.dtcm.base_addr = this->cp15.regs.dtcm_setting & this->cp15.dtcm.mask;
        this->cp15.dtcm.end_addr = this->cp15.dtcm.base_addr + this->cp15.dtcm.size;
    }
}

static void update_itcm(struct ARM946ES *this)
{
    if (!this->cp15.regs.control.itcm_enable) {
        this->cp15.itcm.size = 0;
    }
    else {
        this->cp15.itcm.size = 0x200 << ((this->cp15.regs.itcm_setting >> 1) & 0x1F);
    }
    this->cp15.itcm.end_addr = this->cp15.itcm.size;
}


void NDS_CP_write(struct ARM946ES *this, u32 num, u32 opcode, u32 Cn, u32 Cm, u32 CP, u32 val)
{
    if (num != 15) {
        printf("\n!BAD! CP.w:%d opcode:%d Cn:%d Cm:%d CP:%d val:%08x", num, opcode, Cn, Cm, CP, val);
        return;
    }

    enum CP15_regs cpreg = get_register(opcode, Cn, Cm, CP);

    switch(cpreg) {
        case CP15r_control_register: {
            this->cp15.regs.control.u = (this->cp15.regs.control.u & (~0xFF085)) | (val & 0xFF085);
            update_dtcm(this);
            update_itcm(this);
            this->regs.EBR = 0xFFFF0000 * ((val >> 13) & 1);
            return; }
        case CP15r_PU_cacheability_data_unified_PR:
            this->cp15.regs.pu_data_cacheable = val;
            return;
        case CP15r_PU_cacheability_instruction_PR:
            this->cp15.regs.pu_instruction_cacheable = val;
            return;
        case CP15r_PU_cache_write_buffer_ability_data_PR:
            this->cp15.regs.pu_data_cached_write = val;
            return;
        case CP15r_PU_access_permission_data_unified_PR:
            this->cp15.regs.pu_data_rw = val & 0x0003;
            this->cp15.regs.pu_data_rw |= (val & 0x000C) << 2;
            this->cp15.regs.pu_data_rw |= (val & 0x0030) << 4;
            this->cp15.regs.pu_data_rw |= (val & 0x00C0) << 6;
            this->cp15.regs.pu_data_rw |= (val & 0x0300) << 8;
            this->cp15.regs.pu_data_rw |= (val & 0x0C00) << 10;
            this->cp15.regs.pu_data_rw |= (val & 0x3000) << 12;
            this->cp15.regs.pu_data_rw |= (val & 0xC000) << 14;
            return;
        case CP15r_PU_access_permission_instruction_PR:
            this->cp15.regs.pu_code_rw = val & 0x0003;
            this->cp15.regs.pu_code_rw |= (val & 0x000C) << 2;
            this->cp15.regs.pu_code_rw |= (val & 0x0030) << 4;
            this->cp15.regs.pu_code_rw |= (val & 0x00C0) << 6;
            this->cp15.regs.pu_code_rw |= (val & 0x0300) << 8;
            this->cp15.regs.pu_code_rw |= (val & 0x0C00) << 10;
            this->cp15.regs.pu_code_rw |= (val & 0x3000) << 12;
            this->cp15.regs.pu_code_rw |= (val & 0xC000) << 14;
            return;
        case CP15r_PU_extended_access_permission_data_unified_PR:
            this->cp15.regs.pu_data_rw = val;
            return;
        case CP15r_PU_extended_access_permission_instruction_PTR:
            this->cp15.regs.pu_code_rw = val;
            return;
        case CP15r_PU_data_unified_0_7:
        case CP15r_PU_instruction_0_7:
            this->cp15.regs.pu_region[Cm] = val;
            return;
        case CP15r_cache_commands_and_halt: // 04, 82
            if (((Cm == 0) && (CP == 4)) || (Cm == 8) && (CP == 2)) {
                this->halted = 1;
                return;
            }
            if ((Cm == 5) && ((CP == 0) || (CP == 1) || (CP == 2))) {
                // Invalidate all i cache, invalidate by address, ???
                return;
            }
            if ((Cm == 6) && ((CP == 0) || (CP == 1))) {
                // Invalidate all d cache, invalidate by address
                return;
            }
            if ((Cm == 10 ) && ((CP == 1) || (CP == 2))) {
                // Flush d cache by val, all
                return;
            }
            break;
        case CP15r_TCM_data_TCM_base_and_virtual_size:
            this->cp15.regs.dtcm_setting = val & 0xFFFFF03E;
            update_dtcm(this);
            break;
        case CP15r_TCM_instruction_TCM_base_and_virtual_size:
            this->cp15.regs.itcm_setting = val & 0x3E;
            update_itcm(this);
            break;
        case CP15r_misc_process_ID:
            this->cp15.regs.trace_process_id = val;
            return;
        case CP15r_misc_implementation_defined:
            return;
        default:
            break;
    }
    printf("\nUNHANDLED CP.w:15 opcode:%d Cn:%d Cm:%d CP:%d val:%08x", opcode, Cn, Cm, CP, val);

}

void NDS_CP_init(struct ARM946ES *this)
{

}

void NDS_CP_reset(struct ARM946ES *this)
{
    this->cp15.regs.control.u = 0x2078; // MelonDS says this

    this->cp15.rng_seed = 44203;
    this->cp15.regs.trace_process_id = 0;

    this->cp15.regs.dtcm_setting = 0;
    this->cp15.regs.itcm_setting = 0;

    this->cp15.itcm.size = 0;
    this->cp15.itcm.base_addr = 0;
    this->cp15.itcm.end_addr = 0;
    this->cp15.dtcm.base_addr = 0xFFFFFFFF;
    this->cp15.dtcm.end_addr = 0xFFFFFFFF;
    this->cp15.dtcm.mask = 0;

    this->cp15.regs.pu_instruction_cacheable = 0;
    this->cp15.regs.pu_data_cacheable = 0;
    this->cp15.regs.pu_data_cached_write = 0;

    this->cp15.regs.pu_code_rw = 0;
}

void ARM946ES_NDS_direct_boot(struct ARM946ES *this)
{
    NDS_CP_write(this, 15, 0, 1, 0, 0, 0x0005707D);
    NDS_CP_write(this, 15, 0, 9, 1, 0, 0x0300000A);
    NDS_CP_write(this, 15, 0, 9, 1, 1, 0x00000020);
}
