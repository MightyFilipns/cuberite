
#pragma once

#include "RedstoneHandler.h"
#include "Registries/BlockStates.h"





namespace RedstoneWireHandler
{
	/** A unified representation of wire direction. */
	enum class TemporaryDirection
	{
		Up,
		Side
	};

	/** Invokes Callback with the wire's left, front, and right direction state corresponding to Offset.
	Returns a new block constructed from the directions that the callback may have modified. */
	template <class OffsetCallback>
	static BlockState DoWithDirectionState(const Vector3i Offset, BlockState Block, OffsetCallback Callback)
	{
		auto North = Block::RedstoneWire::North(Block);
		auto South = Block::RedstoneWire::South(Block);
		auto West = Block::RedstoneWire::West(Block);
		auto East = Block::RedstoneWire::East(Block);

		if (Offset.x == -1)
		{
			Callback(South, West, North);
		}
		else if (Offset.x == 1)
		{
			Callback(North, East, South);
		}

		if (Offset.z == -1)
		{
			Callback(West, North, East);
		}
		else if (Offset.z == 1)
		{
			Callback(East, South, West);
		}

		return Block::RedstoneWire::RedstoneWire(East, North, 0, South, West);
	}

	/** Adjusts a given wire block so that the direction represented by Offset has state Direction. */
	static void SetDirectionState(const Vector3i Offset, BlockState & Block, TemporaryDirection Direction)
	{
		Block = DoWithDirectionState(Offset, Block, [Direction](auto, auto & Front, auto)
		{
			using FrontState = std::remove_reference_t<decltype(Front)>;
			switch (Direction)
			{
				case TemporaryDirection::Up:
				{
					Front = FrontState::Up;
					return;
				}
				case TemporaryDirection::Side:
				{
					Front = FrontState::Side;
					return;
				}
			}
		});
	}

	static bool IsDirectlyConnectingMechanism(BlockState a_Block, const Vector3i a_Offset)
	{
		switch (a_Block.Type())
		{
			case BlockType::Repeater:
			{
				auto Facing = Block::Repeater::Facing(a_Block);
				if ((Facing == eBlockFace::BLOCK_FACE_XM) || (Facing == eBlockFace::BLOCK_FACE_XP))
				{
					// Wire connects to repeater if repeater is aligned along X
					// and wire is in front or behind it (#4639)
					return a_Offset.x != 0;
				}
				return a_Offset.z != 0;
			}
			case BlockType::Comparator:
			{
				return Block::Comparator::Powered(a_Block);
			}
			case BlockType::RedstoneBlock:
			case BlockType::Lever:
			case BlockType::RedstoneTorch:
			case BlockType::RedstoneWallTorch:
			case BlockType::RedstoneWire:
			case BlockType::AcaciaButton:
			case BlockType::BirchButton:
			case BlockType::CrimsonButton:
			case BlockType::DarkOakButton:
			case BlockType::JungleButton:
			case BlockType::OakButton:
			case BlockType::PolishedBlackstoneButton:
			case BlockType::SpruceButton:
			case BlockType::StoneButton:
			case BlockType::WarpedButton: return true;
			default: return false;
		}
	}

	static bool IsYPTerracingBlocked(const cChunk & a_Chunk, const Vector3i a_Position)
	{
		const auto Position = a_Position + OffsetYP;

		if (!cChunkDef::IsValidHeight(Position))
		{
			// Certainly cannot terrace at the top of the world:
			return true;
		}

		const auto YPTerraceBlock = a_Chunk.GetBlock(Position);
		return cBlockInfo::IsSolid(YPTerraceBlock) && !cBlockInfo::IsTransparent(YPTerraceBlock);
	}

