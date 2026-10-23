/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
#include <pm_shared.h>
#include <tier1/strtools.h>
#include "hud.h"
#include "cl_util.h"
#include "const.h"
#include "entity_state.h"
#include "cl_entity.h"
#include "entity_types.h"
#include "usercmd.h"
#include "pm_defs.h"
#include "pm_materials.h"

#include "eventscripts.h"
#include "ev_hldm.h"

#include "r_efx.h"
#include "event_api.h"
#include "event_args.h"
#include "in_defs.h"

#include <string.h>

#include "r_studioint.h"
#include "com_model.h"

#include "hud/spectator.h"

#include "zp/zp_shared_weapons.h"
#include "zp/hud/zp_crosshair.h"
#include "hud/ammo.h"

extern engine_studio_api_t IEngineStudio;

static int tracerCount[32];

void V_PunchAxis(int axis, float punch);
void VectorAngles(const float *forward, float *angles);

// ------------------------------------------------------------------------
// Shell ejection cvars cords
// We can adjust the shell ejection position for each weapon
// ------------------------------------------------------------------------

static ConVar cl_shellejects_dbarrel1( "cl_shellejects_dbarrel1", "-20 -12 -4" );
static ConVar cl_shellejects_dbarrel2( "cl_shellejects_dbarrel2", "-20 -12 4" );
static ConVar cl_shellejects_m16( "cl_shellejects_m16", "13 -12 10" );
static ConVar cl_shellejects_sks( "cl_shellejects_sks", "20 -6 8" );
static ConVar cl_shellejects_sig( "cl_shellejects_sig", "20 -12 4" );
static ConVar cl_shellejects_ppk( "cl_shellejects_ppk", "20 -12 4" );
static ConVar cl_shellejects_cz75("cl_shellejects_cz75", "20 -12 4");
static ConVar cl_shellejects_mp5( "cl_shellejects_mp5", "13 -12 10" );
static ConVar cl_shellejects_revolver( "cl_shellejects_revolver", "20 -12 4" );
static ConVar cl_shellejects_shotgun( "cl_shellejects_shotgun", "8 -12 8" );
static ConVar cl_shellejects_357shell( "cl_shellejects_357shell", "-10 -14 -5" );

// ------------------------------------------------------------------------

extern "C"
{

	// HLDM
	void EV_FireFAFO(struct event_args_s *args);
	void EV_FireSig(struct event_args_s *args);
	void EV_Fire1911(struct event_args_s *args);
	void EV_FirePPK(struct event_args_s *args);
	void EV_FireCZ75(struct event_args_s *args);
	void EV_FireGlock(struct event_args_s *args);
	void EV_FireDBarrel(struct event_args_s *args);
	void EV_DBarrelReload(struct event_args_s *args);
	void EV_ShotgunPump(struct event_args_s *args);
	void EV_FireShotGunSingle(struct event_args_s *args);
	void EV_FireShotGunDouble(struct event_args_s *args);
	void EV_FireSKS(struct event_args_s *args);
	void EV_FireM16(struct event_args_s *args);
	void EV_FireMP5(struct event_args_s *args);
	void EV_FireMP52(struct event_args_s *args);
	void EV_FirePython(struct event_args_s *args);
	void EV_PythonShells(struct event_args_s *args);
	void EV_FireNailgun(struct event_args_s *args);
	void EV_FireRpg(struct event_args_s *args);
	void EV_TripmineFire(struct event_args_s *args);
	void EV_DecalReset(struct event_args_s *args);

	void EV_TrainPitchAdjust(struct event_args_s *args);
	void EV_VehiclePitchAdjust(struct event_args_s *args);
}

#define VECTOR_CONE_1DEGREES  Vector(0.00873, 0.00873, 0.00873)
#define VECTOR_CONE_2DEGREES  Vector(0.01745, 0.01745, 0.01745)
#define VECTOR_CONE_3DEGREES  Vector(0.02618, 0.02618, 0.02618)
#define VECTOR_CONE_4DEGREES  Vector(0.03490, 0.03490, 0.03490)
#define VECTOR_CONE_5DEGREES  Vector(0.04362, 0.04362, 0.04362)
#define VECTOR_CONE_6DEGREES  Vector(0.05234, 0.05234, 0.05234)
#define VECTOR_CONE_7DEGREES  Vector(0.06105, 0.06105, 0.06105)
#define VECTOR_CONE_8DEGREES  Vector(0.06976, 0.06976, 0.06976)
#define VECTOR_CONE_9DEGREES  Vector(0.07846, 0.07846, 0.07846)
#define VECTOR_CONE_10DEGREES Vector(0.08716, 0.08716, 0.08716)
#define VECTOR_CONE_15DEGREES Vector(0.13053, 0.13053, 0.13053)
#define VECTOR_CONE_20DEGREES Vector(0.17365, 0.17365, 0.17365)

