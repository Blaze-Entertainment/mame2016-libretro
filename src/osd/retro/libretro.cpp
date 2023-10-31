#include <unistd.h>
#include <stdint.h>
#include <string.h>

#include "osdepend.h"

#include "../frontend/mame/mame.h"
#include "emu.h"
#include "emuopts.h"
#include "render.h"
#include "ui/uimain.h"
#include "../frontend/mame/ui/ui.h"
#include "uiinput.h"
#include "drivenum.h"

#include <libretro.h>
#include "libretro_core_options.h"
#include "libretro_options.h"
#include "libretro_shared.h"
#include "libretro_mapper.h"

#include <sys/syscall.h>
#define gettid() syscall(SYS_gettid)
/* forward decls / externs / prototypes */

#include <string/stdstring.h>
#include <file/file_path.h>
#include <streams/file_stream.h>
#include <file/config_file.h>

#define EVERCADE_OPTION_OVERRIDE_FILE_EXT ".opt"
static config_file_t *evercade_option_overrides = NULL;

#if defined(EVERCADE_DEBUG)
#include <unordered_set>
#include <vector>
#include "modules/lib/osdobj_common.h"
#include <formats/rjson.h>

#define EVERCADE_ROM_LIST_FILE_EXT_NO_DOT "romlist"
#define EVERCADE_DRIVER_LIST_FILE_EXT ".driverlist"
#define EVERCADE_DIP_DESC_FILE_EXT ".diplist"

typedef struct
{
   std::string name;
   UINT32 value;
} dip_setting_desc_t;

typedef struct
{
   std::string port_tag;
   std::string field_name;
   UINT32 field_mask;
   UINT32 field_defvalue;
   std::vector<dip_setting_desc_t> field_settings;
} dip_desc_t;

#define DIP_NAME_UNUSED "Unused"
#define DIP_NAME_UNKNOWN "Unknown"

static bool dump_dip_list = false;
#endif

extern const char bare_build_version[];
bool retro_load_ok  = false;
int retro_pause = 0;

int fb_width   = 640;
int fb_height  = 480;
int fb_pitch   = 640;
int max_width   = 640;
int max_height  = 480;

float view_aspect=1.0f; // aspect for current view
float retro_aspect = (float)4.0f/(float)3.0f;

float retro_fps = 60.0;
int SHIFTON           = -1;
int NEWGAME_FROM_OSD  = 0;
char RPATH[512];

static int cpu_overclock = 100;

/* - Minimum turbo pulse train
 *   is 1 frames ON, 1 frames OFF
 * - Default turbo pulse train
 *   is 2 frames ON, 2 frames OFF */
#define TURBO_PERIOD_MIN          2
#define TURBO_PERIOD_MAX          120
#define TURBO_PERIOD_DEFAULT      4
#define TURBO_PULSE_WIDTH_MIN     1
#define TURBO_PULSE_WIDTH_MAX     15
#define TURBO_PULSE_WIDTH_DEFAULT 2

unsigned retropad_turbo_period      = TURBO_PERIOD_DEFAULT;
unsigned retropad_turbo_pulse_width = TURBO_PULSE_WIDTH_DEFAULT;
unsigned retropad_turbo_counters[MAX_PADS][MAX_BUTTONS];

int scheduler_allow_target_update;
const char *retro_save_directory;
const char *retro_system_directory;
const char *retro_content_directory;

retro_log_printf_t log_cb;

static bool draw_this_frame;

#ifdef M16B
uint16_t videoBuffer[1600*1200];
#define LOG_PIXEL_BYTES 1
#else
unsigned int videoBuffer[1600*1200];
#define LOG_PIXEL_BYTES 2*1
#endif

retro_video_refresh_t video_cb = NULL;
retro_environment_t environ_cb = NULL;

#if defined(HAVE_GL)
#include "retroogl.c"
#endif

static void extract_basename(char *buf, const char *path, size_t size)
{
   char *ext = NULL;
   const char *base = strrchr(path, '/');

   if (!base)
      base = strrchr(path, '\\');
   if (!base)
      base = path;

   if (*base == '\\' || *base == '/')
      base++;

   strncpy(buf, base, size - 1);
   buf[size - 1] = '\0';

   ext = strrchr(buf, '.');
   if (ext)
      *ext = '\0';
}

