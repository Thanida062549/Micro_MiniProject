#include "hp.h"

void HP_Init(GameContext_t *ctx)
{
    ctx->hp = HP_INIT;
}

void HP_Decrement(GameContext_t *ctx)
{
    if (ctx->hp > 0) {
        ctx->hp--;
    }
}

uint8_t HP_IsGameOver(const GameContext_t *ctx)
{
    return (ctx->hp == 0) ? 1 : 0;
}
