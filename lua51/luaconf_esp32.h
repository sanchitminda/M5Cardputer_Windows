/* lua51/luaconf_esp32.h
 * ESP32-specific overrides for Lua 5.1.5.
 *
 * HOW TO APPLY:
 *   1. Download Lua 5.1.5 source: https://www.lua.org/ftp/lua-5.1.5.tar.gz
 *   2. Copy all files from lua-5.1.5/src/ into this lua51/ directory.
 *      Required .c files (24):
 *        lapi.c lauxlib.c lbaselib.c lcode.c ldebug.c ldo.c ldump.c lfunc.c
 *        lgc.c  llex.c    lmathlib.c lmem.c  lobject.c lopcodes.c lparser.c
 *        lstate.c lstring.c lstrlib.c ltable.c ltablib.c ltm.c lundump.c lvm.c lzio.c
 *      Required internal headers (all .h files from src/).
 *      EXCLUDE (saves ~25 KB flash): liolib.c loslib.c ldblib.c loadlib.c linit.c
 *
 *   3. Open lua51/luaconf.h (from the stock source) and ADD the block below
 *      near the top, just after the first #ifndef lconfig_h guard:
 *
 *      --- BEGIN paste into luaconf.h ---
 *      #include "luaconf_esp32.h"
 *      --- END paste ---
 *
 *   This file uses #undef/#define to override stock defaults cleanly.
 */

#ifndef LUACONF_ESP32_H
#define LUACONF_ESP32_H

/* ---- Platform ---- */
#define LUA_USE_C89          /* avoid C99 VLAs, // comments, etc. */
#undef  LUA_USE_POSIX        /* no signal(), popen() on ESP32     */
#undef  LUA_USE_READLINE     /* no libreadline on ESP32           */

/* ---- Number type: doubles (ESP32 has hardware FPU) ---- */
#ifndef LUA_NUMBER_DOUBLE
  #define LUA_NUMBER_DOUBLE
#endif

/* ---- Memory limits (critical: ESP32 has ~320 KB usable DRAM) ---- */
#undef  LUAI_MAXSTACK
#define LUAI_MAXSTACK    256    /* default 8192 is wasteful; 256 is ample */

#undef  LUAI_MAXCSTACK
#define LUAI_MAXCSTACK   200

#undef  LUA_MINBUFFER
#define LUA_MINBUFFER    32     /* default 512; reduce read buffer */

#undef  LUA_IDSIZE
#define LUA_IDSIZE       60     /* shorter source names in error messages */

/* ---- Garbage collector: more aggressive to stay under OOM threshold ---- */
#undef  LUAI_GCPAUSE
#define LUAI_GCPAUSE     110    /* kick in at 110% growth (default 200) */

#undef  LUAI_GCMUL
#define LUAI_GCMUL       200    /* keep default collector speed */

/* ---- Package/path: disable (no filesystem loader on ESP32) ---- */
#undef  LUA_PATH_DEFAULT
#define LUA_PATH_DEFAULT  ""

#undef  LUA_CPATH_DEFAULT
#define LUA_CPATH_DEFAULT ""

/* ---- Keep vararg compat (coroutine.* in lbaselib is free) ---- */
#define LUA_COMPAT_VARARG

/* ---- Max C calls (FreeRTOS task stack is 8 KB; keep this moderate) ---- */
#undef  LUAI_MAXCCALLS
#define LUAI_MAXCCALLS   200

#endif /* LUACONF_ESP32_H */
