#ifndef __debug_header__
#define __debug_header__

#include <stdio.h> // fprintf()

#define TRUE  1
#define FALSE 0

#ifdef DEBUG
        #define _DEBUG 1
    #else
        #define _DEBUG 0
#endif

#ifdef ENABLE_LOG
    #define _LOG 1
        #else
    #define _LOG 0
#endif

#define DEBUG_PRINT(fmt, ...) \
        do { if (_DEBUG) fprintf(stderr, "%s:%d:%s(): " fmt, __FILE__, \
                __LINE__, __func__, ##__VA_ARGS__); } while (0)

#define LOG(fmt, ...) \
        do { if(_LOG) fprintf(stdout, "%s:%d:%s(): " fmt, __FILE__, \
                __LINE__, __func__, ##__VA_ARGS__); } while (0)

#endif
