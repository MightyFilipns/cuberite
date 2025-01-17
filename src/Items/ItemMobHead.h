
#pragma once

#include "ItemHandler.h"
#include "../World.h"
#include "../BlockEntities/MobHeadEntity.h"





class cItemMobHeadHandler:
	public cItemHandler
{
	using Super = cItemHandler;

public:

	cItemMobHeadHandler(int a_ItemType):
		Super(a_ItemType)
	{
	}





	virtual bool OnPlayerPlace(
		cWorld & a_World,
		cPlayer & a_Player,
		const cItem & a_EquippedItem,
		const Vector3i a_ClickedBlockPos,
		eBlockFace a_ClickedBlockFace,
		const Vector3i a_CursorPos
	) override
	{
		// Cannot place a head at "no face" and from the bottom:
		if ((a_ClickedBlockFace == BLOCK_FACE_NONE) || (a_ClickedBlockFace == BLOCK_FACE_BOTTOM))
		{
			return true;
		}
		const auto PlacePos = AddFaceDirection(a_ClickedBlockPos, a_ClickedBlockFace);

		// If the placed head is a wither, try to spawn the wither first:
		if (a_EquippedItem.m_ItemDamage == E_META_HEAD_WITHER)
		{
			if (TrySpawnWitherAround(a_World, a_Player, PlacePos))
			{
				return true;
			}
			// Wither not created, proceed with regular head placement
		}

		cItem ItemCopy(a_EquippedItem);  // Make a copy in case this is the player's last head item and OnPlayerPlace removes it
		if (!Super::OnPlayerPlace(a_World, a_Player, a_EquippedItem, a_ClickedBlockPos, a_ClickedBlockFace, a_CursorPos))
		{
			return false;
		}
		RegularHeadPlaced(a_World, a_Player, ItemCopy, PlacePos, a_ClickedBlockFace);
		return true;
	}





	/** Called after placing a regular head block with no mob spawning.
	Adjusts the mob head entity based on the equipped item's data. */
	void RegularHeadPlaced(
		cWorld & a_World, cPlayer & a_Player, const cItem & a_EquippedItem,
		const Vector3i a_PlacePos, eBlockFace a_ClickedBlockFace
	)
	{
		auto HeadType = static_cast<eMobHeadType>(a_EquippedItem.m_ItemDamage);

		// Use a callback to set the properties of the mob head block entity:
		a_World.DoWithBlockEntityAt(a_PlacePos, [&](cBlockEntity & a_BlockEntity)
		{
			switch (a_BlockEntity.GetBlockType())
			{
				case BlockType::CreeperHead:
				case BlockType::CreeperWallHead:
				case BlockType::DragonHead:
				case BlockType::DragonWallHead:
				case BlockType::PlayerHead:
				case BlockType::PlayerWallHead:
				case BlockType::SkeletonSkull:
				case BlockType::SkeletonWallSkull:
				case BlockType::WitherSkeletonSkull:
				case BlockType::WitherSkeletonWallSkull:
				case BlockType::ZombieHead:
				case BlockType::ZombieWallHead:
					break;
				default: return false;
			}

			auto & MobHeadEntity = static_cast<cMobHeadEntity &>(a_BlockEntity);

			int Rotation = 0;
			if (a_ClickedBlockFace == BLOCK_FACE_TOP)
			{
				Rotation = FloorC(a_Player.GetYaw() * 16.0f / 360.0f + 0.5f) & 0x0f;
			}

			MobHeadEntity.SetType(HeadType);
			MobHeadEntity.SetRotation(static_cast<eMobHeadRotation>(Rotation));
			MobHeadEntity.GetWorld()->BroadcastBlockEntity(MobHeadEntity.GetPos());
			return false;
		});
	}





	/** Spawns a wither if the wither skull placed at the specified coords completes wither's spawning formula.
	Returns true if the wither was created. */
	bool TrySpawnWitherAround(
		cWorld & a_World, cPlayer & a_Player,
		const Vector3i a_BlockPos
	)
	{
		// No wither can be created at Y < 2 - not enough space for the formula:
		if (a_BlockPos.y < 2)
		{
			return false;
		}

		static constexpr std::array<Vector3i, 4> SoulSandOffsetX =
		{
			Vector3i( 0, -1, 0),
			Vector3i(-1, -1, 0),
			Vector3i( 1, -1, 0),
			Vector3i( 0, -2, 0)
		};

		static constexpr std::array<Vector3i, 4> SoulSandOffsetZ =
		{
			Vector3i( 0, -1, 0),
			Vector3i(-1, -1, 0),
			Vector3i( 1, -1, 0),
			Vector3i( 0, -2, 0)
		};

		static constexpr std::array<Vector3i, 5> XHeadCoords =
		{
			Vector3i(-2, 0,  0),
			Vector3i(-1, 0,  0),
			Vector3i( 0, 0,  0),
			Vector3i( 1, 0,  0),
			Vector3i( 2, 0,  0),
		};

		static constexpr std::array<Vector3i, 5> ZHeadCoords =
		{
			Vector3i(0,  0, -2),
			Vector3i(0,  0, -1),
			Vector3i(0,  0,  0),
			Vector3i(0,  0,  1),
			Vector3i(0,  0,  2),
		};

		static constexpr std::array<Vector3i, 7> XAirPos =
		{
			Vector3i(-1,  0, 0),
			Vector3i( 0,  0, 0),
			Vector3i( 1,  0, 0),
			Vector3i(-1, -1, 0),
			Vector3i( 0, -1, 0),
			Vector3i( 1, -1, 0),
			Vector3i( 0, -2, 0)
		};

		static constexpr std::array<Vector3i, 7> ZAirPos =
		{
			Vector3i(0,  0, -1),
			Vector3i(0,  0,  0),
			Vector3i(0,  0,  1),
			Vector3i(0, -1, -1),
			Vector3i(0, -1,  0),
			Vector3i(0, -1,  1),
			Vector3i(0, -2,  0)
		};

		cBlockArea Area;
		Area.Read(a_World, a_BlockPos - Vector3i(2, 2, 2), a_BlockPos + Vector3i(2, 0, 2), cBlockArea::baBlocks);

		auto RelHeadPos = Vector3i(3, 2, 3);
		Vector3i CenterHeadPos;

		if (FindCenterHead(Area, XHeadCoords, CenterHeadPos, RelHeadPos))
		{
			if (!ValidateSoulSand(Area, SoulSandOffsetX, RelHeadPos))
			{
				return false;
			}
			if (!ReplaceAir(a_Player, XAirPos, CenterHeadPos - RelHeadPos + a_BlockPos))
			{
				return false;
			}
		}
		else
		{
			if (!FindCenterHead(Area, ZHeadCoords, CenterHeadPos, RelHeadPos))
			{
				return false;
			}
			if (!ValidateSoulSand(Area, SoulSandOffsetZ, RelHeadPos))
			{
				return false;
			}
			if (!ReplaceAir(a_Player, ZAirPos, CenterHeadPos - RelHeadPos + a_BlockPos))
			{
				return false;
			}
		}


		// Spawn the wither:
		Vector3d WitherPos = CenterHeadPos - RelHeadPos + a_BlockPos + Vector3d(0.5, -2, 0.5);
		a_World.SpawnMob(WitherPos.x, WitherPos.y, WitherPos.z, mtWither, false);
		AwardSpawnWitherAchievement(a_World, CenterHeadPos - RelHeadPos + a_BlockPos);
		return true;
	}

	static bool FindCenterHead(cBlockArea & a_Area, const std::array<Vector3i, 5> & a_Offsets, Vector3i & a_CenterHeadPos, Vector3i a_StartPos)
	{
		int HeadCount = 0;
		for (const auto & Offset : a_Offsets)
		{
			switch (a_Area.GetRelBlock(a_StartPos + Offset).Type())
			{
				case BlockType::WitherSkeletonSkull: HeadCount += 1; break;
				default: HeadCount = 0;
			}
			if (HeadCount == 2)
			{
				a_CenterHeadPos = a_StartPos + Offset;
			}
		}
		return (HeadCount >= 3);
	}

	static bool ValidateSoulSand(cBlockArea & a_Area, const std::array<Vector3i, 4> a_Offsets, Vector3i a_StartPos)
	{
		for (const auto & Offset : a_Offsets)
		{
			if (a_Area.GetRelBlock(a_StartPos + Offset).Type() != BlockType::SoulSand)
			{
				return false;
			}
		}
		return true;
	}

	static bool ReplaceAir(cPlayer & a_Player, const std::array<Vector3i, 7> a_Offsets, Vector3i a_StartPos)
	{
		sSetBlockVector AirBlocks;
		for (const auto & Offset : a_Offsets)
		{
			AirBlocks.emplace_back(a_StartPos + Offset, Block::Air::Air());
		}
		return a_Player.PlaceBlocks(AirBlocks);
	}





	/** Awards the achievement to all players close to the specified point. */
	void AwardSpawnWitherAchievement(cWorld & a_World, Vector3i a_BlockPos)
	{
		Vector3f Pos(a_BlockPos);
		a_World.ForEachPlayer([=](cPlayer & a_Player)
			{
				// If player is close, award achievement:
				double Dist = (a_Player.GetPosition() - Pos).Length();
				if (Dist < 50.0)
				{
					a_Player.AwardAchievement(Statistic::AchSpawnWither);
				}
				return false;
			}
		);
	}





	virtual bool IsPlaceable(void) override
	{
		return true;
	}





	virtual bool GetPlacementBlockTypeMeta(
		cWorld * a_World, cPlayer * a_Player,
		const Vector3i a_PlacedBlockPos,
		eBlockFace a_ClickedBlockFace,
		const Vector3i a_CursorPos,
		BlockState & a_Block
	) override
	{
		if (a_Player == nullptr)
		{
			return false;
		}
		using namespace Block;

		switch (a_Player->GetEquippedItem().m_ItemDamage)
		{
			case E_META_HEAD_SKELETON:
			{
				switch (a_ClickedBlockFace)
				{
					case BLOCK_FACE_YP:
					{
						a_Block = SkeletonSkull::SkeletonSkull(RotationToFineFace(a_Player->GetYaw()));
						break;
					}
					case BLOCK_FACE_XM:
					case BLOCK_FACE_XP:
					case BLOCK_FACE_ZM:
					case BLOCK_FACE_ZP:
					{
						a_Block = SkeletonWallSkull::SkeletonWallSkull(RotationToBlockFace(a_Player->GetYaw()));
						break;
					}
					default: return false;
				}
				break;
			}
			case E_META_HEAD_WITHER:
			{
				switch (a_ClickedBlockFace)
				{
					case BLOCK_FACE_YP:
					{
						a_Block = WitherSkeletonSkull::WitherSkeletonSkull(RotationToFineFace(a_Player->GetYaw()));
						break;
					}
					case BLOCK_FACE_XM:
					case BLOCK_FACE_XP:
					case BLOCK_FACE_ZM:
					case BLOCK_FACE_ZP:
					{
						a_Block = WitherSkeletonWallSkull::WitherSkeletonWallSkull(RotationToBlockFace(a_Player->GetYaw()));
						break;
					}
					default: return false;
				}
				break;
			}
			case E_META_HEAD_ZOMBIE:
			{
				switch (a_ClickedBlockFace)
				{
					case BLOCK_FACE_YP:
					{
						a_Block = ZombieHead::ZombieHead(RotationToFineFace(a_Player->GetYaw()));
						break;
					}
					case BLOCK_FACE_XM:
					case BLOCK_FACE_XP:
					case BLOCK_FACE_ZM:
					case BLOCK_FACE_ZP:
					{
						a_Block = ZombieWallHead::ZombieWallHead(RotationToBlockFace(a_Player->GetYaw()));
						break;
					}
					default: return false;
				}
				break;
			}
			case E_META_HEAD_PLAYER:
			{
				switch (a_ClickedBlockFace)
				{
					case BLOCK_FACE_YP:
					{
						a_Block = PlayerHead::PlayerHead(RotationToFineFace(a_Player->GetYaw()));
						break;
					}
					case BLOCK_FACE_XM:
					case BLOCK_FACE_XP:
					case BLOCK_FACE_ZM:
					case BLOCK_FACE_ZP:
					{
						a_Block = PlayerWallHead::PlayerWallHead(RotationToBlockFace(a_Player->GetYaw()));
						break;
					}
					default: return false;
				}
				break;
			}
			case E_META_HEAD_CREEPER:
			{
				switch (a_ClickedBlockFace)
				{
					case BLOCK_FACE_YP:
					{
						a_Block = CreeperHead::CreeperHead(RotationToFineFace(a_Player->GetYaw()));
						break;
					}
					case BLOCK_FACE_XM:
					case BLOCK_FACE_XP:
					case BLOCK_FACE_ZM:
					case BLOCK_FACE_ZP:
					{
						a_Block = CreeperWallHead::CreeperWallHead(RotationToBlockFace(a_Player->GetYaw()));
						break;
					}
					default: return false;
				}
				break;
			}
			case E_META_HEAD_DRAGON:
			{
				switch (a_ClickedBlockFace)
				{
					case BLOCK_FACE_YP:
					{
						a_Block = CreeperHead::CreeperHead(RotationToFineFace(a_Player->GetYaw()));
						break;
					}
					case BLOCK_FACE_XM:
					case BLOCK_FACE_XP:
					case BLOCK_FACE_ZM:
					case BLOCK_FACE_ZP:
					{
						a_Block = CreeperWallHead::CreeperWallHead(RotationToBlockFace(a_Player->GetYaw()));
						break;
					}
					default: return false;
				}
				break;
			}
			default:
			{
				ASSERT(!"UNHANDLED MOB HEAD TYPE IN ITEMMOBHEAD");
			}
		}
		return true;
	}
} ;




