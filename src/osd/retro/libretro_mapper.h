#ifndef LIBRETRO_MAPPER_H__
#define LIBRETRO_MAPPER_H__

#include "libretro_shared.h"

#include <stdlib.h>
#include <string.h>
#include <libretro.h>

#define RETROPAD_TURBO_UNMAPPED -1

extern int retropad_turbo_btn_map[MAX_BUTTONS];

void libretro_mapping_init(void);
void libretro_mapping_deinit(void);

void libretro_fill_turbo_mapping_core_options(
      struct retro_core_option_v2_definition *option_defs);
void libretro_update_turbo_btn_map(void);

#endif /* LIBRETRO_MAPPER_H__ */
