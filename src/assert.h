#pragma once

// clang-format off
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
// clang-format on

#ifdef NDEBUG
#define DBMNGR_ACTION() printf("ohno assertion failed in %s:%d\n", __FILE__, __LINE__);
#else
#define DBMNGR_ACTION() abort()
#endif

#define DBMNGR_ASSERT(expr)                                                                        \
  do {                                                                                             \
    assert((expr));                                                                                \
    if (!(expr)) {                                                                                 \
      DBMNGR_ACTION();                                                                             \
    }                                                                                              \
  } while (0)
