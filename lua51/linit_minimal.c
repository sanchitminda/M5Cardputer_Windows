/* lua51/linit_minimal.c
 * Minimal Lua library initializer — opens only the four libs used by MicroOS:
 * base, math, string, table.  Excludes io, os, package, debug to save ~25 KB flash.
 *
 * LuaRuntime.h calls each luaopen_* individually and does NOT call luaL_openlibs(),
 * so this file is an optional alternative.  It is compiled in automatically by
 * Arduino IDE (which compiles all .c files in sketch subdirectories) but only
 * exported if you switch LuaRuntime::onLaunch to call luaL_openlibs_minimal(L).
 */
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

static const luaL_Reg microos_libs[] = {
  { "",              luaopen_base   },   /* base must be opened with empty name */
  { LUA_MATHLIBNAME, luaopen_math   },
  { LUA_STRLIBNAME,  luaopen_string },
  { LUA_TABLIBNAME,  luaopen_table  },
  { NULL, NULL }
};

LUALIB_API void luaL_openlibs_minimal(lua_State *L) {
  const luaL_Reg *lib = microos_libs;
  for (; lib->func; lib++) {
    lua_pushcfunction(L, lib->func);
    lua_pushstring(L, lib->name);
    lua_call(L, 1, 0);
  }
}
