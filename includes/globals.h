#pragma once
#include "version.h"
#include <content_redirection/redirection.h>

#define VERSION                                "v0.1.0"
#define VERSION_FULL                           VERSION VERSION_EXTRA

#define AUTO_APPLY_SINGLE_MODPACK_STRING       "autoApplySingleModpack"
#define SKIP_PREPARE_FOR_SINGLE_MODPACK_STRING "skipPrepareForSingleModpack"
#define SDCAFIINE_ENABLED_STRING               "sdCafiinePlusEnabled"

#define DEFAULT_AUTO_APPLY_SINGLE_MODPACK      false
#define DEFAULT_SKIP_PREPARE_IF_SINGLE_MODPACK false
#define DEFAULT_SDCAFIINE_PLUS_ENABLED         true

extern bool gAutoApplySingleModpack;
extern bool gSkipPrepareIfSingleModpack;
extern bool gSDCafiinePlusEnabled;

extern CRLayerHandle gAocLayerHandle;
extern CRLayerHandle gContentLayerHandle;
extern CRLayerHandle gSaveLayerHandle;
