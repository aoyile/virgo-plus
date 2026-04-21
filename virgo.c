#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>

#define sb_free(a) ((a) ? HeapFree(GetProcessHeap(), 0, stb__sbraw(a)), 0 : 0)
#define sb_push(a, v) (stb__sbmaybegrow(a, 1), (a)[stb__sbn(a)++] = (v))
#define sb_count(a) ((a) ? stb__sbn(a) : 0)

#define stb__sbraw(a) ((int *)(a)-2)
#define stb__sbm(a) stb__sbraw(a)[0]
#define stb__sbn(a) stb__sbraw(a)[1]

#define stb__sbneedgrow(a, n) ((a) == 0 || stb__sbn(a) + (n) >= stb__sbm(a))
#define stb__sbmaybegrow(a, n) (stb__sbneedgrow(a, (n)) ? stb__sbgrow(a, n) : 0)
#define stb__sbgrow(a, n) ((a) = stb__sbgrowf((a), (n), sizeof(*(a))))

#define VIRGO_NUM_DESKTOPS 4

#ifndef MOD_NOREPEAT
#define MOD_NOREPEAT 0x4000
#endif

#define VIRGO_MOD_GO_ID      (MOD_ALT     | MOD_NOREPEAT )
#define VIRGO_MOD_MOVE_ID    (MOD_CONTROL | MOD_NOREPEAT )
#define VIRGO_MOD_MOVE_GO_ID (MOD_SHIFT   | MOD_CONTROL | MOD_ALT | MOD_NOREPEAT)
#define VIRGO_MOD_GO         (MOD_CONTROL | MOD_ALT     | MOD_NOREPEAT)
#define VIRGO_MOD_MOVE       (MOD_SHIFT   | MOD_ALT     | MOD_NOREPEAT)
#define VIRGO_MOD_MOVE_GO    (MOD_SHIFT   | MOD_CONTROL | MOD_ALT | MOD_CONTROL | MOD_NOREPEAT)

#define VIRGO_KEY_ID_BASE 3

typedef struct {
  HWND *windows;
  unsigned count;
} VirgoWindows;

typedef struct {
  NOTIFYICONDATA nid;
  HBITMAP hBitmap;
  HFONT hFont;
  HWND hwnd;
  HDC mdc;
  unsigned bitmapWidth;
} VirgoTrayicon;

typedef struct {
  unsigned current;
  unsigned handle_hotkeys;
  VirgoWindows desktops[VIRGO_NUM_DESKTOPS];
  VirgoTrayicon trayicon;
} Virgo;

void *stb__sbgrowf(void *arr, unsigned increment, unsigned itemsize)
{
  unsigned dbl_cur = arr ? 2 * stb__sbm(arr) : 0;
  unsigned min_needed = sb_count(arr) + increment;
  unsigned m = dbl_cur > min_needed ? dbl_cur : min_needed;
  unsigned *p;
  if (arr) {
    p = HeapReAlloc(GetProcessHeap(), 0, stb__sbraw(arr),
                    itemsize * m + sizeof(unsigned) * 2);
  } else {
    p = HeapAlloc(GetProcessHeap(), 0, itemsize * m + sizeof(unsigned) * 2);
  }
  if (p) {
    if (!arr) {
      p[1] = 0;
    }
    p[0] = m;
    return p + 2;
  } else {
    ExitProcess(1);
    return (void *)(2 * sizeof(unsigned));
  }
}

HICON _virgo_trayicon_draw(VirgoTrayicon *t, char *text, unsigned len)
{
  ICONINFO iconInfo;
  HBITMAP hOldBitmap;
  HFONT hOldFont;
  hOldBitmap = (HBITMAP)SelectObject(t->mdc, t->hBitmap);
  hOldFont = (HFONT)SelectObject(t->mdc, t->hFont);
  TextOut(t->mdc, t->bitmapWidth / 4, 0, (LPCWSTR)(text), len);
  SelectObject(t->mdc, hOldBitmap);
  SelectObject(t->mdc, hOldFont);
  iconInfo.fIcon = TRUE;
  iconInfo.xHotspot = iconInfo.yHotspot = 0;
  iconInfo.hbmMask = iconInfo.hbmColor = t->hBitmap;
  return CreateIconIndirect(&iconInfo);
}

