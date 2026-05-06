#include <libultraship.h>

#include <fast/interpreter.h>
#include "Engine.h"

#ifdef __vita__
#include <vitasdk.h>
int _newlib_heap_size_user = 256 * 1024 * 1024;
#endif

extern "C" {
#include "audio/external.h"
#include "game/game_init.h"
#include "sm64.h"
}

void alloc_pool() {
    static u64 pool[1024 * 1024 * 4];
    main_pool_init(pool, pool + sizeof(pool) / sizeof(pool[0]));
    gEffectsMemoryPool = mem_pool_init(0x4000, MEMORY_POOL_LEFT);
}

extern "C" void exec_display_list(SPTask* spTask) {
    GameEngine::ProcessGfxCommands((Gfx*)spTask->task.t.data_ptr);
}

void push_frame() {
    GameEngine::StartAudioFrame();
    GameEngine::Instance->StartFrame();
    thread5_iteration();
    GameEngine::EndAudioFrame();
}

#ifdef __vita__
extern "C" void *vita_main(void *argv);
#endif

#ifdef _WIN32
int SDL_main(int argc, char** argv) {
#else
int main() {
#endif
#ifdef __vita__
	//sceSysmoduleLoadModule(SCE_SYSMODULE_RAZOR_CAPTURE);
    scePowerSetArmClockFrequency(444);
    scePowerSetBusClockFrequency(222);
    scePowerSetGpuClockFrequency(222);
    scePowerSetGpuXbarClockFrequency(166);
    sceIoMkdir("ux0:data/ghostship/shader_cache", 0777);
    
    sceClibPrintf("Starting main thread...\n");
    pthread_t t;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, 2 * 1024 * 1024);
    pthread_create(&t, &attr, vita_main, NULL);
    return sceKernelExitDeleteThread(0);
}

extern "C" void *vita_main(void *argv) {
#endif
    GameEngine::Create();
    alloc_pool();
    audio_init();
    sound_init();
    thread5_game_loop();
    while (WindowIsRunning()) {
        push_frame();
    }
    GameEngine::Instance->Destroy();
    return 0;
}