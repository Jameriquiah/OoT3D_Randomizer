.arm

// Hook Link's CMB retrieval to swap tunic models.
.section .patch_PlayerEditAndRetrieveCMB
.global PlayerEditAndRetrieveCMB_patch
PlayerEditAndRetrieveCMB_patch:
    bl Player_EditAndRetrieveCMB

// Ensure loader jumps into our hook installer.
.section .patch_loader
.global loader_patch
loader_patch:
    b hook_into_loader

// Minimal loader wrapper: call loader_main then return to the game's original entry.
.section .loader
.global hook_into_loader
hook_into_loader:
    push {r0-r12, lr}
    bl loader_main
    pop {r0-r12, lr}
    bl 0x100028    @ original code
    b  0x100004