void _virgo_trayicon_init(VirgoTrayicon *t)
{
  HDC hdc;
  t->hwnd = CreateWindowA("STATIC", "virgo", 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL);
  t->bitmapWidth = GetSystemMetrics(SM_CXSMICON);
  t->nid.cbSize = sizeof(t->nid);
  t->nid.hWnd = t->hwnd;
  t->nid.uID = 100;
  t->nid.uFlags = NIF_ICON;
  hdc = GetDC(t->hwnd);
  t->hBitmap = CreateCompatibleBitmap(hdc, t->bitmapWidth, t->bitmapWidth);
  t->mdc = CreateCompatibleDC(hdc);
  ReleaseDC(t->hwnd, hdc);
  SetBkColor(t->mdc, RGB(0x00, 0x00, 0x00));
  SetTextColor(t->mdc, RGB(0x00, 0xFF, 0x00));
  t->hFont = CreateFont(-MulDiv(11, GetDeviceCaps(t->mdc, LOGPIXELSY), 72), 0,
                        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, TEXT("Arial"));
  t->nid.hIcon = _virgo_trayicon_draw(t, "1", 1);
  Shell_NotifyIcon(NIM_ADD, &t->nid);
}

void _virgo_trayicon_set(VirgoTrayicon *t, unsigned number)
{
  char snumber[2];
  if (number > 9) {
    return;
  }
  snumber[0] = number + '0';
  snumber[1] = 0;
  DestroyIcon(t->nid.hIcon);
  t->nid.hIcon = _virgo_trayicon_draw(t, snumber, 1);
  Shell_NotifyIcon(NIM_MODIFY, &t->nid);
}

void _virgo_trayicon_deinit(VirgoTrayicon *t)
{
  Shell_NotifyIcon(NIM_DELETE, &t->nid);
  DestroyIcon(t->nid.hIcon);
  DeleteObject(t->hBitmap);
  DeleteObject(t->hFont);
  DeleteDC(t->mdc);
  DestroyWindow(t->hwnd);
}

void _virgo_windows_mod(VirgoWindows *wins, unsigned state)
{
  unsigned i;
  for (i = 0; i < wins->count; i++) {
    ShowWindow(wins->windows[i], state);
  }
}

inline void _virgo_windows_show(VirgoWindows *wins) {
  _virgo_windows_mod(wins, SW_SHOW);
}

inline void _virgo_windows_hide(VirgoWindows *wins) {
  _virgo_windows_mod(wins, SW_HIDE);
}

inline void _virgo_windows_add(VirgoWindows *wins, HWND hwnd)
{
  if ((int)(wins->count) >= sb_count(wins->windows)) {
    sb_push(wins->windows, hwnd);
  } else {
    wins->windows[wins->count] = hwnd;
  }
  wins->count++;
}

inline void _virgo_windows_del(VirgoWindows *wins, HWND hwnd)
{
  unsigned i, e;
  for (i = 0; i < wins->count; i++) {
    if (wins->windows[i] != hwnd) {
      continue;
    }
    if (i != wins->count - 1) {
      for (e = i; e < wins->count - 1; e++) {
        wins->windows[e] = wins->windows[e + 1];
      }
    }
    wins->count--;
    break;
  }
}

unsigned _virgo_is_valid_window(HWND hwnd)
{
  WINDOWINFO wi;
  wi.cbSize = sizeof(wi);
  GetWindowInfo(hwnd, &wi);
  return (wi.dwStyle & WS_VISIBLE) && !(wi.dwExStyle & WS_EX_TOOLWINDOW);
}