// play a strike sound based on the texture that was hit by the attack traceline.  VecSrc/VecEnd are the
// original traceline endpoints used by the attacker, iBulletType is the type of bullet that hit the texture.
// returns volume of strike instrument (crowbar) to play
float EV_HLDM_PlayTextureSound(int idx, pmtrace_t *ptr, float *vecSrc, float *vecEnd, int iBulletType)
{
	// hit the world, try to play sound based on texture material type
	char chTextureType = CHAR_TEX_CONCRETE;
	float fvol;
	float fvolbar;
	char *rgsz[4];
	int cnt;
	float fattn = ATTN_NORM;
	int entity;
	char *pTextureName;
	char texname[64];
	char szbuffer[64];

	entity = gEngfuncs.pEventAPI->EV_IndexFromTrace(ptr);

	// FIXME check if playtexture sounds movevar is set
	//

	chTextureType = 0;

	// Player
	if (entity == 0)
	{
		// get texture from entity or world (world is ent(0))
		pTextureName = (char *)gEngfuncs.pEventAPI->EV_TraceTexture(ptr->ent, vecSrc, vecEnd);

		if (pTextureName)
		{
			V_strcpy_safe(texname, pTextureName);
			pTextureName = texname;

			// strip leading '-0' or '+0~' or '{' or '!'
			if (*pTextureName == '-' || *pTextureName == '+')
			{
				pTextureName += 2;
			}

			if (*pTextureName == '{' || *pTextureName == '!' || *pTextureName == '~' || *pTextureName == ' ')
			{
				pTextureName++;
			}

			// '}}'
			V_strcpy_safe(szbuffer, pTextureName);
			szbuffer[CBTEXTURENAMEMAX - 1] = 0;

			// get texture type
			chTextureType = PM_FindTextureType(szbuffer);
		}
	}
	else
	{
		// JoshA: Look up the entity and find the EFLAG_FLESH_SOUND flag.
		// This broke at some point then TF:C added prediction.
		//
		// It used to use Classify of pEntity->Classify() != CLASS_NONE && pEntity->Classify() != CLASS_MACHINE
		// to determine what sound to play, but that's server side and isn't available on the client
		// and got lost in the translation to that.
		// Now the server will replicate that state via an eflag.
		cl_entity_t *cl_entity = gEngfuncs.GetEntityByIndex(entity);

		if (cl_entity && !!(cl_entity->curstate.eflags & EFLAG_FLESH_SOUND))
			chTextureType = CHAR_TEX_FLESH;
	}

	switch (chTextureType)
	{
	default:
	case CHAR_TEX_CONCRETE:
		fvol = 0.9;
		fvolbar = 0.6;
		rgsz[0] = "player/pl_step1.wav";
		rgsz[1] = "player/pl_step2.wav";
		cnt = 2;
		break;
	case CHAR_TEX_METAL:
		fvol = 0.9;
		fvolbar = 0.3;
		rgsz[0] = "player/pl_metal1.wav";
		rgsz[1] = "player/pl_metal2.wav";
		cnt = 2;
		break;
	case CHAR_TEX_DIRT:
		fvol = 0.9;
		fvolbar = 0.1;
		rgsz[0] = "player/pl_dirt1.wav";
		rgsz[1] = "player/pl_dirt2.wav";
		rgsz[2] = "player/pl_dirt3.wav";
		cnt = 3;
		break;
	case CHAR_TEX_VENT:
		fvol = 0.5;
		fvolbar = 0.3;
		rgsz[0] = "player/pl_duct1.wav";
		rgsz[1] = "player/pl_duct1.wav";
		cnt = 2;
		break;
	case CHAR_TEX_GRATE:
		fvol = 0.9;
		fvolbar = 0.5;
		rgsz[0] = "player/pl_grate1.wav";
		rgsz[1] = "player/pl_grate4.wav";
		cnt = 2;
		break;
	case CHAR_TEX_TILE:
		fvol = 0.8;
		fvolbar = 0.2;
		rgsz[0] = "player/pl_tile1.wav";
		rgsz[1] = "player/pl_tile3.wav";
		rgsz[2] = "player/pl_tile2.wav";
		rgsz[3] = "player/pl_tile4.wav";
		cnt = 4;
		break;
	case CHAR_TEX_SLOSH:
		fvol = 0.9;
		fvolbar = 0.0;
		rgsz[0] = "player/pl_slosh1.wav";
		rgsz[1] = "player/pl_slosh3.wav";
		rgsz[2] = "player/pl_slosh2.wav";
		rgsz[3] = "player/pl_slosh4.wav";
		cnt = 4;
		break;
	case CHAR_TEX_WOOD:
		fvol = 0.9;
		fvolbar = 0.2;
		rgsz[0] = "debris/wood1.wav";
		rgsz[1] = "debris/wood2.wav";
		rgsz[2] = "debris/wood3.wav";
		cnt = 3;
		break;
	case CHAR_TEX_GLASS:
	case CHAR_TEX_GLASS_UNBREAK:
	case CHAR_TEX_COMPUTER:
		fvol = 0.8;
		fvolbar = 0.2;
		rgsz[0] = "debris/glass1.wav";
		rgsz[1] = "debris/glass2.wav";
		rgsz[2] = "debris/glass3.wav";
		cnt = 3;
		break;
	case CHAR_TEX_FLESH:
		if (iBulletType == BULLET_PLAYER_CROWBAR || iBulletType == BULLET_PLAYER_SHARP || iBulletType == BULLET_PLAYER_SWIPE)
			return 0.0; // crowbar already makes this sound
		fvol = 1.0;
		fvolbar = 0.2;
		rgsz[0] = "weapons/bullet_hit1.wav";
		rgsz[1] = "weapons/bullet_hit2.wav";
		fattn = 1.0;
		cnt = 2;
		break;
	}

	// play material hit sound
	gEngfuncs.pEventAPI->EV_PlaySound(0, ptr->endpos, CHAN_STATIC, rgsz[gEngfuncs.pfnRandomLong(0, cnt - 1)], fvol, fattn, 0, 96 + gEngfuncs.pfnRandomLong(0, 0xf));
	return fvolbar;
}

char *EV_HLDM_DamageDecal(physent_t *pe)
{
	static char decalname[32];
	int idx;

	if (pe->classnumber == 1)
	{
		idx = gEngfuncs.pfnRandomLong(0, 2);
		sprintf(decalname, "{break%i", idx + 1);
	}
	else if (pe->rendermode != kRenderNormal)
	{
		sprintf(decalname, "{bproof1");
	}
	else
	{
		idx = gEngfuncs.pfnRandomLong(0, 4);
		sprintf(decalname, "{shot%i", idx + 1);
	}
	return decalname;
}

void EV_HLDM_GunshotDecalTrace(pmtrace_t *pTrace, char *decalName)
{
	int iRand;
	physent_t *pe;

	gEngfuncs.pEfxAPI->R_BulletImpactParticles(pTrace->endpos);

	iRand = gEngfuncs.pfnRandomLong(0, 0x7FFF);
	if (iRand < (0x7fff / 2)) // not every bullet makes a sound.
	{
		switch (iRand % 5)
		{
		case 0:
			gEngfuncs.pEventAPI->EV_PlaySound(-1, pTrace->endpos, 0, "weapons/ric1.wav", 1.0, ATTN_NORM, 0, PITCH_NORM);
			break;
		case 1:
			gEngfuncs.pEventAPI->EV_PlaySound(-1, pTrace->endpos, 0, "weapons/ric2.wav", 1.0, ATTN_NORM, 0, PITCH_NORM);
			break;
		case 2:
			gEngfuncs.pEventAPI->EV_PlaySound(-1, pTrace->endpos, 0, "weapons/ric3.wav", 1.0, ATTN_NORM, 0, PITCH_NORM);
			break;
		case 3:
			gEngfuncs.pEventAPI->EV_PlaySound(-1, pTrace->endpos, 0, "weapons/ric4.wav", 1.0, ATTN_NORM, 0, PITCH_NORM);
			break;
		case 4:
			gEngfuncs.pEventAPI->EV_PlaySound(-1, pTrace->endpos, 0, "weapons/ric5.wav", 1.0, ATTN_NORM, 0, PITCH_NORM);
			break;
		}
	}

	pe = gEngfuncs.pEventAPI->EV_GetPhysent(pTrace->ent);

	// Only decal brush models such as the world etc.
	if (decalName && decalName[0] && pe && (pe->solid == SOLID_BSP || pe->movetype == MOVETYPE_PUSHSTEP))
	{
		if (CVAR_GET_FLOAT("r_decals"))
		{
			gEngfuncs.pEfxAPI->R_DecalShoot(
			    gEngfuncs.pEfxAPI->Draw_DecalIndex(gEngfuncs.pEfxAPI->Draw_DecalIndexFromName(decalName)),
			    gEngfuncs.pEventAPI->EV_IndexFromTrace(pTrace), 0, pTrace->endpos, 0);
		}
	}
}