	/** Temporary. Discovers a wire's connection state, including terracing, storing the block inside redstone chunk data.
	TODO: once the server supports block states this should go in the block handler, with data saved in the world. */
	static void SetWireState(const cChunk & a_Chunk, const Vector3i a_Position)
	{
		auto Block = Block::RedstoneWire::RedstoneWire();
		const bool IsYPTerracingBlocked = RedstoneWireHandler::IsYPTerracingBlocked(a_Chunk, a_Position);

		// Loop through laterals, discovering terracing connections:
		for (const auto & Offset : RelativeLaterals)
		{
			auto Adjacent = a_Position + Offset;
			auto NeighbourChunk = a_Chunk.GetRelNeighborChunkAdjustCoords(Adjacent);

			if ((NeighbourChunk == nullptr) || !NeighbourChunk->IsValid())
			{
				continue;
			}


			auto LateralBlock = NeighbourChunk->GetBlock(Adjacent);

			if (IsDirectlyConnectingMechanism(LateralBlock, Offset))
			{
				// Any direct connections on a lateral means the wire has side connection in that direction:
				SetDirectionState(Offset, Block, TemporaryDirection::Side);

				// Temporary: this case will eventually be handled when wires are placed, with the state saved as blocks
				// When a neighbour wire was loaded into its chunk, its neighbour chunks may not have loaded yet
				// This function is called during chunk load (through AddBlock). Attempt to tell it its new state:
				if ((NeighbourChunk != &a_Chunk) && (LateralBlock.Type() == BlockType::RedstoneWire))
				{
					auto & NeighbourBlock = DataForChunk(*NeighbourChunk).WireStates.find(Adjacent)->second;
					SetDirectionState(-Offset, NeighbourBlock, TemporaryDirection::Side);
				}

				continue;
			}

			if (
				!IsYPTerracingBlocked &&  // A block above us blocks all YP terracing, so the check is static in the loop
				(Adjacent.y < (cChunkDef::Height - 1)) &&
				(NeighbourChunk->GetBlock(Adjacent + OffsetYP).Type() == BlockType::RedstoneWire)  // Only terrace YP with another wire
			)
			{
				SetDirectionState(Offset, Block, cBlockInfo::IsTransparent(LateralBlock) ? TemporaryDirection::Side : TemporaryDirection::Up);

				if (NeighbourChunk != &a_Chunk)
				{
					auto & NeighbourBlock = DataForChunk(*NeighbourChunk).WireStates.find(Adjacent + OffsetYP)->second;
					SetDirectionState(-Offset, NeighbourBlock, TemporaryDirection::Side);
				}

				continue;
			}

			if (
				// IsYMTerracingBlocked (i.e. check block above lower terracing position, a.k.a. just the plain adjacent)
				(!cBlockInfo::IsSolid(LateralBlock) || cBlockInfo::IsTransparent(LateralBlock)) &&
				(Adjacent.y > 0) &&
				(NeighbourChunk->GetBlock(Adjacent + OffsetYM).Type() == BlockType::RedstoneWire)  // Only terrace YM with another wire
			)
			{
				SetDirectionState(Offset, Block, TemporaryDirection::Side);

				if (NeighbourChunk != &a_Chunk)
				{
					auto & NeighbourBlock = DataForChunk(*NeighbourChunk).WireStates.find(Adjacent + OffsetYM)->second;
					SetDirectionState(-Offset, NeighbourBlock, TemporaryDirection::Up);
				}
			}
		}

		auto & States = DataForChunk(a_Chunk).WireStates;
		const auto FindResult = States.find(a_Position);
		if (FindResult != States.end())
		{
			if (Block != FindResult->second)
			{
				FindResult->second = Block;

				// TODO: when state is stored as the block, the block handler updating via SetBlock will do this automatically
				// When a wire changes connection state, it needs to update its neighbours:
				a_Chunk.GetWorld()->WakeUpSimulators(cChunkDef::RelativeToAbsolute(a_Position, a_Chunk.GetPos()));
			}

			return;
		}

		DataForChunk(a_Chunk).WireStates.emplace(a_Position, Block);
	}