static void extract_directory(char *buf, const char *path, size_t size)
{
   char *base = NULL;

   strncpy(buf, path, size - 1);
   buf[size - 1] = '\0';

   base = strrchr(buf, '/');

   if (!base)
      base = strrchr(buf, '\\');

   if (base)
      *base = '\0';
   else
      buf[0] = '\0';
}

retro_input_state_t input_state_cb = NULL;
retro_audio_sample_batch_t audio_batch_cb = NULL;

void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { audio_batch_cb = cb; }
static retro_input_poll_t input_poll_cb;

void retro_set_input_state(retro_input_state_t cb) { input_state_cb = cb; }
void retro_set_input_poll(retro_input_poll_t cb) { input_poll_cb = cb; }

void retro_set_video_refresh(retro_video_refresh_t cb) { video_cb = cb; }
void retro_set_audio_sample(retro_audio_sample_t cb) { }

void retro_set_environment(retro_environment_t cb)
{
   environ_cb = cb;

   pthread_attr_t tattr;
   int policy;
   int ret;
   struct retro_vfs_interface_info vfs_iface_info;

   /* set the scheduling policy to SCHED_RR */
   ret = pthread_attr_setschedpolicy(&tattr, SCHED_RR);

   vfs_iface_info.required_interface_version = 1;
   vfs_iface_info.iface                      = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VFS_INTERFACE, &vfs_iface_info))
      filestream_vfs_init(&vfs_iface_info);
}

static void update_runtime_variables(void)
{
  // update CPU Overclock
  if (mame_machine_manager::instance() != NULL && mame_machine_manager::instance()->machine() != NULL && 
      mame_machine_manager::instance()->machine()->firstcpu != NULL)
    mame_machine_manager::instance()->machine()->firstcpu->set_clock_scale((float)cpu_overclock * 0.01f);
}

static void check_variables(void)
{
   struct retro_variable var = {0};

   var.key   = MAME_OPT(boot_from_cli);
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp(var.value, "enabled"))
         experimental_cmdline = true;
      if (!strcmp(var.value, "disabled"))
         experimental_cmdline = false;
   }

   var.key   = MAME_OPT(mouse_enable);
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp(var.value, "disabled"))
         mouse_enable = false;
      if (!strcmp(var.value, "enabled"))
         mouse_enable = true;
   }

   var.key   = MAME_OPT(throttle);
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp(var.value, "disabled"))
         throttle_enable = false;
      if (!strcmp(var.value, "enabled"))
         throttle_enable = true;
   }

   var.key   = MAME_OPT(nobuffer);
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp(var.value, "disabled"))
         nobuffer_enable = false;
      if (!strcmp(var.value, "enabled"))
         nobuffer_enable = true;
   }

   var.key   = MAME_OPT(cheats_enable);
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp(var.value, "disabled"))
         cheats_enable = false;
      if (!strcmp(var.value, "enabled"))
         cheats_enable = true;
   }

   var.key   = MAME_OPT(cpu_overclock);
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      cpu_overclock = 100;
      if (strcmp(var.value, "default"))
        cpu_overclock = atoi(var.value);
   }

   var.key   = MAME_OPT(scheduler_allow_target_update);
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp(var.value, "disabled"))
         scheduler_allow_target_update = 0;
      if (!strcmp(var.value, "enabled"))
         scheduler_allow_target_update = 1;
   }

   /* Disabled in src/frontend/mame/ui/ui.cpp */
#if 0
   var.key   = MAME_OPT(hide_nagscreen);
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp(var.value, "disabled"))
         hide_nagscreen = false;
      if (!strcmp(var.value, "enabled"))
         hide_nagscreen = true;
   }

   var.key   = MAME_OPT(hide_infoscreen);
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp(var.value, "disabled"))
         hide_gameinfo = false;
      if (!strcmp(var.value, "enabled"))
         hide_gameinfo = true;
   }

   var.key   = MAME_OPT(hide_warnings);
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp(var.value, "disabled"))
         hide_warnings = false;
      if (!strcmp(var.value, "enabled"))
         hide_warnings = true;
   }
