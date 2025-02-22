#pragma once

#include "BlockHandler.h"
#include "../FastRandom.h"





class cBlockCocoaPodHandler final :
	public cBlockHandler
{
	using Super = cBlockHandler;

public:

	using Super::Super;

	static NIBBLETYPE BlockFaceToMeta(eBlockFace a_BlockFace)
	{
		switch (a_BlockFace)
		{
			case BLOCK_FACE_ZM: return 0;
			case BLOCK_FACE_XM: return 3;
			case BLOCK_FACE_XP: return 1;
			case BLOCK_FACE_ZP: return 2;
			case BLOCK_FACE_NONE:
			case BLOCK_FACE_YM:
			case BLOCK_FACE_YP:
			{
				break;
			}
		}
		UNREACHABLE("Unsupported block face");
	}

private:

	virtual bool CanBeAt(const cChunk & a_Chunk, const Vector3i a_Position, const NIBBLETYPE a_Meta) const override
	{
		// Check that we're attached to a jungle log block:
		eBlockFace BlockFace = MetaToBlockFace(a_Meta);
		auto LogPos = AddFaceDirection(a_Position, BlockFace, true);
		BLOCKTYPE BlockType;
		NIBBLETYPE BlockMeta;
		if (!a_Chunk.UnboundedRelGetBlock(LogPos, BlockType, BlockMeta))
		{
			// Don't pop if chunk not loaded.
			return true;
		}

		return ((BlockType == E_BLOCK_LOG) && ((BlockMeta & 0x03) == E_META_LOG_JUNGLE));
	}





	virtual void OnUpdate(
		cChunkInterface & a_ChunkInterface,
		cWorldInterface & a_WorldInterface,
		cBlockPluginInterface & a_PluginInterface,
		cChunk & a_Chunk,
		const Vector3i a_RelPos
	) const override
	{
		if (GetRandomProvider().RandBool(0.20))
		{
			Grow(a_Chunk, a_RelPos);
		}
	}





	virtual cItems ConvertToPickups(const NIBBLETYPE a_BlockMeta, const cItem * const a_Tool) const override
	{
		// If fully grown, give 3 items, otherwise just one:
		auto growState = a_BlockMeta >> 2;
		return cItem(E_ITEM_DYE, ((growState >= 2) ? 3 : 1), E_META_DYE_BROWN);
	}





	virtual int Grow(cChunk & a_Chunk, Vector3i a_RelPos, int a_NumStages = 1) const override
	{
		auto meta = a_Chunk.GetMeta(a_RelPos);
		auto typeMeta = meta & 0x03;
		auto growState = meta >> 2;

		if (growState >= 2)
		{
			return 0;
		}
		auto newState = std::min(growState + a_NumStages, 2);
		a_Chunk.SetMeta(a_RelPos, static_cast<NIBBLETYPE>(newState << 2 | typeMeta));
		return newState - growState;
	}





	static eBlockFace MetaToBlockFace(NIBBLETYPE a_Meta)
	{
		switch (a_Meta & 0x03)
		{
			case 0: return BLOCK_FACE_ZM;
			case 1: return BLOCK_FACE_XP;
			case 2: return BLOCK_FACE_ZP;
			case 3: return BLOCK_FACE_XM;
			default:
			{
				ASSERT(!"Bad meta");
				return BLOCK_FACE_NONE;
			}
		}
	}





	virtual ColourID GetMapBaseColourID(NIBBLETYPE a_Meta) const override
	{
		UNUSED(a_Meta);
		return 34;
	}
} ;




