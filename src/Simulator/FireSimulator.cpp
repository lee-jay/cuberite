
#include "Globals.h"  // NOTE: MSVC stupidness requires this to be the same across all modules

#include "FireSimulator.h"
#include "../World.h"
#include "../BlockID.h"
#include "../Defines.h"
#include "../Chunk.h"
#include "Root.h"
#include "../Bindings/PluginManager.h"





// Easy switch for turning on debugging logging:
#if 0
	#define FLOG LOGD
#else
	#define FLOG(...)
#endif





#define MAX_CHANCE_REPLACE_FUEL 100000
#define MAX_CHANCE_FLAMMABILITY 100000





static const struct
{
	int x, y, z;
} gCrossCoords[] =
{
	{ 1, 0,  0},
	{-1, 0,  0},
	{ 0, 0,  1},
	{ 0, 0, -1},
} ;





static const struct
{
	int x, y, z;
} gNeighborCoords[] =
{
	{ 1,  0,  0},
	{-1,  0,  0},
	{ 0,  1,  0},
	{ 0, -1,  0},
	{ 0,  0,  1},
	{ 0,  0, -1},
} ;





////////////////////////////////////////////////////////////////////////////////
// cFireSimulator:

cFireSimulator::cFireSimulator(cWorld & a_World, cIniFile & a_IniFile) :
	cSimulator(a_World)
{
	// Read params from the ini file:
	m_BurnStepTimeFuel    = a_IniFile.GetValueSetI("FireSimulator", "BurnStepTimeFuel",     500);
	m_BurnStepTimeNonFuel = a_IniFile.GetValueSetI("FireSimulator", "BurnStepTimeNonfuel",  100);
	m_Flammability        = a_IniFile.GetValueSetI("FireSimulator", "Flammability",          50);
	m_ReplaceFuelChance   = a_IniFile.GetValueSetI("FireSimulator", "ReplaceFuelChance",  50000);
}





cFireSimulator::~cFireSimulator()
{
}





void cFireSimulator::SimulateChunk(float a_Dt, int a_ChunkX, int a_ChunkZ, cChunk * a_Chunk)
{
	cCoordWithIntList & Data = a_Chunk->GetFireSimulatorData();

	int NumMSecs = (int)a_Dt;
	for (cCoordWithIntList::iterator itr = Data.begin(); itr != Data.end();)
	{
		int x = itr->x;
		int y = itr->y;
		int z = itr->z;
		BLOCKTYPE BlockType;
		NIBBLETYPE BlockMeta;
		a_Chunk->GetBlockTypeMeta(x, y, z, BlockType, BlockMeta);

		if (!IsAllowedBlock(BlockType))
		{
			// The block is no longer eligible (not a fire block anymore; a player probably placed a block over the fire)
			FLOG("FS: Removing block {%d, %d, %d}",
				itr->x + a_ChunkX * cChunkDef::Width, itr->y, itr->z + a_ChunkZ * cChunkDef::Width
			);
			itr = Data.erase(itr);
			continue;
		}

		// Try to spread the fire:
		TrySpreadFire(a_Chunk, itr->x, itr->y, itr->z);

		BLOCKTYPE BlockBelow = E_BLOCK_AIR;
		if (y > 0)
		{
			BlockBelow = a_Chunk->GetBlock(x, y - 1, z);
			if (DoesBurnForever(BlockBelow))
			{
				continue;
			}
		}

		itr->Data -= NumMSecs;
		if (itr->Data >= 0)
		{
			// Not yet, wait for it longer
			++itr;
			continue;
		}
		
		// Burn out the fire one step by increasing the meta:
		/*
		FLOG("FS: Fire at {%d, %d, %d} is stepping",
			itr->x + a_ChunkX * cChunkDef::Width, itr->y, itr->z + a_ChunkZ * cChunkDef::Width
		);
		*/
		if (BlockMeta == 0x0f)
		{
			// The fire burnt out completely
			FLOG("FS: Fire at {%d, %d, %d} burnt out, removing the fire block",
				itr->x + a_ChunkX * cChunkDef::Width, itr->y, itr->z + a_ChunkZ * cChunkDef::Width
			);
			a_Chunk->SetBlock(itr->x, itr->y, itr->z, E_BLOCK_AIR, 0);
			RemoveFuelNeighbors(a_Chunk, itr->x, itr->y, itr->z);
			itr = Data.erase(itr);
			continue;
		}

		int NextCheck = GetBurnStepTime(a_Chunk, BlockBelow, x, y, z);
		if (NextCheck == 0)
		{
			/*
			A return value of zero means the fire block can't exist here
			The fire block's CanBeAt() doesn't check for this because fire can exist depending on its surroundings,
			and since we're checking that here, we might as well amalgamate all checks
			*/
			FLOG("FS: Removing block {%d, %d, %d}",
				itr->x + a_ChunkX * cChunkDef::Width, itr->y, itr->z + a_ChunkZ * cChunkDef::Width
				);
			a_Chunk->SetBlock(itr->x, itr->y, itr->z, E_BLOCK_AIR, 0);
			itr = Data.erase(itr);
			continue;
		}

		a_Chunk->SetMeta(x, y, z, BlockMeta + 1);
		x = x + a_Chunk->GetPosX() * cChunkDef::Width;
		z = z + a_Chunk->GetPosZ() * cChunkDef::Width;
		itr->Data = (NextCheck / ((m_World.IsWeatherWetAt(x, z) && (a_Chunk->GetHeight(x, z) == y)) ? 10 : 1)) + (m_World.GetTickRandomNumber(40) - 20);
	}  // for itr - Data[]
}





