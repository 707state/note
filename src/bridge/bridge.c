// QuickJS module boundary for the Zig-owned runtime.
//
// This file intentionally contains no terminal or event-loop code. Zig owns the
// runtime lifecycle, Terminal, input parsing, render scheduling, and all native
// resources. JavaScript can only register a declarative application contract.

#include "quickjs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static JSValue js_define_app(JSContext *ctx, JSValueConst this_val, int argc,
                             JSValueConst *argv) {
    (void)this_val;
    if (argc != 1 || !JS_IsObject(argv[0]))
        return JS_ThrowTypeError(ctx, "defineApp(app) expects an object");

    JSValue view = JS_GetPropertyStr(ctx, argv[0], "view");
    const int valid = JS_IsFunction(ctx, view);
    JS_FreeValue(ctx, view);
    if (!valid)
        return JS_ThrowTypeError(ctx, "app.view(context) must be a function");

    JSValue global = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, global, "__zigzag_app", JS_DupValue(ctx, argv[0]));
    JS_SetPropertyStr(ctx, global, "__zigzag_should_quit", JS_NewBool(ctx, 0));
    JS_FreeValue(ctx, global);
    return JS_UNDEFINED;
}

static JSValue js_quit(JSContext *ctx, JSValueConst this_val, int argc,
                       JSValueConst *argv) {
    (void)this_val; (void)argc; (void)argv;
    JSValue global = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, global, "__zigzag_should_quit", JS_NewBool(ctx, 1));
    JS_FreeValue(ctx, global);
    return JS_UNDEFINED;
}

static const JSCFunctionListEntry zigzag_funcs[] = {
    JS_CFUNC_DEF("defineApp", 1, js_define_app),
    JS_CFUNC_DEF("quit", 0, js_quit),
};

static int zigzag_module_init(JSContext *ctx, JSModuleDef *m) {
    return JS_SetModuleExportList(ctx, m, zigzag_funcs,
                                  sizeof(zigzag_funcs) / sizeof(zigzag_funcs[0]));
}

JSModuleDef *js_init_module_zigzag(JSContext *ctx, const char *module_name) {
    JSModuleDef *m = JS_NewCModule(ctx, module_name, zigzag_module_init);
    if (!m) return NULL;
    JS_AddModuleExportList(ctx, m, zigzag_funcs,
                           sizeof(zigzag_funcs) / sizeof(zigzag_funcs[0]));
    return m;
}

static JSValue get_app(JSContext *ctx) {
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue app = JS_GetPropertyStr(ctx, global, "__zigzag_app");
    JS_FreeValue(ctx, global);
    return app;
}

static int call_optional(JSContext *ctx, const char *name, JSValueConst arg) {
    JSValue app = get_app(ctx);
    if (!JS_IsObject(app)) {
        JS_FreeValue(ctx, app);
        return -1;
    }
    JSValue handler = JS_GetPropertyStr(ctx, app, name);
    if (JS_IsUndefined(handler)) {
        JS_FreeValue(ctx, handler);
        JS_FreeValue(ctx, app);
        return 0;
    }
    if (!JS_IsFunction(ctx, handler)) {
        JS_FreeValue(ctx, handler);
        JS_FreeValue(ctx, app);
        return -1;
    }
    JSValue result = JS_Call(ctx, handler, app, 1, &arg);
    const int failed = JS_IsException(result);
    JS_FreeValue(ctx, result);
    JS_FreeValue(ctx, handler);
    JS_FreeValue(ctx, app);
    return failed ? -1 : 0;
}

static JSValue make_context(JSContext *ctx, int width, int height) {
    JSValue value = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, value, "width", JS_NewInt32(ctx, width));
    JS_SetPropertyStr(ctx, value, "height", JS_NewInt32(ctx, height));
    return value;
}

int bridge_app_init(JSContext *ctx, int width, int height) {
    JSValue value = make_context(ctx, width, height);
    const int result = call_optional(ctx, "init", value);
    JS_FreeValue(ctx, value);
    return result;
}

int bridge_app_tick(JSContext *ctx, int width, int height) {
    JSValue value = make_context(ctx, width, height);
    const int result = call_optional(ctx, "onTick", value);
    JS_FreeValue(ctx, value);
    return result;
}

