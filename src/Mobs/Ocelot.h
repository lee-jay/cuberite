
#pragma once

#include "PassiveMonster.h"





class cOcelot :
	public cPassiveMonster
{
	typedef cPassiveMonster super;
	
public:
	cOcelot(CreateMonsterInfo a_Info) :
		super("Ocelot", mtOcelot, "mob.cat.hitt", "mob.cat.hitt", 0.6, 0.8)
	{
	}

	CLASS_PROTODEF(cOcelot)
} ;




