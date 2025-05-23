
#pragma once

#include "Monster.h"





class cAggressiveMonster:
	public cMonster
{
	using Super = cMonster;

public:

	cAggressiveMonster(
		const AString & a_ConfigName,
		eEntityType a_MobType,
		const AString & a_SoundHurt,
		const AString & a_SoundDeath,
		const AString & a_SoundAmbient,
		float a_Width,
		float a_Height
	);

	virtual void Tick          (std::chrono::milliseconds a_Dt, cChunk & a_Chunk) override;
	virtual void InStateChasing(std::chrono::milliseconds a_Dt, cChunk & a_Chunk) override;


	virtual void EventSeePlayer(cPlayer * a_Player, cChunk & a_Chunk) override;

	/**
	* Check if a monster of certain type is in sight
	*
	* @param a_mobtype the mob type to find
	* @param SightDistance max distance to check
	*
	* @return pointer to the mob found
	*/
	cMonster * GetMonsterOfTypeInSight(eEntityType a_mobtype, unsigned int SightDistance=16);

	/** Try to perform attack
	returns true if attack was deemed successful (hit player, fired projectile, creeper exploded, etc.) even if it didn't actually do damage
	return false if e.g. the mob is still in cooldown from a previous attack */
	virtual bool Attack(std::chrono::milliseconds a_Dt);
} ;
