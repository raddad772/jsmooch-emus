#include "stdlib.h"
#include <cstdio>
#include "assert.h"

#include "mapper.h"
#include "no_mapper.h"
#include "mbc1.h"
#include "mbc2.h"
#include "mbc3.h"
#include "mbc5.h"

struct GB_mapper* new_GB_mapper(GB_clock* clock, GB_bus* bus, enum GB_mappers which)
{
	struct GB_mapper* mapper = malloc(sizeof(GB_mapper));
	mapper->which = which;
    mapper->ptr = NULL;
	switch (which) {
	case NONE: // No mapper!
		GB_mapper_none_new(mapper, clock, bus);
        printf("\nNO MAPPER!");
		break;
    case MBC1:
        GB_mapper_MBC1_new(mapper, clock, bus);
        printf("\nMBC1");
        break;
    case MBC2:
        GB_mapper_MBC2_new(mapper, clock, bus);
        printf("\nMBC2");
        break;
    case MBC3:
        GB_mapper_MBC3_new(mapper, clock, bus);
        printf("\nMBC3");
        break;
    case MBC5:
        GB_mapper_MBC5_new(mapper, clock, bus);
        printf("\nMBC5");
        break;
	default:
		printf("\nNO SUPPORTED MAPPER! %d", which);
		break;
	}
    fflush(stdout);
	return mapper;
}

void delete_GB_mapper(GB_mapper* whom)
{
	switch (whom->which) {
	case NONE: // No-mapper!
		GB_mapper_none_delete(whom);
		break;
    case MBC1:
        GB_mapper_MBC1_delete(whom);
        break;
    case MBC2:
        GB_mapper_MBC2_delete(whom);
        break;
    case MBC3:
        GB_mapper_MBC3_delete(whom);
        break;
    case MBC5:
        GB_mapper_MBC5_delete(whom);
        break;
    default:
        assert(1==0);
        break;
	}
    free(whom);
}