void _virgo_register_hotkey(unsigned id, unsigned mod, unsigned vk)
{
  if (!RegisterHotKey(NULL, id, mod, vk)) {
    MessageBox(NULL, (LPCWSTR)("could not register hotkey"), (LPCWSTR)("error"), MB_ICONEXCLAMATION);
    ExitProcess(1);
  }
}

BOOL _virgo_enum_func(HWND hwnd, LPARAM lParam)
{
  unsigned i, e;
  Virgo *v;
  VirgoWindows *desk;
  v = (Virgo *)lParam;
  if (!_virgo_is_valid_window(hwnd)) {
    return 1;
  }
  for (i = 0; i < VIRGO_NUM_DESKTOPS; i++) {
    desk = &(v->desktops[i]);
    for (e = 0; e < desk->count; e++) {
      if (desk->windows[e] == hwnd) {
        return 1;
      }
    }
  }
  _virgo_windows_add(&(v->desktops[v->current]), hwnd);
  return 1;
}

void _virgo_update(Virgo *v)
{
  unsigned i, e;
  VirgoWindows *desk;
  HWND hwnd;
  for (i = 0; i < VIRGO_NUM_DESKTOPS; i++) {
    desk = &(v->desktops[i]);
    for (e = 0; e < desk->count; e++) {
      hwnd = desk->windows[e];
      if (!GetWindowThreadProcessId(desk->windows[e], NULL)) {
        _virgo_windows_del(desk, hwnd);
      }
    }
  }
  desk = &v->desktops[v->current];
  for (i = 0; i < desk->count; i++) {
    hwnd = desk->windows[i];
    if (!IsWindowVisible(hwnd)) {
      _virgo_windows_del(desk, hwnd);
    }
  }
  EnumWindows((WNDENUMPROC)&_virgo_enum_func, (LPARAM)v);
}

unsigned VIRGO_KEY_ID_ADD          = 0;
unsigned VIRGO_KEY_ID_GO_NEXT      = 0;
unsigned VIRGO_KEY_ID_GO_PREV      = 0;
unsigned VIRGO_KEY_ID_MOVE_NEXT    = 0;
unsigned VIRGO_KEY_ID_MOVE_PREV    = 0;
unsigned VIRGO_KEY_ID_MOVE_GO_NEXT = 0;
unsigned VIRGO_KEY_ID_MOVE_GO_PREV = 0;
unsigned VIRGO_KEY_ID_QUIT         = 0;
unsigned VIRGO_KEY_ID_TOGGLE       = 0;

void _virgo_enable_hotkey(Virgo *v)
{
  unsigned i;
  for (i = 0; i < VIRGO_NUM_DESKTOPS; i++) {
    _virgo_register_hotkey(i * VIRGO_KEY_ID_BASE,     VIRGO_MOD_GO_ID,      i + 1 + '0');
    _virgo_register_hotkey(i * VIRGO_KEY_ID_BASE + 1, VIRGO_MOD_MOVE_ID,    i + 1 + '0');
    _virgo_register_hotkey(i * VIRGO_KEY_ID_BASE + 2, VIRGO_MOD_MOVE_GO_ID, i + 1 + '0');
  }
  VIRGO_KEY_ID_ADD          = i * VIRGO_KEY_ID_BASE;
  VIRGO_KEY_ID_GO_NEXT      = VIRGO_KEY_ID_ADD;
  VIRGO_KEY_ID_GO_PREV      = VIRGO_KEY_ID_ADD + 1;
  VIRGO_KEY_ID_MOVE_NEXT    = VIRGO_KEY_ID_ADD + 2;
  VIRGO_KEY_ID_MOVE_PREV    = VIRGO_KEY_ID_ADD + 3;
  VIRGO_KEY_ID_MOVE_GO_NEXT = VIRGO_KEY_ID_ADD + 4;
  VIRGO_KEY_ID_MOVE_GO_PREV = VIRGO_KEY_ID_ADD + 5;
  VIRGO_KEY_ID_QUIT         = VIRGO_KEY_ID_ADD + 6;
  VIRGO_KEY_ID_TOGGLE       = VIRGO_KEY_ID_ADD + 7;
  _virgo_register_hotkey(VIRGO_KEY_ID_GO_NEXT, VIRGO_MOD_GO, VK_RIGHT);
  _virgo_register_hotkey(VIRGO_KEY_ID_GO_PREV, VIRGO_MOD_GO, VK_LEFT);
  _virgo_register_hotkey(VIRGO_KEY_ID_MOVE_NEXT, VIRGO_MOD_MOVE, VK_RIGHT);
  _virgo_register_hotkey(VIRGO_KEY_ID_MOVE_PREV, VIRGO_MOD_MOVE, VK_LEFT);
  _virgo_register_hotkey(VIRGO_KEY_ID_MOVE_GO_NEXT, VIRGO_MOD_MOVE_GO, VK_RIGHT);
  _virgo_register_hotkey(VIRGO_KEY_ID_MOVE_GO_PREV, VIRGO_MOD_MOVE_GO, VK_LEFT);
}

