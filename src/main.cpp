#include "globals.h"
#include "modpackSelector.h"
#include "utils/config.h"
#include "utils/logger.h"
#include <content_redirection/redirection.h>
#include <coreinit/title.h>
#include <wups.h>
#include <wups/config/WUPSConfigItemBoolean.h>

WUPS_PLUGIN_NAME("SDCafiine Plus");
WUPS_PLUGIN_DESCRIPTION("SDCafiine with Save Redirection");
WUPS_PLUGIN_VERSION(VERSION_FULL);
WUPS_PLUGIN_AUTHOR("Maschell & Lekoopapaul");
WUPS_PLUGIN_LICENSE("GPL");

WUPS_USE_WUT_DEVOPTAB();
WUPS_USE_STORAGE("sdcafiine_plus"); // Unique id for the storage api

INITIALIZE_PLUGIN() {
    ContentRedirectionStatus error;
    if ((error = ContentRedirection_InitLibrary()) != CONTENT_REDIRECTION_RESULT_SUCCESS) {
        DEBUG_FUNCTION_LINE_ERR("Failed to init ContentRedirection. Error %s %d", ContentRedirection_GetStatusStr(error), error);
        OSFatal("Failed to init ContentRedirection.");
    }

    InitStorageAndConfig();

    gContentLayerHandle = 0;
    gAocLayerHandle     = 0;
    gSaveLayerHandle    = 0;
}

/* Entry point */
ON_APPLICATION_START() {
    initLogging();
    if (gSDCafiinePlusEnabled) {
        HandleMultiModPacks(OSGetTitleID());
    } else {
        DEBUG_FUNCTION_LINE("SDCafiine Plus is disabled");
    }
}

ON_APPLICATION_ENDS() {
    if (gContentLayerHandle != 0) {
        ContentRedirection_RemoveFSLayer(gContentLayerHandle);
        gContentLayerHandle = 0;
    }
    if (gAocLayerHandle != 0) {
        ContentRedirection_RemoveFSLayer(gAocLayerHandle);
        gAocLayerHandle = 0;
    }
    if (gSaveLayerHandle != 0) {
        ContentRedirection_RemoveFSLayer(gSaveLayerHandle);
        gSaveLayerHandle = 0;
    }
    deinitLogging();
}