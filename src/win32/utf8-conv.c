#include "common.h"
#include "utf8-conv.h"

wchar_t* conv_utf8_to_utf16(const char* str)
{
        wchar_t* ret;
        int cb;

        if (!str) {
                return NULL;
        }

        cb = strlen(str) * sizeof(wchar_t);
        if (cb == 0) {
                ret = (wchar_t*)git__malloc(sizeof(wchar_t));
                ret[0] = 0;
                return ret;
        }

        /* Add space for null terminator */
        cb += sizeof(wchar_t);

        ret = (wchar_t*)git__malloc(cb);

        if (MultiByteToWideChar(CP_UTF8, 0, str, -1, ret, cb) == 0) {
                free(ret);
                ret = NULL;
        }

        return ret;
}

char* conv_utf16_to_utf8(const wchar_t* str)
{
        char* ret;
        int cb;

        if (!str) {
                return NULL;
        }

        cb = wcslen(str) * sizeof(char);
        if (cb == 0) {
                ret = (char*)git__malloc(sizeof(char));
                ret[0] = 0;
                return ret;
        }

        /* Add space for null terminator */
        cb += sizeof(char);

        ret = (char*)git__malloc(cb);

        if (WideCharToMultiByte(CP_UTF8, 0, str, -1, ret, cb, NULL, NULL) == 0) {
                free(ret);
                ret = NULL;
        }

        return ret;
}
