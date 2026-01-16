#include "z3D/z3D.h"
#include "objects.h"
#include "player.h"
#include "settings.h"
#include <string.h>

#define PlayerActor_Init ((ActorFunc)GAME_ADDR(0x191844))
#define PlayerActor_Update ((ActorFunc)GAME_ADDR(0x1E1B54))
#define PlayerActor_Destroy ((ActorFunc)GAME_ADDR(0x19262C))
#define PlayerActor_Draw ((ActorFunc)GAME_ADDR(0x4BF618))

#define LINK_ADULT_BODY_CMB_INDEX 0
#define LINK_ADULT_GORON_BODY_CMB_INDEX 4
#define LINK_ADULT_ZORA_BODY_CMB_INDEX 5

static u8 sLastAdultTunic = 0xFF;
static u8 sPrevPauseState = 0;
static u8 sPendingAdultTunicReinit = 0;
static u8 sPendingAdultTunic = 0xFF;

// This internal routine fully rebuilds Link's model state from a given CMB manager pointer.
// It does much more than just `SkelAnime_InitLink` (colliders, draw lists, etc.), which avoids the corruption
// seen when attempting to free/init the skeleton directly mid-game.
typedef void (*Player_RebuildModelFromCmb_proc)(Player* player, GlobalContext* globalCtx, void* cmbMan);
#define Player_RebuildModelFromCmb ((Player_RebuildModelFromCmb_proc)GAME_ADDR(0x250768))

// Called during PlayerActor_Init immediately before the model rebuild, likely to refresh internal pointers/state
// for held items, face, and other per-frame systems.
typedef void (*Player_PreRebuildInit_proc)(GlobalContext* globalCtx, Player* player);
#define Player_PreRebuildInit ((Player_PreRebuildInit_proc)GAME_ADDR(0x34D688))

typedef void* (*Player_InitPostCmbThing_proc)(Player* player, GlobalContext* globalCtx, u32 arg2, u32 arg3);
#define Player_InitPostCmbThing ((Player_InitPostCmbThing_proc)GAME_ADDR(0x36A924))

typedef void (*Player_LinkPostCmbThing_proc)(void* thing, void* manager);
#define Player_LinkPostCmbThing ((Player_LinkPostCmbThing_proc)GAME_ADDR(0x347774))

#define PLAYER_UNK_STRUCT_24F0_OFFSET 0x24F0
#define PLAYER_UNK_STRUCT_2508_OFFSET 0x2508
#define PLAYER_UNK_STRUCT_2520_OFFSET 0x2520
#define PLAYER_UNK_PTR_ARRAY_28CC_OFFSET 0x28CC
#define PLAYER_UNK_PTR_28F8_OFFSET 0x28F8

// These vtable pointers are taken from the game's PlayerActor_Init literal pool (USA 1.0),
// used to initialize several internal per-frame systems (face animation, held items, effects).
#define PLAYER_UNK_24F0_VTBL 0x4EC008
#define PLAYER_UNK_2508_VTBL 0x4EBFD4
#define PLAYER_UNK_2520_VTBL 0x4EBFE4