void EV_HLDM_DecalGunshot(pmtrace_t *pTrace, const Vector &vDir, int iBulletType)
{
	physent_t *pe;

	pe = gEngfuncs.pEventAPI->EV_GetPhysent(pTrace->ent);

	if (pe && pe->solid == SOLID_BSP)
	{
		switch (iBulletType)
		{
			default:
				// smoke and decal
				EV_HLDM_GunshotDecalTrace(pTrace, EV_HLDM_DamageDecal(pe));
			break;
		}
	}
}

int EV_HLDM_CheckTracer(int idx, float *vecSrc, float *end, float *forward, float *right, int iBulletType, int iTracerFreq, int *tracerCount)
{
	int tracer = 0;
	int i;

	if (iTracerFreq != 0 && ((*tracerCount)++ % iTracerFreq) == 0)
	{
		Vector vecTracerSrc;

		if (EV_IsPlayer(idx))
		{
			Vector offset(0, 0, -4);

			// adjust tracer position for player
			for (i = 0; i < 3; i++)
			{
				vecTracerSrc[i] = vecSrc[i] + offset[i] + right[i] * 2 + forward[i] * 16;
			}
		}
		else
		{
			VectorCopy(vecSrc, vecTracerSrc);
		}

		if (iTracerFreq != 1) // guns that always trace also always decal
			tracer = 1;

		switch (iBulletType)
		{
		case BULLET_PLAYER_MP5:
		case BULLET_MONSTER_MP5:
		case BULLET_MONSTER_9MM:
		case BULLET_MONSTER_12MM:
		default:
			EV_CreateTracer(vecTracerSrc, end);
			break;
		}
	}

	return tracer;
}

/*
================
FireBullets

Go to the trouble of combining multiple pellets into a single damage call.
================
*/
void EV_HLDM_FireBullets(int idx, float *forward, float *right, float *up, int cShots, float *vecSrc, float *vecDirShooting, float flDistance, int iBulletType, int iTracerFreq, int *tracerCount, float flSpreadX, float flSpreadY)
{
	WEAPON *pActive = CHudAmmo::Get()->GetActiveWeapon();
	// Update our crosshair
	if ( pActive )
	{
		// If not active, return default settings.
		WeaponData wpnDat = GetWeaponSlotInfo( pActive->szName );
		CHudZPCrossHair::Get()->UpdateDynamicCrosshair( wpnDat.DynCrossHair.increase_spanner, wpnDat.DynCrossHair.increase_length, 0.15f );
	}

	int i;
	pmtrace_t tr;
	int iShot;
	int tracer;

	for (iShot = 1; iShot <= cShots; iShot++)
	{
		Vector vecDir, vecEnd;

		float x, y, z;
		//We randomize for the Shotgun.
		if (iBulletType == BULLET_PLAYER_BUCKSHOT || iBulletType == BULLET_PLAYER_DBARREL)
		{
			do
			{
				x = gEngfuncs.pfnRandomFloat(-0.5, 0.5) + gEngfuncs.pfnRandomFloat(-0.5, 0.5);
				y = gEngfuncs.pfnRandomFloat(-0.5, 0.5) + gEngfuncs.pfnRandomFloat(-0.5, 0.5);
				z = x * x + y * y;
			} while (z > 1);

			for (i = 0; i < 3; i++)
			{
				vecDir[i] = vecDirShooting[i] + x * flSpreadX * right[i] + y * flSpreadY * up[i];
				vecEnd[i] = vecSrc[i] + flDistance * vecDir[i];
			}
		} //But other guns already have their spread randomized in the synched spread.
		else
		{

			for (i = 0; i < 3; i++)
			{
				vecDir[i] = vecDirShooting[i] + flSpreadX * right[i] + flSpreadY * up[i];
				vecEnd[i] = vecSrc[i] + flDistance * vecDir[i];
			}
		}

		gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction(false, true);

		// Store off the old count
		gEngfuncs.pEventAPI->EV_PushPMStates();

		// Now add in all of the players.
		gEngfuncs.pEventAPI->EV_SetSolidPlayers(idx - 1);

		gEngfuncs.pEventAPI->EV_SetTraceHull(2);

		// JoshA: Changed from PM_STUDIO_BOX to PM_NORMAL in prediction code as otherwise if you hit an NPC or player's
		// bounding box but not one of their hitboxes, the shot won't hit on the server but it will
		// play a hit sound on the client and not make a decal (as if it hit the NPC/player).
		// We should mirror the way the server does the test here as close as possible.
		//
		// I initially thought I was just fixing some stupid Half-Life bug but no,
		// this is *the* root cause of all the ghost shot bad prediction bugs in Half-Life Deathmatch!
		//
		// Also... CStrike was always using PM_NORMAL for all of these so it didn't have the problem.
		gEngfuncs.pEventAPI->EV_PlayerTrace(vecSrc, vecEnd, PM_NORMAL, -1, &tr);

		tracer = EV_HLDM_CheckTracer(idx, vecSrc, tr.endpos, forward, right, iBulletType, iTracerFreq, tracerCount);

		// do damage, paint decals
		if (tr.fraction != 1.0)
		{
			switch (iBulletType)
			{
				default:
					EV_HLDM_PlayTextureSound(idx, &tr, vecSrc, vecEnd, iBulletType);
					EV_HLDM_DecalGunshot(&tr, forward, iBulletType);
				break;
			}
		}

		gEngfuncs.pEventAPI->EV_PopPMStates();
	}
}