#endif

   var.key   = MAME_OPT(alternate_renderer);
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp(var.value, "disabled"))
         alternate_renderer = false;
      if (!strcmp(var.value, "enabled"))
         alternate_renderer = true;
   }

   var.key   = MAME_OPT(boot_to_osd);
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp(var.value, "enabled"))
         boot_to_osd_enable = true;
      if (!strcmp(var.value, "disabled"))
         boot_to_osd_enable = false;
   }

   var.key = MAME_OPT(read_config);
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp(var.value, "disabled"))
         read_config_enable = false;
      if (!strcmp(var.value, "enabled"))
         read_config_enable = true;
   }

   var.key   = MAME_OPT(auto_save);
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp(var.value, "disabled"))
         auto_save_enable = false;
      if (!strcmp(var.value, "enabled"))
         auto_save_enable = true;
   }

   var.key   = MAME_OPT(saves);
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp(var.value, "game"))
         game_specific_saves_enable = true;
      if (!strcmp(var.value, "system"))
         game_specific_saves_enable = false;
   }

   var.key   = MAME_OPT(media_type);
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      sprintf(mediaType,"-%s",var.value);
   }

   var.key   = MAME_OPT(softlists_enable);
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp(var.value, "enabled"))
         softlist_enable = true;
      if (!strcmp(var.value, "disabled"))
         softlist_enable = false;
   }

   var.key   = MAME_OPT(softlists_auto_media);
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (strcmp(var.value, "enabled") == 0)
         softlist_auto = true;
      if (strcmp(var.value, "disabled") == 0)
         softlist_auto = false;
   }

   var.key = MAME_OPT(boot_to_bios);
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (strcmp(var.value, "enabled") == 0)
         boot_to_bios_enable = true;
      if (strcmp(var.value, "disabled") == 0)
         boot_to_bios_enable = false;
   }

   var.key = MAME_OPT(write_config);
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp(var.value, "disabled"))
         write_config_enable = false;
      if (!strcmp(var.value, "enabled"))
         write_config_enable = true;
   }

#if defined(EVERCADE_DEBUG)
   var.key = MAME_OPT(dump_dip_info);
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp(var.value, "disabled"))
         dump_dip_list = false;
      if (!strcmp(var.value, "enabled"))
         dump_dip_list = true;
   }
#endif

   libretro_update_turbo_btn_map();

   retropad_turbo_period      = TURBO_PERIOD_DEFAULT;
   retropad_turbo_pulse_width = TURBO_PULSE_WIDTH_DEFAULT;
   var.key                    = MAME_OPT(turbo_period);
   var.value                  = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      retropad_turbo_period = atoi(var.value);
      retropad_turbo_period = (retropad_turbo_period < TURBO_PERIOD_MIN) ?
            TURBO_PERIOD_MIN : retropad_turbo_period;
      retropad_turbo_period = (retropad_turbo_period > TURBO_PERIOD_MAX) ?
            TURBO_PERIOD_MAX : retropad_turbo_period;

      retropad_turbo_pulse_width = retropad_turbo_period >> 1;
      retropad_turbo_pulse_width = (retropad_turbo_pulse_width < TURBO_PULSE_WIDTH_MIN) ?
            TURBO_PULSE_WIDTH_MIN : retropad_turbo_pulse_width;
      retropad_turbo_pulse_width = (retropad_turbo_pulse_width > TURBO_PULSE_WIDTH_MAX) ?
            TURBO_PULSE_WIDTH_MAX : retropad_turbo_pulse_width;

      memset(retropad_turbo_counters, 0, sizeof(retropad_turbo_counters));
   }
}

unsigned retro_api_version(void)
{
   return RETRO_API_VERSION;
}

void retro_get_system_info(struct retro_system_info *info)
{
   memset(info, 0, sizeof(*info));

   info->library_name     = "MAME 2016";
   info->library_version  = bare_build_version;
#if defined(EVERCADE_DEBUG)
   info->valid_extensions = "zip|chd|7z|cmd|" EVERCADE_ROM_LIST_FILE_EXT_NO_DOT;
#else
   info->valid_extensions = "zip|chd|7z|cmd";
#endif
   info->need_fullpath    = true;
   info->block_extract    = true;
}

void update_geometry()
{
   struct retro_system_av_info system_av_info;
   system_av_info.geometry.base_width = fb_width;
   system_av_info.geometry.base_height = fb_height;
   system_av_info.geometry.aspect_ratio = retro_aspect;
   environ_cb(RETRO_ENVIRONMENT_SET_GEOMETRY, &system_av_info);
}

