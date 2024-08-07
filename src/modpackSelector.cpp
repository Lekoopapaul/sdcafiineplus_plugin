#include "modpackSelector.h"
#include "globals.h"
#include "utils/input.h"
#include "version.h"
#include <content_redirection/redirection.h>
#include <coreinit/screen.h>
#include <coreinit/thread.h>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <fs/DirList.h>
#include <malloc.h>
#include <map>
#include <memory/mappedmemory.h>
#include <string>
#include <utils/logger.h>
#include <wups/storage.h>
#include <nn/act.h>

#define TEXT_SEL(x, text1, text2) ((x) ? (text1) : (text2))

uint8_t *screenBuffer_0 = nullptr;
uint8_t *screenBuffer_1 = nullptr;

bool ScreenInit() {
    if (screenBuffer_0 != nullptr) {
        // allocated
        return true;
    }
    // Init screen and screen buffers
    OSScreenInit();
    uint32_t screen_buf0_size = OSScreenGetBufferSizeEx(SCREEN_TV);
    uint32_t screen_buf1_size = OSScreenGetBufferSizeEx(SCREEN_DRC);
    screenBuffer_0            = (uint8_t *) MEMAllocFromMappedMemoryForGX2Ex(screen_buf0_size, 0x100);
    screenBuffer_1            = (uint8_t *) MEMAllocFromMappedMemoryForGX2Ex(screen_buf1_size, 0x100);
    if (screenBuffer_0 == nullptr || screenBuffer_1 == nullptr) {
        if (screenBuffer_0) {
            MEMFreeToMappedMemory(screenBuffer_0);
            screenBuffer_0 = nullptr;
        }
        if (screenBuffer_1) {
            MEMFreeToMappedMemory(screenBuffer_1);
            screenBuffer_1 = nullptr;
        }
        DEBUG_FUNCTION_LINE_ERR("Failed to allocate screenBuffer");
        OSFatal("SDCafiine Plus plugin: Failed to allocate screenBuffer.");
        return false;
    }
    OSScreenSetBufferEx(SCREEN_TV, (void *) screenBuffer_0);
    OSScreenSetBufferEx(SCREEN_DRC, (void *) (screenBuffer_1));

    OSScreenEnableEx(SCREEN_TV, 1);
    OSScreenEnableEx(SCREEN_DRC, 1);

    // Clear screens
    OSScreenClearBufferEx(SCREEN_TV, 0);
    OSScreenClearBufferEx(SCREEN_DRC, 0);

    OSScreenFlipBuffersEx(SCREEN_TV);
    OSScreenFlipBuffersEx(SCREEN_DRC);
    return true;
}

bool ScreenDeInit() {
    OSScreenClearBufferEx(SCREEN_TV, 0);
    OSScreenClearBufferEx(SCREEN_DRC, 0);

    // Flip buffers
    OSScreenFlipBuffersEx(SCREEN_TV);
    OSScreenFlipBuffersEx(SCREEN_DRC);
    if (screenBuffer_0) {
        MEMFreeToMappedMemory(screenBuffer_0);
        screenBuffer_0 = nullptr;
    }
    if (screenBuffer_1) {
        MEMFreeToMappedMemory(screenBuffer_1);
        screenBuffer_1 = nullptr;
    }
    return true;
}