//======================
//	    PISTOLS START
//======================
void EV_Fire1911(event_args_t *args)
{
	int idx;
	Vector origin;
	Vector angles;
	Vector velocity;
	int empty;

	Vector ShellVelocity;
	Vector ShellOrigin;
	int shell;
	Vector vecSrc, vecAiming;
	Vector up, right, forward;

	idx = args->entindex;
	VectorCopy(args->origin, origin);
	VectorCopy(args->angles, angles);
	VectorCopy(args->velocity, velocity);

	empty = args->bparam1;
	AngleVectors(angles, forward, right, up);

	shell = gEngfuncs.pEventAPI->EV_FindModelIndex("models/shell.mdl"); // brass shell

	if (EV_IsLocal(idx))
	{
		EV_MuzzleFlash();
		gEngfuncs.pEventAPI->EV_WeaponAnimation(empty ? ANIM_PISTOL_SHOOT_EMPTY : ANIM_PISTOL_SHOOT, 2);

		V_PunchAxis(0, -2.0);
	}

	Vector shellOffset;
	ParseVector( cl_shellejects_sig.GetString(), shellOffset );
	EV_GetDefaultShellInfo(args, origin, velocity, ShellVelocity, ShellOrigin, forward, right, up, shellOffset.x, shellOffset.y, shellOffset.z);
	EV_EjectBrass(ShellOrigin, ShellVelocity, angles[YAW], shell, TE_BOUNCE_SHELL);

	gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, "weapons/1911/fire.wav", gEngfuncs.pfnRandomFloat(0.92, 1.0), ATTN_NORM, 0, 98 + gEngfuncs.pfnRandomLong(0, 3));

	EV_GetGunPosition(args, vecSrc, origin);

	VectorCopy(forward, vecAiming);

	EV_HLDM_FireBullets(idx, forward, right, up, 1, vecSrc, vecAiming, 8192, BULLET_PLAYER_1911, 0, NULL, args->fparam1, args->fparam2);
}

void EV_FireSig(event_args_t *args)
{
	int idx;
	Vector origin;
	Vector angles;
	Vector velocity;
	int empty;

	Vector ShellVelocity;
	Vector ShellOrigin;
	int shell;
	Vector vecSrc, vecAiming;
	Vector up, right, forward;

	idx = args->entindex;
	VectorCopy(args->origin, origin);
	VectorCopy(args->angles, angles);
	VectorCopy(args->velocity, velocity);

	empty = args->bparam1;
	AngleVectors(angles, forward, right, up);

	shell = gEngfuncs.pEventAPI->EV_FindModelIndex("models/shell.mdl"); // brass shell

	if (EV_IsLocal(idx))
	{
		EV_MuzzleFlash();
		gEngfuncs.pEventAPI->EV_WeaponAnimation(empty ? ANIM_SIG_SHOOT_EMPTY : ANIM_SIG_SHOOT1 + gEngfuncs.pfnRandomLong(0, 2), 2);

		V_PunchAxis(0, -2.0);
	}

	Vector shellOffset;
	ParseVector( cl_shellejects_sig.GetString(), shellOffset );
	EV_GetDefaultShellInfo(args, origin, velocity, ShellVelocity, ShellOrigin, forward, right, up, shellOffset.x, shellOffset.y, shellOffset.z);
	EV_EjectBrass(ShellOrigin, ShellVelocity, angles[YAW], shell, TE_BOUNCE_SHELL);

	gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, "weapons/sig/fire.wav", gEngfuncs.pfnRandomFloat(0.92, 1.0), ATTN_NORM, 0, 98 + gEngfuncs.pfnRandomLong(0, 3));

	EV_GetGunPosition(args, vecSrc, origin);

	VectorCopy(forward, vecAiming);

	EV_HLDM_FireBullets(idx, forward, right, up, 1, vecSrc, vecAiming, 8192, BULLET_PLAYER_SIG, 0, NULL, args->fparam1, args->fparam2);
}

void EV_FireFAFO(event_args_t *args)
{
	int idx;
	Vector origin;
	Vector angles;
	Vector velocity;

	Vector ShellVelocity;
	Vector ShellOrigin;
	int shell;
	Vector vecSrc, vecAiming;
	Vector up, right, forward;

	idx = args->entindex;
	VectorCopy(args->origin, origin);
	VectorCopy(args->angles, angles);
	VectorCopy(args->velocity, velocity);

	AngleVectors(angles, forward, right, up);

	shell = gEngfuncs.pEventAPI->EV_FindModelIndex("models/shell.mdl"); // brass shell

	if (EV_IsLocal(idx))
	{
		EV_MuzzleFlash();
		gEngfuncs.pEventAPI->EV_WeaponAnimation(ANIM_PISTOL_SHOOT, 2);

		V_PunchAxis(0, -2.0);
	}

	Vector shellOffset;
	ParseVector( cl_shellejects_sig.GetString(), shellOffset );
	EV_GetDefaultShellInfo(args, origin, velocity, ShellVelocity, ShellOrigin, forward, right, up, shellOffset.x, shellOffset.y, shellOffset.z);
	EV_EjectBrass(ShellOrigin, ShellVelocity, angles[YAW], shell, TE_BOUNCE_SHELL);

	gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, "weapons/sig/fire.wav", gEngfuncs.pfnRandomFloat(0.92, 1.0), ATTN_NORM, 0, 98 + gEngfuncs.pfnRandomLong(0, 3));

	EV_GetGunPosition(args, vecSrc, origin);

	VectorCopy(forward, vecAiming);

	EV_HLDM_FireBullets(idx, forward, right, up, 1, vecSrc, vecAiming, 8192, BULLET_PLAYER_FAFO, 0, NULL, args->fparam1, args->fparam2);
}

void EV_FireCZ75(event_args_t *args)
{
	int idx;
	Vector origin;
	Vector angles;
	Vector velocity;
	int empty;

	Vector ShellVelocity;
	Vector ShellOrigin;
	int shell;
	Vector vecSrc, vecAiming;
	Vector up, right, forward;

	idx = args->entindex;
	VectorCopy(args->origin, origin);
	VectorCopy(args->angles, angles);
	VectorCopy(args->velocity, velocity);

	empty = args->bparam1;
	AngleVectors(angles, forward, right, up);

	shell = gEngfuncs.pEventAPI->EV_FindModelIndex("models/shell.mdl"); // brass shell

	if (EV_IsLocal(idx))
	{
		EV_MuzzleFlash();
		gEngfuncs.pEventAPI->EV_WeaponAnimation(empty ? ANIM_PISTOL_SHOOT_EMPTY : ANIM_PISTOL_SHOOT, 2);

		V_PunchAxis(0, -2.0);
	}
	
	Vector shellOffset;
	ParseVector( cl_shellejects_cz75.GetString(), shellOffset );
	EV_GetDefaultShellInfo(args, origin, velocity, ShellVelocity, ShellOrigin, forward, right, up, shellOffset.x, shellOffset.y, shellOffset.z);
	EV_EjectBrass(ShellOrigin, ShellVelocity, angles[YAW], shell, TE_BOUNCE_SHELL);

	gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, "weapons/cz75/fire.wav", gEngfuncs.pfnRandomFloat(0.92, 1.0), ATTN_NORM, 0, 98 + gEngfuncs.pfnRandomLong(0, 3));

	EV_GetGunPosition(args, vecSrc, origin);

	VectorCopy(forward, vecAiming);

	EV_HLDM_FireBullets(idx, forward, right, up, 1, vecSrc, vecAiming, 8192, BULLET_PLAYER_CZ75, 0, NULL, args->fparam1, args->fparam2);
}

