.arm

// Hook Link's CMB retrieval to swap tunic models.
.section .patch_PlayerEditAndRetrieveCMB
.global PlayerEditAndRetrieveCMB_patch
PlayerEditAndRetrieveCMB_patch:
    bl Player_EditAndRetrieveCMB
