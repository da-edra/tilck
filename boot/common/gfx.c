/* SPDX-License-Identifier: BSD-2-Clause */

#include <tilck_gen_headers/config_boot.h>
#include <tilck/common/basic_defs.h>
#include <tilck/common/failsafe_assert.h>
#include <tilck/common/string_util.h>
#include <tilck/common/printk.h>

#include <tilck/boot/gfx.h>
#include "common_int.h"

const struct bootloader_intf *intf;

void init_common_bootloader_code(const struct bootloader_intf *i)
{
   if (!intf)
      intf = i;
}

static bool
is_usable_video_mode(struct generic_video_mode_info *gi)
{
   return gi->xres >= TILCK_MIN_RES_X && gi->yres >= TILCK_MIN_RES_Y;
}

static bool
is_optimal_video_mode(struct generic_video_mode_info *gi)
{
   if (!is_usable_video_mode(gi))
      return false;

   if (gi->xres % 8) {

      /*
       * Tilck's fb console won't be able to use the optimized functions in this
       * case (they ultimately use a 256-bit wide memcpy()).
       */
      return false;
   }

   return true;
}

static bool
is_default_resolution(u32 w, u32 h)
{
   return w == PREFERRED_GFX_MODE_W && h == PREFERRED_GFX_MODE_H;
}

static bool
exists_mode_in_array(video_mode_t mode, video_mode_t *arr, int array_sz)
{
   for (int i = 0; i < array_sz; i++)
      if (arr[i] == mode)
         return true;

   return false;
}

static void
show_mode(int num, struct generic_video_mode_info *gi, bool is_default)
{
   printk("Mode [%d]: %d x %d x %d%s\n",
          num, gi->xres, gi->yres,
          gi->bpp, is_default ? " [DEFAULT]" : "");
}

void
filter_video_modes(video_mode_t *all_modes,
                   int all_modes_cnt,
                   void *opaque_mi,
                   bool show_modes,
                   int bpp,
                   video_mode_t text_mode,
                   struct ok_modes_info *okm)
{
   struct generic_video_mode_info gi;
   video_mode_t curr_mode_num;
   video_mode_t max_mode = INVALID_VIDEO_MODE;
   video_mode_t min_mode = INVALID_VIDEO_MODE;
   u32 min_mode_pixels = 0;
   u32 max_mode_pixels = 0;
   int cnt = 0;
   u32 p;

   okm->defmode = INVALID_VIDEO_MODE;

   if (text_mode != INVALID_VIDEO_MODE) {

      if (show_modes)
         printk("Mode [0]: text mode 80 x 25\n");

      okm->ok_modes[cnt++] = text_mode;
   }

   for (int i = 0; i < all_modes_cnt; i++) {

      curr_mode_num = all_modes ? all_modes[i] : (video_mode_t)i;

      if (!intf->get_mode_info(curr_mode_num, opaque_mi, &gi))
         continue;

      if (!intf->is_mode_usable(opaque_mi))
         continue;

      if (gi.bpp != bpp)
         continue;

      if (!is_usable_video_mode(&gi))
         continue;

      p = gi.xres * gi.yres;

      if (!min_mode_pixels || p < min_mode_pixels) {
         min_mode_pixels = p;
         min_mode = curr_mode_num;
      }

      if (p > max_mode_pixels) {
         max_mode_pixels = p;
         max_mode = curr_mode_num;
      }

      if (!is_optimal_video_mode(&gi))
         continue;

      if (is_default_resolution(gi.xres, gi.yres))
         okm->defmode = curr_mode_num;

      if (cnt < okm->ok_modes_array_size - 1) {

         if (show_modes)
            show_mode(cnt, &gi, okm->defmode == curr_mode_num);

         okm->ok_modes[cnt++] = curr_mode_num;
      }
   }

   if (okm->defmode == INVALID_VIDEO_MODE) {
      okm->defmode = min_mode;
   }

   if (max_mode != INVALID_VIDEO_MODE) {

      /* Display the max mode, even if might not be optimal for Tilck */
      if (!exists_mode_in_array(max_mode, okm->ok_modes, cnt)) {

         if (!intf->get_mode_info(max_mode, opaque_mi, &gi))
            panic("get_mode_info(0x%x) failed", max_mode);

         if (show_modes)
            show_mode(cnt, &gi, false);

         okm->ok_modes[cnt++] = max_mode;

         if (okm->defmode == INVALID_VIDEO_MODE)
            okm->defmode = max_mode;
      }
   }

   if (okm->defmode == INVALID_VIDEO_MODE) {

      if (cnt > 0) {

         okm->defmode = okm->ok_modes[0];

         if (okm->defmode == text_mode && cnt > 1)
            okm->defmode = okm->ok_modes[1];
      }
   }

   okm->ok_modes_cnt = cnt;
}

video_mode_t
get_user_video_mode_choice(struct ok_modes_info *okm)
{
   int len, err = 0;
   char buf[16];
   long s;

   while (true) {

      printk("Select a video mode [0 - %d]: ", okm->ok_modes_cnt - 1);

      len = read_line(buf, sizeof(buf));

      if (!len) {
         printk("<default>\n");
         return okm->defmode;
      }

      s = tilck_strtol(buf, NULL, 10, &err);

      if (err || s < 0 || s > okm->ok_modes_cnt - 1) {
         printk("Invalid selection.\n");
         continue;
      }

      break;
   }

   return okm->ok_modes[s];
}