void EV_FirePPK(event_args_t *args)
{
	int idx;
	Vector origin;
	Vector angles;
	Vector velocity;
	int empty;

	Vector ShellVelocity;
	Vector ShellOrigin;
	int shell;
	Vector vecSrc, vecAiming;
	Vector up, right, forward;

	idx = args->entindex;
	VectorCopy(args->origin, origin);
	VectorCopy(args->angles, angles);
	VectorCopy(args->velocity, velocity);

	empty = args->bparam1;
	AngleVectors(angles, forward, right, up);

	shell = gEngfuncs.pEventAPI->EV_FindModelIndex("models/shell.mdl"); // brass shell

	if (EV_IsLocal(idx))
	{
		EV_MuzzleFlash();
		gEngfuncs.pEventAPI->EV_WeaponAnimation(empty ? ANIM_PISTOL_SHOOT_EMPTY : ANIM_PISTOL_SHOOT, 2);

		V_PunchAxis(0, -2.0);
	}
	
	Vector shellOffset;
	ParseVector( cl_shellejects_ppk.GetString(), shellOffset );
	EV_GetDefaultShellInfo(args, origin, velocity, ShellVelocity, ShellOrigin, forward, right, up, shellOffset.x, shellOffset.y, shellOffset.z);
	EV_EjectBrass(ShellOrigin, ShellVelocity, angles[YAW], shell, TE_BOUNCE_SHELL);

	gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, "weapons/ppk/fire.wav", gEngfuncs.pfnRandomFloat(0.92, 1.0), ATTN_NORM, 0, 98 + gEngfuncs.pfnRandomLong(0, 3));

	EV_GetGunPosition(args, vecSrc, origin);

	VectorCopy(forward, vecAiming);

	EV_HLDM_FireBullets(idx, forward, right, up, 1, vecSrc, vecAiming, 8192, BULLET_PLAYER_PPK, 0, NULL, args->fparam1, args->fparam2);
}

void EV_FireGlock(event_args_t *args)
{
	int idx;
	Vector origin;
	Vector angles;
	Vector velocity;
	int empty;

	Vector ShellVelocity;
	Vector ShellOrigin;
	int shell;
	Vector vecSrc, vecAiming;
	Vector vecSpread;
	Vector up, right, forward;

	idx = args->entindex;
	VectorCopy(args->origin, origin);
	VectorCopy(args->angles, angles);
	VectorCopy(args->velocity, velocity);

	empty = args->bparam1;
	AngleVectors(angles, forward, right, up);

	shell = gEngfuncs.pEventAPI->EV_FindModelIndex("models/shell.mdl"); // brass shell

	if (EV_IsLocal(idx))
	{
		// Add muzzle flash to current weapon model
		EV_MuzzleFlash();
		gEngfuncs.pEventAPI->EV_WeaponAnimation(empty ? ANIM_PISTOL_SHOOT_EMPTY : ANIM_PISTOL_SHOOT, 2);

		V_PunchAxis(0, -2.0);
	}

	EV_GetDefaultShellInfo(args, origin, velocity, ShellVelocity, ShellOrigin, forward, right, up, 20, -12, 4);

	EV_EjectBrass(ShellOrigin, ShellVelocity, angles[YAW], shell, TE_BOUNCE_SHELL);

	gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, "weapons/glock17/fire.wav", gEngfuncs.pfnRandomFloat(0.92, 1.0), ATTN_NORM, 0, 98 + gEngfuncs.pfnRandomLong(0, 3));

	EV_GetGunPosition(args, vecSrc, origin);

	VectorCopy(forward, vecAiming);

	EV_HLDM_FireBullets(idx, forward, right, up, 1, vecSrc, vecAiming, 8192, BULLET_PLAYER_GLOCK, 0, NULL, args->fparam1, args->fparam2);
}
//======================
//	   GLOCK END
//======================

//======================
//	  SHOTGUN START
//======================
void EV_FireShotGunDouble(event_args_t *args)
{
}

void EV_ShotgunPump(event_args_t *args)
{
	Vector origin;
	Vector angles;
	Vector velocity;
	Vector up, right, forward;

	VectorCopy(args->origin, origin);
	VectorCopy(args->angles, angles);
	VectorCopy(args->velocity, velocity);

	AngleVectors(angles, forward, right, up);

	Vector ShellVelocity;
	Vector ShellOrigin;
	int shell = gEngfuncs.pEventAPI->EV_FindModelIndex("models/shotgunshell.mdl"); // brass shell

	Vector shellOffset;
	ParseVector( cl_shellejects_shotgun.GetString(), shellOffset );
	EV_GetDefaultShellInfo(args, origin, velocity, ShellVelocity, ShellOrigin, forward, right, up, shellOffset.x, shellOffset.y, shellOffset.z);
	EV_EjectBrass(ShellOrigin, ShellVelocity, angles[YAW], shell, TE_BOUNCE_SHOTSHELL);
}

void EV_FireShotGunSingle(event_args_t *args)
{
	int idx;
	Vector origin;
	Vector angles;
	Vector velocity;

	Vector vecSrc, vecAiming;
	Vector vecSpread;
	Vector up, right, forward;
	float flSpread = 0.01;

	idx = args->entindex;
	VectorCopy(args->origin, origin);
	VectorCopy(args->angles, angles);
	VectorCopy(args->velocity, velocity);

	AngleVectors(angles, forward, right, up);

	if (EV_IsLocal(idx))
	{
		// Add muzzle flash to current weapon model
		EV_MuzzleFlash();
		gEngfuncs.pEventAPI->EV_WeaponAnimation(ANIM_SHOTGUN_FIRE, 2);

		V_PunchAxis(0, -5.0);
	}

	gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, "weapons/shotgun/fire.wav", gEngfuncs.pfnRandomFloat(0.95, 1.0), ATTN_NORM, 0, 93 + gEngfuncs.pfnRandomLong(0, 0x1f));

	EV_GetGunPosition(args, vecSrc, origin);
	VectorCopy(forward, vecAiming);

	WeaponData data = GetWeaponSlotInfo( WEAPON_SHOTGUN );

	EV_HLDM_FireBullets(idx, forward, right, up, data.Bullets, vecSrc, vecAiming, 2048, BULLET_PLAYER_BUCKSHOT, 0, NULL, data.WeaponSpread[0], data.WeaponSpread[0]);
}

