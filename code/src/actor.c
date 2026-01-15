#include "actor.h"
#include "player.h"
#include "z3D/z3D.h"

void Actor_Init() {
    // Install our player hooks so we can swap tunic models.
    ActorInit* initInfo = gActorOverlayTable[ACTORTYPE_PLAYER].initInfo;
    if (initInfo != NULL) {
        initInfo->init    = PlayerActor_rInit;
        initInfo->update  = PlayerActor_rUpdate;
        initInfo->destroy = PlayerActor_rDestroy;
        initInfo->draw    = PlayerActor_rDraw;
    }
}

s32 Actor_IsBoss(Actor* actor) {
    (void)actor;
    return 0;
}
