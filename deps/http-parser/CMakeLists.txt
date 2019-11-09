ADD_LIBRARY(HttpParser STATIC http_parser.c http_parser.h)
TARGET_INCLUDE_DIRECTORIES(HttpParser PUBLIC .)
ENABLE_WARNINGS(implicit-fallthrough=1)