void EV_FireDBarrel(event_args_t *args)
{
	int idx;
	Vector origin;
	Vector angles;
	Vector velocity;

	Vector vecSrc, vecAiming;
	Vector vecSpread;
	Vector up, right, forward;
	float flSpread = 0.01;

	idx = args->entindex;
	VectorCopy(args->origin, origin);
	VectorCopy(args->angles, angles);
	VectorCopy(args->velocity, velocity);

	AngleVectors(angles, forward, right, up);

	if (EV_IsLocal(idx))
	{
		// Add muzzle flash to current weapon model
		EV_MuzzleFlash();
		gEngfuncs.pEventAPI->EV_WeaponAnimation( ANIM_DBARREL_FIRE1 + gEngfuncs.pfnRandomLong(0, 2), 2 );

		V_PunchAxis(0, -5.0);
	}

	gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, "weapons/doublebarrel/fire.wav", gEngfuncs.pfnRandomFloat(0.95, 1.0), ATTN_NORM, 0, 93 + gEngfuncs.pfnRandomLong(0, 0x1f));

	EV_GetGunPosition(args, vecSrc, origin);
	VectorCopy(forward, vecAiming);

	WeaponData data = GetWeaponSlotInfo( WEAPON_DOUBLEBARREL );

	EV_HLDM_FireBullets(idx, forward, right, up, data.Bullets, vecSrc, vecAiming, 2048, BULLET_PLAYER_DBARREL, 0, NULL, data.WeaponSpread[0], data.WeaponSpread[0]);
}

void EV_DBarrelReload(event_args_t *args)
{
	Vector origin;
	Vector angles;
	Vector velocity;
	Vector up, right, forward;

	VectorCopy(args->origin, origin);
	VectorCopy(args->angles, angles);
	VectorCopy(args->velocity, velocity);

	AngleVectors(angles, forward, right, up);

	Vector ShellVelocity;
	Vector ShellOrigin;
	int shell = gEngfuncs.pEventAPI->EV_FindModelIndex("models/shotgunshell.mdl"); // brass shell

	// Shell 1
	Vector shellOffset;
	ParseVector( cl_shellejects_dbarrel1.GetString(), shellOffset );
	EV_GetDefaultShellInfo(args, origin, velocity, ShellVelocity, ShellOrigin, forward, right, up, shellOffset.x, shellOffset.y, shellOffset.z);
	EV_EjectBrass(ShellOrigin, ShellVelocity, angles[YAW], shell, TE_BOUNCE_SHOTSHELL);

	if ( args->iparam1 == 1 )
		return; // only eject one shell

	// Shell 2
	ParseVector( cl_shellejects_dbarrel2.GetString(), shellOffset );
	EV_GetDefaultShellInfo(args, origin, velocity, ShellVelocity, ShellOrigin, forward, right, up, shellOffset.x, shellOffset.y, shellOffset.z);
	EV_EjectBrass(ShellOrigin, ShellVelocity, angles[YAW], shell, TE_BOUNCE_SHOTSHELL);
}
//======================
//	   SHOTGUN END
//======================

//======================
//	    SKS START
//======================
void EV_FireSKS(event_args_t *args)
{
	int idx;
	Vector origin;
	Vector angles;
	Vector velocity;

	Vector ShellVelocity;
	Vector ShellOrigin;
	int shell;
	Vector vecSrc, vecAiming;
	Vector up, right, forward;
	float flSpread = 0.01;

	idx = args->entindex;
	VectorCopy(args->origin, origin);
	VectorCopy(args->angles, angles);
	VectorCopy(args->velocity, velocity);

	int clipsize = args->iparam1;

	AngleVectors(angles, forward, right, up);

	shell = gEngfuncs.pEventAPI->EV_FindModelIndex("models/shell_rifle.mdl"); // brass shell

	if (EV_IsLocal(idx))
	{
		// Add muzzle flash to current weapon model
		EV_MuzzleFlash();
		if ( clipsize > 0 )
			gEngfuncs.pEventAPI->EV_WeaponAnimation(ANIM_SKS_SHOOT01 + gEngfuncs.pfnRandomLong(0, 2), 2);
		else
			gEngfuncs.pEventAPI->EV_WeaponAnimation(ANIM_SKS_SHOOT_EMPTY, 2);

		V_PunchAxis(0, gEngfuncs.pfnRandomFloat(-2, 2));
	}

	Vector shellOffset;
	ParseVector( cl_shellejects_sks.GetString(), shellOffset );
	EV_GetDefaultShellInfo(args, origin, velocity, ShellVelocity, ShellOrigin, forward, right, up, shellOffset.x, shellOffset.y, shellOffset.z);
	EV_EjectBrass(ShellOrigin, ShellVelocity, angles[YAW], shell, TE_BOUNCE_SHELL);

	gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, "weapons/sks/fire.wav", 1, ATTN_NORM, 0, 94 + gEngfuncs.pfnRandomLong(0, 0xf));

	EV_GetGunPosition(args, vecSrc, origin);
	VectorCopy(forward, vecAiming);

	// Array overflow fix: Use increased tracerCount array and wrap indexes in case if amount of entities is set very high, with hope that they will not overlap after wrap.
	// If they do, well ... it will not hurt much.
	EV_HLDM_FireBullets(idx, forward, right, up, 1, vecSrc, vecAiming, 8192, BULLET_PLAYER_SKS, 2, &tracerCount[idx % ARRAYSIZE(tracerCount) - 1], args->fparam1, args->fparam2);
}

//======================
//	    M16 START
//======================
void EV_FireM16(event_args_t *args)
{
	int idx;
	Vector origin;
	Vector angles;
	Vector velocity;

	Vector ShellVelocity;
	Vector ShellOrigin;
	int shell;
	Vector vecSrc, vecAiming;
	Vector up, right, forward;
	float flSpread = 0.01;

	idx = args->entindex;
	VectorCopy(args->origin, origin);
	VectorCopy(args->angles, angles);
	VectorCopy(args->velocity, velocity);

	AngleVectors(angles, forward, right, up);

	shell = gEngfuncs.pEventAPI->EV_FindModelIndex("models/shell_rifle.mdl"); // brass shell

	if (EV_IsLocal(idx))
	{
		// Add muzzle flash to current weapon model
		EV_MuzzleFlash();
		gEngfuncs.pEventAPI->EV_WeaponAnimation(ANIM_AR556_FIRE1 + gEngfuncs.pfnRandomLong(0, 2), 2);

		V_PunchAxis(0, gEngfuncs.pfnRandomFloat(-2, 2));
	}

	Vector shellOffset;
	ParseVector( cl_shellejects_m16.GetString(), shellOffset );
	EV_GetDefaultShellInfo(args, origin, velocity, ShellVelocity, ShellOrigin, forward, right, up, shellOffset.x, shellOffset.y, shellOffset.z);
	EV_EjectBrass(ShellOrigin, ShellVelocity, angles[YAW], shell, TE_BOUNCE_SHELL);

	switch (gEngfuncs.pfnRandomLong(0, 1))
	{
	case 0:
		gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, "weapons/556ar/fire1.wav", 1, ATTN_NORM, 0, 94 + gEngfuncs.pfnRandomLong(0, 0xf));
		break;
	case 1:
		gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, "weapons/556ar/fire2.wav", 1, ATTN_NORM, 0, 94 + gEngfuncs.pfnRandomLong(0, 0xf));
		break;
	}

	EV_GetGunPosition(args, vecSrc, origin);
	VectorCopy(forward, vecAiming);

	// Array overflow fix: Use increased tracerCount array and wrap indexes in case if amount of entities is set very high, with hope that they will not overlap after wrap.
	// If they do, well ... it will not hurt much.
	EV_HLDM_FireBullets(idx, forward, right, up, 1, vecSrc, vecAiming, 8192, BULLET_PLAYER_MP5, 2, &tracerCount[idx % ARRAYSIZE(tracerCount) - 1], args->fparam1, args->fparam2);
}

