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
   info->valid_extensions = "zip|chd|7z|cmd";
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

   if (log_cb)
      log_cb(RETRO_LOG_INFO, "AV_INFO: width=%d height=%d\n",info->geometry.base_width,info->geometry.base_height);

   max_width   = fb_width;
   max_height  = fb_height;

   if (log_cb)
      log_cb(RETRO_LOG_INFO, "AV_INFO: max_width=%d max_height=%d\n",info->geometry.max_width,info->geometry.max_height);

   info->geometry.aspect_ratio = retro_aspect;

   if (log_cb)
      log_cb(RETRO_LOG_INFO, "AV_INFO: aspect_ratio = %f\n",info->geometry.aspect_ratio);

   info->timing.fps            = retro_fps;
   info->timing.sample_rate    = 48000.0;

   if (log_cb)
      log_cb(RETRO_LOG_INFO, "AV_INFO: fps = %f sample_rate = %f\n",info->timing.fps,info->timing.sample_rate);

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

   if (log_cb)
      log_cb(RETRO_LOG_INFO, "SYSTEM_DIRECTORY: %s", retro_system_directory);

   if (environ_cb(RETRO_ENVIRONMENT_GET_CONTENT_DIRECTORY, &content_dir) && content_dir)
   {
      // if defined, use the system directory
      retro_content_directory=content_dir;
   }

   if (log_cb)
      log_cb(RETRO_LOG_INFO, "CONTENT_DIRECTORY: %s", retro_content_directory);


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
   if (log_cb)
      log_cb(RETRO_LOG_INFO, "SAVE_DIRECTORY: %s", retro_save_directory);

   if (!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt))
   {
      if (log_cb)
         log_cb(RETRO_LOG_ERROR, "pixel format not supported");
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
    printf("RETRO DEINIT\n");
    if(retro_load_ok)retro_finish();

    libretro_mapping_deinit();
}

void retro_reset (void)
{
   mame_reset = 1;
   //mame_machine_manager::instance()->ui().show_menu();
}

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
      mfirst++;
      mmain(1,RPATH);
      retro_load_ok=true;
      update_runtime_variables();
      return;
   }

   if (NEWGAME_FROM_OSD == 1)
   {
      struct retro_system_av_info ninfo;

      retro_get_system_av_info(&ninfo);

      environ_cb(RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO, &ninfo);

      if (log_cb)
         log_cb(RETRO_LOG_INFO, "ChangeAV: w:%d h:%d ra:%f.\n",
               ninfo.geometry.base_width, ninfo.geometry.base_height, ninfo.geometry.aspect_ratio);

      NEWGAME_FROM_OSD=0;
   }
   else if (NEWGAME_FROM_OSD == 2){
      update_geometry();
      printf("w:%d h:%d a:%f\n",fb_width,fb_height,retro_aspect);
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

bool retro_load_game(const struct retro_game_info *info)
{
    char basename[256];
    bool opt_categories_supported;

    /* Load option overrides */
    if (!string_is_empty(info->path))
    {
        char override_path[2048] = {0};
        config_file_t *option_overrides = NULL;
        struct retro_core_option_v2_definition *option_def = NULL;

        /* Get options file path */
        fill_pathname(override_path, info->path,
            EVERCADE_OPTION_OVERRIDE_FILE_EXT,
            sizeof(override_path));

        /* Read options file */
        if (!string_is_empty(override_path) &&
            path_is_valid(override_path) &&
            (option_overrides = config_file_new_from_path_to_string(override_path)))
        {
            if (log_cb)
                log_cb(RETRO_LOG_INFO, "Reading option overrides from: %s\n", override_path);

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

    extract_basename(basename, info->path, sizeof(basename));
    extract_directory(g_rom_dir, info->path, sizeof(g_rom_dir));
    strcpy(RPATH,info->path);

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