static void Player_PostRebuildInitSystems(Player* player, GlobalContext* globalCtx) {
    if (player == NULL || globalCtx == NULL) {
        return;
    }

    // Re-init a few internal manager structs the game sets up after the initial model build.
    void* s24f0 = (u8*)player + PLAYER_UNK_STRUCT_24F0_OFFSET;
    void* s2508 = (u8*)player + PLAYER_UNK_STRUCT_2508_OFFSET;
    void* s2520 = (u8*)player + PLAYER_UNK_STRUCT_2520_OFFSET;

    memset(s24f0, 0, 0x18);
    *(u32*)s24f0 = PLAYER_UNK_24F0_VTBL;

    memset(s2508, 0, 0x18);
    *(u32*)s2508 = PLAYER_UNK_2508_VTBL;

    memset(s2520, 0, 0x18);
    *(u32*)s2520 = PLAYER_UNK_2520_VTBL;

    static const u32 args[8] = { 0x58, 0x5B, 0x5A, 0x59, 0x57, 0x5D, 0x56, 0x5C };
    void** ptrArray = (void**)((u8*)player + PLAYER_UNK_PTR_ARRAY_28CC_OFFSET);
    for (u32 i = 0; i < 8; i++) {
        void* thing = Player_InitPostCmbThing(player, globalCtx, 1, args[i]);
        ptrArray[i] = thing;
        if (i == 3) {
            Player_LinkPostCmbThing(thing, s2508);
        }
    }

    // Initialize and (re)spawn a CMAB used for face animation.
    void* faceThing = Player_InitPostCmbThing(player, globalCtx, 1, 0x3C);
    *(void**)((u8*)player + PLAYER_UNK_PTR_28F8_OFFSET) = faceThing;

    if (player->zarInfo != NULL && faceThing != NULL) {
        void* cmabMan = ZAR_GetCMABByIndex(player->zarInfo, 0x2C);
        if (cmabMan != NULL) {
            void* texAnimTarget = *(void**)((u8*)faceThing + 0x0C);
            if (texAnimTarget != NULL) {
                TexAnim_Spawn(texAnimTarget, cmabMan);
            }
        }
    }
}

static u8 Player_GetAdultTunic(void) {
    if (gSaveContext.linkAge != AGE_ADULT) {
        return 0xFF;
    }

    return (gSaveContext.equips.equipment >> 8) & 3;
}

static u32 Player_GetAdultBodyCMBIndex(u8 tunic) {
    switch (tunic) {
        case 2:
            return LINK_ADULT_GORON_BODY_CMB_INDEX;
        case 3:
            return LINK_ADULT_ZORA_BODY_CMB_INDEX;
        default:
            return LINK_ADULT_BODY_CMB_INDEX;
    }
}

static void* Player_GetAdultBodyCMB(Player* player, u8 tunic) {
    if (player == NULL || player->zarInfo == NULL) {
        return NULL;
    }
    return ZAR_GetCMBByIndex(player->zarInfo, Player_GetAdultBodyCMBIndex(tunic));
}

static void Player_RebuildAdultTunicModel(GlobalContext* globalCtx, Player* player, u8 tunic) {
    if (player == NULL || tunic == 0xFF) {
        return;
    }
    void* cmb = Player_GetAdultBodyCMB(player, tunic);
    if (cmb == NULL) {
        return;
    }

    player->cmbMan = cmb;
    Player_PreRebuildInit(globalCtx, player);
    Player_RebuildModelFromCmb(player, globalCtx, cmb);
    Player_PostRebuildInitSystems(player, globalCtx);
    player->currentTunic = tunic;
}

// Wrapper used by patched call sites of `Player_SetEquipmentData` (0x34913C).
// The vanilla function updates `player->currentTunic`, but does not rebuild the model/skeleton to pick a different CMB.
// We call it, then if the tunic changed for adult Link we re-init the skeleton with the correct CMB and re-apply
// equipment state to the new skeleton.
void Player_SetEquipmentData_Wrapper(GlobalContext* globalCtx, Player* player) {
    if (globalCtx == NULL || player == NULL) {
        return;
    }

    s8 prevTunic = player->currentTunic;
    Player_SetEquipmentData(globalCtx, player);

    if (gSaveContext.linkAge != AGE_ADULT) {
        return;
    }

    u8 currentTunic = Player_GetAdultTunic();
    if ((s8)currentTunic == prevTunic) {
        return;
    }

    // Do not rebuild the skeleton/model here: this function runs during various engine update flows
    // (including pause/equipment application) and rebuilding here can corrupt animation state.
    // Instead, mark a pending rebuild and perform it from a safe per-frame hook after the player update.
    sPendingAdultTunic = currentTunic;
    sPendingAdultTunicReinit = 1;
}

