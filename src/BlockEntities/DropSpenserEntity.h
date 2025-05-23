
// DropSpenser.h

// Declares the cDropSpenser class representing a common ancestor to the cDispenserEntity and cDropperEntity
// The dropper and dispenser only needs to override the DropSpenseFromSlot() function to provide the specific item behavior





#pragma once

#include "BlockEntityWithItems.h"





class cClientHandle;





// tolua_begin
class cDropSpenserEntity :
	public cBlockEntityWithItems
{
	// tolua_end

	using Super = cBlockEntityWithItems;

	// tolua_begin

public:

	enum
	{
		ContentsHeight = 3,
		ContentsWidth  = 3,
	} ;

	// tolua_end

	cDropSpenserEntity(BlockState a_Block, Vector3i a_Pos, cWorld * a_World);

	// cBlockEntity overrides:
	virtual void CopyFrom(const cBlockEntity & a_Src) override;
	virtual void OnRemoveFromWorld() override;
	virtual bool Tick(std::chrono::milliseconds a_Dt, cChunk & a_Chunk) override;
	virtual void SendTo(cClientHandle & a_Client) override;
	virtual bool UsedBy(cPlayer * a_Player) override;

	// tolua_begin

	/** Sets the dropspenser to dropspense an item in the next tick */
	void Activate(void);

	// tolua_end

protected:

	bool m_ShouldDropSpense;  ///< If true, the dropspenser will dropspense an item in the next tick

	/** Does the actual work on dropspensing an item. Chooses the slot, calls DropSpenseFromSlot() and handles smoke / sound effects */
	void DropSpense(cChunk & a_Chunk);

	/** Override this function to provide the specific behavior for item dropspensing (drop / shoot / pour / ...) */
	virtual void DropSpenseFromSlot(cChunk & a_Chunk, int a_SlotNum) = 0;

	/** Helper function, drops one item from the specified slot (like a dropper) */
	void DropFromSlot(cChunk & a_Chunk, int a_SlotNum);
} ;  // tolua_export
