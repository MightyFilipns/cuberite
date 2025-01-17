
#pragma once

#include "BlockHandler.h"





class cBlockLilypadHandler final :
	public cBlockHandler
{
	using Super = cBlockHandler;

public:

	using Super::Super;

private:

	virtual ColourID GetMapBaseColourID() const override
	{
		return 7;
	}





	virtual bool CanBeAt(const cChunk & a_Chunk, const Vector3i a_Position, const NIBBLETYPE a_Meta) const override
	{
		auto UnderPos = a_Position.addedY(-1);
		if (!cChunkDef::IsValidHeight(UnderPos))
		{
			return false;
		}

		auto BlockBelow = a_Chunk.GetBlock(UnderPos);
		return (
			((BlockBelow.Type() == BlockType::Water) && (cBlockFluidHandler::GetFalloff(BlockBelow) == 0)) ||  // A water source is below
			(BlockBelow.Type() == BlockType::Ice) || (BlockBelow.Type() == BlockType::FrostedIce)               // Or (frosted) ice
		);
	}
};