int bridge_app_resize(JSContext *ctx, int width, int height) {
    JSValue value = make_context(ctx, width, height);
    const int result = call_optional(ctx, "onResize", value);
    JS_FreeValue(ctx, value);
    return result;
}

int bridge_app_key(JSContext *ctx, const char *key, int codepoint,
                   int shift, int alt, int ctrl) {
    JSValue event = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, event, "key", JS_NewString(ctx, key));
    if (codepoint > 0) {
        char utf8[5] = {0};
        int len = 0;
        if (codepoint <= 0x7f) utf8[len++] = (char)codepoint;
        else if (codepoint <= 0x7ff) {
            utf8[len++] = (char)(0xc0 | (codepoint >> 6));
            utf8[len++] = (char)(0x80 | (codepoint & 0x3f));
        } else if (codepoint <= 0xffff) {
            utf8[len++] = (char)(0xe0 | (codepoint >> 12));
            utf8[len++] = (char)(0x80 | ((codepoint >> 6) & 0x3f));
            utf8[len++] = (char)(0x80 | (codepoint & 0x3f));
        } else {
            utf8[len++] = (char)(0xf0 | (codepoint >> 18));
            utf8[len++] = (char)(0x80 | ((codepoint >> 12) & 0x3f));
            utf8[len++] = (char)(0x80 | ((codepoint >> 6) & 0x3f));
            utf8[len++] = (char)(0x80 | (codepoint & 0x3f));
        }
        JS_SetPropertyStr(ctx, event, "text", JS_NewStringLen(ctx, utf8, len));
    }
    JS_SetPropertyStr(ctx, event, "shift", JS_NewBool(ctx, shift));
    JS_SetPropertyStr(ctx, event, "alt", JS_NewBool(ctx, alt));
    JS_SetPropertyStr(ctx, event, "ctrl", JS_NewBool(ctx, ctrl));
    const int result = call_optional(ctx, "onKey", event);
    JS_FreeValue(ctx, event);
    return result;
}

int bridge_app_view(JSContext *ctx, int width, int height,
                    const char **out, size_t *out_len) {
    JSValue app = get_app(ctx);
    if (!JS_IsObject(app)) {
        JS_FreeValue(ctx, app);
        return -1;
    }
    JSValue view = JS_GetPropertyStr(ctx, app, "view");
    JSValue render_context = make_context(ctx, width, height);
    JSValue result = JS_Call(ctx, view, app, 1, &render_context);
    JS_FreeValue(ctx, render_context);
    JS_FreeValue(ctx, view);
    JS_FreeValue(ctx, app);
    if (JS_IsException(result)) return -1;

    const char *text = JS_ToCStringLen(ctx, out_len, result);
    JS_FreeValue(ctx, result);
    if (!text) return -1;
    *out = text;
    return 0;
}

void bridge_free_view(JSContext *ctx, const char *view) {
    JS_FreeCString(ctx, view);
}

int bridge_app_should_quit(JSContext *ctx) {
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue value = JS_GetPropertyStr(ctx, global, "__zigzag_should_quit");
    const int should_quit = JS_ToBool(ctx, value);
    JS_FreeValue(ctx, value);
    JS_FreeValue(ctx, global);
    return should_quit;
}

void bridge_dump_exception(JSContext *ctx) {
    JSValue exception = JS_GetException(ctx);
    const char *message = JS_ToCString(ctx, exception);
    if (message) {
        fprintf(stderr, "QuickJS error: %s\n", message);
        JS_FreeCString(ctx, message);
    }
    JS_FreeValue(ctx, exception);
}

// The opaque host keeps QuickJS ABI-only details in C while Zig owns when the
// runtime is created, evaluated, ticked, and destroyed.
typedef struct {
    JSRuntime *runtime;
    JSContext *context;
} BridgeHost;

BridgeHost *bridge_host_create(void) {
    BridgeHost *host = calloc(1, sizeof(*host));
    if (!host) return NULL;
    host->runtime = JS_NewRuntime();
    if (!host->runtime) goto fail;
    host->context = JS_NewContext(host->runtime);
    if (!host->context) goto fail;
    if (!js_init_module_zigzag(host->context, "zigzag")) goto fail;
    return host;
fail:
    if (host->context) JS_FreeContext(host->context);
    if (host->runtime) JS_FreeRuntime(host->runtime);
    free(host);
    return NULL;
}