bool cFireSimulator::IsAllowedBlock(BLOCKTYPE a_BlockType)
{
	return (a_BlockType == E_BLOCK_FIRE);
}





bool cFireSimulator::IsFuel(BLOCKTYPE a_BlockType)
{
	switch (a_BlockType)
	{
		case E_BLOCK_PLANKS:
		case E_BLOCK_DOUBLE_WOODEN_SLAB:
		case E_BLOCK_WOODEN_SLAB:
		case E_BLOCK_WOODEN_STAIRS:
		case E_BLOCK_SPRUCE_WOOD_STAIRS:
		case E_BLOCK_BIRCH_WOOD_STAIRS:
		case E_BLOCK_JUNGLE_WOOD_STAIRS:
		case E_BLOCK_LEAVES:
		case E_BLOCK_NEW_LEAVES:
		case E_BLOCK_LOG:
		case E_BLOCK_NEW_LOG:
		case E_BLOCK_WOOL:
		case E_BLOCK_BOOKCASE:
		case E_BLOCK_FENCE:
		case E_BLOCK_SPRUCE_FENCE:
		case E_BLOCK_BIRCH_FENCE:
		case E_BLOCK_JUNGLE_FENCE:
		case E_BLOCK_DARK_OAK_FENCE:
		case E_BLOCK_ACACIA_FENCE:
		case E_BLOCK_FENCE_GATE:
		case E_BLOCK_SPRUCE_FENCE_GATE:
		case E_BLOCK_BIRCH_FENCE_GATE:
		case E_BLOCK_JUNGLE_FENCE_GATE:
		case E_BLOCK_DARK_OAK_FENCE_GATE:
		case E_BLOCK_ACACIA_FENCE_GATE:
		case E_BLOCK_TNT:
		case E_BLOCK_VINES:
		case E_BLOCK_HAY_BALE:
		case E_BLOCK_TALL_GRASS:
		case E_BLOCK_BIG_FLOWER:
		case E_BLOCK_DANDELION:
		case E_BLOCK_FLOWER:
		case E_BLOCK_CARPET:
		{
			return true;
		}
	}
	return false;
}





bool cFireSimulator::DoesBurnForever(BLOCKTYPE a_BlockType)
{
	return (a_BlockType == E_BLOCK_NETHERRACK);
}





void cFireSimulator::AddBlock(int a_BlockX, int a_BlockY, int a_BlockZ, cChunk * a_Chunk)
{
	if ((a_Chunk == NULL) || !a_Chunk->IsValid())
	{
		return;
	}
	
	int RelX = a_BlockX - a_Chunk->GetPosX() * cChunkDef::Width;
	int RelZ = a_BlockZ - a_Chunk->GetPosZ() * cChunkDef::Width;
	BLOCKTYPE BlockType = a_Chunk->GetBlock(RelX, a_BlockY, RelZ);
	if (!IsAllowedBlock(BlockType))
	{
		return;
	}
	
	// Check for duplicates:
	cFireSimulatorChunkData & ChunkData = a_Chunk->GetFireSimulatorData();
	for (cCoordWithIntList::iterator itr = ChunkData.begin(), end = ChunkData.end(); itr != end; ++itr)
	{
		if ((itr->x == RelX) && (itr->y == a_BlockY) && (itr->z == RelZ))
		{
			// Already present, skip adding
			return;
		}
	}  // for itr - ChunkData[]

	FLOG("FS: Adding block {%d, %d, %d}", a_BlockX, a_BlockY, a_BlockZ);
	ChunkData.push_back(cCoordWithInt(RelX, a_BlockY, RelZ, 100));
}





int cFireSimulator::GetBurnStepTime(cChunk * a_Chunk, BLOCKTYPE a_BlockBelow, int a_RelX, int a_RelY, int a_RelZ)
{
	if (IsFuel(a_BlockBelow))
	{
		return m_BurnStepTimeFuel;
	}
	
	for (size_t i = 0; i < ARRAYCOUNT(gCrossCoords); i++)
	{
		BLOCKTYPE  BlockType;
		NIBBLETYPE BlockMeta;
		if (a_Chunk->UnboundedRelGetBlock(a_RelX + gCrossCoords[i].x, a_RelY, a_RelZ + gCrossCoords[i].z, BlockType, BlockMeta))
		{
			if (IsFuel(BlockType))
			{
				return m_BurnStepTimeFuel;
			}
		}
	}  // for i - gCrossCoords[]

	if (!cBlockInfo::IsSolid(a_BlockBelow))
	{
		// Checked through everything, nothing was flammable
		// If block below isn't solid, we can't have fire, it would be a non-fueled fire
		// Return zero to tell our caller to kill us
		return 0;
	}
	return m_BurnStepTimeNonFuel;
}





