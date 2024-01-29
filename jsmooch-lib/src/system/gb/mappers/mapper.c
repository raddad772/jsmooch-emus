#include <malloc.h>
#include <stdio.h>
#include "assert.h"

#include "mapper.h"
#include "no_mapper.h"
#include "mbc1.h"

struct GB_mapper* new_GB_mapper(struct GB_clock* clock, struct GB_bus* bus, enum GB_mappers which)
{
	struct GB_mapper* mapper = malloc(sizeof(struct GB_mapper));
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
	default:
		printf("\nNO SUPPORTED MAPPER! %d", which);
		break;
	}
    fflush(stdout);
	return mapper;
}

void delete_GB_mapper(struct GB_mapper* whom)
{
	switch (whom->which) {
	case NONE: // No-mapper!
		GB_mapper_none_delete(whom);
		break;
    case MBC1:
        GB_mapper_MBC1_delete(whom);
        break;
    default:
        assert(1==0);
        break;
	}
    free(whom);
}