void bridge_host_destroy(BridgeHost *host) {
    if (!host) return;
    JS_FreeContext(host->context);
    JS_FreeRuntime(host->runtime);
    free(host);
}

static int bridge_host_drain_jobs(BridgeHost *host) {
    while (JS_IsJobPending(host->runtime)) {
        JSContext *job_context = NULL;
        if (JS_ExecutePendingJob(host->runtime, &job_context) < 0) {
            bridge_dump_exception(job_context ? job_context : host->context);
            return -1;
        }
    }
    return 0;
}

int bridge_host_eval_module(BridgeHost *host, const char *source, size_t source_len,
                            const char *filename) {
    JSValue result = JS_Eval(host->context, source, source_len, filename, JS_EVAL_TYPE_MODULE);
    if (JS_IsException(result)) {
        JS_FreeValue(host->context, result);
        bridge_dump_exception(host->context);
        return -1;
    }
    JS_FreeValue(host->context, result);
    return bridge_host_drain_jobs(host);
}

// qjsc's C frontend is deliberately not involved. These are direct QuickJS
// runtime APIs used by Zig's --compile command to produce executable bytecode.
int bridge_host_compile_module(BridgeHost *host, const char *source, size_t source_len,
                               const char *filename, unsigned char **out, size_t *out_len) {
    JSValue compiled = JS_Eval(host->context, source, source_len, filename,
                               JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
    if (JS_IsException(compiled)) {
        JS_FreeValue(host->context, compiled);
        bridge_dump_exception(host->context);
        return -1;
    }
    *out = JS_WriteObject(host->context, out_len, compiled,
                          JS_WRITE_OBJ_BYTECODE | JS_WRITE_OBJ_STRIP_SOURCE | JS_WRITE_OBJ_STRIP_DEBUG);
    JS_FreeValue(host->context, compiled);
    if (!*out) {
        bridge_dump_exception(host->context);
        return -1;
    }
    return 0;
}

void bridge_host_free_bytecode(BridgeHost *host, unsigned char *bytecode) {
    js_free(host->context, bytecode);
}

int bridge_host_eval_bytecode(BridgeHost *host, const unsigned char *bytecode, size_t bytecode_len) {
    JSValue compiled = JS_ReadObject(host->context, bytecode, bytecode_len, JS_READ_OBJ_BYTECODE);
    if (JS_IsException(compiled)) {
        JS_FreeValue(host->context, compiled);
        bridge_dump_exception(host->context);
        return -1;
    }
    if (JS_ResolveModule(host->context, compiled) < 0) {
        JS_FreeValue(host->context, compiled);
        bridge_dump_exception(host->context);
        return -1;
    }
    JSValue result = JS_EvalFunction(host->context, compiled);
    if (JS_IsException(result)) {
        JS_FreeValue(host->context, result);
        bridge_dump_exception(host->context);
        return -1;
    }
    JS_FreeValue(host->context, result);
    return bridge_host_drain_jobs(host);
}

int bridge_host_app_init(BridgeHost *host, int width, int height) {
    return bridge_app_init(host->context, width, height);
}
int bridge_host_app_tick(BridgeHost *host, int width, int height) {
    const int status = bridge_app_tick(host->context, width, height);
    if (status != 0) return status;
    return bridge_host_drain_jobs(host);
}
int bridge_host_app_resize(BridgeHost *host, int width, int height) {
    return bridge_app_resize(host->context, width, height);
}
int bridge_host_app_key(BridgeHost *host, const char *key, int codepoint,
                        int shift, int alt, int ctrl) {
    return bridge_app_key(host->context, key, codepoint, shift, alt, ctrl);
}
int bridge_host_app_view(BridgeHost *host, int width, int height,
                         const char **out, size_t *out_len) {
    return bridge_app_view(host->context, width, height, out, out_len);
}
void bridge_host_free_view(BridgeHost *host, const char *view) {
    bridge_free_view(host->context, view);
}
int bridge_host_should_quit(BridgeHost *host) {
    return bridge_app_should_quit(host->context);
}
void bridge_host_dump_exception(BridgeHost *host) {
    bridge_dump_exception(host->context);
}

