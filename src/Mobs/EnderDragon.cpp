
#include "Globals.h"  // NOTE: MSVC stupidness requires this to be the same across all modules

#include "EnderDragon.h"





cEnderDragon::cEnderDragon(CreateMonsterInfo a_Info) :
	// TODO: Vanilla source says this, but is it right? Dragons fly, they don't stand
	super(a_Info, "EnderDragon", mtEnderDragon, "mob.enderdragon.hit", "mob.enderdragon.end", 16.0, 8.0)
{
}





void cEnderDragon::GetDrops(cItems & a_Drops, cEntity * a_Killer)
{
	return;
}




