//
// Created by . on 1/19/25.
//

#include <cstdio>
#include "arm946es.h"
namespace ARM946ES {
    namespace CP15 {
enum regs {
    unknown,
    main_id,
    cache_type_and_size,
    TCM_physical_size,
    control_register,
    PU_cacheability_data_unified_PR,
    PU_cacheability_instruction_PR,
    PU_cache_write_buffer_ability_data_PR,
    PU_access_permission_data_unified_PR,
    PU_access_permission_instruction_PR,
    PU_extended_access_permission_data_unified_PR,
    PU_extended_access_permission_instruction_PTR,
    PU_data_unified_0_7,
    PU_instruction_0_7,
    cache_commands_and_halt,
    cache_data_lockdown,
    cache_instruction_lockdown,
    TCM_data_TCM_base_and_virtual_size,
    TCM_instruction_TCM_base_and_virtual_size,
    misc_process_ID,
    misc_implementation_defined
};
}
static CP15::regs get_register(u32 opcode, u32 cn, u32 cm, u32 cp)
{
#define CP(name, opce, cne, cme, cpe) if ((opcode == opce) && (cn == cne) && (cm == cme) && (cp == cpe)) return CP15::name
    CP(main_id, 0, 0, 0, 0);
    CP(cache_type_and_size, 0, 0, 0, 1);
    CP(TCM_physical_size, 0, 0, 0, 2);
    CP(control_register, 0, 1, 0, 0);
    CP(PU_cacheability_data_unified_PR, 0, 2, 0, 0);
    CP(PU_cacheability_instruction_PR, 0, 2, 0, 1);
    CP(PU_cache_write_buffer_ability_data_PR, 0, 3, 0, 0);
    CP(PU_access_permission_data_unified_PR, 0, 5, 0, 0);
    CP(PU_access_permission_instruction_PR, 0, 5, 0, 1);
    CP(PU_extended_access_permission_data_unified_PR, 0, 5, 0, 2);
    CP(PU_extended_access_permission_instruction_PTR, 0, 5, 0, 3);
    if ((opcode == 0) && (cn == 6) && (cm < 7) && (cp == 0)) return CP15::PU_data_unified_0_7;
    if ((opcode == 0) && (cn == 6) && (cm < 7) && (cp == 1)) return CP15::PU_instruction_0_7;
    if ((opcode == 0) && (cn == 7)) return CP15::cache_commands_and_halt;
    CP(cache_data_lockdown, 0, 9, 0, 0);
    CP(cache_instruction_lockdown, 0, 9, 0, 1);
    CP(TCM_data_TCM_base_and_virtual_size, 0, 9, 1, 0);
    CP(TCM_instruction_TCM_base_and_virtual_size, 0, 9, 1, 1);
    if ((opcode == 0) && (cn == 13)) return CP15::misc_process_ID;
    if ((opcode == 0) && (cn == 15)) return CP15::misc_implementation_defined;
#undef CP
    return CP15::unknown;
}

u32 core::NDS_CP_read(u32 num, u32 opcode, u32 Cn, u32 Cm, u32 CP) const {
    if (num != 15) {
        printf("\n!BAD! CP.r:%d opcode:%d Cn:%d Cm:%d CP:%d", num, opcode, Cn, Cm, CP);
        return 0;
    }

    // Register is selected by <cpopc>,Cn,Cm,<cp>

    CP15::regs cpreg = get_register(opcode, Cn, Cm, CP);

    u32 v = 0;
    switch(cpreg) {
        case CP15::main_id:
            return 0x41059461; // ARM946ES
        case CP15::cache_type_and_size:
            return 0x0F0D2112;
        case CP15::TCM_physical_size:
            return 0x00140180;
        case CP15::control_register:
            return cp15.regs.control.u;
        case CP15::PU_cacheability_data_unified_PR:
            return cp15.regs.pu_data_cacheable;
        case CP15::PU_cacheability_instruction_PR:
            return cp15.regs.pu_instruction_cacheable;
        case CP15::PU_cache_write_buffer_ability_data_PR:
            return cp15.regs.pu_data_cached_write;
        case CP15::PU_access_permission_data_unified_PR:
            v = cp15.regs.pu_data_rw & 0x00000003;
            v |= (cp15.regs.pu_data_rw & 0x00000030) >> 2;
            v |= (cp15.regs.pu_data_rw & 0x00000300) >> 4;
            v |= (cp15.regs.pu_data_rw & 0x00003000) >> 6;
            v |= (cp15.regs.pu_data_rw & 0x00030000) >> 8;
            v |= (cp15.regs.pu_data_rw & 0x00300000) >> 10;
            v |= (cp15.regs.pu_data_rw & 0x03000000) >> 12;
            v |= (cp15.regs.pu_data_rw & 0x30000000) >> 14;
            return v;
        case CP15::PU_access_permission_instruction_PR:
            v = cp15.regs.pu_code_rw & 0x00000003;
            v |= (cp15.regs.pu_code_rw & 0x00000030) >> 2;
            v |= (cp15.regs.pu_code_rw & 0x00000300) >> 4;
            v |= (cp15.regs.pu_code_rw & 0x00003000) >> 6;
            v |= (cp15.regs.pu_code_rw & 0x00030000) >> 8;
            v |= (cp15.regs.pu_code_rw & 0x00300000) >> 10;
            v |= (cp15.regs.pu_code_rw & 0x03000000) >> 12;
            v |= (cp15.regs.pu_code_rw & 0x30000000) >> 14;
            return v;
        case CP15::PU_extended_access_permission_data_unified_PR:
            return cp15.regs.pu_data_rw;
        case CP15::PU_extended_access_permission_instruction_PTR:
            return cp15.regs.pu_code_rw;
        case CP15::PU_instruction_0_7:
        case CP15::PU_data_unified_0_7:
            return cp15.regs.pu_region[Cm];
        case CP15::TCM_data_TCM_base_and_virtual_size:
            return cp15.regs.dtcm_setting;
        case CP15::TCM_instruction_TCM_base_and_virtual_size:
            return cp15.regs.itcm_setting;
        case CP15::misc_process_ID:
            return cp15.regs.trace_process_id;
        case CP15::misc_implementation_defined:
            return 0;

        default:
            break;
    }

    printf("\nUNHANDLED CP.r:15 opcode:%d Cn:%d Cm:%d CP:%d", opcode, Cn, Cm, CP);
    return 0;
}

void core::update_dtcm()
{
    if (!cp15.regs.control.dtcm_enable) {
        cp15.dtcm.size = cp15.dtcm.mask = 0;
        cp15.dtcm.base_addr = 0xFFFFFFFF;
        cp15.dtcm.end_addr = 0xFFFFFFFF;
    }
    else {
        cp15.dtcm.size = 0x200 << ((cp15.regs.dtcm_setting >> 1) & 0x1F);
        if (cp15.dtcm.size < 0x1000) cp15.dtcm.size = 0x1000;
        u32 mask = 0xFFFFF000 & ((cp15.dtcm.size - 1) ^ 0xFFFFFFFF);
        cp15.dtcm.base_addr = cp15.regs.dtcm_setting & mask;
        cp15.dtcm.end_addr = cp15.dtcm.base_addr + cp15.dtcm.size;
        cp15.dtcm.mask = cp15.dtcm.size > DTCM_SIZE ? DTCM_SIZE : cp15.dtcm.size;
        cp15.dtcm.mask--;
    }
#ifdef DBG_TCM
    printf("\nDTCM enable:%d base_addr:%08x end_addr:%08x size:%04x", cp15.regs.control.dtcm_enable, cp15.dtcm.base_addr, cp15.dtcm.end_addr, cp15.dtcm.size);
#endif
}

void core::update_itcm()
{
    if (!cp15.regs.control.itcm_enable) {
        cp15.itcm.size = 0;
    }
    else {
        cp15.itcm.size = 0x200 << ((cp15.regs.itcm_setting >> 1) & 0x1F);
        cp15.itcm.mask = cp15.itcm.size > ITCM_SIZE ? ITCM_SIZE : cp15.itcm.size;
        cp15.itcm.mask--;
    }
    cp15.itcm.end_addr = cp15.itcm.size;
#ifdef DBG_TCN
    printf("\nITCM enable:%d base_addr:%08x end_addr:%08x size:%04x", cp15.regs.control.itcm_enable, cp15.itcm.base_addr, cp15.itcm.end_addr, cp15.itcm.size);
#endif
}


void core::NDS_CP_write(u32 num, u32 opcode, u32 Cn, u32 Cm, u32 CP, u32 val)
{
    if (num != 15) {
        printf("\n!BAD! CP.w:%d opcode:%d Cn:%d Cm:%d CP:%d val:%08x", num, opcode, Cn, Cm, CP, val);
        return;
    }

    CP15::regs cpreg = get_register(opcode, Cn, Cm, CP);

    switch(cpreg) {
        case CP15::control_register: {
            // 42078
            cp15.regs.control.u = (cp15.regs.control.u & (~0xFF085)) | (val & 0xFF085);
            update_dtcm();
            update_itcm();
            regs.EBR = 0xFFFF0000 * ((val >> 13) & 1);
            return; }
        case CP15::PU_cacheability_data_unified_PR:
            cp15.regs.pu_data_cacheable = val;
            return;
        case CP15::PU_cacheability_instruction_PR:
            cp15.regs.pu_instruction_cacheable = val;
            return;
        case CP15::PU_cache_write_buffer_ability_data_PR:
            cp15.regs.pu_data_cached_write = val;
            return;
        case CP15::PU_access_permission_data_unified_PR:
            cp15.regs.pu_data_rw = val & 0x0003;
            cp15.regs.pu_data_rw |= (val & 0x000C) << 2;
            cp15.regs.pu_data_rw |= (val & 0x0030) << 4;
            cp15.regs.pu_data_rw |= (val & 0x00C0) << 6;
            cp15.regs.pu_data_rw |= (val & 0x0300) << 8;
            cp15.regs.pu_data_rw |= (val & 0x0C00) << 10;
            cp15.regs.pu_data_rw |= (val & 0x3000) << 12;
            cp15.regs.pu_data_rw |= (val & 0xC000) << 14;
            return;
        case CP15::PU_access_permission_instruction_PR:
            cp15.regs.pu_code_rw = val & 0x0003;
            cp15.regs.pu_code_rw |= (val & 0x000C) << 2;
            cp15.regs.pu_code_rw |= (val & 0x0030) << 4;
            cp15.regs.pu_code_rw |= (val & 0x00C0) << 6;
            cp15.regs.pu_code_rw |= (val & 0x0300) << 8;
            cp15.regs.pu_code_rw |= (val & 0x0C00) << 10;
            cp15.regs.pu_code_rw |= (val & 0x3000) << 12;
            cp15.regs.pu_code_rw |= (val & 0xC000) << 14;
            return;
        case CP15::PU_extended_access_permission_data_unified_PR:
            cp15.regs.pu_data_rw = val;
            return;
        case CP15::PU_extended_access_permission_instruction_PTR:
            cp15.regs.pu_code_rw = val;
            return;
        case CP15::PU_data_unified_0_7:
        case CP15::PU_instruction_0_7:
            cp15.regs.pu_region[Cm] = val;
            return;
        case CP15::cache_commands_and_halt: // 04, 82
            if (((Cm == 0) && (CP == 4)) || (Cm == 8) && (CP == 2)) {
                //printf("\nHALT ARM9");
                halted = true;
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
            return;
        case CP15::TCM_data_TCM_base_and_virtual_size:
            cp15.regs.dtcm_setting = val & 0xFFFFF03E;
            update_dtcm();
            return;
        case CP15::TCM_instruction_TCM_base_and_virtual_size:
            cp15.regs.itcm_setting = val & 0x3E;
            update_itcm();
            return;
        case CP15::misc_process_ID:
            cp15.regs.trace_process_id = val;
            return;
        case CP15::misc_implementation_defined:
            return;
        default:
            break;
    }
    if ((opcode == 0) && (Cn==6) && (Cm == 7) && (CP==0)) {
        // IGNORE!
        // this'll never bite me in the butt!
        return;
    }
    printf("\nUNHANDLED CP.w:15 opcode:%d Cn:%d Cm:%d CP:%d val:%08x", opcode, Cn, Cm, CP, val);

}
void core::NDS_CP_reset()
{
    cp15.regs.control.u = 0x2078; // MelonDS says this

    cp15.rng_seed = 44203;
    cp15.regs.trace_process_id = 0;

    cp15.regs.dtcm_setting = 0;
    cp15.regs.itcm_setting = 0;

    cp15.itcm.size = 0;
    cp15.itcm.base_addr = 0;
    cp15.itcm.end_addr = 0;
    cp15.dtcm.base_addr = 0xFFFFFFFF;
    cp15.dtcm.end_addr = 0xFFFFFFFF;
    cp15.dtcm.mask = 0;

    cp15.regs.pu_instruction_cacheable = 0;
    cp15.regs.pu_data_cacheable = 0;
    cp15.regs.pu_data_cached_write = 0;

    cp15.regs.pu_code_rw = 0;
}

void core::NDS_direct_boot()
{
    NDS_CP_write(15, 0, 1, 0, 0, 0x0005707D);
    NDS_CP_write(15, 0, 9, 1, 0, 0x0300000A);
    NDS_CP_write(15, 0, 9, 1, 1, 0x00000020);
}
}