void retro_get_system_av_info(struct retro_system_av_info *info)
{
   check_variables();

   info->geometry.base_width  = fb_width;
   info->geometry.base_height = fb_height;
   info->geometry.max_width  = fb_width;
   info->geometry.max_height = fb_height;

   osd_printf_info("AV_INFO: width=%d height=%d\n",
         info->geometry.base_width, info->geometry.base_height);

   max_width   = fb_width;
   max_height  = fb_height;

   osd_printf_info("AV_INFO: max_width=%d max_height=%d\n",
         info->geometry.max_width, info->geometry.max_height);

   info->geometry.aspect_ratio = retro_aspect;

   osd_printf_info("AV_INFO: aspect_ratio = %f\n", info->geometry.aspect_ratio);

   info->timing.fps            = retro_fps;
   info->timing.sample_rate    = 48000.0;

   osd_printf_info("AV_INFO: fps = %f sample_rate = %f\n",
      info->timing.fps, info->timing.sample_rate);
}

void retro_init (void)
{
   struct retro_log_callback log;
   const char *system_dir = NULL;
   const char *content_dir = NULL;
   const char *save_dir = NULL;
#ifdef M16B
   enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_RGB565;
#else
   enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_XRGB8888;
#endif

   if (environ_cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &log))
      log_cb = log.log;
   else
      log_cb = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &system_dir) && system_dir)
   {
      /* if defined, use the system directory */
      retro_system_directory = system_dir;
   }

   osd_printf_info("SYSTEM_DIRECTORY: %s", retro_system_directory);

   if (environ_cb(RETRO_ENVIRONMENT_GET_CONTENT_DIRECTORY, &content_dir) && content_dir)
   {
      // if defined, use the system directory
      retro_content_directory=content_dir;
   }

   osd_printf_info("CONTENT_DIRECTORY: %s", retro_content_directory);

   if (environ_cb(RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY, &save_dir) && save_dir)
   {
      /* If save directory is defined use it, 
       * otherwise use system directory. */
      retro_save_directory = *save_dir ? save_dir : retro_system_directory;

   }
   else
   {
      /* make retro_save_directory the same,
       * in case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY 
       * is not implemented by the frontend. */
      retro_save_directory=retro_system_directory;
   }

   osd_printf_info("SAVE_DIRECTORY: %s", retro_save_directory);

   if (!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt))
   {
      osd_printf_error("pixel format not supported");
      exit(0);
   }

   libretro_mapping_init();
   libretro_fill_turbo_mapping_core_options(option_defs_us);
}

int RLOOP=1;
extern void retro_main_loop();
extern void retro_finish();

void retro_deinit(void)
{
    osd_printf_info("RETRO DEINIT\n");

    if(retro_load_ok)retro_finish();

    libretro_mapping_deinit();
}

void retro_reset (void)
{
   mame_reset = 1;
   //mame_machine_manager::instance()->ui().show_menu();
}

