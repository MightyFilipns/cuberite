
#pragma once

#include "ItemHandler.h"





class cItemHopperHandler final  :
	public cSimplePlaceableItemHandler
{
	using Super = cSimplePlaceableItemHandler;

public:

	using Super::Super;

private:

	virtual bool CommitPlacement(cPlayer & a_Player, const cItem & a_HeldItem, const Vector3i a_PlacePosition, const eBlockFace a_ClickedBlockFace, const Vector3i a_CursorPosition) const override
	{
		return a_Player.PlaceBlock(a_PlacePosition, Block::Hopper::Hopper(true, ReverseBlockFace(a_ClickedBlockFace)));
	}
};
