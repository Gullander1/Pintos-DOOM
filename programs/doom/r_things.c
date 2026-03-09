//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//	Refresh of things, i.e. objects represented by sprites.
//




#include <stdio.h>
#include <stdlib.h>


#include "deh_main.h"
#include "doomdef.h"

#include "i_swap.h"
#include "i_system.h"
#include "z_zone.h"
#include "w_wad.h"

#include "r_local.h"

#include "doomstat.h"



#define MINZ				(FRACUNIT*4)
#define BASEYCENTER			100

//void R_DrawColumn (void);
//void R_DrawFuzzColumn (void);



typedef struct
{
    int		x1;
    int		x2;
	
    int		column;
    int		topclip;
    int		bottomclip;

} maskdraw_t;



//
// Sprite rotation 0 is facing the viewer,
//  rotation 1 is one angle turn CLOCKWISE around the axis.
// This is not the same as the angle,
//  which increases counter clockwise (protractor).
// There was a lot of stuff grabbed wrong, so I changed it...
//
fixed_t		pspritescale;
fixed_t		pspriteiscale;

lighttable_t**	spritelights;

// constant arrays
//  used for psprite clipping and initializing clipping
short		negonearray[SCREENWIDTH];
short		screenheightarray[SCREENWIDTH];


//
// INITIALIZATION FUNCTIONS
//

// variables used to look up
//  and range check thing_t sprites patches
spritedef_t*	sprites;
int		numsprites;

spriteframe_t	sprtemp[29];
int		maxframe;
char*		spritename;




//
// R_InstallSpriteLump
// Local function for R_InitSprites.
//
void
R_InstallSpriteLump
( int		lump,
  unsigned	frame,
  unsigned	rotation,
  boolean	flipped )
{
    int		r;
	
    if (frame >= 29 || rotation > 8)
	I_Error("R_InstallSpriteLump: "
		"Bad frame characters in lump %i", lump);
	
    if ((int)frame > maxframe)
	maxframe = frame;
		
    if (rotation == 0)
    {
        if (sprtemp[frame].rotate == false) {
            return; 
        }

        if (sprtemp[frame].rotate == true) {
            return;
        }
            
        sprtemp[frame].rotate = false;
        for (r=0 ; r<8 ; r++)
        {
            sprtemp[frame].lump[r] = lump - firstspritelump;
            sprtemp[frame].flip[r] = (byte)flipped;
        }
        return;
    }
	
    // the lump is only used for one rotation
    if (sprtemp[frame].rotate == false) {
        return;
    }
		
    sprtemp[frame].rotate = true;

    // make 0 based
    rotation--;		
    if (sprtemp[frame].lump[rotation] != -1)
	I_Error ("R_InitSprites: Sprite %s : %c : %c "
		 "has two lumps mapped to it",
		 spritename, 'A'+frame, '1'+rotation);
		
    sprtemp[frame].lump[rotation] = lump - firstspritelump;
    sprtemp[frame].flip[rotation] = (byte)flipped;
}




//
// R_InitSpriteDefs
// Pass a null terminated list of sprite names
//  (4 chars exactly) to be used.
// Builds the sprite rotation matrixes to account
//  for horizontally flipped sprites.
// Will report an error if the lumps are inconsistant. 
// Only called at startup.
//
// Sprite lump names are 4 characters for the actor,
//  a letter for the frame, and a number for the rotation.
// A sprite that is flippable will have an additional
//  letter/number appended.
// The rotation character can be 0 to signify no rotations.
//

// --- CUSTOM STRING COMPARE ---
int doom_cmp(const char *s1, const char *s2, int n) {
    for (int i = 0; i < n; i++) {
        char c1 = s1[i]; if (c1 >= 'a' && c1 <= 'z') c1 -= 32;
        char c2 = s2[i]; if (c2 >= 'a' && c2 <= 'z') c2 -= 32;
        if (c1 != c2) return 1;
        if (c1 == 0) return 0;
    }
    return 0;
}

