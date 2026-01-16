#include "z3D/z3D.h"
#include "objects.h"
#include "player.h"
#include "settings.h"

#define PlayerActor_Init ((ActorFunc)GAME_ADDR(0x191844))
#define PlayerActor_Update ((ActorFunc)GAME_ADDR(0x1E1B54))
#define PlayerActor_Destroy ((ActorFunc)GAME_ADDR(0x19262C))
#define PlayerActor_Draw ((ActorFunc)GAME_ADDR(0x4BF618))

#define LINK_ADULT_BODY_CMB_INDEX 0
#define LINK_ADULT_GORON_BODY_CMB_INDEX 4
#define LINK_ADULT_ZORA_BODY_CMB_INDEX 5

static u8 sLastAdultTunic = 0xFF;
static u8 sPrevPauseState = 0;

typedef void (*SkelAnime_InitLink_proc)(SkelAnime*, ZARInfo*, GlobalContext*, void*, void*, u32, s32, void*, void*);
#define SkelAnime_InitLink ((SkelAnime_InitLink_proc)GAME_ADDR(0x3413EC))

typedef void (*SkelAnime_Free2_proc)(SkelAnime*);
#define SkelAnime_Free2 ((SkelAnime_Free2_proc)GAME_ADDR(0x350BE0))

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

static void Player_ReinitAdultTunicSkel(GlobalContext* globalCtx, Player* player, u8 tunic) {
    if (player == NULL || tunic == 0xFF) {
        return;
    }
    void* cmb = Player_GetAdultBodyCMB(player, tunic);
    if (cmb == NULL) {
        return;
    }

    player->cmbMan = cmb;
    SkelAnime_Free2(&player->skelAnime);
    SkelAnime_InitLink(&player->skelAnime, player->zarInfo, globalCtx, player->cmbMan, player->actor.unk_178, 0, 9,
                       player->jointTable, player->morphTable);
    player->currentTunic = tunic;
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
        Player_ReinitAdultTunicSkel(globalCtx, PLAYER, currentTunic);
        sLastAdultTunic = currentTunic;
    }
    sPrevPauseState = pauseState;

    if (gSaveContext.linkAge == AGE_ADULT) {
        u8 currentTunic = Player_GetAdultTunic();
        if (currentTunic != sLastAdultTunic) {
            sLastAdultTunic = currentTunic;
            Player_ReinitAdultTunicSkel(globalCtx, PLAYER, currentTunic);
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
