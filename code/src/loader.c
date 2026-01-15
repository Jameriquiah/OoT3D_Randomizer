// Minimal loader stub; no special memory handling needed for the tunic patch.
#include "newcodeinfo.h"

void loader_main(void) {
    (void)NEWCODE_OFFSET;
    (void)NEWCODE_SIZE;
}

void* getCurrentProcessHandle(void) {
    return 0;
}