void HandleMultiModPacks(uint64_t titleID) {
    screenBuffer_0 = nullptr;
    screenBuffer_1 = nullptr;
    char TitleIDString[17];
    snprintf(TitleIDString, 17, "%016llX", titleID);

    std::map<std::string, std::string> modTitlePath;

    std::map<std::string, std::string> mounting_points;

    const std::string modTitleIDPath    = std::string("fs:/vol/external01/wiiu/sdcafiine_plus/").append(TitleIDString);
    const std::string modTitleIDPathOld = std::string("fs:/vol/external01/wiiu/sdcafiine/").append(TitleIDString);
    DirList modTitleDirList(modTitleIDPath, nullptr, DirList::Dirs);

    modTitleDirList.SortList();

    for (int index = 0; index < modTitleDirList.GetFilecount(); index++) {
        std::string curFile = modTitleDirList.GetFilename(index);

        if (curFile == "." || curFile == "..") {
            continue;
        }

        const std::string &packageName = curFile;
        modTitlePath[packageName]      = (modTitleIDPath + "/").append(curFile);
        DEBUG_FUNCTION_LINE_VERBOSE("Found %s  %s", packageName.c_str(), modTitlePath[packageName].c_str());
    }

    if (modTitlePath.empty()) {
        DIR *dir = opendir(modTitleIDPathOld.c_str());
        if (dir) {
            if (!ScreenInit()) {
                OSFatal("SDCafiine Plus plugin: Please migrate sd:/wiiu/sdcafiine to sd:/wiiu/sdcafiine_plus.");
            }
            OSScreenClearBufferEx(SCREEN_TV, 0);
            OSScreenClearBufferEx(SCREEN_DRC, 0);
            console_print_pos(-2, -1, "SDCafiine Plus plugin " VERSION_FULL);
            console_print_pos(-2, 2, "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
            console_print_pos(-2, 3, "!!!              OLD DIRECTORY STRUCTURE DETECTED.                !!!");
            console_print_pos(-2, 4, "!!! Please migrate sd:/wiiu/sdcafiine to sd:/wiiu/sdcafiine_plus. !!!");
            console_print_pos(-2, 5, "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
            console_print_pos(-2, 7, "Loading game without mods");
            closedir(dir);

            OSScreenFlipBuffersEx(SCREEN_TV);
            OSScreenFlipBuffersEx(SCREEN_DRC);

            OSSleepTicks(OSMillisecondsToTicks(10000));
            ScreenDeInit();
        }
        return;
    } else if (modTitlePath.size() == 1 && gAutoApplySingleModpack && gSkipPrepareIfSingleModpack) {
        ReplaceContent(modTitlePath.begin()->second, modTitlePath.begin()->first);
        return;
    }

    int selected   = 0;
    int initScreen = 1;
    int x_offset   = -2;

    uint32_t buttonsTriggered;

    VPADStatus vpad_data{};
    VPADReadError vpad_error;
    KPADStatus kpad_data{};
    KPADError kpad_error;

    bool displayAutoSkipOption = modTitlePath.size() == 1;

    int wantToExit = 0;
    int page       = 0;
    int per_page   = displayAutoSkipOption ? 11 : 12;
    int max_pages  = (modTitlePath.size() / per_page) + (modTitlePath.size() % per_page != 0 ? 1 : 0);

    int curState = 0;
    if (gAutoApplySingleModpack && modTitlePath.size() == 1) {
        curState = 1;
    }

    int durationInFrames = 60;
    int frameCounter     = 0;
    KPADInit();
    WPADEnableURCC(true);

    if (!ScreenInit()) {
        return;
    }

    while (true) {
        buttonsTriggered = 0;

        VPADRead(VPAD_CHAN_0, &vpad_data, 1, &vpad_error);
        if (vpad_error == VPAD_READ_SUCCESS) {
            buttonsTriggered = vpad_data.trigger;
        }

        for (int i = 0; i < 4; i++) {
            memset(&kpad_data, 0, sizeof(kpad_data));
            if (KPADReadEx((KPADChan) i, &kpad_data, 1, &kpad_error) > 0) {
                if (kpad_error == KPAD_ERROR_OK && kpad_data.extensionType != 0xFF) {
                    if (kpad_data.extensionType == WPAD_EXT_CORE || kpad_data.extensionType == WPAD_EXT_NUNCHUK) {
                        buttonsTriggered |= remapWiiMoteButtons(kpad_data.trigger);
                    } else {
                        buttonsTriggered |= remapClassicButtons(kpad_data.classic.trigger);
                    }
                }
            }
        }

        if (curState == 1) {
            if (buttonsTriggered & VPAD_BUTTON_MINUS) {
                curState = 0;
                continue;
            }

            if (initScreen) {
                OSScreenClearBufferEx(SCREEN_TV, 0);
                OSScreenClearBufferEx(SCREEN_DRC, 0);
                console_print_pos(x_offset, -1, "SDCafiine Plus plugin " VERSION_FULL);
                console_print_pos(x_offset, 1, "Preparing modpack \"%s\"...", modTitlePath.begin()->first.c_str());
                console_print_pos(x_offset, 3, "Press MINUS to open menu");
                // Flip buffers
                OSScreenFlipBuffersEx(SCREEN_TV);
                OSScreenFlipBuffersEx(SCREEN_DRC);
            }

            if (frameCounter >= durationInFrames) {
                ReplaceContent(modTitlePath.begin()->second, modTitlePath.begin()->first);
                break;
            }

            frameCounter++;
        } else {
            if (buttonsTriggered & VPAD_BUTTON_A) {
                wantToExit = 1;
                initScreen = 1;
            } else if (modTitlePath.size() == 1 && (buttonsTriggered & VPAD_BUTTON_MINUS)) {
                OSScreenClearBufferEx(SCREEN_TV, 0);
                OSScreenClearBufferEx(SCREEN_DRC, 0);

                console_print_pos(x_offset, -1, "SDCafiine Plus plugin " VERSION_FULL);
                console_print_pos(x_offset, 1, "Save settings...");

                // Flip buffers
                OSScreenFlipBuffersEx(SCREEN_TV);
                OSScreenFlipBuffersEx(SCREEN_DRC);

                gAutoApplySingleModpack = !gAutoApplySingleModpack;
                // If the value has changed, we store it in the storage.
                if (WUPSStorageAPI::Store(AUTO_APPLY_SINGLE_MODPACK_STRING, gAutoApplySingleModpack) == WUPS_STORAGE_ERROR_SUCCESS) {
                    if (WUPSStorageAPI::SaveStorage() != WUPS_STORAGE_ERROR_SUCCESS) {
                        DEBUG_FUNCTION_LINE_ERR("Failed to close storage");
                    }
                } else {
                    DEBUG_FUNCTION_LINE_WARN("Failed to save to storage");
                }

                initScreen = 1;
            } else if (buttonsTriggered & VPAD_BUTTON_B) {
                break;
            } else if (buttonsTriggered & VPAD_BUTTON_DOWN) {
                selected++;
                initScreen = 1;
            } else if (buttonsTriggered & VPAD_BUTTON_UP) {
                selected--;
                initScreen = 1;
            } else if (buttonsTriggered & VPAD_BUTTON_L) {
                selected -= per_page;
                initScreen = 1;
            } else if (buttonsTriggered & VPAD_BUTTON_R) {
                selected += per_page;
                initScreen = 1;
            }
            if (selected < 0) {
                selected = 0;
            }
            if (selected >= (int) modTitlePath.size()) {
                selected = modTitlePath.size() - 1;
            }
            page = selected / per_page;

            if (initScreen) {
                OSScreenClearBufferEx(SCREEN_TV, 0);
                OSScreenClearBufferEx(SCREEN_DRC, 0);
                console_print_pos(x_offset, -1, "SDCafiine Plus plugin " VERSION_FULL);
                console_print_pos(x_offset, 1, "Press A to launch a modpack");
                console_print_pos(x_offset, 2, "Press B to launch without a modpack");
                if (modTitlePath.size() == 1) {
                    if (gAutoApplySingleModpack) {
                        console_print_pos(x_offset, 4, "Press MINUS to disable autostart for a single modpack");
                    } else {
                        console_print_pos(x_offset, 4, "Press MINUS to enable autostart for a single modpack");
                    }
                }
                int y_offset = displayAutoSkipOption ? 6 : 4;
                int cur_     = 0;

                for (auto &it : modTitlePath) {
                    std::string key   = it.first;
                    std::string value = it.second;

                    if (wantToExit && cur_ == selected) {
                        ReplaceContent(value, key);
                        break;
                    }

                    if (cur_ >= (page * per_page) && cur_ < ((page + 1) * per_page)) {
                        console_print_pos(x_offset, y_offset++, "%s %s", TEXT_SEL((selected == cur_), "--->", "    "), key.c_str());
                    }
                    cur_++;
                }

                if (wantToExit) { //just in case.
                    break;
                }

                if (max_pages > 0) {
                    console_print_pos(x_offset, 17, "Page %02d/%02d. Press L/R to change page.", page + 1, max_pages);
                }

                // Flip buffers
                OSScreenFlipBuffersEx(SCREEN_TV);
                OSScreenFlipBuffersEx(SCREEN_DRC);

                initScreen = 0;
            }
        }
    }

    ScreenDeInit();

    KPADShutdown();
}

bool ReplaceContentInternal(const std::string &basePath, const std::string &subdir, CRLayerHandle *layerHandle,FSLayerType layerType);

bool ReplaceContent(const std::string &basePath, const std::string &modpack) {
    bool saveRes    = ReplaceContentInternal(basePath, "save", &gSaveLayerHandle,FS_LAYER_TYPE_SAVE_REPLACE);

    if(!saveRes){

        auto screenWasAllocated = screenBuffer_0 != nullptr;

        if (!ScreenInit()) {
            OSFatal("SDCafiine plugin: Failed to apply the modpack.");
        }
        uint32_t sleepTime = 3000;
        DEBUG_FUNCTION_LINE_ERR("Failed to apply the save redirection. Starting without mods.");
        OSScreenClearBufferEx(SCREEN_TV, 0);
        OSScreenClearBufferEx(SCREEN_DRC, 0);
        console_print_pos(-2, -1, "SDCafiine plugin " VERSION VERSION_EXTRA);
        console_print_pos(-2, 1, "Failed to apply the save redirection. Starting without mods...");

        OSScreenFlipBuffersEx(SCREEN_TV);
        OSScreenFlipBuffersEx(SCREEN_DRC);

        OSSleepTicks(OSMillisecondsToTicks(sleepTime));
        if (!screenWasAllocated) {
            ScreenDeInit();
        }
        return false;
    }

    bool contentRes = ReplaceContentInternal(basePath, "content", &gContentLayerHandle,FS_LAYER_TYPE_CONTENT_MERGE);
    bool aocRes     = ReplaceContentInternal(basePath, "aoc", &gAocLayerHandle,FS_LAYER_TYPE_AOC_MERGE);

    if (!contentRes && !aocRes) {
        auto screenWasAllocated = screenBuffer_0 != nullptr;

        if (!ScreenInit()) {
            OSFatal("SDCafiine Plus plugin: Failed to apply the modpack.");
        }
        uint32_t sleepTime = 3000;
        DEBUG_FUNCTION_LINE_ERR("Failed to apply the modpack. Starting without mods.");
        OSScreenClearBufferEx(SCREEN_TV, 0);
        OSScreenClearBufferEx(SCREEN_DRC, 0);
        console_print_pos(-2, -1, "SDCafiine Plus plugin " VERSION VERSION_EXTRA);
        console_print_pos(-2, 1, "Failed to apply the modpack. Starting without mods...");
        bool folderExists = false;
        struct stat st {};
        std::string contentPath = (std::string(modpack) + "/content");
        if (stat(contentPath.c_str(), &st) >= 0) {
            folderExists = true;
        } else {
            console_print_pos(-2, 3, "No /vol/content replacement found (%s)", contentPath.c_str());
        }

        std::string aocPath = (std::string(modpack) + "/aoc");
        if (stat(aocPath.c_str(), &st) >= 0) {
            folderExists = true;
        } else {
            console_print_pos(-2, 4, "No /vol/aoc replacement found (%s)", aocPath.c_str());
        }
        if (folderExists) {
            console_print_pos(-2, 6, "ContentRedirection_AddFSLayer failed");
        } else {
            sleepTime = 5000;
            console_print_pos(-2, 6, "You need at least one of the replacements!");

            DIR *dir = opendir(basePath.c_str());
            if (dir) {
                auto res = readdir(dir);
                if (res != nullptr) {
                    sleepTime = 7000;
                    console_print_pos(-2, 8, "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
                    console_print_pos(-2, 9, "!!! OLD DIRECTORY STRUCTURE DETECTED.                          !!!");
                    console_print_pos(-2, 10, "!!! Please migrate to the new directory structure.             !!!");
                    console_print_pos(-2, 11, R"(!!! Move the files into the "content" directory of the modpack !!!)");
                    console_print_pos(-2, 12, "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
                }
                closedir(dir);
            }
        }

        OSScreenFlipBuffersEx(SCREEN_TV);
        OSScreenFlipBuffersEx(SCREEN_DRC);

        OSSleepTicks(OSMillisecondsToTicks(sleepTime));
        if (!screenWasAllocated) {
            ScreenDeInit();
        }
        return false;
    }
    return true;
}

bool ReplaceContentInternal(const std::string &basePath, const std::string &subdir, CRLayerHandle *layerHandle,FSLayerType layerType) {
    std::string layerName = "SDCafiine Plus /vol/" + subdir;
    std::string fullPath  = basePath + "/" + subdir;
    if(layerType == FS_LAYER_TYPE_SAVE_REPLACE){
        nn::act::Initialize();
        nn::act::PersistentId id = nn::act::GetPersistentId();
        nn::act::Finalize();

        char user[9];
        snprintf(user, 9, "%08x", 0x80000000 | id);

        mkdir(fullPath.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
        mkdir((fullPath+"/"+"common").c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
        mkdir((fullPath+"/"+user).c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);  
    }
    struct stat st {};
    if (stat(fullPath.c_str(), &st) < 0) {
        DEBUG_FUNCTION_LINE_WARN("Skip /vol/%s to %s redirection. Dir does not exist", subdir.c_str(), fullPath.c_str());
        return false;
    }

    auto res = ContentRedirection_AddFSLayer(layerHandle,
                                             layerName.c_str(),
                                             fullPath.c_str(),
                                             layerType);
    if (res == CONTENT_REDIRECTION_RESULT_SUCCESS) {
        DEBUG_FUNCTION_LINE("Redirect /vol/%s to %s", subdir.c_str(), fullPath.c_str());
    } else {
        DEBUG_FUNCTION_LINE_ERR("Failed to redirect /vol/%s to %s", subdir.c_str(), fullPath.c_str());
        return false;
    }
    return true;
}


void console_print_pos(int x, int y, const char *format, ...) {
    char *tmp = nullptr;

    va_list va;
    va_start(va, format);
    if ((vasprintf(&tmp, format, va) >= 0) && tmp) {
        if (strlen(tmp) > 79) {
            tmp[79] = 0;
        }

        OSScreenPutFontEx(SCREEN_TV, x, y, tmp);
        OSScreenPutFontEx(SCREEN_DRC, x, y, tmp);
    }
    va_end(va);

    if (tmp) {
        free(tmp);
    }
}
