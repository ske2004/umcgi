#include "fcgi_stdio.h"
#include "umbox/umka/umka_api.h"

#define TRY(cond) if (cond) { print_error(umka); umkaFree(umka); return; }

void print_error(void *umka)
{
    printf("Content-type: text/html\r\n");
    printf("\r\n");
    
    UmkaError *error = umkaGetError(umka);
    if (!error)
    {
        printf("Error: Unknown\n");
        return;
    }

    printf("Error: %s\n", error->msg);
}

void _umka_FCGI_Write(UmkaStackSlot *params, UmkaStackSlot *result)
{
    UmkaDynArray(uint8_t) *data = (void *)umkaGetParam(params, 0);
    size_t len = umkaGetDynArrayLen(data);
    fwrite(data->data, len, 1, stdout);
}

void _umka_FCGI_GetChar(UmkaStackSlot *params, UmkaStackSlot *result)
{
    int c = fgetc(stdin);
    umkaGetResult(params,result)->intVal = c;
}

void _umka_FCGI_GetEnv(UmkaStackSlot *params, UmkaStackSlot *result)
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
    TRY(!umkaAddFunc(umka, "FCGI_Write", _umka_FCGI_Write));
    TRY(!umkaAddFunc(umka, "FCGI_GetChar", _umka_FCGI_GetChar));
    TRY(!umkaAddFunc(umka, "FCGI_GetEnv", _umka_FCGI_GetEnv));
    TRY(!umkaAddModule(umka, "fcgi.um", "fn FCGI_Write*(data: []uint8)\nfn FCGI_GetChar*(): int\n fn FCGI_GetEnv*(strtype: ^void): []str"));
    TRY(!umkaCompile(umka));
    TRY(umkaRun(umka) != 0);
    umkaFree(umka);
}

int main(void)
{
    while(FCGI_Accept() >= 0)
    {
        _umka_run();
    }

    return 0;
}