// --- FIXED INIT SPRITE DEFS ---
void R_InitSpriteDefs (char** namelist) 
{ 
    char** check;
    int i, l, frame, rotation;
        
    check = namelist;
    while (*check != NULL) check++;
    numsprites = check-namelist;
    if (!numsprites) return;
        
    sprites = Z_Malloc(numsprites *sizeof(*sprites), PU_STATIC, NULL);
    
    for (i=0 ; i<numsprites ; i++)
    {
        spritename = DEH_String(namelist[i]);
        memset (sprtemp, -1, sizeof(sprtemp));
        maxframe = -1;
        
        for (l=0 ; l < numlumps ; l++)
        {
            if (!strncasecmp(lumpinfo[l].name, spritename, 4))
            {
                frame = lumpinfo[l].name[4] - 'A';
                rotation = lumpinfo[l].name[5] - '0';

                if (frame >= 0 && frame <= 29 && rotation >= 0 && rotation <= 8) {
                    if (frame > maxframe) maxframe = frame;
                    
                    if (rotation == 0) {
                        for (int r=0 ; r<8 ; r++) {
                            sprtemp[frame].lump[r] = l;
                            sprtemp[frame].flip[r] = 0;
                        }
                        sprtemp[frame].rotate = false;
                    } else {
                        sprtemp[frame].lump[rotation-1] = l;
                        sprtemp[frame].flip[rotation-1] = 0;
                        sprtemp[frame].rotate = true;
                    }

                    if (lumpinfo[l].name[6]) {
                        frame = lumpinfo[l].name[6] - 'A';
                        rotation = lumpinfo[l].name[7] - '0';
                        if (frame >= 0 && frame <= 29 && rotation >= 0 && rotation <= 8) {
                            if (frame > maxframe) maxframe = frame;
                            sprtemp[frame].lump[rotation-1] = l;
                            sprtemp[frame].flip[rotation-1] = 1;
                            sprtemp[frame].rotate = true;
                        }
                    }
                }
            }
        }
        
        if (maxframe == -1) {
            sprites[i].numframes = 0;
            continue;
        }
        maxframe++;
        
        for (frame = 0 ; frame < maxframe ; frame++) {
            if (sprtemp[frame].rotate == 1) {
                for (rotation=0 ; rotation<8 ; rotation++) {
                    if (sprtemp[frame].lump[rotation] == -1) {   
                        sprtemp[frame].lump[rotation] = sprtemp[frame].lump[0]; 
                    }
                }
            }
        }
        
        sprites[i].numframes = maxframe;
        sprites[i].spriteframes = Z_Malloc (maxframe * sizeof(spriteframe_t), PU_STATIC, NULL);
        memcpy (sprites[i].spriteframes, sprtemp, maxframe*sizeof(spriteframe_t));
    }
}




//
// GAME FUNCTIONS
//
vissprite_t	vissprites[MAXVISSPRITES];
vissprite_t*	vissprite_p;
int		newvissprite;



//
// R_InitSprites
// Called at program start.
//
void R_InitSprites (char** namelist)
{
    int		i;
	
    for (i=0 ; i<SCREENWIDTH ; i++)
    {
	negonearray[i] = -1;
    }
	
    R_InitSpriteDefs (namelist);
}



//
// R_ClearSprites
// Called at frame start.
//
void R_ClearSprites (void)
{
    vissprite_p = vissprites;
}


//
// R_NewVisSprite
//
vissprite_t	overflowsprite;

vissprite_t* R_NewVisSprite (void)
{
    if (vissprite_p == &vissprites[MAXVISSPRITES])
	return &overflowsprite;
    
    vissprite_p++;
    return vissprite_p-1;
}



//
// R_DrawMaskedColumn
// Used for sprites and masked mid textures.
// Masked means: partly transparent, i.e. stored
//  in posts/runs of opaque pixels.
//
short*		mfloorclip;
short*		mceilingclip;

fixed_t		spryscale;
fixed_t		sprtopscreen;

