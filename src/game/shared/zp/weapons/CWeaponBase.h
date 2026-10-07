// ============== Copyright (c) 2025 Monochrome Games ============== \\

#ifndef SHARED_WEAPON_BASE_H
#define SHARED_WEAPON_BASE_H

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "player.h"
#include "zp/zp_shared_weapons.h"

#ifndef DECLARE_CLASS_SIMPLE
#define DECLARE_CLASS_SIMPLE( className, baseClassName ) typedef baseClassName BaseClass;
#endif

class CWeaponBase : public CBasePlayerWeapon
{
	DECLARE_CLASS_SIMPLE( CWeaponBase, CBasePlayerWeapon );

public:
	// Default, required for client.
	ZPWeaponID GetWeaponID() { return WEAPON_NONE; }

	virtual int ObjectCaps(void) { return BaseClass::ObjectCaps() | FCAP_IMPULSE_USE; }
	bool IsUseableItem() const override { return true; }
	virtual bool AllowAmmoSteal() const { return true; }

	int iItemSlot( void ) override;
	bool DoDeploy(const char *szViewModel, const char *szWeaponModel, int iAnim, const char *szAnimExt, int skiplocal = 0, int body = 0);
	int DefaultReload(int iAnim, float fDelay, int body = 0) override;

	void FallInit() override;

	void EXPORT OnWeaponTouch( CBaseEntity *pOther );
	void EXPORT OnWeaponThink();
	virtual void BounceSound( void );

	void DoDeployAnimation() override;
	void BeginHolster( CBasePlayerWeapon *pWeapon );
	virtual float DoHolsterAnimation() { return 0.0f; }
	void FinishHolster();
	void StopHolstering();

	// We don't use secondary attack in Zombie Panic! by default
	virtual void SecondaryAttack( void ) {}
	virtual void ItemPostFrame( void );

	/// <summary>
	/// Used to play audio, such as delayed reload sounds or other sounds that need to be played
	/// </summary>
	/// <param name=""></param>
	virtual void DoAudioFrame( void );

	virtual BOOL UseDecrement( void )
	{
#if defined(CLIENT_WEAPONS)
		return TRUE;
#else
		return FALSE;
#endif
	}

	// Unload the weapon, and grab it's ammo
	virtual void Unload();
	virtual int UnloadAmount() { return m_iClip; }
	virtual float DoWeaponUnload() { return 0.1f; }
	virtual bool CanUnloadWeapon() { return true; }
	void FinishUnloading();

	// Get the duration of an animation in seconds
	float GetAnimationTime( int iFrames, int iFrameRate )
	{
		if ( iFrameRate <= 0 )
			return 0.1f;
		return (float)iFrames / (float)iFrameRate;
	}

	virtual bool IsEmpty( void ) const { return ( m_iClip <= 0 ); }
	void AddWeaponSound( const char *szSoundFile, float volume = 1.0f, float attenuation = ATTN_NORM, float delay = 0.0f );
	void ClearWeaponSounds( void );
	virtual void Use(CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value);

	bool IsHolstering( void ) const { return m_bIsHolstering; }
	float GetHolsterTime( void ) const { return m_flHolsterTime; }

#if !defined( CLIENT_DLL )
	bool m_bHasDropped = false; // Have we dropped this weapon?
	int m_StuckChecks = 0; // How many times have we checked if we are stuck?
	void CheckIfStuckInWorld();
#endif
	float m_flNextBounceSound = 0.0f; // Cooldown to prevent impact sound from spamming

protected:
	void DefaultSpawn();
	bool CanAttack( float attack_time, float curtime, bool isPredicted );
	float GetWeaponTimerBase();
	float GetPlayerTimerBase();
	void EmitWeaponSound( const char *szSoundFile, int channel = CHAN_AUTO, float volume = 1.0f, float attenuation = ATTN_NORM, int flags = 0, int pitch = PITCH_NORM );

#if !defined( CLIENT_DLL )
	CBasePlayerWeapon *FindNextBestWeapon();
	void DestroyAndSwitchToNextBestWeapon();
#endif

	unsigned short m_nEventPrimary;
	unsigned short m_nEventSecondary;

	struct WeaponSoundData
	{
		const char *File; // Path to sound file
		float Volume; // Volume of sound
		float Attenuation; // Attenuation of sound
		float Delay; // Delay before playing sound
	};
	std::vector<WeaponSoundData> m_vecWeaponSoundData;

	float m_flHolsterTime = 0.0f; // Time when we holstered the weapon
	float m_flUnloadTime = 0.0f; // Time when we unloaded the weapon

private:
	bool m_bIsHolstering = false; // Are we holstering the weapon?
	bool m_bIsUnloading = false; // Are we unloading the weapon?
	CBasePlayerWeapon *m_pNewWeapon = nullptr; // The weapon to switch to after holstering
};

#endif