	static PowerLevel GetPowerDeliveredToPosition(const cChunk & a_Chunk, Vector3i a_Position, BlockState a_Block, Vector3i a_QueryPosition, BlockState a_QueryBlock, bool IsLinked)
	{
		// Starts off as the wire's meta value, modified appropriately and returned
		auto Power = Block::RedstoneWire::Power(a_Block);
		const auto QueryOffset = a_QueryPosition - a_Position;

		if (
			(QueryOffset == OffsetYP) ||  // Wires do not power things above them
			(IsLinked && (a_QueryBlock.Type() == BlockType::RedstoneWire))  // Nor do they link power other wires
		)
		{
			return 0;
		}

		if (QueryOffset == OffsetYM)
		{
			// Wires always deliver power to the block underneath
			return Power;
		}

		const auto & Data = DataForChunk(a_Chunk);
		const auto find_pos = Data.WireStates.find(a_Position);


		if (find_pos == Data.WireStates.end())
		{
			LOGERROR("Cant find Redstone wire in WireStates");
			return 0;
		}
		const auto Block = find_pos->second;

		DoWithDirectionState(QueryOffset, Block, [a_QueryBlock, &Power](const auto Left, const auto Front, const auto Right)
		{
			using LeftState = std::remove_reference_t<decltype(Left)>;
			using FrontState = std::remove_reference_t<decltype(Front)>;
			using RightState = std::remove_reference_t<decltype(Right)>;

			// Wires always deliver power to any directly connecting mechanisms:
			if (Front != FrontState::None)
			{
				if ((a_QueryBlock.Type() == BlockType::RedstoneWire) && (Power != 0))
				{
					// For mechanisms, wire of power one will still power them
					// But for wire-to-wire connections, power level decreases by 1:
					Power--;
				}

				return;
			}

			/*
			Okay, we do not directly connect to the wire.
			1. If there are no DC mechanisms at all, the wire powers all laterals. Great, left and right are both None.
			2. If there is one DC mechanism, the wire "goes straight" along the axis of the wire and mechanism.
				The only possible way for us to be powered is for us to be on the opposite end, with the wire pointing towards us.
				Check that left and right are both None.
			3. If there is more than one DC, no non-DCs are powered. Left, right, cannot both be None.
			*/
			if ((Left == LeftState::None) && (Right == RightState::None))
			{
				// Case 1
				// Case 2
				return;
			}

			// Case 3
			Power = 0;
		});

		return Power;
	}

#define CHECK_WIRE test_block.Type() == BlockType::RedstoneWire

#define CALC_SIDE(Dir) (a_Chunk.UnboundedRelGetBlock(AddFaceDirection(a_Position, eBlockFace::Dir), test_block) && CHECK_WIRE) || \
			(cBlockInfo::IsTransparent(test_block.Type()) && a_Chunk.UnboundedRelGetBlock(AddFaceDirection(a_Position.addedY(-1), eBlockFace::Dir), test_block) && CHECK_WIRE)