void R_DrawMaskedColumn (column_t* column)
{
    int		topscreen;
    int 	bottomscreen;
    fixed_t	basetexturemid;
	
    basetexturemid = dc_texturemid;
	
    for ( ; column->topdelta != 0xff ; ) 
    {
	// calculate unclipped screen coordinates
	//  for post
	topscreen = sprtopscreen + spryscale*column->topdelta;
	bottomscreen = topscreen + spryscale*column->length;

	dc_yl = (topscreen+FRACUNIT-1)>>FRACBITS;
	dc_yh = (bottomscreen-1)>>FRACBITS;
		
	if (dc_yh >= mfloorclip[dc_x])
	    dc_yh = mfloorclip[dc_x]-1;
	if (dc_yl <= mceilingclip[dc_x])
	    dc_yl = mceilingclip[dc_x]+1;

	if (dc_yl <= dc_yh)
	{
	    dc_source = (byte *)column + 3;
	    dc_texturemid = basetexturemid - (column->topdelta<<FRACBITS);
	    // dc_source = (byte *)column + 3 - column->topdelta;

	    // Drawn by either R_DrawColumn
	    //  or (SHADOW) R_DrawFuzzColumn.
	    colfunc ();	
	}
	column = (column_t *)(  (byte *)column + column->length + 4);
    }
	
    dc_texturemid = basetexturemid;
}



//
// R_DrawVisSprite
//  mfloorclip and mceilingclip should also be set.
//
void
R_DrawVisSprite
( vissprite_t*		vis,
  int			x1,
  int			x2 )
{
    column_t*		column;
    int			texturecolumn;
    fixed_t		frac;
    patch_t*		patch;

    if (!vis || vis->patch < 0 || vis->patch >= numlumps) {
        return; 
    }
    patch = W_CacheLumpNum (vis->patch, PU_CACHE);

    patch = W_CacheLumpNum (vis->patch, PU_CACHE);
    if (!patch) {
        return;
    }

    dc_colormap = vis->colormap;
    
    if (!dc_colormap)
    {
	// NULL colormap = shadow draw
	colfunc = fuzzcolfunc;
    }
    else if (vis->mobjflags & MF_TRANSLATION)
    {
	colfunc = transcolfunc;
	dc_translation = translationtables - 256 +
	    ( (vis->mobjflags & MF_TRANSLATION) >> (MF_TRANSSHIFT-8) );
    }
	
    dc_iscale = abs(vis->xiscale)>>detailshift;
    dc_texturemid = vis->texturemid;
    frac = vis->startfrac;
    spryscale = vis->scale;
    sprtopscreen = centeryfrac - FixedMul(dc_texturemid,spryscale);
	
    for (dc_x=vis->x1 ; dc_x<=vis->x2 ; dc_x++, frac += vis->xiscale)
    {
        texturecolumn = frac>>FRACBITS;
        
        if (texturecolumn < 0) texturecolumn = 0;
        if (texturecolumn >= SHORT(patch->width)) texturecolumn = SHORT(patch->width) - 1;

        column = (column_t *) ((byte *)patch + LONG(patch->columnofs[texturecolumn]));
        R_DrawMaskedColumn (column);
    }

    colfunc = basecolfunc;
}



