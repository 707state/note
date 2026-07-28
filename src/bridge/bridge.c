// C side of zigzag QuickJS bridge — handles QuickJS API with correct JSValue ABI.
// Calls into Zig functions (bridge_term_*, bridge_style_*) for actual TUI work.

#include "quickjs.h"
#include <string.h>

// ── Zig exports ───────────────────────────────────────────────────────
extern int bridge_term_init(void);
extern int bridge_term_deinit(void);
extern int bridge_term_size(uint16_t *cols, uint16_t *rows);
extern int bridge_term_write_at(uint16_t row, uint16_t col, const char *text);
extern int bridge_term_clear(void);
extern int bridge_term_hide_cursor(void);
extern int bridge_term_show_cursor(void);
extern int bridge_term_poll_event(char *buf, size_t buf_size, int32_t timeout_ms);
extern int bridge_style_render(const char *text, char **out_ptr, size_t *out_len);
extern int bridge_style_render_ex(
    const char *text,
    uint8_t fg_r, uint8_t fg_g, uint8_t fg_b, int has_fg,
    uint8_t bg_r, uint8_t bg_g, uint8_t bg_b, int has_bg,
    int bold, int italic, int underline, int dim,
    const char *border_name, int has_border,
    uint8_t bc_r, uint8_t bc_g, uint8_t bc_b, int has_bc,
    uint16_t padding, int has_padding,
    uint16_t width, uint16_t height, uint16_t max_w, uint16_t max_h,
    int alignment,
    char **out_ptr, size_t *out_len
);

// ── JS-callable functions ─────────────────────────────────────────────

static JSValue js_init(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)ctx; (void)this_val; (void)argc; (void)argv;
    bridge_term_init();
    return JS_UNDEFINED;
}

static JSValue js_deinit(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)ctx; (void)this_val; (void)argc; (void)argv;
    bridge_term_deinit();
    return JS_UNDEFINED;
}

static JSValue js_termSize(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)this_val; (void)argc; (void)argv;
    uint16_t cols, rows;
    bridge_term_size(&cols, &rows);
    JSValue arr = JS_NewArray(ctx);
    JS_SetPropertyUint32(ctx, arr, 0, JS_NewInt32(ctx, cols));
    JS_SetPropertyUint32(ctx, arr, 1, JS_NewInt32(ctx, rows));
    return arr;
}

static JSValue js_writeAt(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 3) return JS_ThrowTypeError(ctx, "writeAt(row, col, text)");
    int32_t row, col;
    if (JS_ToInt32(ctx, &row, argv[0]) || JS_ToInt32(ctx, &col, argv[1]))
        return JS_ThrowTypeError(ctx, "row and col must be numbers");
    const char *text = JS_ToCString(ctx, argv[2]);
    if (!text) return JS_ThrowTypeError(ctx, "text must be a string");
    bridge_term_write_at((uint16_t)row, (uint16_t)col, text);
    JS_FreeCString(ctx, text);
    return JS_UNDEFINED;
}

static JSValue js_clear(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)ctx; (void)this_val; (void)argc; (void)argv;
    bridge_term_clear();
    return JS_UNDEFINED;
}

static JSValue js_hideCursor(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)ctx; (void)this_val; (void)argc; (void)argv;
    bridge_term_hide_cursor();
    return JS_UNDEFINED;
}

static JSValue js_showCursor(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)ctx; (void)this_val; (void)argc; (void)argv;
    bridge_term_show_cursor();
    return JS_UNDEFINED;
}

static JSValue js_pollEvent(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)this_val;
    int32_t timeout_ms = -1;
    if (argc > 0) JS_ToInt32(ctx, &timeout_ms, argv[0]);

    char buf[64];
    int n = bridge_term_poll_event(buf, sizeof(buf), timeout_ms);
    if (n <= 0) return JS_NULL;

    JSValue obj = JS_NewObject(ctx);
    if (n == 1) {
        unsigned char ch = (unsigned char)buf[0];
        if (ch == 0x1b) JS_SetPropertyStr(ctx, obj, "key", JS_NewString(ctx, "escape"));
        else if (ch == 0x0d) JS_SetPropertyStr(ctx, obj, "key", JS_NewString(ctx, "enter"));
        else if (ch == 0x09) JS_SetPropertyStr(ctx, obj, "key", JS_NewString(ctx, "tab"));
        else if (ch == 0x7f) JS_SetPropertyStr(ctx, obj, "key", JS_NewString(ctx, "backspace"));
        else if (ch < 0x20) {
            JS_SetPropertyStr(ctx, obj, "key", JS_NewString(ctx, "ctrl"));
            JS_SetPropertyStr(ctx, obj, "char", JS_NewStringLen(ctx, &buf[0], 1));
            JS_SetPropertyStr(ctx, obj, "ctrl", JS_NewBool(ctx, 1));
        } else {
            JS_SetPropertyStr(ctx, obj, "char", JS_NewStringLen(ctx, &buf[0], 1));
        }
        JS_SetPropertyStr(ctx, obj, "type", JS_NewString(ctx, "key"));
    } else if (n >= 3 && buf[0] == 0x1b && buf[1] == '[') {
        JS_SetPropertyStr(ctx, obj, "type", JS_NewString(ctx, "key"));
        const char *key = "unknown";
        switch (buf[2]) {
            case 'A': key = "up"; break;
            case 'B': key = "down"; break;
            case 'C': key = "right"; break;
            case 'D': key = "left"; break;
            case 'H': key = "home"; break;
            case 'F': key = "end"; break;
        }
        JS_SetPropertyStr(ctx, obj, "key", JS_NewString(ctx, key));
    } else {
        JS_SetPropertyStr(ctx, obj, "type", JS_NewString(ctx, "unknown"));
    }
    return obj;
}

