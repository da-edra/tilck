/* SPDX-License-Identifier: BSD-2-Clause */

#include <tilck/common/basic_defs.h>
#include <tilck/common/string_util.h>
#include <tilck/common/color_defs.h>
#include <tilck/kernel/term.h>
#include "termutil.h"

void dp_write_raw(const char *fmt, ...)
{
   char buf[256];
   va_list args;
   int rc;

   va_start(args, fmt);
   rc = vsnprintk(buf, sizeof(buf), fmt, args);
   va_end(args);

   term_write(buf, (size_t)rc, DP_COLOR);
}

void dp_write(int row, int col, const char *fmt, ...)
{
   char buf[256];
   va_list args;
   int rc;
   const int relrow = row - dp_screen_start_row;

   if (relrow > dp_ctx->row_max)
      dp_ctx->row_max = relrow;

   if (relrow < dp_ctx->row_off)
      return;

   row -= dp_ctx->row_off;

   if (row > dp_end_row - 2)
     return;

   va_start(args, fmt);
   rc = vsnprintk(buf, sizeof(buf), fmt, args);
   va_end(args);

   if (col == 0)
      col = dp_start_col + 2;

   dp_move_cursor(row, col);
   term_write(buf, (size_t)rc, DP_COLOR);
}

void dp_draw_rect_raw(int row, int col, int h, int w)
{
   ASSERT(w >= 2);
   ASSERT(h >= 2);

   dp_write_raw(GFX_ON);
   dp_move_cursor(row, col);
   dp_write_raw("l");

   for (int i = 0; i < w-2; i++) {
      dp_write_raw("q");
   }

   dp_write_raw("k");

   for (int i = 1; i < h-1; i++) {

      dp_move_cursor(row+i, col);
      dp_write_raw("x");

      dp_move_cursor(row+i, col+w-1);
      dp_write_raw("x");
   }

   dp_move_cursor(row+h-1, col);
   dp_write_raw("m");

   for (int i = 0; i < w-2; i++) {
      dp_write_raw("q");
   }

   dp_write_raw("j");
   dp_write_raw(GFX_OFF);
}

void dp_draw_rect(const char *label, int row, int col, int h, int w)
{
   ASSERT(w >= 2);
   ASSERT(h >= 2);

   dp_write_raw(GFX_ON);
   dp_write(row, col, "l");

   for (int i = 0; i <= w-2; i++) {
      dp_write(row, col+i+1, "q");
   }

   dp_write(row, col+w-2+1, "k");

   for (int i = 1; i < h-1; i++) {
      dp_write(row+i, col, "x");
      dp_write(row+i, col+w-1, "x");
   }

   dp_write(row+h-1, col, "m");

   for (int i = 0; i <= w-2; i++) {
      dp_write(row+h-1, col+i+1, "q");
   }

   dp_write(row+h-1, col+w-2+1, "j");
   dp_write_raw(GFX_OFF);

   if (label) {
      dp_write(row, col + 2, ESC_COLOR_GREEN "[ %s ]" RESET_ATTRS, label);
   }
}