//
// R_ProjectSprite
// Generates a vissprite for a thing
//  if it might be visible.
//
void R_ProjectSprite (mobj_t* thing)
{
    fixed_t tr_x, tr_y, gxt, gyt, tx, tz, xscale, iscale;
    int x1, x2, index;
    unsigned rot; boolean flip;
    
    tr_x = thing->x - viewx; tr_y = thing->y - viewy;
    gxt = FixedMul(tr_x,viewcos); gyt = -FixedMul(tr_y,viewsin);
    tz = gxt-gyt; 
    if (tz < MINZ) return;
    
    xscale = FixedDiv(projection, tz);
    gxt = -FixedMul(tr_x,viewsin); gyt = FixedMul(tr_y,viewcos); 
    tx = -(gyt+gxt); 
    if (abs(tx)>(tz<<2)) return;
    
    spritedef_t* sprdef = &sprites[thing->sprite];
    spriteframe_t* sprframe = &sprdef->spriteframes[ thing->frame & FF_FRAMEMASK];

    if (sprframe->rotate) {
        angle_t ang = R_PointToAngle (thing->x, thing->y);
        rot = (ang-thing->angle+(unsigned)(ANG45/2)*9)>>29;
    } else {
        rot = 0;
    }
    
    int real_lump_index = sprframe->lump[rot];
    flip = (boolean)sprframe->flip[rot];

    if (real_lump_index < 0 || real_lump_index >= numlumps) return;
    patch_t* real_patch = W_CacheLumpNum(real_lump_index, PU_CACHE);
    
    if (!real_patch) return;

    tx -= SHORT(real_patch->leftoffset) << FRACBITS;   
    x1 = (centerxfrac + FixedMul (tx,xscale) ) >>FRACBITS;
    if (x1 > viewwidth) return;
    
    tx += SHORT(real_patch->width) << FRACBITS;
    x2 = ((centerxfrac + FixedMul (tx,xscale) ) >>FRACBITS) - 1;
    if (x2 < 0) return;
    
    vissprite_t* vis = R_NewVisSprite ();
    vis->mobjflags = thing->flags;
    vis->scale = xscale<<detailshift;
    vis->gx = thing->x; vis->gy = thing->y;
    vis->gz = thing->z;
    vis->gzt = thing->z + (SHORT(real_patch->topoffset) << FRACBITS);
    vis->texturemid = vis->gzt - viewz;
    vis->x1 = x1 < 0 ? 0 : x1;
    vis->x2 = x2 >= viewwidth ? viewwidth-1 : x2;   
    iscale = FixedDiv (FRACUNIT, xscale);

    if (flip) {
        vis->xiscale = -iscale;
        vis->startfrac = (SHORT(real_patch->width) << FRACBITS) - 1;
    } else {
        vis->xiscale = iscale;
        vis->startfrac = 0;
    }

    if (vis->x1 > x1) vis->startfrac += vis->xiscale*(vis->x1-x1);
    
    vis->patch = real_lump_index; // Som förut
    
    if (thing->flags & MF_SHADOW) vis->colormap = NULL;
    else if (fixedcolormap) vis->colormap = fixedcolormap;
    else if (thing->frame & FF_FULLBRIGHT) vis->colormap = colormaps;
    else {
        index = xscale>>(LIGHTSCALESHIFT-detailshift);
        if (index >= MAXLIGHTSCALE) index = MAXLIGHTSCALE-1;
        vis->colormap = spritelights[index];
    }   
}




//
// R_AddSprites
// During BSP traversal, this adds sprites by sector.
//
void R_AddSprites (sector_t* sec)
{
    mobj_t*		thing;
    int			lightnum;

    // BSP is traversed by subsector.
    // A sector might have been split into several
    //  subsectors during BSP building.
    // Thus we check whether its already added.
    if (sec->validcount == validcount)
	return;		

    // Well, now it will be done.
    sec->validcount = validcount;
	
    lightnum = (sec->lightlevel >> LIGHTSEGSHIFT)+extralight;

    if (lightnum < 0)		
	spritelights = scalelight[0];
    else if (lightnum >= LIGHTLEVELS)
	spritelights = scalelight[LIGHTLEVELS-1];
    else
	spritelights = scalelight[lightnum];

    // Handle all things in sector.
    for (thing = sec->thinglist ; thing ; thing = thing->snext)
	R_ProjectSprite (thing);
}


