
#pragma once

#include "Entity.h"




class cPlayer;
class cItem;






class cFallingBlock :
	public cEntity
{
	typedef cEntity super;
	
public:
	CLASS_PROTODEF(cFallingBlock)

	/// Creates a new falling block. a_BlockPosition is expected in world coords
	cFallingBlock(CreateEntityInfo a_Info, BLOCKTYPE a_BlockType, NIBBLETYPE a_BlockMeta);

	BLOCKTYPE  GetBlockType(void) const { return m_BlockType; }
	NIBBLETYPE GetBlockMeta(void) const { return m_BlockMeta; }
	
	// cEntity overrides:
	virtual void SpawnOn(cClientHandle & a_ClientHandle) override;
	virtual void Tick(float a_Dt, cChunk & a_Chunk) override;
	
private:
	BLOCKTYPE  m_BlockType;
	NIBBLETYPE m_BlockMeta;
} ;




