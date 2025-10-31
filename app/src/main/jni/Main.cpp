#include <list>
#include <utility>
#include <vector>
#include <cstring>
#include <pthread.h>
#include <thread>
#include <cstring>
#include <string>
#include <jni.h>
#include <unistd.h>
#include <fstream>
#include <iostream>
#include <dlfcn.h>
#include "Includes/Logger.h"
#include "Includes/obfuscate.h"
#include "Includes/Utils.hpp"
#include "Menu/Menu.hpp"
#include "Menu/Jni.hpp"
#include "Includes/Macros.h"
#include <string>

// Dobby is a very powerful hook framework that including hook, stub, patch, and symbol resolve.
// It can completely replace And64InlineHook and KittyMemory, so they are deprecated.
#include "dobby.h" // https://github.com/jmpews/Dobby

bool noDeath;
int scoreMul = 1, coinsMul = 1;

struct MemPatches {
    // let's assume we have patches for these functions for whatever game
    // boolean get_canShoot() function
    MemoryPatch merge;
    MemoryPatch free;
    MemoryPatch spent;
    // etc...
} gPatches;

// Do not change or translate the first text unless you know what you are doing
// Assigning feature numbers is optional. Without it, it will automatically count for you, starting from 0
// Assigned feature numbers can be like any numbers 1,3,200,10... instead in order 0,1,2,3,4,5...
// ButtonLink, Category, RichTextView and RichWebView is not counted. They can't have feature number assigned
// Toggle, ButtonOnOff and Checkbox can be switched on by default, if you add True_. Example: CheckBox_True_The Check Box
// To learn HTML, go to this page: https://www.w3schools.com/

jobjectArray GetFeatureList(JNIEnv *env, jobject context) {
    jobjectArray ret;

    const char *features[] = {
//            OBFUSCATE("Category_Examples"), //Not counted
            OBFUSCATE("Toggle_Merge"), // CanBeMerged
            OBFUSCATE("Toggle_Free"), // IsNowFreeByTime
            OBFUSCATE("Toggle_Spent"), // TrySpent TryEventSpent
    };

    int Total_Feature = (sizeof features / sizeof features[0]);
    ret = (jobjectArray)
            env->NewObjectArray(Total_Feature, env->FindClass(OBFUSCATE("java/lang/String")),
                                env->NewStringUTF(""));

    for (int i = 0; i < Total_Feature; i++)
        env->SetObjectArrayElement(ret, i, env->NewStringUTF(features[i]));

    return (ret);
}

bool btnPressed = false;
bool spent = false;

void Changes(JNIEnv *env, jclass clazz, jobject obj, jint featNum, jstring featName, jint value, jlong Lvalue, jboolean boolean, jstring text) {

    switch (featNum) {
        case 0:
        {
            if (boolean)
                gPatches.merge.Modify();
            else
                gPatches.merge.Restore();
            break;
        }
        case 1:
        {
            if (boolean)
                gPatches.free.Modify();
            else
                gPatches.free.Restore();
            break;
        }
        case 2:
        {
            spent = boolean;
            break;
        }
    }
}

void (*old_Spent)(void *instance, int type, int value, void *com);

void Spent(void *instance, int type, int value, void *com) {
    if (instance != nullptr) {
        if (spent) {
            value = -value;
        }
    }
    return old_Spent(instance, type, value, com);
}
void (*old_EventSpent)(void *instance, int type, int value, char* eventType);

void EventSpent(void *instance, int type, int value, char* eventType) {
    if (instance != nullptr) {
        if (spent) {
            value = -value;
        }
    }
    return old_EventSpent(instance, type, value, eventType);
}

//CharacterPlayer
void (*StartInvcibility)(void *instance, float duration);

void (*old_Update)(void *instance);

void Update(void *instance) {
    if (instance != nullptr) {
        if (btnPressed) {
            StartInvcibility(instance, 30);
            btnPressed = false;
        }
    }
    return old_Update(instance);
}

// This pattern of orig_xxx and hook_xxx can be completely replaced by macro `install_hook_name` from dobby.h.
// You can modify it if you want.
void (*old_AddScore)(void *instance, int score);
void AddScore(void *instance, int score) {
    return old_AddScore(instance, score * scoreMul);
}

