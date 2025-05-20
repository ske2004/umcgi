#include "fcgi_stdio.h"
#include "umbox/umka/umka_api.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define TRY(cond) if (cond) { _print_error(umka); umkaFree(umka); return; }

struct bump
{
    uint8_t *data;
    size_t len;
    size_t comm;
    size_t cap;
};

static void bump_init(struct bump *buf)
{
    buf->len = 0;
    buf->cap = 4096;
    buf->data = malloc(buf->cap);
}

static void bump_grow(struct bump *buf)
{
    buf->cap *= 2;
    buf->data = realloc(buf->data, buf->cap);
}

static uint8_t *bump_alloc(struct bump *buf, size_t len)
{
    while (buf->len + len > buf->cap)
    {
        bump_grow(buf);
    }

    uint8_t *ptr = buf->data + buf->len;
    buf->len += len;
    return ptr;
}

static void bump_commit(struct bump *buf, size_t len)
{
    buf->comm += len;
    assert(buf->comm <= buf->len);
    buf->len = buf->comm;
}

static void bump_free(struct bump *buf)
{
    free(buf->data);
}

const char *source = "fn fcgi_write(data: []uint8): uint\n"
                     "fn fcgi_getchar(): int\n"
                     "fn fcgi_getenv(strtype: ^void): []str\n"
                     "fn fcgi_getbody(u8type: ^void): []uint8\n"
                     "fn write*(data: []uint8): uint { return fcgi_write(data) }\n"
                     "fn getchar*(): int { return fcgi_getchar() }\n"
                     "fn getenv*(): []str { return fcgi_getenv(typeptr([]str)) }\n"
                     "fn getbody*(): []uint8 { return fcgi_getbody(typeptr([]uint8)) }\n";

static void _print_error(void *umka)
{
    printf("Content-type: text/plain\r\n");
    printf("\r\n");
    
    UmkaError *error = umkaGetError(umka);
    if (!error)
    {
        printf("Error: Unknown\n");
        return;
    }

    printf("Error: %s\n", error->msg);
    printf("    %s:%d:%d (%s)\n", error->fileName, error->line, error->pos, error->fnName);
}

uint8_t *_get_chunk(size_t *len)
{
    struct bump buf = { 0 };
    bump_init(&buf);
    size_t readsz = 4096;

    while (true)
    {
        uint8_t *ptr = bump_alloc(&buf, readsz);
        bump_commit(&buf, fread(ptr, 1, readsz, stdin));
        
        if (feof(stdin))
        {
            *len = buf.comm;
            return buf.data;
        }

        if (ferror(stdin))
        {
            bump_free(&buf);
            *len = 0;
            return NULL;
        }

        readsz *= 2;
    }

    assert(0 && "unreachable");
}

void _umka_fcgi_write(UmkaStackSlot *params, UmkaStackSlot *result)
{
    UmkaDynArray(uint8_t) *data = (void *)umkaGetParam(params, 0);
    size_t len = umkaGetDynArrayLen(data);
    size_t written = fwrite(data->data, 1, len, stdout);
    umkaGetResult(params, result)->uintVal = written;
}

void _umka_fcgi_getchar(UmkaStackSlot *params, UmkaStackSlot *result)
{
    int c = fgetc(stdin);
    umkaGetResult(params,result)->intVal = c;
}

void _umka_fcgi_getbody(UmkaStackSlot *params, UmkaStackSlot *result)
{
    size_t len = 0;
    uint8_t *buffer = _get_chunk(&len);
    if (!buffer)
    {
        umkaGetResult(params, result)->ptrVal = NULL;
        return;
    }

    void *umka = umkaGetInstance(result);
    void *u8arrtype = umkaGetParam(params, 0)->ptrVal;
    UmkaDynArray(uint8_t) *u8arr = umkaGetResult(params, result)->ptrVal;
    umkaMakeDynArray(umka, u8arr, u8arrtype, len);

    memcpy(u8arr->data, buffer, len);
    free(buffer);
}

void _umka_fcgi_getenv(UmkaStackSlot *params, UmkaStackSlot *result)
{
    void *umka = umkaGetInstance(result);
    extern char **environ;
    int len = 0;
    for (int i = 0; environ[i] != NULL; i++)
    {
        len++;
    }

    void *strtype = umkaGetParam(params, 0)->ptrVal;

    UmkaDynArray(char*) *envs = umkaGetResult(params, result)->ptrVal;
	umkaMakeDynArray(umka, envs, strtype, len);

    for (int i = 0; environ[i] != NULL; i++)
    {
        envs->data[i] = umkaMakeStr(umka, environ[i]);
    }
}

void _umka_run()
{
    void *umka = umkaAlloc();
    TRY(!umkaInit(umka, "cgi-bin/main.um", NULL, 1<<14, NULL, 0, NULL, true, true, NULL));
    TRY(!umkaAddFunc(umka, "fcgi_write", _umka_fcgi_write));
    TRY(!umkaAddFunc(umka, "fcgi_getchar", _umka_fcgi_getchar));
    TRY(!umkaAddFunc(umka, "fcgi_getenv", _umka_fcgi_getenv));
    TRY(!umkaAddFunc(umka, "fcgi_getbody", _umka_fcgi_getbody));
    TRY(!umkaAddModule(umka, "fcgi.um", source));
    TRY(!umkaCompile(umka));
    TRY(umkaRun(umka) != 0);
    umkaFree(umka);
}

int main(int argc, char **argv)
{
    while(FCGI_Accept() >= 0)
    {
        _umka_run();
    }

    return 0;
}