#if defined(EVERCADE_DEBUG)
static void write_dip_list(void)
{
   /* .cfg file DIP switch entry:
    *    <port tag="port.tag()" type="DIPSWITCH" mask="field.mask()" defvalue="field.defvalue()" value="current_value" />
    * For each DIP switch, need to dump:
    * - port tag
    * - field name
    * - field mask
    * - field defvalue
    * - all setting values
    * - all setting value names */
   std::vector<dip_desc_t> dip_descs;

   /* Loop over all machine ports */
   for (ioport_port &port : mame_machine_manager::instance()->machine()->ioport().ports())
   {
      const char *port_tag = port.tag();

      if (port.fields().empty())
         continue;

      /* Loop over all fields of the current port */
      for (ioport_field &field : port.fields())
      {
         const char *field_name = field.name();
         dip_desc_t dip_desc;

         if (string_is_empty(field_name))
            continue;

         /* Skip invalid field types */
         if ((field.type() != IPT_DIPSWITCH) || field.diplocations().empty())
            continue;

         /* Skip unused, unknown switches */
         if (string_is_equal(field_name, DIP_NAME_UNUSED) ||
             string_is_equal(field_name, DIP_NAME_UNKNOWN))
            continue;

         /* Cache field parameters */
         dip_desc.port_tag = std::string(port_tag);
         dip_desc.field_name = std::string(field_name);
         dip_desc.field_mask = field.mask();
         dip_desc.field_defvalue = field.defvalue();

         /* Loop over all field settings */
         for (ioport_setting &setting : field.settings())
         {
            dip_setting_desc_t dip_setting_desc;
            dip_setting_desc.name = setting.name();
            dip_setting_desc.value = setting.value();
            dip_desc.field_settings.push_back(dip_setting_desc);
         }

         if (dip_desc.field_settings.size() > 0)
            dip_descs.push_back(dip_desc);
      }
   }

   /* If DIP switch entries were found, write to disk */
   if (dip_descs.size() > 0)
   {
      char dip_desc_path[2048] = {0};
      RFILE *dip_desc_file = NULL;
      rjsonwriter_t *json_writer = NULL;
      size_t i, j;

      /* Get output file name */
      fill_pathname(dip_desc_path, RPATH,
            EVERCADE_DIP_DESC_FILE_EXT,
            sizeof(dip_desc_path));

      /* Open file */
      dip_desc_file = filestream_open(dip_desc_path,
            RETRO_VFS_FILE_ACCESS_WRITE,
            RETRO_VFS_FILE_ACCESS_HINT_NONE);
      if (!dip_desc_file)
      {
         osd_printf_error("Failed to open DIP description file: %s\n", dip_desc_path);
         return;
      }
      json_writer = rjsonwriter_open_rfile(dip_desc_file);
      if (!json_writer)
      {
         osd_printf_error("Failed to create JSON writer for DIP description file\n");
         filestream_close(dip_desc_file);
         return;
      }

      rjsonwriter_add_start_object(json_writer);
      rjsonwriter_add_newline(json_writer);
      rjsonwriter_add_spaces(json_writer, 2);
      rjsonwriter_add_string(json_writer, "ports");
      rjsonwriter_add_colon(json_writer);
      rjsonwriter_add_spaces(json_writer, 1);
      rjsonwriter_add_start_array(json_writer);

      /* Write DIP switch descriptions */
      for (i = 0; i < dip_descs.size(); i++)
      {
         rjsonwriter_add_newline(json_writer);
         rjsonwriter_add_spaces(json_writer, 4);
         rjsonwriter_add_start_object(json_writer);

         rjsonwriter_add_newline(json_writer);
         rjsonwriter_add_spaces(json_writer, 6);
         rjsonwriter_add_string(json_writer, "name");
         rjsonwriter_add_colon(json_writer);
         rjsonwriter_add_space(json_writer);
         rjsonwriter_add_string(json_writer, dip_descs[i].field_name.c_str());
         rjsonwriter_add_comma(json_writer);

         rjsonwriter_add_newline(json_writer);
         rjsonwriter_add_spaces(json_writer, 6);
         rjsonwriter_add_string(json_writer, "tag");
         rjsonwriter_add_colon(json_writer);
         rjsonwriter_add_space(json_writer);
         rjsonwriter_add_string(json_writer, dip_descs[i].port_tag.c_str());
         rjsonwriter_add_comma(json_writer);

         rjsonwriter_add_newline(json_writer);
         rjsonwriter_add_spaces(json_writer, 6);
         rjsonwriter_add_string(json_writer, "mask");
         rjsonwriter_add_colon(json_writer);
         rjsonwriter_add_space(json_writer);
         rjsonwriter_add_int(json_writer, (int)dip_descs[i].field_mask);
         rjsonwriter_add_comma(json_writer);

         rjsonwriter_add_newline(json_writer);
         rjsonwriter_add_spaces(json_writer, 6);
         rjsonwriter_add_string(json_writer, "defvalue");
         rjsonwriter_add_colon(json_writer);
         rjsonwriter_add_space(json_writer);
         rjsonwriter_add_int(json_writer, (int)dip_descs[i].field_defvalue);
         rjsonwriter_add_comma(json_writer);

         /* Write settings array */
         rjsonwriter_add_newline(json_writer);
         rjsonwriter_add_spaces(json_writer, 6);
         rjsonwriter_add_string(json_writer, "settings");
         rjsonwriter_add_colon(json_writer);
         rjsonwriter_add_spaces(json_writer, 1);
         rjsonwriter_add_start_array(json_writer);

         for (j = 0; j < dip_descs[i].field_settings.size(); j++)
         {
            rjsonwriter_add_newline(json_writer);
            rjsonwriter_add_spaces(json_writer, 8);
            rjsonwriter_add_start_object(json_writer);

            rjsonwriter_add_newline(json_writer);
            rjsonwriter_add_spaces(json_writer, 10);
            rjsonwriter_add_string(json_writer, "name");
            rjsonwriter_add_colon(json_writer);
            rjsonwriter_add_space(json_writer);
            rjsonwriter_add_string(json_writer, dip_descs[i].field_settings[j].name.c_str());
            rjsonwriter_add_comma(json_writer);

            rjsonwriter_add_newline(json_writer);
            rjsonwriter_add_spaces(json_writer, 10);
            rjsonwriter_add_string(json_writer, "value");
            rjsonwriter_add_colon(json_writer);
            rjsonwriter_add_space(json_writer);
            rjsonwriter_add_int(json_writer, (int)dip_descs[i].field_settings[j].value);

            rjsonwriter_add_newline(json_writer);
            rjsonwriter_add_spaces(json_writer, 8);
            rjsonwriter_add_end_object(json_writer);

            if (j < dip_descs[i].field_settings.size() - 1)
               rjsonwriter_add_comma(json_writer);
         }

         rjsonwriter_add_newline(json_writer);
         rjsonwriter_add_spaces(json_writer, 6);
         rjsonwriter_add_end_array(json_writer);
         rjsonwriter_add_newline(json_writer);
         rjsonwriter_add_spaces(json_writer, 4);
         rjsonwriter_add_end_object(json_writer);

         if (i < dip_descs.size() - 1)
            rjsonwriter_add_comma(json_writer);
      }

      rjsonwriter_add_newline(json_writer);
      rjsonwriter_add_spaces(json_writer, 2);
      rjsonwriter_add_end_array(json_writer);
      rjsonwriter_add_newline(json_writer);
      rjsonwriter_add_end_object(json_writer);
      rjsonwriter_add_newline(json_writer);

      /* Close file */
      if (rjsonwriter_free(json_writer))
         osd_printf_info("Wrote DIP description file: %s\n", dip_desc_path);
      else
         osd_printf_error("Failed to write DIP description file: %s\n", dip_desc_path);
      filestream_close(dip_desc_file);
   }
   else
      osd_printf_info("No DIP switch settings found for ROM: %s\n", RPATH);
}
#endif