void (*old_AddCoins)(void *instance, int count);
void AddCoins(void *instance, int count) {
    return old_AddCoins(instance, count * coinsMul);
}

//Target lib here
#define targetLibName OBFUSCATE("libil2cpp.so")

ElfScanner g_il2cppELF;

// we will run our hacks in a new thread so our while loop doesn't block process main thread
void hack_thread() {
    LOGI(OBFUSCATE("pthread created"));

    // This loop should be always enabled in unity game
    // because libil2cpp.so is not loaded into memory immediately.
    while (!isLibraryLoaded(targetLibName)) {
        sleep(1); // Wait for target lib be loaded.
    }

    // ElfScanner::createWithPath can actually be replaced by xdl_open() and xdl_info(),
    // but that's from https://github.com/hexhacking/xDL.
    // You can compile it if you want.
    do {
        sleep(1);
        // getElfBaseMap can also find lib base even if it was loaded from zipped base.apk
        g_il2cppELF = ElfScanner::createWithPath(targetLibName);
    } while (!g_il2cppELF.isValid());

    LOGI(OBFUSCATE("%s has been loaded"), (const char *) targetLibName);

    // In Android Studio, to switch between arm64-v8a and armeabi-v7a syntax highlighting,
    // You can modify the "Active ABI" in "Build Variants" to switch to another architecture for parsing.
#if defined(__aarch64__)
    uintptr_t il2cppBase = g_il2cppELF.base();

    //Il2Cpp: Use RVA offset
//    StartInvcibility = (void (*)(void *, float)) getAbsoluteAddress(targetLibName, str2Offset(
//            OBFUSCATE("0x107A3BC")));
//
//    HOOK(targetLibName, str2Offset(OBFUSCATE("0x107A2E0")), AddScore, old_AddScore);
//    HOOK(targetLibName, str2Offset(OBFUSCATE("0x107A2FC")), AddCoins, old_AddCoins);
    HOOK(targetLibName, str2Offset(OBFUSCATE("0x243E6AC")), Spent, old_Spent);
    HOOK(targetLibName, str2Offset(OBFUSCATE("0x243EA48")), EventSpent, old_EventSpent);

    // This function can completely replace MemoryPatch::createWithHex:
    // int DobbyCodePatch(void *address, uint8_t *buffer, uint32_t buffer_size); (from dobby.h)
    // And it is more powerful and intuitive.
    gPatches.merge = MemoryPatch::createWithHex(il2cppBase + str2Offset(OBFUSCATE("0x25AF414")), "20 00 80 D2 C0 03 5F D6");
    gPatches.free = MemoryPatch::createWithHex(il2cppBase + str2Offset(OBFUSCATE("0x277C9F0")), "20 00 80 D2 C0 03 5F D6");

    //HOOK(targetLibName, str2Offset(OBFUSCATE("0x1079728")), Kill, old_Kill);

    //PATCH(targetLibName, str2Offset("0x10709AC"), "E05F40B2 C0035FD6");
    //HOOK(OBFUSCATE("libFileB.so"), str2Offset(OBFUSCATE("0x123456")), FunctionExample, old_FunctionExample);
    //HOOK("libFileB.so", 4646464, FunctionExample, old_FunctionExample);
    //HOOK_NO_ORIG("libFileC.so", str2Offset("0x123456"), FunctionExample);
    //HOOKSYM("libFileB.so", "__SymbolNameExample", FunctionExample, old_FunctionExample);
    //HOOKSYM_NO_ORIG("libFileB.so", "__SymbolNameExample", FunctionExample);

#elif defined(__arm__)
    //Put your code here if you want the code to be compiled for armv7 only
#endif

    LOGI(OBFUSCATE("Done"));
}

// Functions with `__attribute__((constructor))` are executed immediately when System.loadLibrary("lib_name") is called.
// If there are multiple such functions at the same time, `constructor(priority)` (the priority is an integer)
// will determine the execution priority, otherwise the execution order is undefined behavior.
__attribute__((constructor))
void lib_main() {
    // Create a new thread so it does not block the main thread, means the game would not freeze
    // pthread_t ptid;
    // pthread_create(&ptid, NULL, hack_thread, NULL);

    // In modern C++, you should use std::thread(yourFunction).detach() instead of pthread_create
    // because it is cross-platform and more intuitive.
    std::thread(hack_thread).detach();
}