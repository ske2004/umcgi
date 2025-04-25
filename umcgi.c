#include "fcgi_stdio.h"
#include "umbox/umka/umka_api.h"

#define TRY(cond) if (cond) { print_error(umka); umkaFree(umka); return; }

const char *source = "fn fcgi_write(data: []uint8): uint\n"
                     "fn fcgi_getchar(): int\n"
                     "fn fcgi_getenv(strtype: ^void): []str\n"
                     "fn write*(data: []uint8): uint { return fcgi_write(data) }\n"
                     "fn getchar*(): int { return fcgi_getchar() }\n"
                     "fn getenv*(): []str { return fcgi_getenv(typeptr([]str)) }\n";

void print_error(void *umka)
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
    TRY(!umkaInit(umka, "cgi-bin/main.um", NULL, 2048, NULL, 0, NULL, true, true, NULL));
    TRY(!umkaAddFunc(umka, "fcgi_write", _umka_fcgi_write));
    TRY(!umkaAddFunc(umka, "fcgi_getchar", _umka_fcgi_getchar));
    TRY(!umkaAddFunc(umka, "fcgi_getenv", _umka_fcgi_getenv));
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