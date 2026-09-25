#pragma once
#include <stdbool.h>

/* The scenes the kit builds from its own sources, through libobs, in place. */
int sbk_build_scenes(void);          /* the ten scenes, into the current collection */
void sbk_create_collection(void);    /* a collection called "Swarnil Broadcast Kit", then the scenes */
void sbk_add_live_pack(void);        /* light + lower third + ticker + frame into the current scene */
bool sbk_use_profile(void);          /* switch to the profile called "Swarnil Broadcast Kit", if installed */
void sbk_selftest(void);             /* create the collection and screenshot every scene */
void sbk_scenes_free(void);            /* at module unload */
