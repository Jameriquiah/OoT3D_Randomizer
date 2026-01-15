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
    if (actor == NULL) {
        return 0;
    }
    switch (actor->id) {
        case 0x0095: // Gohma
        case 0x0096: // King Dodongo
        case 0x0097: // Barinade
        case 0x0098: // Phantom Ganon
        case 0x0099: // Volvagia
        case 0x009A: // Morpha
        case 0x009B: // Bongo Bongo
        case 0x009C: // Twinrova
        case 0x009D: // Ganondorf
        case 0x009E: // Ganon
            return 1;
        default:
            return 0;
    }
}
