.arm

// Hook Link's CMB retrieval to swap tunic models.
.section .patch_PlayerEditAndRetrieveCMB
.global PlayerEditAndRetrieveCMB_patch
PlayerEditAndRetrieveCMB_patch:
    bl Player_EditAndRetrieveCMB

// Hook a safe per-frame point in the player update so we can rebuild Link's skeleton/model
// after the pause menu closes (rebuilding during equipment application corrupts state).
.section .patch_Player_PostUpdate_Call_1E1D68
.global Player_PostUpdate_Call_1E1D68_patch
Player_PostUpdate_Call_1E1D68_patch:
    bl Player_PostUpdateHook

// Run deferred tunic rebuild at the very end of PlayerActor_Update so we don't clobber per-frame
// calculations like face animation and held-item/spin effects.
.section .patch_PlayerActorUpdate_End_1E1DB8
.global PlayerActorUpdate_End_1E1DB8_patch
PlayerActorUpdate_End_1E1DB8_patch:
    b hook_PlayerActorUpdate_End_1E1DB8

// Patch specific call sites of `Player_SetEquipmentData` (0x34913C) to call our wrapper
// so adult tunic CMB swaps can force a skeleton/model rebuild immediately.
.section .patch_Player_SetEquipmentData_Call_44E5A8
.global Player_SetEquipmentData_Call_44E5A8_patch
Player_SetEquipmentData_Call_44E5A8_patch:
    bl Player_SetEquipmentData_Wrapper

.section .patch_Player_SetEquipmentData_Call_191A3C
.global Player_SetEquipmentData_Call_191A3C_patch
Player_SetEquipmentData_Call_191A3C_patch:
    bl Player_SetEquipmentData_Wrapper

.section .patch_Player_SetEquipmentData_Call_2E9BF0
.global Player_SetEquipmentData_Call_2E9BF0_patch
Player_SetEquipmentData_Call_2E9BF0_patch:
    bl Player_SetEquipmentData_Wrapper

.section .patch_Player_SetEquipmentData_Call_2E9C94
.global Player_SetEquipmentData_Call_2E9C94_patch
Player_SetEquipmentData_Call_2E9C94_patch:
    bl Player_SetEquipmentData_Wrapper

.section .patch_Player_SetEquipmentData_Call_2E9D28
.global Player_SetEquipmentData_Call_2E9D28_patch
Player_SetEquipmentData_Call_2E9D28_patch:
    bl Player_SetEquipmentData_Wrapper

.section .patch_Player_SetEquipmentData_Call_34D744
.global Player_SetEquipmentData_Call_34D744_patch
Player_SetEquipmentData_Call_34D744_patch:
    bl Player_SetEquipmentData_Wrapper

.section .patch_Player_SetEquipmentData_Call_35D22C
.global Player_SetEquipmentData_Call_35D22C_patch
Player_SetEquipmentData_Call_35D22C_patch:
    bl Player_SetEquipmentData_Wrapper

.section .patch_Player_SetEquipmentData_Call_43456C
.global Player_SetEquipmentData_Call_43456C_patch
Player_SetEquipmentData_Call_43456C_patch:
    bl Player_SetEquipmentData_Wrapper

.section .patch_Player_SetEquipmentData_Call_4345A0
.global Player_SetEquipmentData_Call_4345A0_patch
Player_SetEquipmentData_Call_4345A0_patch:
    bl Player_SetEquipmentData_Wrapper

.section .patch_Player_SetEquipmentData_Call_4345FC
.global Player_SetEquipmentData_Call_4345FC_patch
Player_SetEquipmentData_Call_4345FC_patch:
    bl Player_SetEquipmentData_Wrapper

.section .patch_Player_SetEquipmentData_Call_434630
.global Player_SetEquipmentData_Call_434630_patch
Player_SetEquipmentData_Call_434630_patch:
    bl Player_SetEquipmentData_Wrapper

.section .patch_Player_SetEquipmentData_Call_492104
.global Player_SetEquipmentData_Call_492104_patch
Player_SetEquipmentData_Call_492104_patch:
    bl Player_SetEquipmentData_Wrapper

.section .patch_Player_SetEquipmentData_Call_49211C
.global Player_SetEquipmentData_Call_49211C_patch
Player_SetEquipmentData_Call_49211C_patch:
    bl Player_SetEquipmentData_Wrapper

.arm
.text
.syntax unified

.section .text.PlayerActorUpdate_End_1E1DB8
.global hook_PlayerActorUpdate_End_1E1DB8
hook_PlayerActorUpdate_End_1E1DB8:
    // Overwritten instruction:
    //   str r7, [r5, #0x9e8]
    str r7, [r5, #0x9e8]

    push {lr}
    mov r0, r4  // Player*
    mov r1, r6  // GlobalContext*
    bl Player_ApplyPendingTunicRebuild_EndOfUpdate
    pop {lr}

    b 0x1E1DBC