	static void Update(cChunk & a_Chunk, cChunk & CurrentlyTicking, Vector3i a_Position, BlockState a_Block, const PowerLevel a_Power)
	{
		using namespace Block::RedstoneWire;
		LOGREDSTONE("Evaluating dusty the wire (%d %d %d) %i", a_Position.x, a_Position.y, a_Position.z, a_Power);

		// TODO: optimize this function somehow as it called every tick

		// All cheking here is purely for visuals. Does not affect how redstone will work. (Probably)

		// Auto is necessary here, as the compiler cannot deduce the type of the vars as enums
		auto east = East::None;
		auto north = North::None;
		auto south = South::None;
		auto west = West::None;
		BlockState test_block;
		// Checks sorround blocks for wires
		if (CALC_SIDE(BLOCK_FACE_EAST))
		{
			east = East::Side;
		}
		if (CALC_SIDE(BLOCK_FACE_NORTH))
		{
			north = North::Side;
		}
		if (CALC_SIDE(BLOCK_FACE_SOUTH))
		{
			south = South::Side;
		}
		if (CALC_SIDE(BLOCK_FACE_WEST))
		{
			west = West::Side;
		}

		auto new_pos = a_Position.addedY(1);
		if (a_Chunk.UnboundedRelGetBlock(new_pos, test_block) && cBlockInfo::IsTransparent(test_block.Type()))
		{
			if (a_Chunk.UnboundedRelGetBlock(AddFaceDirection(new_pos, eBlockFace::BLOCK_FACE_EAST), test_block) && CHECK_WIRE)
			{
				east = East::Up;
			}
			if (a_Chunk.UnboundedRelGetBlock(AddFaceDirection(new_pos, eBlockFace::BLOCK_FACE_NORTH), test_block) && CHECK_WIRE)
			{
				north = North::Up;
			}
			if (a_Chunk.UnboundedRelGetBlock(AddFaceDirection(new_pos, eBlockFace::BLOCK_FACE_SOUTH), test_block) && CHECK_WIRE)
			{
				south = South::Up;
			}
			if (a_Chunk.UnboundedRelGetBlock(AddFaceDirection(new_pos, eBlockFace::BLOCK_FACE_WEST), test_block) && CHECK_WIRE)
			{
				west = West::Up;
			}
		}

		// Makes sure the wire is straight if it's connected to only one side
		if ((east == East::None) && (west == West::None))
		{
			if ((north != North::Up) && (south != South::None))
			{
				north = North::Side;
			}
			if ((south != South::Up) && (north != North::None))
			{
				south = South::Side;
			}
		}

		if ((north == North::None) && (south == South::None))
		{
			if ((west != West::Up) && (east != East::None))
			{
				west = West::Side;
			}
			if ((east != East::Up) && (west != West::None))
			{
				east = East::Side;
			}
		}

		if ((Power(a_Block) == a_Power) && (East(a_Block) == east) && (North(a_Block) == north) && (South(a_Block) == south) && (West(a_Block) == west))
		{
			// Everything is the same
			return;
		}

		a_Chunk.FastSetBlock(a_Position, RedstoneWire
		(
			east,
			north,
			a_Power,
			south,
			west
		));

		// Notify all positions, sans YP, to update:
		UpdateAdjustedRelative(a_Chunk, CurrentlyTicking, a_Position, OffsetYM);
		UpdateAdjustedRelatives(a_Chunk, CurrentlyTicking, a_Position, RelativeLaterals);
	}

	static void ForValidSourcePositions(const cChunk & a_Chunk, Vector3i a_Position, BlockState a_Block, ForEachSourceCallback & Callback)
	{
		UNUSED(a_Block);

		Callback(a_Position + OffsetYP);
		Callback(a_Position + OffsetYM);

		const auto & Data = DataForChunk(a_Chunk);
		const auto Iterator = Data.WireStates.find(a_Position);
		if (Iterator == Data.WireStates.end())
		{
			return;
		}
		const auto Block = Iterator->second;

		// Figure out, based on our pre-computed block, where we connect to:
		for (const auto & Offset : RelativeLaterals)
		{
			const auto Relative = a_Position + Offset;
			Callback(Relative);

			DoWithDirectionState(Offset, Block, [&a_Chunk, &Callback, Relative](auto, const auto Front, auto)
			{
				using FrontState = std::remove_reference_t<decltype(Front)>;

				if (Front == FrontState::Up)
				{
					Callback(Relative + OffsetYP);
				}
				else if (Front == FrontState::Side)
				{
					// Alas, no way to distinguish side lateral and side diagonal
					// Have to do a manual check to only accept power from YM diagonal if there's a wire there

					const auto YMDiagonalPosition = Relative + OffsetYM;
					if (
						BlockState QueryBlock;
						cChunkDef::IsValidHeight(YMDiagonalPosition) &&
						a_Chunk.UnboundedRelGetBlock(YMDiagonalPosition, QueryBlock) &&
						(QueryBlock == BlockType::RedstoneWire)
					)
					{
						Callback(YMDiagonalPosition);
					}
				}
			});
		}
	}
};