static JSValue js_render(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 1) return JS_ThrowTypeError(ctx, "render(text, opts?)");
    const char *text = JS_ToCString(ctx, argv[0]);
    if (!text) return JS_ThrowTypeError(ctx, "text must be a string");

    char *out_ptr = NULL;
    size_t out_len = 0;

    if (argc > 1 && JS_IsObject(argv[1])) {
        JSValue opts = argv[1];
        JSValue fg = JS_GetPropertyStr(ctx, opts, "fg");
        JSValue bg = JS_GetPropertyStr(ctx, opts, "bg");
        JSValue borderColor = JS_GetPropertyStr(ctx, opts, "borderColor");

        uint8_t fg_r = 255, fg_g = 255, fg_b = 255, bg_r = 0, bg_g = 0, bg_b = 0, bc_r = 0, bc_g = 0, bc_b = 0;
        int has_fg = 0, has_bg = 0, has_bc = 0;

        if (JS_IsArray(fg)) {
            JSValue r = JS_GetPropertyUint32(ctx, fg, 0);
            JSValue g = JS_GetPropertyUint32(ctx, fg, 1);
            JSValue b = JS_GetPropertyUint32(ctx, fg, 2);
            int32_t v;
            if (!JS_ToInt32(ctx, &v, r)) fg_r = (uint8_t)v;
            if (!JS_ToInt32(ctx, &v, g)) fg_g = (uint8_t)v;
            if (!JS_ToInt32(ctx, &v, b)) fg_b = (uint8_t)v;
            has_fg = 1;
            JS_FreeValue(ctx, r); JS_FreeValue(ctx, g); JS_FreeValue(ctx, b);
        }
        if (JS_IsArray(bg)) {
            JSValue r = JS_GetPropertyUint32(ctx, bg, 0);
            JSValue g = JS_GetPropertyUint32(ctx, bg, 1);
            JSValue b = JS_GetPropertyUint32(ctx, bg, 2);
            int32_t v;
            if (!JS_ToInt32(ctx, &v, r)) bg_r = (uint8_t)v;
            if (!JS_ToInt32(ctx, &v, g)) bg_g = (uint8_t)v;
            if (!JS_ToInt32(ctx, &v, b)) bg_b = (uint8_t)v;
            has_bg = 1;
            JS_FreeValue(ctx, r); JS_FreeValue(ctx, g); JS_FreeValue(ctx, b);
        }
        if (JS_IsArray(borderColor)) {
            JSValue r = JS_GetPropertyUint32(ctx, borderColor, 0);
            JSValue g = JS_GetPropertyUint32(ctx, borderColor, 1);
            JSValue b = JS_GetPropertyUint32(ctx, borderColor, 2);
            int32_t v;
            if (!JS_ToInt32(ctx, &v, r)) bc_r = (uint8_t)v;
            if (!JS_ToInt32(ctx, &v, g)) bc_g = (uint8_t)v;
            if (!JS_ToInt32(ctx, &v, b)) bc_b = (uint8_t)v;
            has_bc = 1;
            JS_FreeValue(ctx, r); JS_FreeValue(ctx, g); JS_FreeValue(ctx, b);
        }
        JS_FreeValue(ctx, fg); JS_FreeValue(ctx, bg); JS_FreeValue(ctx, borderColor);

        JSValue v_bold = JS_GetPropertyStr(ctx, opts, "bold");
        JSValue v_italic = JS_GetPropertyStr(ctx, opts, "italic");
        JSValue v_underline = JS_GetPropertyStr(ctx, opts, "underline");
        JSValue v_dim = JS_GetPropertyStr(ctx, opts, "dim");
        int bold = JS_ToBool(ctx, v_bold);
        int italic = JS_ToBool(ctx, v_italic);
        int underline = JS_ToBool(ctx, v_underline);
        int dim = JS_ToBool(ctx, v_dim);
        JS_FreeValue(ctx, v_bold); JS_FreeValue(ctx, v_italic);
        JS_FreeValue(ctx, v_underline); JS_FreeValue(ctx, v_dim);

        const char *border_name = "";
        int has_border = 0;
        JSValue v_border = JS_GetPropertyStr(ctx, opts, "border");
        if (!JS_IsUndefined(v_border)) {
            border_name = JS_ToCString(ctx, v_border);
            has_border = 1;
        }

        JSValue v_pad = JS_GetPropertyStr(ctx, opts, "padding");
        uint16_t padding = 0;
        int has_padding = 0;
        if (JS_IsArray(v_pad)) {
            JSValue v0 = JS_GetPropertyUint32(ctx, v_pad, 0);
            int32_t p;
            if (!JS_ToInt32(ctx, &p, v0)) { padding = (uint16_t)p; has_padding = 1; }
            JS_FreeValue(ctx, v0);
        } else if (!JS_IsUndefined(v_pad)) {
            int32_t p;
            if (!JS_ToInt32(ctx, &p, v_pad)) { padding = (uint16_t)p; has_padding = 1; }
        }
        JS_FreeValue(ctx, v_pad);

        JSValue v_w = JS_GetPropertyStr(ctx, opts, "width");
        JSValue v_h = JS_GetPropertyStr(ctx, opts, "height");
        JSValue v_mw = JS_GetPropertyStr(ctx, opts, "maxWidth");
        JSValue v_mh = JS_GetPropertyStr(ctx, opts, "maxHeight");
        int32_t w = 0, h = 0, mw = 0, mh = 0;
        JS_ToInt32(ctx, &w, v_w); JS_ToInt32(ctx, &h, v_h);
        JS_ToInt32(ctx, &mw, v_mw); JS_ToInt32(ctx, &mh, v_mh);
        JS_FreeValue(ctx, v_w); JS_FreeValue(ctx, v_h);
        JS_FreeValue(ctx, v_mw); JS_FreeValue(ctx, v_mh);

        JSValue v_align = JS_GetPropertyStr(ctx, opts, "align");
        int alignment = 0;
        if (!JS_IsUndefined(v_align)) {
            const char *al = JS_ToCString(ctx, v_align);
            if (al) {
                if (!strcmp(al, "center")) alignment = 1;
                else if (!strcmp(al, "right")) alignment = 2;
                JS_FreeCString(ctx, al);
            }
        }
        JS_FreeValue(ctx, v_align);

        bridge_style_render_ex(text, fg_r, fg_g, fg_b, has_fg,
            bg_r, bg_g, bg_b, has_bg, bold, italic, underline, dim,
            border_name, has_border, bc_r, bc_g, bc_b, has_bc,
            padding, has_padding,
            (uint16_t)w, (uint16_t)h, (uint16_t)mw, (uint16_t)mh,
            alignment, &out_ptr, &out_len);

        if (has_border && border_name) JS_FreeCString(ctx, border_name);
    } else {
        bridge_style_render(text, &out_ptr, &out_len);
    }

    JS_FreeCString(ctx, text);
    if (!out_ptr) return JS_ThrowPlainError(ctx, "render failed");
    JSValue result = JS_NewStringLen(ctx, out_ptr, out_len);
    return result;
}

