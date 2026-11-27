#include <switch.h>
#include "module.h"
#include "log.h"

/* IME (raw keyboard) bring-up: LittleBigPlanet opens IME contexts for text
 * entry. Library NID 0xD2553017 per vitasdk db/360. No host keyboard UI:
 * opens succeed but no input events ever arrive. */

static Mutex ime_lock;
static int ime_next_id;

EXPORT(SceIme,0x0E050613,int,sceImeOpen,void *param,unsigned int size,int *wid,int *eid)
{
    mutexLock(&ime_lock);
    int id=++ime_next_id;
    mutexUnlock(&ime_lock);
    LOG("sceImeOpen param=%p -> id=%d",param,id);
    /* Event flag handles 0 tell the game no IME events will arrive. */
    if(wid)*wid=0;
    if(eid)*eid=0;
    return id;
}

EXPORT(SceIme,0x889A8421,int,sceImeClose,int id)
{
    LOG("sceImeClose id=%d",id);
    return 0;
}

EXPORT(SceIme,0x71D6898A,int,sceImeUpdate,void)
{
    return 0;
}

EXPORT(SceIme,0xF3BD9A76,int,sceImeSetText,int id,const void *text)
{
    return 0;
}

EXPORT(SceIme,0xD8342D2A,int,sceImeSetCaret,int id,unsigned int pos)
{
    return 0;
}

EXPORT(SceIme,0x7B1EFAA5,int,sceImeSetPreeditGeometry,int id,void *param)
{
    return 0;
}

DECLARE_LIBRARY(SceIme,0xD2553017);