typedef void (*Player_PostUpdateProc)(Player* player, GlobalContext* globalCtx, void* arg2);
#define Player_PostUpdate ((Player_PostUpdateProc)GAME_ADDR(0x250AD0))

// Hooked from a call site inside the player update, so we run once per frame in a safe context.
// This executes the original call first.
void Player_PostUpdateHook(Player* player, GlobalContext* globalCtx, void* arg2) {
    Player_PostUpdate(player, globalCtx, arg2);
}

void Player_ApplyPendingTunicRebuild_EndOfUpdate(Player* player, GlobalContext* globalCtx) {
    if (!sPendingAdultTunicReinit || gSaveContext.linkAge != AGE_ADULT || player == NULL || globalCtx == NULL) {
        return;
    }

    // Rebuild only after leaving pause. Doing it earlier resets face/held-item state for the rest of the frame.
    if (PauseContext_GetState() != 0) {
        return;
    }

    u8 tunic = sPendingAdultTunic;
    sPendingAdultTunicReinit = 0;
    sPendingAdultTunic = 0xFF;

    Player_RebuildAdultTunicModel(globalCtx, player, tunic);
    Player_SetEquipmentData(globalCtx, player);
}

void** Player_EditAndRetrieveCMB(ZARInfo* zarInfo, u32 objModelIdx) {
    u32 cmbIndex = objModelIdx;
    if (gSaveContext.linkAge == AGE_ADULT &&
        (objModelIdx == LINK_ADULT_BODY_CMB_INDEX || objModelIdx == LINK_ADULT_GORON_BODY_CMB_INDEX ||
         objModelIdx == LINK_ADULT_ZORA_BODY_CMB_INDEX)) {
        u8 currentTunic = Player_GetAdultTunic();
        if (currentTunic == 2) {
            cmbIndex = LINK_ADULT_GORON_BODY_CMB_INDEX;
        } else if (currentTunic == 3) {
            cmbIndex = LINK_ADULT_ZORA_BODY_CMB_INDEX;
        } else {
            cmbIndex = LINK_ADULT_BODY_CMB_INDEX;
        }
    }
    return ZAR_GetCMBByIndex(zarInfo, cmbIndex);
}

void PlayerActor_rInit(Actor* thisx, GlobalContext* globalCtx) {
    PlayerActor_Init(thisx, globalCtx);
    gGlobalContext = globalCtx;
    sLastAdultTunic = Player_GetAdultTunic();
    sPrevPauseState = PauseContext_GetState();
}

void PlayerActor_rUpdate(Actor* thisx, GlobalContext* globalCtx) {
    PlayerActor_Update(thisx, globalCtx);
    u32 pauseState = PauseContext_GetState();

    if (sPrevPauseState != 0 && pauseState == 0 && gSaveContext.linkAge == AGE_ADULT) {
        u8 currentTunic = Player_GetAdultTunic();
        Player_RebuildAdultTunicModel(globalCtx, PLAYER, currentTunic);
        sLastAdultTunic = currentTunic;
    }
    sPrevPauseState = pauseState;

    if (gSaveContext.linkAge == AGE_ADULT) {
        u8 currentTunic = Player_GetAdultTunic();
        if (currentTunic != sLastAdultTunic) {
            sLastAdultTunic = currentTunic;
            Player_RebuildAdultTunicModel(globalCtx, PLAYER, currentTunic);
        }
    } else {
        sLastAdultTunic = 0xFF;
    }
}

void PlayerActor_rDestroy(Actor* thisx, GlobalContext* globalCtx) {
    PlayerActor_Destroy(thisx, globalCtx);
}

void PlayerActor_rDraw(Actor* thisx, GlobalContext* globalCtx) {
    PlayerActor_Draw(thisx, globalCtx);
}
