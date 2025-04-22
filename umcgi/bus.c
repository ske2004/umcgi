#include "../umbox/umka/umka_api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#ifdef _WIN32
#define UMCGI_EXPORT __declspec(dllexport)
#else
#define UMCGI_EXPORT
#endif

enum
{
    CODE_OK = 0,
    CODE_ERROR_LOAD_DLL = 1,
    CODE_ERROR_GET_PROC_ADDRESS_UMKAALLOC = 2,
    CODE_ERROR_GET_PROC_ADDRESS_UMKAINIT = 3,
    CODE_ERROR_ALLOC_UMKA = 4,
    CODE_ERROR_INIT_UMKA = 5,
    CODE_ERROR_COMPILE_UMKA = 6,
    CODE_ERROR_GET_FUNC = 7,
    CODE_ERROR_CALL_UMKA = 8,
};

typedef struct
{
    void *umka;
    void *umka_dll;
    UmkaAPI *umka_api;
} umka_ctx;

typedef struct
{
    char *headers;
    char *body;
} request;

typedef struct
{
    int code;
    char *headers;
    char *body;
} response;

typedef void *(*UmkaAlloc)();

typedef bool (*UmkaInit)(
    void *umka,
    const char *fileName,
    const char *sourceString,
    int stackSize,
    void *reserved,
    int argc,
    char **argv,
    bool fileSystemEnabled,
    bool implLibsEnabled,
    UmkaWarningCallback warningCallback
);

static char *_copy_str(const char *str)
{
    size_t len = strlen(str) + 1;
    char *copy = malloc(len);
    memcpy(copy, str, len);
    return copy;
}

static char *_format_error(UmkaError *error)
{
    size_t err_len = strlen(error->msg);
    size_t file_len = strlen(error->fileName);
    size_t fn_len = strlen(error->fnName);
    size_t final_len = err_len + file_len + fn_len + 1024;

    printf("fn: %s\n", error->fnName);

    char *error_msg = malloc(final_len);
    snprintf(error_msg, final_len, "%zu:%s%zu:%s%zu:%s %d %d", err_len, error->msg, file_len, error->fileName, fn_len, error->fnName, error->line, error->pos);
    return error_msg;
}

static void *_dll_open(const char *dll_name, const char *dll_name_unix)
{
#ifdef _WIN32
    HMODULE umka_dll = LoadLibrary(dll_name);
    if (!umka_dll)
    {
        return NULL;
    }
#else
    void *umka_dll = dlopen(dll_name_unix, RTLD_NOW);
    if (!umka_dll)
    {
        return NULL;
    }
#endif
    return umka_dll;
}

static void _dll_close(void *umka_dll)
{
#ifdef _WIN32
    FreeLibrary(umka_dll);
#else
    dlclose(umka_dll);
#endif
}

static void* _dll_sym(void *umka_dll, const char *sym_name)
{
#ifdef _WIN32
    return GetProcAddress(umka_dll, sym_name);
#else
    return dlsym(umka_dll, sym_name);
#endif
}

static void _umka_ctx_free(umka_ctx *ctx)
{
    if (ctx->umka)
    {
        ctx->umka_api->umkaFree(ctx->umka);
    }
    if (ctx->umka_dll)
    {
        _dll_close(ctx->umka_dll);
    }
}

static int _umka_ctx_init(umka_ctx *ctx, const char *source)
{
#define ONFAIL(condition, code) if (condition) { _umka_ctx_free(ctx); return code; }
    ctx->umka_dll = _dll_open("umbox/umka/libumka.dll", "umbox/umka/libumka.so");
    ONFAIL(!ctx->umka_dll, CODE_ERROR_LOAD_DLL);
    UmkaAlloc umkaAlloc = (UmkaAlloc)_dll_sym(ctx->umka_dll, "umkaAlloc");
    ONFAIL(!umkaAlloc, CODE_ERROR_GET_PROC_ADDRESS_UMKAALLOC);
    UmkaInit umkaInit = (UmkaInit)_dll_sym(ctx->umka_dll, "umkaInit");
    ONFAIL(!umkaInit, CODE_ERROR_GET_PROC_ADDRESS_UMKAINIT);

    ctx->umka = umkaAlloc();
    ONFAIL(!ctx->umka, CODE_ERROR_ALLOC_UMKA);
    ONFAIL(!umkaInit(ctx->umka, source, NULL, 2048, NULL, 0, NULL, 1, 1, NULL), CODE_ERROR_INIT_UMKA);

    ctx->umka_api = umkaGetAPI(ctx->umka);
    return 0;
}

static char *_umka_ctx_error(umka_ctx *ctx)
{
    UmkaError *error = ctx->umka_api->umkaGetError(ctx->umka);
    return _format_error(error);
}

static response _respond_with_code(int code)
{
    return (response){code, _copy_str(""), _copy_str("")};
}

static response _respond_with_error(UmkaError *error)
{
    return (response){CODE_ERROR_COMPILE_UMKA, _copy_str(""), _format_error(error)};
}

static response _umka_process(umka_ctx *ctx, request req)
{
    if (!ctx->umka_api->umkaCompile(ctx->umka))
    {
        UmkaError *error = ctx->umka_api->umkaGetError(ctx->umka);
        return _respond_with_error(error);
    }

    UmkaFuncContext fn;
    if (!ctx->umka_api->umkaGetFunc(ctx->umka, NULL, "process", &fn))
    {
        return _respond_with_code(CODE_ERROR_GET_FUNC);
    }
    
    request request = {
        .headers = ctx->umka_api->umkaMakeStr(ctx->umka, req.headers),
        .body = ctx->umka_api->umkaMakeStr(ctx->umka, req.body)
    };

    response res = {
        .headers = ctx->umka_api->umkaMakeStr(ctx->umka, ""),
        .body = ctx->umka_api->umkaMakeStr(ctx->umka, "")
    };

    umkaGetParam(fn.params, 0)->ptrVal = &request;
    umkaGetParam(fn.params, 1)->ptrVal = &res;

    if (ctx->umka_api->umkaCall(ctx->umka, &fn) != 0)
    {
        UmkaError *error = ctx->umka_api->umkaGetError(ctx->umka);
        return _respond_with_error(error);
    }

    res.code = CODE_OK;
    res.headers = _copy_str(res.headers);
    res.body = _copy_str(res.body);

    return res;
}

UMCGI_EXPORT
response umcgi_process(request req)
{
    umka_ctx ctx = { 0 };
    if (_umka_ctx_init(&ctx, "cgi-bin/main.um") != 0)
    {
        return _respond_with_code(CODE_ERROR_INIT_UMKA);
    }

    response res = _umka_process(&ctx, req);
    _umka_ctx_free(&ctx);
    return res;
}

UMCGI_EXPORT
void umcgi_free(response res)
{
    free(res.headers);
    free(res.body);
}

