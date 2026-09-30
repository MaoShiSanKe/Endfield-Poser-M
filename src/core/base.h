#pragma once

#include <windows.h>
#include <mutex>
// Serialize GUI and HTTP pose access; recursive for shared control entrypoints.
static std::recursive_mutex g_poseMutex;
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include "plugin_paths.h"

#include "core/version.h" // 版本号唯一来源（资源文件 src/poser.rc 也读它）

#define POSER_STRINGIFY2(x) #x
#define POSER_STRINGIFY(x) POSER_STRINGIFY2(x)
#define POSER_VERSION POSER_STRINGIFY(POSER_VERSION_MAJOR) "." POSER_STRINGIFY(POSER_VERSION_MINOR) "." POSER_STRINGIFY(POSER_VERSION_PATCH)

static HANDLE g_logHandle = INVALID_HANDLE_VALUE;
static CRITICAL_SECTION g_logLock;
static const wchar_t *g_logPath = nullptr; // 打开失败时用来自动重试
static DWORD g_logRetryAt = 0;             // 重试节流（GetTickCount）

// 同一进程里可能同时存在多个插件实例（代理加载一次、插件管理器再加载一次）。
// 所以日志文件必须允许别人同时读写，否则第二个实例打开必然失败，而失败后它的
// 日志会被整段静默丢弃——排查问题时等于没有日志。
// 用 FILE_APPEND_DATA 而不是 GENERIC_WRITE + 手动 seek：前者由系统保证每次都写到
// 当时的文件末尾，多个实例同时写也不会互相覆盖。
static void OpenLogAttempt() {
  g_logHandle = CreateFileW(g_logPath, FILE_APPEND_DATA,
                            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                            nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
}

static void OpenLog(const wchar_t *path) {
  InitializeCriticalSection(&g_logLock);
  g_logPath = path;
  OpenLogAttempt();
  if (g_logHandle == INVALID_HANDLE_VALUE) {
    char msg[192] = {};
    snprintf(msg, sizeof(msg),
             "Endfield Poser: cannot open log (err=%lu); will retry\n",
             GetLastError());
    OutputDebugStringA(msg);
  }
}

// 打开失败（或写入中失效）后低频重试，最多每 5 秒一次，避免整个会话没有日志。
static void RetryLogIfNeeded() {
  if (g_logHandle != INVALID_HANDLE_VALUE || !g_logPath)
    return;
  DWORD now = GetTickCount();
  if (g_logRetryAt && (DWORD)(now - g_logRetryAt) < 5000)
    return;
  g_logRetryAt = now;
  OpenLogAttempt();
}

// 布料子系统的诊断（[CLOTH-*]）默认静默：那是调试仪表，量极大——实测一次会话就能写几十 MB，
// 既费磁盘也拖性能，还会把真正有用的行冲掉。需要时在 poser_config.txt 里写 debug_cloth=1。
static bool g_debugCloth = false;

void Log(const char *fmt, ...) {
  if (g_logHandle == INVALID_HANDLE_VALUE) {
    RetryLogIfNeeded();
    if (g_logHandle == INVALID_HANDLE_VALUE)
      return;
  }
  if (!g_debugCloth && fmt && strncmp(fmt, "[CLOTH-", 7) == 0)
    return;
  EnterCriticalSection(&g_logLock);
  char buf[4096];
  va_list args;
  va_start(args, fmt);
  int len = vsnprintf(buf, sizeof(buf) - 2, fmt, args);
  va_end(args);
  if (len < 0)
    len = 0;
  // vsnprintf returns the required length, which may exceed the buffer.
  // Leave room for the newline even when a diagnostic was truncated.
  if (len > int(sizeof(buf) - 3))
    len = int(sizeof(buf) - 3);
  buf[len] = '\n';
  len++;
  DWORD written = 0;
  if (!WriteFile(g_logHandle, buf, len, &written, NULL)) {
    // 句柄失效（被别的程序删除/改名等）时释放，交给下一次重试重新打开。
    CloseHandle(g_logHandle);
    g_logHandle = INVALID_HANDLE_VALUE;
  }
  LeaveCriticalSection(&g_logLock);
}

// ---- IL2CPP 布局常量（对象内常用偏移，需与版本匹配，探测失败时回落）----
#define IL2CPP_STR_LEN      0x10
#define IL2CPP_STR_CHARS    0x14
#define IL2CPP_ARRAY_LEN    0x18
#define IL2CPP_ARRAY_DATA   0x20
#define IL2CPP_LIST_ITEMS   0x10
#define IL2CPP_LIST_SIZE    0x18
#define IL2CPP_BOXED_DATA   16

// 字段偏移动态解析失败时回落的默认值；解析成功则用解析值
static int SafeOff(int resolved, int fallback, const char *name) {
  if (resolved >= 0)
    return resolved;
  static unsigned s_warnedMask = 0;
  unsigned h = 0;
  for (const char *p = name; *p; p++)
    h = h * 31 + (unsigned)*p;
  unsigned bit = 1u << (h & 31);
  if (!(s_warnedMask & bit)) {
    s_warnedMask |= bit;
    Log("[WARN] Using fallback offset 0x%X for %s (dynamic resolution failed)",
        fallback, name);
  }
  return fallback;
}