// ── Module registration ───────────────────────────────────────────────

static const JSCFunctionListEntry zigzag_funcs[] = {
    JS_CFUNC_DEF("init", 0, js_init),
    JS_CFUNC_DEF("deinit", 0, js_deinit),
    JS_CFUNC_DEF("termSize", 0, js_termSize),
    JS_CFUNC_DEF("writeAt", 3, js_writeAt),
    JS_CFUNC_DEF("clear", 0, js_clear),
    JS_CFUNC_DEF("hideCursor", 0, js_hideCursor),
    JS_CFUNC_DEF("showCursor", 0, js_showCursor),
    JS_CFUNC_DEF("pollEvent", 1, js_pollEvent),
    JS_CFUNC_DEF("render", 2, js_render),
};

static int zigzag_module_init(JSContext *ctx, JSModuleDef *m) {
    return JS_SetModuleExportList(ctx, m, zigzag_funcs, sizeof(zigzag_funcs)/sizeof(zigzag_funcs[0]));
}

JSModuleDef *js_init_module_zigzag(JSContext *ctx, const char *module_name) {
    JSModuleDef *m = JS_NewCModule(ctx, module_name, zigzag_module_init);
    if (!m) return NULL;
    JS_AddModuleExportList(ctx, m, zigzag_funcs, sizeof(zigzag_funcs)/sizeof(zigzag_funcs[0]));
    return m;
}