void retro_run (void)
{  
   static int mfirst=1;
   bool updated = false;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated) {
      check_variables();
      update_runtime_variables();
   }

   if(mfirst==1)
   {
      mfirst=0;
      mmain(1,RPATH);
      retro_load_ok=true;
      update_runtime_variables();
#if defined(EVERCADE_DEBUG)
      if (dump_dip_list)
         write_dip_list();
#endif
      return;
   }

   if (NEWGAME_FROM_OSD == 1)
   {
      struct retro_system_av_info ninfo;

      retro_get_system_av_info(&ninfo);

      environ_cb(RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO, &ninfo);

      osd_printf_info("ChangeAV: w:%d h:%d ra:%f.\n",
            ninfo.geometry.base_width, ninfo.geometry.base_height, ninfo.geometry.aspect_ratio);

      NEWGAME_FROM_OSD=0;
   }
   else if (NEWGAME_FROM_OSD == 2){
      update_geometry();
      osd_printf_info("w:%d h:%d a:%f\n", fb_width, fb_height, retro_aspect);
      NEWGAME_FROM_OSD=0;
   }

   input_poll_cb();

   process_mouse_state();
   process_keyboard_state();
   process_joypad_state();

   if(retro_pause==0)retro_main_loop();

   RLOOP=1;

#ifdef HAVE_GL
   do_glflush();
#else
   if (draw_this_frame)
      video_cb(videoBuffer, fb_width, fb_height, fb_pitch << LOG_PIXEL_BYTES);
   else
      video_cb(NULL, fb_width, fb_height, fb_pitch << LOG_PIXEL_BYTES);
#endif

}