//
// R_DrawPSprite
//
void R_DrawPSprite (pspdef_t* psp)
{
    fixed_t tx; int x1, x2; vissprite_t avis;
    if (!psp || !psp->state) return;

    spritedef_t* sprdef = &sprites[psp->state->sprite];
    spriteframe_t* sprframe = &sprdef->spriteframes[ psp->state->frame & FF_FRAMEMASK ];

    int real_lump_index = sprframe->lump[0];
    boolean flip = (boolean)sprframe->flip[0];
    
    if (real_lump_index < 0 || real_lump_index >= numlumps) return;
    patch_t* real_patch = W_CacheLumpNum(real_lump_index, PU_CACHE);

    if (!real_patch) return;

    tx = psp->sx - 160*FRACUNIT;
    tx -= SHORT(real_patch->leftoffset) << FRACBITS;   
    x1 = (centerxfrac + FixedMul (tx,pspritescale) ) >>FRACBITS;

    if (x1 > viewwidth) return;     

    tx += SHORT(real_patch->width) << FRACBITS;
    x2 = ((centerxfrac + FixedMul (tx, pspritescale) ) >>FRACBITS) - 1;

    if (x2 < 0) return;
    
    vissprite_t* vis = &avis;
    vis->mobjflags = 0;
    vis->texturemid = (BASEYCENTER<<FRACBITS)+FRACUNIT/2-(psp->sy - (SHORT(real_patch->topoffset) << FRACBITS));
    vis->scale = pspritescale<<detailshift;
    
    if (flip) {
        vis->xiscale = -pspriteiscale;
        vis->startfrac = (SHORT(real_patch->width) << FRACBITS) - 1;
    } else {
        vis->xiscale = pspriteiscale;
        vis->startfrac = 0;
    }
    
    vis->x1 = x1 < 0 ? 0 : x1;
    vis->x2 = x2 >= viewwidth ? viewwidth-1 : x2;   
    if (vis->x1 > x1) vis->startfrac += vis->xiscale*(vis->x1-x1);


    vis->patch = real_lump_index; 

    if (viewplayer->powers[pw_invisibility] > 4*32 || viewplayer->powers[pw_invisibility] & 8)
        vis->colormap = NULL;
    else if (fixedcolormap)
        vis->colormap = fixedcolormap;
    else if (psp->state->frame & FF_FULLBRIGHT)
        vis->colormap = colormaps;
    else
        vis->colormap = spritelights[MAXLIGHTSCALE-1];
    
    R_DrawVisSprite (vis, vis->x1, vis->x2);
}



//
// R_DrawPlayerSprites
//
void R_DrawPlayerSprites (void)
{
    int		i;
    int		lightnum;
    pspdef_t*	psp;
    
    // get light level
    lightnum =
	(viewplayer->mo->subsector->sector->lightlevel >> LIGHTSEGSHIFT) 
	+extralight;

    if (lightnum < 0)		
	spritelights = scalelight[0];
    else if (lightnum >= LIGHTLEVELS)
	spritelights = scalelight[LIGHTLEVELS-1];
    else
	spritelights = scalelight[lightnum];
    
    // clip to screen bounds
    mfloorclip = screenheightarray;
    mceilingclip = negonearray;
    
    // add all active psprites
    for (i=0, psp=viewplayer->psprites;
	 i<NUMPSPRITES;
	 i++,psp++)
    {
	if (psp->state)
	    R_DrawPSprite (psp);
    }
}




//
// R_SortVisSprites
//
vissprite_t	vsprsortedhead;


void R_SortVisSprites (void)
{
    int			i;
    int			count;
    vissprite_t*	ds;
    vissprite_t*	best;
    vissprite_t		unsorted;
    fixed_t		bestscale;

    count = vissprite_p - vissprites;
	
    unsorted.next = unsorted.prev = &unsorted;

    if (!count)
	return;
		
    for (ds=vissprites ; ds<vissprite_p ; ds++)
    {
	ds->next = ds+1;
	ds->prev = ds-1;
    }
    
    vissprites[0].prev = &unsorted;
    unsorted.next = &vissprites[0];
    (vissprite_p-1)->next = &unsorted;
    unsorted.prev = vissprite_p-1;
    
    // pull the vissprites out by scale

    vsprsortedhead.next = vsprsortedhead.prev = &vsprsortedhead;
    for (i=0 ; i<count ; i++)
    {
	bestscale = INT_MAX;
        best = unsorted.next;
	for (ds=unsorted.next ; ds!= &unsorted ; ds=ds->next)
	{
	    if (ds->scale < bestscale)
	    {
		bestscale = ds->scale;
		best = ds;
	    }
	}
	best->next->prev = best->prev;
	best->prev->next = best->next;
	best->next = &vsprsortedhead;
	best->prev = vsprsortedhead.prev;
	vsprsortedhead.prev->next = best;
	vsprsortedhead.prev = best;
    }
}



