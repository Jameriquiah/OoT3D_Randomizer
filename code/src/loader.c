// Minimal loader that installs our actor hooks so the tunic swap code runs.
#include "newcodeinfo.h"
#include "actor.h"

void loader_main(void) {
    (void)NEWCODE_OFFSET;
    (void)NEWCODE_SIZE;

    // Ensure the player actor vtable is replaced with our hook functions.
    Actor_Init();
}

void* getCurrentProcessHandle(void) {
    return 0;
}