//======================
//	    MP5 START
//======================
void EV_FireMP5(event_args_t *args)
{
	int idx;
	Vector origin;
	Vector angles;
	Vector velocity;

	Vector ShellVelocity;
	Vector ShellOrigin;
	int shell;
	Vector vecSrc, vecAiming;
	Vector up, right, forward;
	float flSpread = 0.01;

	idx = args->entindex;
	VectorCopy(args->origin, origin);
	VectorCopy(args->angles, angles);
	VectorCopy(args->velocity, velocity);

	AngleVectors(angles, forward, right, up);

	shell = gEngfuncs.pEventAPI->EV_FindModelIndex("models/shell.mdl"); // brass shell

	if (EV_IsLocal(idx))
	{
		// Add muzzle flash to current weapon model
		EV_MuzzleFlash();
		gEngfuncs.pEventAPI->EV_WeaponAnimation(ANIM_MP5_FIRE1 + gEngfuncs.pfnRandomLong(0, 2), 2);

		V_PunchAxis(0, gEngfuncs.pfnRandomFloat(-2, 2));
	}
	
	Vector shellOffset;
	ParseVector( cl_shellejects_mp5.GetString(), shellOffset );
	EV_GetDefaultShellInfo(args, origin, velocity, ShellVelocity, ShellOrigin, forward, right, up, shellOffset.x, shellOffset.y, shellOffset.z);
	EV_EjectBrass(ShellOrigin, ShellVelocity, angles[YAW], shell, TE_BOUNCE_SHELL);

	gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, "weapons/mp5/fire.wav", 1, ATTN_NORM, 0, 94 + gEngfuncs.pfnRandomLong(0, 0xf));

	EV_GetGunPosition(args, vecSrc, origin);
	VectorCopy(forward, vecAiming);

	// Array overflow fix: Use increased tracerCount array and wrap indexes in case if amount of entities is set very high, with hope that they will not overlap after wrap.
	// If they do, well ... it will not hurt much.
	EV_HLDM_FireBullets(idx, forward, right, up, 1, vecSrc, vecAiming, 8192, BULLET_PLAYER_MP5, 2, &tracerCount[idx % ARRAYSIZE(tracerCount) - 1], args->fparam1, args->fparam2);
}

// We only predict the animation and sound
// The grenade is still launched from the server.
void EV_FireMP52(event_args_t *args)
{
#if 0
	int idx;
	Vector origin;

	idx = args->entindex;
	VectorCopy(args->origin, origin);

	if (EV_IsLocal(idx))
	{
		gEngfuncs.pEventAPI->EV_WeaponAnimation(MP5_LAUNCH, 2);
		V_PunchAxis(0, -10);
	}

	switch (gEngfuncs.pfnRandomLong(0, 1))
	{
	case 0:
		gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, "weapons/glauncher.wav", 1, ATTN_NORM, 0, 94 + gEngfuncs.pfnRandomLong(0, 0xf));
		break;
	case 1:
		gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, "weapons/glauncher2.wav", 1, ATTN_NORM, 0, 94 + gEngfuncs.pfnRandomLong(0, 0xf));
		break;
	}
#endif
}
//======================
//		 MP5 END
//======================

//======================
//	   PHYTON START
//	     ( .357 )
//======================
void EV_FirePython(event_args_t *args)
{
	int idx;
	Vector origin;
	Vector angles;
	Vector velocity;

	Vector vecSrc, vecAiming;
	Vector up, right, forward;
	float flSpread = 0.01;

	idx = args->entindex;
	VectorCopy(args->origin, origin);
	VectorCopy(args->angles, angles);
	VectorCopy(args->velocity, velocity);

	AngleVectors(angles, forward, right, up);

	if (EV_IsLocal(idx))
	{
		// Python uses different body in multiplayer versus single player
		int multiplayer = gEngfuncs.GetMaxClients() == 1 ? 0 : 1;

		// Add muzzle flash to current weapon model
		EV_MuzzleFlash();
		gEngfuncs.pEventAPI->EV_WeaponAnimation(ANIM_357_FIRE1, multiplayer ? 1 : 0);

		V_PunchAxis(0, -10.0);
	}

	switch (gEngfuncs.pfnRandomLong(0, 1))
	{
	case 0:
		gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, "weapons/revolver/fire1.wav", gEngfuncs.pfnRandomFloat(0.8, 0.9), ATTN_NORM, 0, PITCH_NORM);
		break;
	case 1:
		gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, "weapons/revolver/fire2.wav", gEngfuncs.pfnRandomFloat(0.8, 0.9), ATTN_NORM, 0, PITCH_NORM);
		break;
	}

	EV_GetGunPosition(args, vecSrc, origin);

	VectorCopy(forward, vecAiming);

	EV_HLDM_FireBullets(idx, forward, right, up, 1, vecSrc, vecAiming, 8192, BULLET_PLAYER_357, 0, NULL, args->fparam1, args->fparam2);
}

void EV_PythonShells( event_args_t *args )
{
	Vector origin;
	Vector angles;
	Vector velocity;
	Vector up, right, forward;

	VectorCopy(args->origin, origin);
	VectorCopy(args->angles, angles);
	VectorCopy(args->velocity, velocity);

	AngleVectors(angles, forward, right, up);

	Vector ShellVelocity;
	Vector ShellOrigin;
	int shell = gEngfuncs.pEventAPI->EV_FindModelIndex("models/shell_357.mdl"); // brass shell

	Vector shellOffset;
	ParseVector( cl_shellejects_357shell.GetString(), shellOffset );

	// Eject 6 shells with some randomization
	for ( int i = 0; i < 6; i++ )
	{
		Vector temp = shellOffset + Vector( gEngfuncs.pfnRandomFloat( -5.0, 5.0 ), gEngfuncs.pfnRandomFloat( -5.0, 5.0 ), gEngfuncs.pfnRandomFloat( -5.0, 5.0 ) );
		EV_GetDefaultShellInfo( args, origin, velocity, ShellVelocity, ShellOrigin, forward, right, up, temp.x, temp.y, temp.z );
		EV_EjectBrass( ShellOrigin, Vector( 0, 0, -20 ), angles[YAW], shell, TE_BOUNCE_SHELL );
	}
}