void _virgo_disable_hotkey(Virgo *v)
{
  unsigned i;
  for (i = 0; i < VIRGO_NUM_DESKTOPS; i++) {
    UnregisterHotKey(NULL, i * VIRGO_KEY_ID_BASE);
    UnregisterHotKey(NULL, i * VIRGO_KEY_ID_BASE + 1);
    UnregisterHotKey(NULL, i * VIRGO_KEY_ID_BASE + 2);
  }
  UnregisterHotKey(NULL, VIRGO_KEY_ID_GO_NEXT     );
  UnregisterHotKey(NULL, VIRGO_KEY_ID_GO_PREV     );
  UnregisterHotKey(NULL, VIRGO_KEY_ID_MOVE_NEXT   );
  UnregisterHotKey(NULL, VIRGO_KEY_ID_MOVE_PREV   );
  UnregisterHotKey(NULL, VIRGO_KEY_ID_MOVE_GO_NEXT);
  UnregisterHotKey(NULL, VIRGO_KEY_ID_MOVE_GO_PREV);
}

void virgo_init(Virgo *v)
{
  unsigned i;
  v->handle_hotkeys = 1;
  _virgo_enable_hotkey(v);
  _virgo_register_hotkey(VIRGO_KEY_ID_QUIT, MOD_ALT | MOD_CONTROL | MOD_SHIFT | MOD_NOREPEAT, 'Q');
  _virgo_register_hotkey(VIRGO_KEY_ID_TOGGLE, MOD_ALT | MOD_CONTROL | MOD_SHIFT | MOD_NOREPEAT, 'S');
  _virgo_trayicon_init(&v->trayicon);
}

void virgo_deinit(Virgo *v)
{
  unsigned i;
  for (i = 0; i < VIRGO_NUM_DESKTOPS; i++) {
    _virgo_windows_show(&v->desktops[i]);
    sb_free(v->desktops[i].windows);
  }
  _virgo_trayicon_deinit(&v->trayicon);
}

void virgo_toggle_hotkeys(Virgo *v)
{
  unsigned i;
  v->handle_hotkeys = !v->handle_hotkeys;
  if (v->handle_hotkeys) {
    _virgo_enable_hotkey(v);
  } else {
    _virgo_disable_hotkey(v);
  }
}

void _virgo_move_to_desk(Virgo *v, unsigned desk)
{
  HWND hwnd;
  _virgo_update(v);
  hwnd = GetForegroundWindow();
  if (!hwnd || !_virgo_is_valid_window(hwnd)) {
    return;
  }
  _virgo_windows_del(&v->desktops[v->current], hwnd);
  _virgo_windows_add(&v->desktops[desk], hwnd);
  ShowWindow(hwnd, SW_HIDE);
}

void _virgo_go_to_desk(Virgo *v, unsigned desk)
{
  _virgo_update(v);
  _virgo_windows_hide(&v->desktops[v->current]);
  _virgo_windows_show(&v->desktops[desk]);
  v->current = desk;
  _virgo_trayicon_set(&v->trayicon, v->current + 1);
}