void cFireSimulator::TrySpreadFire(cChunk * a_Chunk, int a_RelX, int a_RelY, int a_RelZ)
{
	/*
	if (m_World.GetTickRandomNumber(10000) > 100)
	{
		// Make the chance to spread 100x smaller
		return;
	}
	*/
	
	for (int x = a_RelX - 1; x <= a_RelX + 1; x++)
	{
		for (int z = a_RelZ - 1; z <= a_RelZ + 1; z++)
		{
			for (int y = a_RelY - 1; y <= a_RelY + 2; y++)  // flames spread up one more block than around
			{
				// No need to check the coords for equality with the parent block,
				// it cannot catch fire anyway (because it's not an air block)
				
				if (m_World.GetTickRandomNumber(MAX_CHANCE_FLAMMABILITY) > m_Flammability)
				{
					continue;
				}
				
				// Start the fire in the neighbor {x, y, z}
				/*
				FLOG("FS: Trying to start fire at {%d, %d, %d}.",
					x + a_Chunk->GetPosX() * cChunkDef::Width, y, z + a_Chunk->GetPosZ() * cChunkDef::Width
				);
				*/
				if (CanStartFireInBlock(a_Chunk, x, y, z))
				{
					int a_PosX = x + a_Chunk->GetPosX() * cChunkDef::Width;
					int a_PosZ = z + a_Chunk->GetPosZ() * cChunkDef::Width;
					
					if (cRoot::Get()->GetPluginManager()->CallHookBlockSpread(&m_World, a_PosX, y, a_PosZ, ssFireSpread))
					{
						return;
					}
					
					FLOG("FS: Starting new fire at {%d, %d, %d}.", a_PosX, y, a_PosZ);
					a_Chunk->UnboundedRelSetBlock(x, y, z, E_BLOCK_FIRE, 0);
				}
			}  // for y
		}  // for z
	}  // for x
}





void cFireSimulator::RemoveFuelNeighbors(cChunk * a_Chunk, int a_RelX, int a_RelY, int a_RelZ)
{
	for (size_t i = 0; i < ARRAYCOUNT(gNeighborCoords); i++)
	{
		BLOCKTYPE  BlockType;
		int X = a_RelX + gNeighborCoords[i].x;
		int Z = a_RelZ + gNeighborCoords[i].z;

		cChunkPtr Neighbour = a_Chunk->GetRelNeighborChunkAdjustCoords(X, Z);
		if (Neighbour == NULL)
		{
			continue;
		}
		BlockType = Neighbour->GetBlock(X, a_RelY + gNeighborCoords[i].y, Z);

		if (!IsFuel(BlockType))
		{
			continue;
		}

		int AbsX = (Neighbour->GetPosX() * cChunkDef::Width) + X;
		int Y = a_RelY + gNeighborCoords[i].y;
		int AbsZ = (Neighbour->GetPosZ() * cChunkDef::Width) + Z;

		if (BlockType == E_BLOCK_TNT)
		{
			m_World.SpawnPrimedTNT(AbsX, Y, AbsZ, 0);
			Neighbour->SetBlock(X, a_RelY + Y, Z, E_BLOCK_AIR, 0);
			return;
		}

		bool ShouldReplaceFuel = (m_World.GetTickRandomNumber(MAX_CHANCE_REPLACE_FUEL) < m_ReplaceFuelChance);
		if (ShouldReplaceFuel && !cRoot::Get()->GetPluginManager()->CallHookBlockSpread(&m_World, AbsX, Y, AbsZ, ssFireSpread))
		{
			Neighbour->SetBlock(X, Y, Z, E_BLOCK_FIRE, 0);
		}
		else
		{
			Neighbour->SetBlock(X, Y, Z, E_BLOCK_AIR, 0);
		}
	}  // for i - Coords[]
}





bool cFireSimulator::CanStartFireInBlock(cChunk * a_NearChunk, int a_RelX, int a_RelY, int a_RelZ)
{
	BLOCKTYPE  BlockType;
	NIBBLETYPE BlockMeta;
	if (!a_NearChunk->UnboundedRelGetBlock(a_RelX, a_RelY, a_RelZ, BlockType, BlockMeta))
	{
		// The chunk is not accessible
		return false;
	}
	
	if (BlockType != E_BLOCK_AIR)
	{
		// Only an air block can be replaced by a fire block
		return false;
	}
	
	for (size_t i = 0; i < ARRAYCOUNT(gNeighborCoords); i++)
	{
		if (!a_NearChunk->UnboundedRelGetBlock(a_RelX + gNeighborCoords[i].x, a_RelY + gNeighborCoords[i].y, a_RelZ + gNeighborCoords[i].z, BlockType, BlockMeta))
		{
			// Neighbor inaccessible, skip it while evaluating
			continue;
		}
		if (IsFuel(BlockType))
		{
			return true;
		}
	}  // for i - Coords[]
	return false;
}




