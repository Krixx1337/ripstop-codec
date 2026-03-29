#pragma once

#include <string>

// Optional project-local config injection, similar to BurnerNet.
#ifdef RIPSTOP_USER_CONFIG_HEADER
#include RIPSTOP_USER_CONFIG_HEADER
#endif

#ifndef RIPSTOP_ERROR_XOR
#define RIPSTOP_ERROR_XOR 0u
#endif

#ifndef RIPSTOP_HARDEN_ERRORS
#define RIPSTOP_HARDEN_ERRORS 0
#endif

#ifndef RIPSTOP_HAS_CUSTOM_ERROR_CODE_ENUM
#define RIPSTOP_HAS_CUSTOM_ERROR_CODE_ENUM 0
#endif