#if defined(EVERCADE_DEBUG)
static void write_driver_list(const char *rom_list_path)
{
   FILE *rom_list_file = NULL;
   char *line = NULL;
   size_t len = 0;
   osd_options tmp_options;
   /* We could do everything in this function
    * using plain C (as elsewhere in this file),
    * but using a set+vector makes the code so
    * much simpler that it would be absurd not
    * to do so... */
   std::unordered_set<std::string> driver_list_set;

   /* Open ROM list */
   rom_list_file = fopen(rom_list_path, "r");
   if (!rom_list_file)
      return;

   /* Read ROM list line-by-line */
   while ((getline(&line, &len, rom_list_file)) != -1)
   {
      char rom_name[256] = {0};
      if (string_is_empty(line))
         continue;

      fill_pathname_base_noext(rom_name,
            string_trim_whitespace(line), sizeof(rom_name));
      if (!string_is_empty(rom_name))
      {
         /* Get driver source file list for current ROM */
         driver_enumerator drivlist(tmp_options, rom_name);
         fprintf(stdout, "# Checking ROM: %s\n", rom_name);

         if (drivlist.count() < 1)
         {
            fprintf(stderr, "  > ERROR: No drivers found\n");
            continue;
         }

         while (drivlist.next())
         {
            /* Add current source file to set */
            const char *driver_src_file =
                  path_basename(drivlist.driver().source_file);
            if (!string_is_empty(driver_src_file))
            {
               fprintf(stdout, "  > Found driver: %s\n", driver_src_file);
               driver_list_set.insert(driver_src_file);
            }
         }
      }
   }

   fclose(rom_list_file);
   if (line)
      free(line);

   if (driver_list_set.size() > 0)
   {
      char driver_list_path[2048] = {0};
      FILE *driver_list_file = NULL;
      size_t i;

      /* Sort driver list */
      std::vector<std::string> driver_list_sorted(
            driver_list_set.begin(), driver_list_set.end());

      /* Get output file name */
      fill_pathname(driver_list_path, rom_list_path,
            EVERCADE_DRIVER_LIST_FILE_EXT,
            sizeof(driver_list_path));

      /* Write list to file */
      driver_list_file = fopen(driver_list_path, "w");
      if (!driver_list_file)
         return;

      for (i = 0; i < driver_list_sorted.size(); i++)
         fprintf(driver_list_file, "%s\n",
               driver_list_sorted[i].c_str());

      fclose(driver_list_file);
   }
}
#endif

bool retro_load_game(const struct retro_game_info *info)
{
    char basename[256] = {0};
    bool opt_categories_supported;
    char override_path[2048] = {0};
    config_file_t *option_overrides = NULL;
    struct retro_core_option_v2_definition *option_def = NULL;

    if (string_is_empty(info->path))
        return false;

#if defined(EVERCADE_DEBUG)
    if (string_is_equal(path_get_extension(info->path),
        EVERCADE_ROM_LIST_FILE_EXT_NO_DOT))
    {
        write_driver_list(info->path);
        return false;
    }
#endif

    /* Cache path components */
    extract_basename(basename, info->path, sizeof(basename));
    extract_directory(g_rom_dir, info->path, sizeof(g_rom_dir));
    strlcpy(RPATH, info->path, sizeof(RPATH));

    /* Load option overrides
     * > Get options file path */
    fill_pathname(override_path, info->path,
        EVERCADE_OPTION_OVERRIDE_FILE_EXT,
        sizeof(override_path));

    /* Read options file */
    if (!string_is_empty(override_path) &&
        path_is_valid(override_path) &&
        (option_overrides = config_file_new_from_path_to_string(override_path)))
    {
        osd_printf_info("Reading option overrides from: %s\n", override_path);

        /* Override default option values */
        for (option_def = option_defs_us; option_def->key; option_def++)
        {
            struct config_entry_list *option_override =
                config_get_entry(option_overrides, option_def->key);

            if (option_override && option_override->value)
                option_def->default_value = option_override->value;
        }

        /* Cache current override config
         * (i.e. want value strings to remain valid for
         * the 'lifetime' of the loaded content) */
        if (evercade_option_overrides)
            config_file_free(evercade_option_overrides);
        evercade_option_overrides = option_overrides;
    }

    /* Can now upload core options to frontend */
    libretro_set_core_options(environ_cb, &opt_categories_supported);
    check_variables();

#ifdef M16B
    memset(videoBuffer, 0, 1600*1200*2);
#else
    memset(videoBuffer, 0, 1600*1200*2*2);
#endif

#if defined(HAVE_GL)
#ifdef GLES
    hw_render.context_type = RETRO_HW_CONTEXT_OPENGLES2;
#else
    hw_render.context_type = RETRO_HW_CONTEXT_OPENGL;
#endif
    hw_render.context_reset = context_reset;
    hw_render.context_destroy = context_destroy;
    /*
       hw_render.depth = true;
       hw_render.stencil = true;
       hw_render.bottom_left_origin = true;
       */
    if (!environ_cb(RETRO_ENVIRONMENT_SET_HW_RENDER, &hw_render))
       return false;
#endif

    return true;
}

void retro_unload_game(void)
{
   if ( mame_machine_manager::instance() != NULL && mame_machine_manager::instance()->machine() != NULL &&
		   mame_machine_manager::instance()->machine()->options().autosave() &&
		   (mame_machine_manager::instance()->machine()->system().flags & MACHINE_SUPPORTS_SAVE) != 0)
	   mame_machine_manager::instance()->machine()->immediate_save("auto");

   if (retro_pause == 0)
   {
      retro_pause = -1;
   }

   if (evercade_option_overrides)
      config_file_free(evercade_option_overrides);
   evercade_option_overrides = NULL;
}