//
// R_DrawSprite
//
static short		clipbot[SCREENWIDTH];
static short		cliptop[SCREENWIDTH];
void R_DrawSprite (vissprite_t* spr)
{
    drawseg_t*		ds;
    int			x;
    int			r1;
    int			r2;
    fixed_t		scale;
    fixed_t		lowscale;
    int			silhouette;

    if (!spr) {
        return;
    }

    if (spr->x1 < 0) spr->x1 = 0;
    if (spr->x2 >= SCREENWIDTH) spr->x2 = SCREENWIDTH - 1;
    if (spr->x1 > spr->x2) return;

    /*
    if ((uint32_t)spr->patch < 0x08000000 || (uint32_t)spr->patch > 0x20000000) {
        static int warn_once = 0;
        if (!warn_once) {
            warn_once = 1;
        }
        return; 
    }
    */
		
    for (x = spr->x1 ; x<=spr->x2 ; x++)
	clipbot[x] = cliptop[x] = -2;
    
    // Scan drawsegs from end to start for obscuring segs.
    // The first drawseg that has a greater scale
    //  is the clip seg.
    for (ds=ds_p-1 ; ds >= drawsegs ; ds--)
    {
	// determine if the drawseg obscures the sprite
	if (ds->x1 > spr->x2
	    || ds->x2 < spr->x1
	    || (!ds->silhouette
		&& !ds->maskedtexturecol) )
	{
	    // does not cover sprite
	    continue;
	}
			
	r1 = ds->x1 < spr->x1 ? spr->x1 : ds->x1;
	r2 = ds->x2 > spr->x2 ? spr->x2 : ds->x2;

	if (ds->scale1 > ds->scale2)
	{
	    lowscale = ds->scale2;
	    scale = ds->scale1;
	}
	else
	{
	    lowscale = ds->scale1;
	    scale = ds->scale2;
	}
		
	if (scale < spr->scale
	    || ( lowscale < spr->scale
		 && !R_PointOnSegSide (spr->gx, spr->gy, ds->curline) ) )
	{
	    // masked mid texture?
	    if (ds->maskedtexturecol)	
		R_RenderMaskedSegRange (ds, r1, r2);
	    // seg is behind sprite
	    continue;			
	}

	
	// clip this piece of the sprite
	silhouette = ds->silhouette;
	
	if (spr->gz >= ds->bsilheight)
	    silhouette &= ~SIL_BOTTOM;

	if (spr->gzt <= ds->tsilheight)
	    silhouette &= ~SIL_TOP;
			
	if (silhouette == 1)
	{
	    // bottom sil
	    for (x=r1 ; x<=r2 ; x++)
		if (clipbot[x] == -2)
		    clipbot[x] = ds->sprbottomclip[x];
	}
	else if (silhouette == 2)
	{
	    // top sil
	    for (x=r1 ; x<=r2 ; x++)
		if (cliptop[x] == -2)
		    cliptop[x] = ds->sprtopclip[x];
	}
	else if (silhouette == 3)
	{
	    // both
	    for (x=r1 ; x<=r2 ; x++)
	    {
		if (clipbot[x] == -2)
		    clipbot[x] = ds->sprbottomclip[x];
		if (cliptop[x] == -2)
		    cliptop[x] = ds->sprtopclip[x];
	    }
	}
		
    }
    
    // all clipping has been performed, so draw the sprite

    // check for unclipped columns
    for (x = spr->x1 ; x<=spr->x2 ; x++)
    {
	if (clipbot[x] == -2)		
	    clipbot[x] = viewheight;

	if (cliptop[x] == -2)
	    cliptop[x] = -1;
    }
		
    mfloorclip = clipbot;
    mceilingclip = cliptop;
    R_DrawVisSprite (spr, spr->x1, spr->x2);
}




//
// R_DrawMasked
//
void R_DrawMasked (void)
{
    vissprite_t* spr;
    drawseg_t* ds;
    int count = 0;
    
    R_SortVisSprites ();

    if (vissprite_p > vissprites)
    {
        for (spr = vsprsortedhead.next ; spr != &vsprsortedhead ; spr=spr->next)
        {
            count++;

            /*
            if ((uint32_t)spr < 0x00100000 || (uint32_t)spr > 0x30000000) {
                return; 
            }
            */
            
            R_DrawSprite (spr);
        }
    }
    
    // render any remaining masked mid textures
    for (ds=ds_p-1 ; ds >= drawsegs ; ds--)
        if (ds->maskedtexturecol)
            R_RenderMaskedSegRange (ds, ds->x1, ds->x2);

    if (!viewangleoffset)       
        R_DrawPlayerSprites ();

}



