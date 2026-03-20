#include "win32_port.h"

#include "esp_log.h"
#include "lvgl_port.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <windowsx.h>

static const char* TAG = "sim_win32";

static HWND s_window = NULL;
static lv_disp_draw_buf_t s_draw_buf;
static lv_disp_drv_t s_disp_drv;
static lv_indev_drv_t s_pointer_drv;
static lv_indev_t* s_pointer = NULL;
static lv_color_t* s_buf1 = NULL;
static lv_color_t* s_buf2 = NULL;
static uint32_t* s_framebuffer = NULL;
static BITMAPINFO s_bitmap_info;

typedef struct
{
    int16_t x;
    int16_t y;
    bool pressed;
} pointer_state_t;

static pointer_state_t s_pointer_state = {0, 0, false};

static void pointer_read_cb(lv_indev_drv_t* drv, lv_indev_data_t* data)
{
    (void)drv;
    data->point.x = s_pointer_state.x;
    data->point.y = s_pointer_state.y;
    data->state = s_pointer_state.pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

static inline uint32_t rgb565_to_argb(const lv_color_t c)
{
    uint16_t raw = c.full;
    uint8_t r = (uint8_t)(((raw >> 11) & 0x1F) * 255 / 31);
    uint8_t g = (uint8_t)(((raw >> 5) & 0x3F) * 255 / 63);
    uint8_t b = (uint8_t)((raw & 0x1F) * 255 / 31);
    return (uint32_t)(0xFF000000 | (r << 16) | (g << 8) | b);
}

static void flush_cb(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* color_p)
{
    if (!s_framebuffer)
    {
        lv_disp_flush_ready(drv);
        return;
    }

    int32_t width = lv_area_get_width(area);
    int32_t height = lv_area_get_height(area);

    for (int32_t y = 0; y < height; ++y)
    {
        int32_t dst_y = area->y1 + y;
        uint32_t* dst = s_framebuffer + dst_y * LVGL_PORT_H_RES + area->x1;
        for (int32_t x = 0; x < width; ++x)
        {
            dst[x] = rgb565_to_argb(color_p[x]);
        }
        color_p += width;
    }

    InvalidateRect(s_window, NULL, FALSE);
    lv_disp_flush_ready(drv);
}

static LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    switch (msg)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        if (s_framebuffer)
        {
            StretchDIBits(hdc, 0, 0, LVGL_PORT_H_RES, LVGL_PORT_V_RES, 0, 0, LVGL_PORT_H_RES,
                          LVGL_PORT_V_RES, s_framebuffer, &s_bitmap_info, DIB_RGB_COLORS, SRCCOPY);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_LBUTTONDOWN:
        SetCapture(hwnd);
        s_pointer_state.pressed = true;
        // fall through
    case WM_MOUSEMOVE:
        if (!s_framebuffer)
        {
            break;
        }
        s_pointer_state.x = (int16_t)GET_X_LPARAM(lparam);
        s_pointer_state.y = (int16_t)GET_Y_LPARAM(lparam);
        if (s_pointer_state.x < 0)
            s_pointer_state.x = 0;
        if (s_pointer_state.y < 0)
            s_pointer_state.y = 0;
        if (s_pointer_state.x >= LVGL_PORT_H_RES)
            s_pointer_state.x = LVGL_PORT_H_RES - 1;
        if (s_pointer_state.y >= LVGL_PORT_V_RES)
            s_pointer_state.y = LVGL_PORT_V_RES - 1;
        return 0;
    case WM_LBUTTONUP:
        ReleaseCapture();
        s_pointer_state.pressed = false;
        return 0;
    default:
        break;
    }
    return DefWindowProc(hwnd, msg, wparam, lparam);
}

static bool create_window(void)
{
    WNDCLASS wc = {0};
    wc.lpfnWndProc = window_proc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = "LVGLPanelSimulator";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    if (!RegisterClass(&wc))
    {
        if (GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        {
            ESP_LOGE(TAG, "Failed to register window class");
            return false;
        }
    }

    RECT rect = {0, 0, LVGL_PORT_H_RES, LVGL_PORT_V_RES};
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

    s_window = CreateWindow(wc.lpszClassName, "Touch Panel Simulator", WS_OVERLAPPEDWINDOW,
                            CW_USEDEFAULT, CW_USEDEFAULT, rect.right - rect.left,
                            rect.bottom - rect.top, NULL, NULL, wc.hInstance, NULL);

    if (!s_window)
    {
        ESP_LOGE(TAG, "Failed to create window");
        return false;
    }
    ShowWindow(s_window, SW_SHOW);
    UpdateWindow(s_window);
    return true;
}

bool win32_port_init(void)
{
    if (!create_window())
    {
        return false;
    }

    size_t buf_pixels = (size_t)LVGL_PORT_H_RES * 60;
    s_buf1 = (lv_color_t*)malloc(buf_pixels * sizeof(lv_color_t));
    s_buf2 = (lv_color_t*)malloc(buf_pixels * sizeof(lv_color_t));
    s_framebuffer =
        (uint32_t*)malloc((size_t)LVGL_PORT_H_RES * LVGL_PORT_V_RES * sizeof(uint32_t));

    if (!s_buf1 || !s_buf2 || !s_framebuffer)
    {
        ESP_LOGE(TAG, "Failed to allocate LVGL buffers");
        win32_port_deinit();
        return false;
    }

    memset(&s_bitmap_info, 0, sizeof(s_bitmap_info));
    s_bitmap_info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    s_bitmap_info.bmiHeader.biWidth = LVGL_PORT_H_RES;
    s_bitmap_info.bmiHeader.biHeight = -LVGL_PORT_V_RES;
    s_bitmap_info.bmiHeader.biPlanes = 1;
    s_bitmap_info.bmiHeader.biBitCount = 32;
    s_bitmap_info.bmiHeader.biCompression = BI_RGB;

    lv_disp_draw_buf_init(&s_draw_buf, s_buf1, s_buf2, buf_pixels);

    lv_disp_drv_init(&s_disp_drv);
    s_disp_drv.hor_res = LVGL_PORT_H_RES;
    s_disp_drv.ver_res = LVGL_PORT_V_RES;
    s_disp_drv.flush_cb = flush_cb;
    s_disp_drv.draw_buf = &s_draw_buf;
    lv_disp_drv_register(&s_disp_drv);

    lv_indev_drv_init(&s_pointer_drv);
    s_pointer_drv.type = LV_INDEV_TYPE_POINTER;
    s_pointer_drv.read_cb = pointer_read_cb;
    s_pointer = lv_indev_drv_register(&s_pointer_drv);
    (void)s_pointer;

    return true;
}

void win32_port_deinit(void)
{
    if (s_window)
    {
        DestroyWindow(s_window);
        s_window = NULL;
    }
    free(s_buf1);
    free(s_buf2);
    free(s_framebuffer);
    s_buf1 = NULL;
    s_buf2 = NULL;
    s_framebuffer = NULL;
}

bool win32_port_process_events(void)
{
    MSG msg;
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
    {
        if (msg.message == WM_QUIT)
        {
            return false;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return true;
}