//======================
//	    PHYTON END
//	     ( .357 )
//======================

//======================
//	    NAILGUN
//======================
void EV_FireNailgun(event_args_t *args)
{
	WEAPON *pActive = CHudAmmo::Get()->GetActiveWeapon();
	// Update our crosshair
	if (pActive)
	{
		// If not active, return default settings.
		WeaponData wpnDat = GetWeaponSlotInfo(pActive->szName);
		CHudZPCrossHair::Get()->UpdateDynamicCrosshair(wpnDat.DynCrossHair.increase_spanner, wpnDat.DynCrossHair.increase_length, 0.15f);
	}
}
//======================
//	    EGON END
//======================

//======================
//	   TRIPMINE START
//======================
enum tripmine_e
{
	TRIPMINE_IDLE1 = 0,
	TRIPMINE_IDLE2,
	TRIPMINE_ARM1,
	TRIPMINE_ARM2,
	TRIPMINE_FIDGET,
	TRIPMINE_HOLSTER,
	TRIPMINE_DRAW,
	TRIPMINE_WORLD,
	TRIPMINE_GROUND,
};

//We only check if it's possible to put a trip mine
//and if it is, then we play the animation. Server still places it.
void EV_TripmineFire(event_args_t *args)
{
	int idx;
	Vector vecSrc, angles, view_ofs, forward;
	pmtrace_t tr;

	idx = args->entindex;
	VectorCopy(args->origin, vecSrc);
	VectorCopy(args->angles, angles);

	AngleVectors(angles, forward, NULL, NULL);

	if (!EV_IsLocal(idx))
		return;

	// Grab predicted result for local player
	gEngfuncs.pEventAPI->EV_LocalPlayerViewheight(view_ofs);

	vecSrc = vecSrc + view_ofs;

	// Store off the old count
	gEngfuncs.pEventAPI->EV_PushPMStates();

	// Now add in all of the players.
	gEngfuncs.pEventAPI->EV_SetSolidPlayers(idx - 1);
	gEngfuncs.pEventAPI->EV_SetTraceHull(2);
	gEngfuncs.pEventAPI->EV_PlayerTrace(vecSrc, vecSrc + forward * 128, PM_NORMAL, -1, &tr);

	//Hit something solid
	if (tr.fraction < 1.0)
		gEngfuncs.pEventAPI->EV_WeaponAnimation(TRIPMINE_DRAW, 0);

	gEngfuncs.pEventAPI->EV_PopPMStates();
}
//======================
//	   TRIPMINE END
//======================

//======================
//	DECALRESET START
//======================
void EV_DecalReset(event_args_t *args)
{
	// Purge all decals
	for ( int x = 0; x < (int)CVAR_GET_FLOAT( "r_decals" ); x++ )
        gEngfuncs.pEfxAPI->R_DecalRemoveAll( x );
}
//======================
//	DECALRESET END
//======================

#define SND_CHANGE_PITCH (1 << 7) // duplicated in protocol.h change sound pitch

void EV_TrainPitchAdjust(event_args_t *args)
{
	int idx;
	Vector origin;

	unsigned short us_params;
	int noise;
	float m_flVolume;
	int pitch;
	int stop;

	char sz[256];

	idx = args->entindex;

	VectorCopy(args->origin, origin);

	us_params = (unsigned short)args->iparam1;
	stop = args->bparam1;

	m_flVolume = (float)(us_params & 0x003f) / 40.0;
	noise = (int)(((us_params) >> 12) & 0x0007);
	pitch = (int)(10.0 * (float)((us_params >> 6) & 0x003f));

	switch (noise)
	{
	case 1:
		V_strcpy_safe(sz, "plats/ttrain1.wav");
		break;
	case 2:
		V_strcpy_safe(sz, "plats/ttrain2.wav");
		break;
	case 3:
		V_strcpy_safe(sz, "plats/ttrain3.wav");
		break;
	case 4:
		V_strcpy_safe(sz, "plats/ttrain4.wav");
		break;
	case 5:
		V_strcpy_safe(sz, "plats/ttrain6.wav");
		break;
	case 6:
		V_strcpy_safe(sz, "plats/ttrain7.wav");
		break;
	default:
		// no sound
		V_strcpy_safe(sz, "");
		return;
	}

	if (stop)
	{
		gEngfuncs.pEventAPI->EV_StopSound(idx, CHAN_STATIC, sz);
	}
	else
	{
		gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_STATIC, sz, m_flVolume, ATTN_NORM, SND_CHANGE_PITCH, pitch);
	}
}

void EV_VehiclePitchAdjust(event_args_t *args)
{
	int idx;
	Vector origin;

	unsigned short us_params;
	int noise;
	float m_flVolume;
	int pitch;
	int stop;

	char sz[256];

	idx = args->entindex;

	VectorCopy(args->origin, origin);

	us_params = (unsigned short)args->iparam1;
	stop = args->bparam1;

	m_flVolume = (float)(us_params & 0x003f) / 40.0;
	noise = (int)(((us_params) >> 12) & 0x0007);
	pitch = (int)(10.0 * (float)((us_params >> 6) & 0x003f));

	switch (noise)
	{
	case 1:
		V_strcpy_safe(sz, "plats/vehicle1.wav");
		break;
	case 2:
		V_strcpy_safe(sz, "plats/vehicle2.wav");
		break;
	case 3:
		V_strcpy_safe(sz, "plats/vehicle3.wav");
		break;
	case 4:
		V_strcpy_safe(sz, "plats/vehicle4.wav");
		break;
	case 5:
		V_strcpy_safe(sz, "plats/vehicle6.wav");
		break;
	case 6:
		V_strcpy_safe(sz, "plats/vehicle7.wav");
		break;
	default:
		// no sound
		V_strcpy_safe(sz, "");
		return;
	}

	if (stop)
	{
		gEngfuncs.pEventAPI->EV_StopSound(idx, CHAN_STATIC, sz);
	}
	else
	{
		gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_STATIC, sz, m_flVolume, ATTN_NORM, SND_CHANGE_PITCH, pitch);
	}
}

int EV_TFC_IsAllyTeam(int iTeam1, int iTeam2)
{
	return 0;
}