inline unsigned _virgo_next_desk(unsigned desk)
{
  desk = desk + 1;
  if (desk == VIRGO_NUM_DESKTOPS) {
    desk = 0;
  }
  return desk;
}

inline unsigned _virgo_prev_desk(unsigned desk)
{
  if (desk == 0) {
    desk = VIRGO_NUM_DESKTOPS - 1;
  } else {
    desk-= 1;
  }
  return desk;
}

void virgo_move_to_desk(Virgo *v, unsigned desk)
{
  if (v->current == desk) {
    return;
  }
  _virgo_move_to_desk(v, desk);
}

void virgo_go_to_desk(Virgo *v, unsigned desk)
{
  if (v->current == desk) {
    return;
  }
  _virgo_go_to_desk(v, desk);
}

void virgo_go_next_desk(Virgo *v)
{
  unsigned desk = _virgo_next_desk(v->current);
  _virgo_go_to_desk(v, desk);
}

void virgo_go_prev_desk(Virgo *v)
{
  unsigned desk = _virgo_prev_desk(v->current);
  _virgo_go_to_desk(v, desk);
}

void virgo_move_next_desk(Virgo *v)
{
  unsigned desk = _virgo_next_desk(v->current);
  _virgo_move_to_desk(v, desk);
}

void virgo_move_prev_desk(Virgo *v)
{
  unsigned desk = _virgo_prev_desk(v->current);
  _virgo_move_to_desk(v, desk);
}

void virgo_move_go_next_desk(Virgo *v)
{
  unsigned desk = _virgo_next_desk(v->current);
  _virgo_move_to_desk(v, desk);
  _virgo_go_to_desk(v, desk);
}

void virgo_move_go_prev_desk(Virgo *v)
{
  unsigned desk = _virgo_prev_desk(v->current);
  _virgo_move_to_desk(v, desk);
  _virgo_go_to_desk(v, desk);
}

int main(void)
{
  Virgo v = {0};
  MSG msg;
  virgo_init(&v);
  while (GetMessage(&msg, NULL, 0, 0)) {
    if (msg.message != WM_HOTKEY) {
      continue;
    }

    if (msg.wParam < VIRGO_KEY_ID_ADD) {
      if (msg.wParam % VIRGO_KEY_ID_BASE == 0) {
        virgo_go_to_desk(&v, msg.wParam / VIRGO_KEY_ID_BASE);
      } else if (msg.wParam % VIRGO_KEY_ID_BASE == 1) {
        virgo_move_to_desk(&v, (msg.wParam - 1) / VIRGO_KEY_ID_BASE);
      } else {
        virgo_move_to_desk(&v, (msg.wParam - 2) / VIRGO_KEY_ID_BASE);
        virgo_go_to_desk(&v, (msg.wParam - 2) / VIRGO_KEY_ID_BASE);
      }
    } else if (msg.wParam == VIRGO_KEY_ID_GO_NEXT) {
      virgo_go_next_desk(&v);
    } else if (msg.wParam == VIRGO_KEY_ID_GO_PREV) {
      virgo_go_prev_desk(&v);
    } else if (msg.wParam == VIRGO_KEY_ID_MOVE_GO_NEXT) {
      virgo_move_go_next_desk(&v);
    } else if (msg.wParam == VIRGO_KEY_ID_MOVE_GO_PREV) {
      virgo_move_go_prev_desk(&v);
    } else if (msg.wParam == VIRGO_KEY_ID_MOVE_NEXT) {
      virgo_move_next_desk(&v);
    } else if (msg.wParam == VIRGO_KEY_ID_MOVE_PREV) {
      virgo_move_prev_desk(&v);
    } else if (msg.wParam == VIRGO_KEY_ID_TOGGLE) {
      virgo_toggle_hotkeys(&v);
    } else if (msg.wParam == VIRGO_KEY_ID_QUIT) {
      break;
    }
  }
  virgo_deinit(&v);
  ExitProcess(0);
}