/* Stubs */
size_t retro_serialize_size(void)
{
	if ( mame_machine_manager::instance() != NULL && mame_machine_manager::instance()->machine() != NULL &&
			mame_machine_manager::instance()->machine()->save().state_size() > 0)
		return mame_machine_manager::instance()->machine()->save().state_size();

	return 0;
}
bool retro_serialize(void *data, size_t size)
{
	if ( mame_machine_manager::instance() != NULL && mame_machine_manager::instance()->machine() != NULL &&
			mame_machine_manager::instance()->machine()->save().state_size() > 0)
		return (mame_machine_manager::instance()->machine()->save().write_data(data, size) == STATERR_NONE);
	return false;
}
bool retro_unserialize(const void * data, size_t size)
{
	if ( mame_machine_manager::instance() != NULL && mame_machine_manager::instance()->machine() != NULL &&
			mame_machine_manager::instance()->machine()->save().state_size() > 0)
		return (mame_machine_manager::instance()->machine()->save().read_data((void*)data, size) == STATERR_NONE);
	return false;
}

unsigned retro_get_region (void) { return RETRO_REGION_NTSC; }

void *find_mame_bank_base(offs_t start, address_space &space)
{
	for ( memory_bank &bank : mame_machine_manager::instance()->machine()->memory().banks() )
		if ( bank.bytestart() == space.address_to_byte(start))
			return bank.base() ;
	return NULL;
}
void *retro_get_memory_data(unsigned type)
{
	void *best_match1 = NULL ;
	void *best_match2 = NULL ;
	void *best_match3 = NULL ;

	/* Eventually the RA cheat system can be updated to accommodate multiple memory
	 * locations, but for now this does a pretty good job for MAME since most of the machines
	 * have a single primary RAM segment that is marked read/write as AMH_RAM.
	 *
	 * This will find a best match based on certain qualities of the address_map_entry objects.
	 */

	if ( type == RETRO_MEMORY_SYSTEM_RAM && mame_machine_manager::instance() != NULL &&
			mame_machine_manager::instance()->machine() != NULL )
		for (address_space &space : mame_machine_manager::instance()->machine()->memory().spaces())
			for (address_map_entry &entry : space.map()->m_entrylist)
				if ( entry.m_read.m_type == AMH_RAM )
					if ( entry.m_write.m_type == AMH_RAM )
						if ( entry.m_share == NULL )
							best_match1 = find_mame_bank_base(entry.m_addrstart, space) ;
						else
							best_match2 = find_mame_bank_base(entry.m_addrstart, space) ;
					else
						best_match3 = find_mame_bank_base(entry.m_addrstart, space) ;


	return ( best_match1 != NULL ? best_match1 : ( best_match2 != NULL ? best_match2 : best_match3 ) );
}
size_t retro_get_memory_size(unsigned type)
{
	size_t best_match1 = NULL ;
	size_t best_match2 = NULL ;
	size_t best_match3 = NULL ;

	if ( type == RETRO_MEMORY_SYSTEM_RAM && mame_machine_manager::instance() != NULL &&
			mame_machine_manager::instance()->machine() != NULL )
		for (address_space &space : mame_machine_manager::instance()->machine()->memory().spaces())
			for (address_map_entry &entry : space.map()->m_entrylist)
				if ( entry.m_read.m_type == AMH_RAM )
					if ( entry.m_write.m_type == AMH_RAM )
						if ( entry.m_share == NULL )
							best_match1 = entry.m_addrend - entry.m_addrstart + 1 ;
						else
							best_match2 = entry.m_addrend - entry.m_addrstart + 1 ;
					else
						best_match3 = entry.m_addrend - entry.m_addrstart + 1 ;

	return ( best_match1 != NULL ? best_match1 : ( best_match2 != NULL ? best_match2 : best_match3 ) );
}
bool retro_load_game_special(unsigned game_type, const struct retro_game_info *info, size_t num_info) {return false; }
void retro_cheat_reset(void) {}
void retro_cheat_set(unsigned unused, bool unused1, const char* unused2) {}
void retro_set_controller_port_device(unsigned in_port, unsigned device) {}

void *retro_get_fb_ptr(void)
{
   return videoBuffer;
}

void retro_frame_draw_enable(bool enable)
{
   draw_this_frame = enable;
}
