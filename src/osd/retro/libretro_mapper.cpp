#include "libretro_mapper.h"
#include "libretro_options.h"

#include <array/rhmap.h>
#include <string/stdstring.h>

extern retro_environment_t environ_cb;

int retropad_turbo_btn_map[MAX_BUTTONS];

typedef struct
{
   const char *name;
   int id;
} turbo_btn_input_type_map_t;

static const turbo_btn_input_type_map_t turbo_btn_input_type_map[] = {
   {RETROPAD_OPT_UNMAPPED, RETROPAD_TURBO_UNMAPPED},
   {"RetroPad Up",         RETRO_DEVICE_ID_JOYPAD_UP},
   {"RetroPad Down",       RETRO_DEVICE_ID_JOYPAD_DOWN},
   {"RetroPad Left",       RETRO_DEVICE_ID_JOYPAD_LEFT},
   {"RetroPad Right",      RETRO_DEVICE_ID_JOYPAD_RIGHT},
   {"RetroPad A",          RETRO_DEVICE_ID_JOYPAD_A},
   {"RetroPad B",          RETRO_DEVICE_ID_JOYPAD_B},
   {"RetroPad X",          RETRO_DEVICE_ID_JOYPAD_X},
   {"RetroPad Y",          RETRO_DEVICE_ID_JOYPAD_Y},
   {"RetroPad Select",     RETRO_DEVICE_ID_JOYPAD_SELECT},
   {"RetroPad Start",      RETRO_DEVICE_ID_JOYPAD_START},
   {"RetroPad R",          RETRO_DEVICE_ID_JOYPAD_R},
   {"RetroPad L",          RETRO_DEVICE_ID_JOYPAD_L},
   {"RetroPad R2",         RETRO_DEVICE_ID_JOYPAD_R2},
   {"RetroPad L2",         RETRO_DEVICE_ID_JOYPAD_L2},
   {"RetroPad R3",         RETRO_DEVICE_ID_JOYPAD_R3},
   {"RetroPad L3",         RETRO_DEVICE_ID_JOYPAD_L3}
};

static const turbo_btn_input_type_map_t **turbo_btn_input_type_hashmap = NULL;

typedef struct
{
   const char *key;
   int retro_input;
} turbo_btn_map_option_keys_t;

static const turbo_btn_map_option_keys_t turbo_btn_map_option_keys[MAX_BUTTONS] = {
   {MAME_OPT(turbo_btn_up),     RETRO_DEVICE_ID_JOYPAD_UP},
   {MAME_OPT(turbo_btn_down),   RETRO_DEVICE_ID_JOYPAD_DOWN},
   {MAME_OPT(turbo_btn_left),   RETRO_DEVICE_ID_JOYPAD_LEFT},
   {MAME_OPT(turbo_btn_right),  RETRO_DEVICE_ID_JOYPAD_RIGHT},
   {MAME_OPT(turbo_btn_a),      RETRO_DEVICE_ID_JOYPAD_A},
   {MAME_OPT(turbo_btn_b),      RETRO_DEVICE_ID_JOYPAD_B},
   {MAME_OPT(turbo_btn_x),      RETRO_DEVICE_ID_JOYPAD_X},
   {MAME_OPT(turbo_btn_y),      RETRO_DEVICE_ID_JOYPAD_Y},
   {MAME_OPT(turbo_btn_select), RETRO_DEVICE_ID_JOYPAD_SELECT},
   {MAME_OPT(turbo_btn_start),  RETRO_DEVICE_ID_JOYPAD_START},
   {MAME_OPT(turbo_btn_r),      RETRO_DEVICE_ID_JOYPAD_R},
   {MAME_OPT(turbo_btn_l),      RETRO_DEVICE_ID_JOYPAD_L},
   {MAME_OPT(turbo_btn_r2),     RETRO_DEVICE_ID_JOYPAD_R2},
   {MAME_OPT(turbo_btn_l2),     RETRO_DEVICE_ID_JOYPAD_L2},
   {MAME_OPT(turbo_btn_r3),     RETRO_DEVICE_ID_JOYPAD_R3},
   {MAME_OPT(turbo_btn_l3),     RETRO_DEVICE_ID_JOYPAD_L3}
};

void libretro_mapping_init(void)
{
   unsigned i;
   unsigned num_turbo_input_types = sizeof(turbo_btn_input_type_map) /
         sizeof(turbo_btn_input_type_map[0]);

   for (i = 0; i < MAX_BUTTONS; i++)
      retropad_turbo_btn_map[i] = RETROPAD_TURBO_UNMAPPED;

   for (i = 0; i < num_turbo_input_types; i++)
      RHMAP_SET_STR(turbo_btn_input_type_hashmap, turbo_btn_input_type_map[i].name, &turbo_btn_input_type_map[i]);
}

void libretro_mapping_deinit(void)
{
   RHMAP_FREE(turbo_btn_input_type_hashmap);
}

void libretro_fill_turbo_mapping_core_options(
   struct retro_core_option_v2_definition *option_defs)
{
   unsigned i = 0;
   unsigned btn_idx;
   unsigned num_turbo_input_types = sizeof(turbo_btn_input_type_map) /
         sizeof(turbo_btn_input_type_map[0]);

   if (!option_defs)
      return;

   while (option_defs[i].key)
   {
      for (btn_idx = 0; btn_idx < MAX_BUTTONS; btn_idx++)
      {
         if (string_is_equal(option_defs[i].key, turbo_btn_map_option_keys[btn_idx].key))
         {
            unsigned val_idx;
            for (val_idx = 0; val_idx < num_turbo_input_types; val_idx++)
            {
               option_defs[i].values[val_idx].value = turbo_btn_input_type_map[val_idx].name;
               option_defs[i].values[val_idx].label = NULL;
            }

            option_defs[i].values[num_turbo_input_types].value = NULL;
            option_defs[i].values[num_turbo_input_types].label = NULL;
            break;
         }
      }

      i++;
   }
}

void libretro_update_turbo_btn_map(void)
{
   struct retro_variable var;
   unsigned opt_idx;

   if (!environ_cb)
      return;

   for (opt_idx = 0; opt_idx < MAX_BUTTONS; opt_idx++)
   {
      bool value_found = false;
      int btn = turbo_btn_map_option_keys[opt_idx].retro_input;

      var.key = turbo_btn_map_option_keys[opt_idx].key;
      var.value = NULL;

      if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
      {
         const turbo_btn_input_type_map_t *type_map =
               RHMAP_GET_STR(turbo_btn_input_type_hashmap, var.value);
         if (type_map)
         {
            retropad_turbo_btn_map[btn] = type_map->id;
            value_found = true;
         }
      }

      if (!value_found)
         retropad_turbo_btn_map[btn] = RETROPAD_TURBO_UNMAPPED;
   }
}
