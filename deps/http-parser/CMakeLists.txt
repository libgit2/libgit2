add_library(http-parser OBJECT http_parser.c http_parser.h)
target_enable_warning(http-parser implicit-fallthrough=1)
