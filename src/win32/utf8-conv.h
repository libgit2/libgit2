#ifndef INCLUDE_git_utf8conv_h__
#define INCLUDE_git_utf8conv_h__

wchar_t* conv_utf8_to_utf16(const char* str);
char* conv_utf16_to_utf8(const wchar_t* str);

#endif
