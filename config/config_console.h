/* SPDX-License-Identifier: BSD-2-Clause */

/*
 * This is a TEMPLATE. The actual config file is generated by CMake and stored
 * in <BUILD_DIR>/tilck_gen_headers/.
 */

#pragma once
#include <tilck_gen_headers/config_kernel.h>

/* ------ Value-based config variables -------- */

#define TTY_COUNT              @TTY_COUNT@
#define TERM_SCROLL_LINES      @TERM_SCROLL_LINES@

/* --------- Boolean config variables --------- */

#cmakedefine01 TERM_BIG_SCROLL_BUF
#cmakedefine01 FB_CONSOLE_BANNER
#cmakedefine01 FB_CONSOLE_CURSOR_BLINK
#cmakedefine01 FB_CONSOLE_USE_ALT_FONTS
#cmakedefine01 KERNEL_SHOW_LOGO
#cmakedefine01 SERIAL_CON_IN_VIDEO_MODE
#cmakedefine01 KRN_PRINTK_ON_CURR_TTY

/*
 * --------------------------------------------------------------------------
 *                  Hard-coded global & derived constants
 * --------------------------------------------------------------------------
 *
 * Here below there are some pseudo-constants not designed to be easily changed
 * because of the code makes assumptions about them. Because of that, those
 * constants are hard-coded and not available as CMake variables. With time,
 * some of those constants get "promoted" and moved in CMake, others remain
 * here. See the comments and think about the potential implications before
 * promoting a hard-coded constant to a configurable CMake variable.
 */

#define TTY_INPUT_BS                                              1024
#define FBCON_OPT_FUNCS_MIN_FREE_HEAP                        (16 * MB)
#define FAILSAFE_FB_VADDR          (KERNEL_BASE_VA + (1024 - 64) * MB)
