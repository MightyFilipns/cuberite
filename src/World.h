
#pragma once

#include <functional>
#include <optional>

#include "Simulator/SimulatorManager.h"
#include "ChunkMap.h"
#include "WorldStorage/WorldStorage.h"
#include "ChunkGeneratorThread.h"
#include "ChunkSender.h"
#include "Defines.h"
#include "LightingThread.h"
#include "IniFile.h"
#include "Item.h"
#include "Mobs/Monster.h"
#include "Entities/ProjectileEntity.h"
#include "Entities/Boat.h"
#include "ForEachChunkProvider.h"
#include "Scoreboard.h"
#include "MapManager.h"
#include "Blocks/WorldInterface.h"
#include "Blocks/BroadcastInterface.h"
#include "EffectID.h"





class cFireSimulator;
class cFluidSimulator;
class cSandSimulator;
class cRedstoneSimulator;
class cItem;
class cPlayer;
class cClientHandle;
class cEntity;
class cChunkGenerator;  // The thread responsible for generating chunks
class cCuboid;
class cCompositeChat;
class cDeadlockDetect;
class cUUID;

struct SetChunkData;





class cWorld  // tolua_export
	final:
	public cForEachChunkProvider,
	public cWorldInterface,
	public cBroadcastInterface
// tolua_begin
{
public:
	// tolua_end

	/** A simple RAII locker for the chunkmap - locks the chunkmap in its constructor, unlocks it in the destructor */
	class cLock:
		public cCSLock
	{
		using Super = cCSLock;
	public:
		cLock(const cWorld & a_World);
	};

	/** Construct the world and read settings from its ini file.
	@param a_DeadlockDetect is used for tracking this world's age, detecting a possible deadlock.
	@param a_WorldNames is a list of all world names, used to validate linked worlds
	*/
	cWorld(
		const AString & a_WorldName, const AString & a_DataPath,
		cDeadlockDetect & a_DeadlockDetect, const AStringVector & a_WorldNames,
		eDimension a_Dimension = dimOverworld, const AString & a_LinkedOverworldName = {}
	);

	virtual ~cWorld() override;

	virtual void FlushPendingBlockChanges() override;

	// tolua_begin

	/** Get whether saving chunks is enabled */
	bool IsSavingEnabled(void) const { return m_IsSavingEnabled; }

	/** Set whether saving chunks is enabled */
	void SetSavingEnabled(bool a_IsSavingEnabled) { m_IsSavingEnabled = a_IsSavingEnabled; }

	int GetTicksUntilWeatherChange(void) const { return m_WeatherInterval; }

	/** Is the daylight cycle enabled? */
	virtual bool IsDaylightCycleEnabled(void) const { return m_IsDaylightCycleEnabled; }

	/** Sets the daylight cycle to true / false. */
	virtual void SetDaylightCycleEnabled(bool a_IsDaylightCycleEnabled)
	{
		m_IsDaylightCycleEnabled = a_IsDaylightCycleEnabled;
		BroadcastTimeUpdate();
	}

	void SetTicksUntilWeatherChange(int a_WeatherInterval)
	{
		m_WeatherInterval = a_WeatherInterval;
	}

	/** Returns the default weather interval for the specific weather type.
	Returns -1 for any unknown weather. */
	int GetDefaultWeatherInterval(eWeather a_Weather) const;

	/** Returns the current game mode. Partly OBSOLETE, you should use IsGameModeXXX() functions wherever applicable */
	eGameMode GetGameMode(void) const { return m_GameMode; }

	/** Returns true if the world is in Creative mode */
	bool IsGameModeCreative(void) const { return (m_GameMode == gmCreative); }

	/** Returns true if the world is in Survival mode */
	bool IsGameModeSurvival(void) const { return (m_GameMode == gmSurvival); }

	/** Returns true if the world is in Adventure mode */
	bool IsGameModeAdventure(void) const { return (m_GameMode == gmAdventure); }

	/** Returns true if the world is in Spectator mode */
	bool IsGameModeSpectator(void) const { return (m_GameMode == gmSpectator); }

	bool IsPVPEnabled(void) const { return m_bEnabledPVP; }

	/** Returns true if farmland trampling is enabled */
	bool IsFarmlandTramplingEnabled(void) const { return m_bFarmlandTramplingEnabled; }

	bool IsDeepSnowEnabled(void) const { return m_IsDeepSnowEnabled; }

	bool ShouldLavaSpawnFire(void) const { return m_ShouldLavaSpawnFire; }

	bool VillagersShouldHarvestCrops(void) const { return m_VillagersShouldHarvestCrops; }

	virtual eDimension GetDimension(void) const override { return m_Dimension; }

	// tolua_end

	virtual cTickTime GetTimeOfDay(void) const override;
	virtual cTickTimeLong GetWorldAge(void) const override;
	cTickTimeLong GetWorldDate() const;
	cTickTimeLong GetWorldTickAge() const;

	virtual void SetTimeOfDay(cTickTime a_TimeOfDay) override;

	/** Retrieves the world height at the specified coords; returns nullopt if chunk not loaded / generated */
	virtual std::optional<int> GetHeight(int a_BlockX, int a_BlockZ) override;  // Exported in ManualBindings.cpp

	// Broadcast respective packets to all clients of the chunk where the event is taking place
	// Implemented in Broadcaster.cpp
	// (Please keep these alpha-sorted)
	virtual void BroadcastAttachEntity       (const cEntity & a_Entity, const cEntity & a_Vehicle) override;
	virtual void BroadcastBlockAction        (Vector3i a_BlockPos, Byte a_Byte1, Byte a_Byte2, BlockState a_BlockType, const cClientHandle * a_Exclude = nullptr) override;  // Exported in ManualBindings_World.cpp
	virtual void BroadcastBlockBreakAnimation(UInt32 a_EntityID, Vector3i a_BlockPos, Int8 a_Stage, const cClientHandle * a_Exclude = nullptr) override;
	virtual void BroadcastBlockEntity        (Vector3i a_BlockPos, const cClientHandle * a_Exclude = nullptr) override;  ///< If there is a block entity at the specified coods, sends it to all clients except a_Exclude
	virtual void BroadcastBossBarUpdateHealth(const cEntity & a_Entity, UInt32 a_UniqueID, float a_FractionFilled) override;

	// tolua_begin
	virtual void BroadcastChat       (const AString & a_Message, const cClientHandle * a_Exclude = nullptr, eMessageType a_ChatPrefix = mtCustom) override;
	virtual void BroadcastChatInfo   (const AString & a_Message, const cClientHandle * a_Exclude = nullptr) override { BroadcastChat(a_Message, a_Exclude, mtInformation); }
	virtual void BroadcastChatFailure(const AString & a_Message, const cClientHandle * a_Exclude = nullptr) override { BroadcastChat(a_Message, a_Exclude, mtFailure); }
	virtual void BroadcastChatSuccess(const AString & a_Message, const cClientHandle * a_Exclude = nullptr) override { BroadcastChat(a_Message, a_Exclude, mtSuccess); }
	virtual void BroadcastChatWarning(const AString & a_Message, const cClientHandle * a_Exclude = nullptr) override { BroadcastChat(a_Message, a_Exclude, mtWarning); }
	virtual void BroadcastChatFatal  (const AString & a_Message, const cClientHandle * a_Exclude = nullptr) override { BroadcastChat(a_Message, a_Exclude, mtFailure); }
	virtual void BroadcastChatDeath  (const AString & a_Message, const cClientHandle * a_Exclude = nullptr) override { BroadcastChat(a_Message, a_Exclude, mtDeath); }
	virtual void BroadcastChat       (const cCompositeChat & a_Message, const cClientHandle * a_Exclude = nullptr) override;
	// tolua_end

	virtual void BroadcastCollectEntity              (const cEntity & a_Collected, const cEntity & a_Collector, unsigned a_Count, const cClientHandle * a_Exclude = nullptr) override;
	virtual void BroadcastDestroyEntity              (const cEntity & a_Entity, const cClientHandle * a_Exclude = nullptr) override;
	virtual void BroadcastDetachEntity               (const cEntity & a_Entity, const cEntity & a_PreviousVehicle) override;
	virtual void BroadcastEntityEffect               (const cEntity & a_Entity, int a_EffectID, int a_Amplifier, int a_Duration, const cClientHandle * a_Exclude = nullptr) override;
	virtual void BroadcastEntityEquipment            (const cEntity & a_Entity, short a_SlotNum, const cItem & a_Item, const cClientHandle * a_Exclude = nullptr) override;
	virtual void BroadcastEntityHeadLook             (const cEntity & a_Entity, const cClientHandle * a_Exclude = nullptr) override;
	virtual void BroadcastEntityLook                 (const cEntity & a_Entity, const cClientHandle * a_Exclude = nullptr) override;
	virtual void BroadcastEntityMetadata             (const cEntity & a_Entity, const cClientHandle * a_Exclude = nullptr) override;
	virtual void BroadcastEntityPosition             (const cEntity & a_Entity, const cClientHandle * a_Exclude = nullptr) override;
	void         BroadcastEntityProperties           (const cEntity & a_Entity);
	virtual void BroadcastEntityVelocity             (const cEntity & a_Entity, const cClientHandle * a_Exclude = nullptr) override;
	virtual void BroadcastEntityAnimation            (const cEntity & a_Entity, EntityAnimation a_Animation, const cClientHandle * a_Exclude = nullptr) override;  // tolua_export
	virtual void BroadcastGameStateChange            (eGameStateReason a_Reason, float a_Value = 0, const cClientHandle * a_Exclude = nullptr) override;
	virtual void BroadcastLeashEntity                (const cEntity & a_Entity, const cEntity & a_EntityLeashedTo) override;
	virtual void BroadcastParticleEffect             (const AString & a_ParticleName, Vector3f a_Src, Vector3f a_Offset, float a_ParticleData, int a_ParticleAmount, const cClientHandle * a_Exclude = nullptr) override;  // Exported in ManualBindings_World.cpp
	virtual void BroadcastParticleEffect             (const AString & a_ParticleName, Vector3f a_Src, Vector3f a_Offset, float a_ParticleData, int a_ParticleAmount, std::array<int, 2> a_Data, const cClientHandle * a_Exclude = nullptr) override;  // Exported in ManualBindings_World.cpp
	virtual void BroadcastPlayerListAddPlayer        (const cPlayer & a_Player, const cClientHandle * a_Exclude = nullptr) override;
	virtual void BroadcastPlayerListHeaderFooter     (const cCompositeChat & a_Header, const cCompositeChat & a_Footer) override;  // tolua_export
	virtual void BroadcastPlayerListRemovePlayer     (const cPlayer & a_Player, const cClientHandle * a_Exclude = nullptr) override;
	virtual void BroadcastPlayerListUpdateDisplayName(const cPlayer & a_Player, const AString & a_CustomName, const cClientHandle * a_Exclude = nullptr) override;
	virtual void BroadcastPlayerListUpdateGameMode   (const cPlayer & a_Player, const cClientHandle * a_Exclude = nullptr) override;
	virtual void BroadcastPlayerListUpdatePing       () override;
	virtual void BroadcastRemoveEntityEffect         (const cEntity & a_Entity, int a_EffectID, const cClientHandle * a_Exclude = nullptr) override;
	virtual void BroadcastScoreboardObjective        (const AString & a_Name, const AString & a_DisplayName, Byte a_Mode) override;
	virtual void BroadcastScoreUpdate                (const AString & a_Objective, const AString & a_Player, cObjective::Score a_Score, Byte a_Mode) override;
	virtual void BroadcastDisplayObjective           (const AString & a_Objective, cScoreboard::eDisplaySlot a_Display) override;
	virtual void BroadcastSoundEffect                (const AString & a_SoundName, Vector3d a_Position, float a_Volume, float a_Pitch, const cClientHandle * a_Exclude = nullptr) override;  // Exported in ManualBindings_World.cpp
	virtual void BroadcastSoundParticleEffect        (const EffectID a_EffectID, Vector3i a_SrcPos, int a_Data, const cClientHandle * a_Exclude = nullptr) override;  // Exported in ManualBindings_World.cpp
	virtual void BroadcastSpawnEntity                (cEntity & a_Entity, const cClientHandle * a_Exclude = nullptr) override;
	virtual void BroadcastThunderbolt                (Vector3i a_BlockPos, const cClientHandle * a_Exclude = nullptr) override;
	virtual void BroadcastTimeUpdate                 (const cClientHandle * a_Exclude = nullptr) override;
	virtual void BroadcastUnleashEntity              (const cEntity & a_Entity) override;
	virtual void BroadcastWeather                    (eWeather a_Weather, const cClientHandle * a_Exclude = nullptr) override;

	virtual cBroadcastInterface & GetBroadcastManager(void) override
	{
		return *this;
	}

	/** If there is a block entity at the specified coords, sends it to the client specified */
	void SendBlockEntity(int a_BlockX, int a_BlockY, int a_BlockZ, cClientHandle & a_Client);

	void MarkChunkDirty (int a_ChunkX, int a_ChunkZ);
	void MarkChunkSaving(int a_ChunkX, int a_ChunkZ);
	void MarkChunkSaved (int a_ChunkX, int a_ChunkZ);

	/** Puts the chunk data into a queue to be set into the chunkmap in the tick thread.
	Modifies the a_SetChunkData - moves the entities contained in it into the queue. */
	void QueueSetChunkData(SetChunkData && a_SetChunkData);

	void ChunkLighted(
		int a_ChunkX, int a_ChunkZ,
		const cChunkDef::LightNibbles & a_BlockLight,
		const cChunkDef::LightNibbles & a_SkyLight
	);

	/** Calls the callback with the chunk's data, if available (with ChunkCS locked).
	Returns true if the chunk was reported successfully, false if not (chunk not present or callback failed). */
	bool GetChunkData(cChunkCoords a_Coords, cChunkDataCallback & a_Callback) const;

	/** Returns true iff the chunk is in the loader / generator queue. */
	bool IsChunkQueued(int a_ChunkX, int a_ChunkZ) const;

	/** Returns true iff the chunk is present and valid. */
	bool IsChunkValid(int a_ChunkX, int a_ChunkZ) const;

	bool HasChunkAnyClients(int a_ChunkX, int a_ChunkZ) const;

	/** Queues a task to unload unused chunks onto the tick thread. The prefferred way of unloading. */
	void QueueUnloadUnusedChunks(void);  // tolua_export

	void CollectPickupsByEntity(cEntity & a_Entity);

	/** Calls the callback for each player in the list; returns true if all players processed, false if the callback aborted by returning true */
	virtual bool ForEachPlayer(cPlayerListCallback a_Callback) override;  // >> EXPORTED IN MANUALBINDINGS <<

	/** Calls the callback for the player of the given name; returns true if the player was found and the callback called, false if player not found.
	Callback return value is ignored. If there are multiple players of the same name, only (random) one is processed by the callback. */
	bool DoWithPlayer(const AString & a_PlayerName, cPlayerListCallback a_Callback);  // >> EXPORTED IN MANUALBINDINGS <<

	/** Finds a player from a partial or complete player name and calls the callback - case-insensitive */
	bool FindAndDoWithPlayer(const AString & a_PlayerNameHint, cPlayerListCallback a_Callback);  // >> EXPORTED IN MANUALBINDINGS <<

	/** Calls the callback for nearest player for given position, Returns false if player not found, otherwise returns the same value as the callback */
	bool DoWithNearestPlayer(Vector3d a_Pos, double a_RangeLimit, cPlayerListCallback a_Callback, bool a_CheckLineOfSight = true, bool a_IgnoreSpectator = true);

	/** Finds the player over his uuid and calls the callback */
	bool DoWithPlayerByUUID(const cUUID & a_PlayerUUID, cPlayerListCallback a_Callback);  // >> EXPORTED IN MANUALBINDINGS <<

	void SendPlayerList(cPlayer * a_DestPlayer);  // Sends playerlist to the player

	/** Adds the entity into its appropriate chunk; takes ownership of the entity ptr.
	The entity is added lazily - this function only puts it in a queue that is then processed by the Tick thread.
	If a_OldWorld is provided, a corresponding ENTITY_CHANGED_WORLD event is triggerred after the addition. */
	void AddEntity(OwnedEntity a_Entity, cWorld * a_OldWorld = nullptr);

	/** Removes the entity from the world.
	Returns an owning reference to the found entity. */
	OwnedEntity RemoveEntity(cEntity & a_Entity);

	/** Calls the callback for each entity in the entire world; returns true if all entities processed, false if the callback aborted by returning true */
	bool ForEachEntity(cEntityCallback a_Callback);  // Exported in ManualBindings.cpp

	/** Calls the callback for each entity in the specified chunk; returns true if all entities processed, false if the callback aborted by returning true */
	bool ForEachEntityInChunk(int a_ChunkX, int a_ChunkZ, cEntityCallback a_Callback);  // Exported in ManualBindings.cpp

	/** Calls the callback for each entity that has a nonempty intersection with the specified boundingbox.
	Returns true if all entities processed, false if the callback aborted by returning true.
	If any chunk in the box is missing, ignores the entities in that chunk silently. */
	virtual bool ForEachEntityInBox(const cBoundingBox & a_Box, cEntityCallback a_Callback) override;  // Exported in ManualBindings.cpp

	/** Returns the number of players currently in this world. */
	size_t GetPlayerCount() const;

	/** Calls the callback if the entity with the specified ID is found, with the entity object as the callback param.
	Returns true if entity found and callback returned false. */
	bool DoWithEntityByID(UInt32 a_UniqueID, cEntityCallback a_Callback);  // Exported in ManualBindings.cpp

	/** Compares clients of two chunks, calls the callback accordingly */
	void CompareChunkClients(int a_ChunkX1, int a_ChunkZ1, int a_ChunkX2, int a_ChunkZ2, cClientDiffCallback & a_Callback);

	/** Adds client to a chunk, if not already present; returns true if added, false if present */
	bool AddChunkClient(int a_ChunkX, int a_ChunkZ, cClientHandle * a_Client);

	/** Removes client from the chunk specified */
	void RemoveChunkClient(int a_ChunkX, int a_ChunkZ, cClientHandle * a_Client);

	/** Removes the client from all chunks it is present in */
	void RemoveClientFromChunks(cClientHandle * a_Client);

	/** Sends the chunk to the client specified, if the client doesn't have the chunk yet.
	If chunk not valid, the request is postponed (ChunkSender will send that chunk when it becomes valid + lighted). */
	void SendChunkTo(int a_ChunkX, int a_ChunkZ, cChunkSender::Priority a_Priority, cClientHandle * a_Client);

	/** Sends the chunk to the client specified, even if the client already has the chunk.
	If the chunk's not valid, the request is postponed (ChunkSender will send that chunk when it becomes valid + lighted). */
	void ForceSendChunkTo(int a_ChunkX, int a_ChunkZ, cChunkSender::Priority a_Priority, cClientHandle * a_Client);

	/** Queues the chunk for preparing - making sure that it's generated and lit.
	The specified chunk is queued to be loaded or generated, and lit if needed.
	The specified callback is called after the chunk has been prepared. If there's no preparation to do, only the callback is called.
	It is legal to call with no callback. */
	void PrepareChunk(int a_ChunkX, int a_ChunkZ, std::unique_ptr<cChunkCoordCallback> a_CallAfter = {});

	/** Marks the chunk as failed-to-load: */
	void ChunkLoadFailed(int a_ChunkX, int a_ChunkZ);

	/** Sets the sign text, asking plugins for permission first. a_Player is the player who this change belongs to, may be nullptr. Returns true if sign text changed. */
	bool SetSignLines(Vector3i a_BlockPos, const AString & a_Line1, const AString & a_Line2, const AString & a_Line3, const AString & a_Line4, cPlayer * a_Player = nullptr);  // Exported in ManualBindings.cpp

	/** Sets the command block command. Returns true if command changed. */
	bool SetCommandBlockCommand(int a_BlockX, int a_BlockY, int a_BlockZ, const AString & a_Command);  // tolua_export
	bool SetCommandBlockCommand(Vector3i a_BlockPos, const AString & a_Command)
	{
		return SetCommandBlockCommand(a_BlockPos.x, a_BlockPos.y, a_BlockPos.z, a_Command);
	}

	/** Is the trapdoor open? Returns false if there is no trapdoor at the specified coords. */
	bool IsTrapdoorOpen(int a_BlockX, int a_BlockY, int a_BlockZ);                                      // tolua_export

	/** Set the state of a trapdoor. Returns true if the trapdoor was updated, false if there was no trapdoor at those coords. */
	bool SetTrapdoorOpen(int a_BlockX, int a_BlockY, int a_BlockZ, bool a_Open);                        // tolua_export

	/** Regenerate the given chunk. */
	void RegenerateChunk(int a_ChunkX, int a_ChunkZ);  // tolua_export

	/** Generates the given chunk. */
	void GenerateChunk(int a_ChunkX, int a_ChunkZ);  // tolua_export

	/** Queues a chunk for lighting; a_Callback is called after the chunk is lighted */
	void QueueLightChunk(int a_ChunkX, int a_ChunkZ, std::unique_ptr<cChunkCoordCallback> a_Callback = {});

	bool IsChunkLighted(int a_ChunkX, int a_ChunkZ);

	/** Calls the callback for each chunk in the coords specified (all cords are inclusive). Returns true if all chunks have been processed successfully */
	virtual bool ForEachChunkInRect(int a_MinChunkX, int a_MaxChunkX, int a_MinChunkZ, int a_MaxChunkZ, cChunkDataCallback & a_Callback) override;

	/** Calls the callback for each loaded chunk. Returns true if all chunks have been processed successfully */
	bool ForEachLoadedChunk(cFunctionRef<bool(int, int)> a_Callback);

	/** Sets the block at the specified coords to the specified value.
	Full processing, incl. updating neighbors, is performed. */
	void SetBlock(Vector3i a_BlockPos, BlockState a_Block);

	/** Sets the block at the specified coords to the specified value.
	The replacement doesn't trigger block updates, nor wake up simulators.
	The replaced blocks aren't checked for block entities (block entity is leaked if it exists at this block) */
	void FastSetBlock(Vector3i a_BlockPos, BlockState a_Block)
	{
		m_ChunkMap.FastSetBlock(a_BlockPos, a_Block);
	}

	/** Returns the block type at the specified position. */
	BlockState GetBlock(Vector3i a_BlockPos) const
	{
		return m_ChunkMap.GetBlock(a_BlockPos);
	}

	bool GetBlock(Vector3i a_BlockPos, BlockState & a_Block) const;

	/** Returns the sky light value at the specified block position.
	The sky light is "raw" - not affected by time-of-day.
	Returns 0 if chunk not valid. */
	LIGHTTYPE GetBlockSkyLight(Vector3i a_BlockPos) const;

	/** Returns the block-light value at the specified block position.
	Returns 0 if chunk not valid. */
	LIGHTTYPE GetBlockBlockLight(Vector3i a_BlockPos) const;

	/** Queries the whole block specification from the world.
	Returns true if all block info was retrieved successfully, false if not (invalid chunk / bad position).
	Exported in ManualBindings_World.cpp. */
	bool GetBlockInfo(Vector3i a_BlockPos, BlockState & a_Block, LIGHTTYPE & a_SkyLight, LIGHTTYPE & a_BlockLight) const;

	// TODO: NIBBLETYPE GetBlockActualLight(int a_BlockX, int a_BlockY, int a_BlockZ);

	/** Writes the block area into the specified coords.
	Returns true if all chunks have been processed.
	Prefer cBlockArea::Write() instead, this is the internal implementation; cBlockArea does error checking, too.
	a_DataTypes is a bitmask of cBlockArea::baXXX constants ORed together.
	Doesn't wake up simulators, use WakeUpSimulatorsInArea() for that. */
	virtual bool WriteBlockArea(cBlockArea & a_Area, int a_MinBlockX, int a_MinBlockY, int a_MinBlockZ, int a_DataTypes) override;

	// tolua_begin

	/** Spawns item pickups for each item in the list.
	The initial position of the pickups is at the center of the specified block, with a small random offset.
	May compress pickups if too many entities. */
	void SpawnItemPickups(const cItems & a_Pickups, Vector3i a_BlockPos, double a_FlyAwaySpeed = 1.0, bool a_IsPlayerCreated = false);

	/** Spawns item pickups for each item in the list.
	May compress pickups if too many entities. */
	void SpawnItemPickups(const cItems & a_Pickups, Vector3d a_Pos, double a_FlyAwaySpeed = 1.0, bool a_IsPlayerCreated = false);

	/** OBSOLETE, use the Vector3d-based overload instead.
	Spawns item pickups for each item in the list.
	May compress pickups if too many entities. */
	virtual void SpawnItemPickups(const cItems & a_Pickups, double a_BlockX, double a_BlockY, double a_BlockZ, double a_FlyAwaySpeed = 1.0, bool a_IsPlayerCreated = false) override
	{
		return SpawnItemPickups(a_Pickups, Vector3d{a_BlockX, a_BlockY, a_BlockZ}, a_FlyAwaySpeed, a_IsPlayerCreated);
	}

	/** Spawns item pickups for each item in the list. May compress pickups if too many entities. All pickups get the speed specified. */
	void SpawnItemPickups(const cItems & a_Pickups, Vector3d a_Pos, Vector3d a_Speed, bool a_IsPlayerCreated = false);

	/** OBSOLETE, use the Vector3d-based overload instead.
	Spawns item pickups for each item in the list. May compress pickups if too many entities. All pickups get the speed specified. */
	virtual void SpawnItemPickups(const cItems & a_Pickups, double a_BlockX, double a_BlockY, double a_BlockZ, double a_SpeedX, double a_SpeedY, double a_SpeedZ, bool a_IsPlayerCreated = false) override
	{
		return SpawnItemPickups(a_Pickups, {a_BlockX, a_BlockY, a_BlockZ}, {a_SpeedX, a_SpeedY, a_SpeedZ}, a_IsPlayerCreated);
	}

	/** Spawns a single pickup containing the specified item. */
	UInt32 SpawnItemPickup(Vector3d a_Pos, const cItem & a_Item, Vector3f a_Speed, int a_LifetimeTicks = 6000, bool a_CanCombine = true);

	/** OBSOLETE, use the Vector3d-based overload instead.
	Spawns a single pickup containing the specified item. */
	virtual UInt32 SpawnItemPickup(double a_PosX, double a_PosY, double a_PosZ, const cItem & a_Item, float a_SpeedX = 0.f, float a_SpeedY = 0.f, float a_SpeedZ = 0.f, int a_LifetimeTicks = 6000, bool a_CanCombine = true) override
	{
		return SpawnItemPickup({a_PosX, a_PosY, a_PosZ}, a_Item, {a_SpeedX, a_SpeedY, a_SpeedZ}, a_LifetimeTicks, a_CanCombine);
	}

	/** Spawns an falling block entity at the given position.
	Returns the UniqueID of the spawned falling block, or cEntity::INVALID_ID on failure. */
	UInt32 SpawnFallingBlock(Vector3d a_Pos, BlockState a_Block);

	/** Spawns an falling block entity at the given position.
	Returns the UniqueID of the spawned falling block, or cEntity::INVALID_ID on failure. */
	UInt32 SpawnFallingBlock(Vector3i a_BlockPos, BlockState a_Block)
	{
		// When creating from a block position (Vector3i), move the spawn point to the middle of the block by adding (0.5, 0, 0.5)
		return SpawnFallingBlock(Vector3d(0.5, 0, 0.5) + a_BlockPos, a_Block);
	}

	/** OBSOLETE, use the Vector3-based overload instead.
	Spawns an falling block entity at the given position.
	Returns the UniqueID of the spawned falling block, or cEntity::INVALID_ID on failure. */
	UInt32 SpawnFallingBlock(int a_X, int a_Y, int a_Z, BlockState a_Block)
	{
		return SpawnFallingBlock(Vector3i{a_X, a_Y, a_Z}, a_Block);
	}

	/** Spawns an minecart at the given coordinates.
	Returns the UniqueID of the spawned minecart, or cEntity::INVALID_ID on failure. */
	UInt32 SpawnMinecart(Vector3d a_Pos, Item a_MinecartType, const cItem & a_Content = cItem(), int a_BlockHeight = 1);

	/** OBSOLETE, use the Vector3d-based overload instead.
	Spawns an minecart at the given coordinates.
	Returns the UniqueID of the spawned minecart, or cEntity::INVALID_ID on failure. */
	UInt32 SpawnMinecart(double a_X, double a_Y, double a_Z, Item a_MinecartType, const cItem & a_Content = cItem(), int a_BlockHeight = 1)
	{
		return SpawnMinecart({a_X, a_Y, a_Z}, a_MinecartType, a_Content, a_BlockHeight);
	}

	// DEPRECATED, use the vector-parametered version instead.
	UInt32 SpawnBoat(double a_X, double a_Y, double a_Z, cBoat::eMaterial a_Material)
	{
		LOGWARNING("cWorld::SpawnBoat(double, double, double) is deprecated, use cWorld::SpawnBoat(Vector3d) instead.");
		return SpawnBoat({a_X, a_Y, a_Z}, a_Material);
	}

	/** Spawns a boat at the given coordinates.
	Returns the UniqueID of the spawned boat, or cEntity::INVALID_ID on failure. */
	UInt32 SpawnBoat(Vector3d a_Pos, cBoat::eMaterial a_Material);

	/** Spawns an experience orb at the given location with the given reward.
	Returns the UniqueID of the spawned experience orb, or cEntity::INVALID_ID on failure. */
	UInt32 SpawnExperienceOrb(Vector3d a_Pos, int a_Reward);

	/** OBSOLETE, use the Vector3d-based overload instead.
	Spawns an experience orb at the given location with the given reward.
	Returns the UniqueID of the spawned experience orb, or cEntity::INVALID_ID on failure. */
	virtual UInt32 SpawnExperienceOrb(double a_X, double a_Y, double a_Z, int a_Reward) override
	{
		return SpawnExperienceOrb({a_X, a_Y, a_Z}, a_Reward);
	}

	// tolua_end

	/** Spawns experience orbs of the specified total value at the given location. The orbs' values are split according to regular Minecraft rules.
	Returns an vector of UniqueID of all the orbs. */
	virtual std::vector<UInt32> SpawnSplitExperienceOrbs(Vector3d a_Pos, int a_Reward) override;  // Exported in ManualBindings_World.cpp

	/** OBSOLETE, use the Vector3d-based overload instead.
	Spawns experience orbs of the specified total value at the given location. The orbs' values are split according to regular Minecraft rules.
	Returns an vector of UniqueID of all the orbs. */
	std::vector<UInt32> SpawnSplitExperienceOrbs(double a_X, double a_Y, double a_Z, int a_Reward)
	{
		return SpawnSplitExperienceOrbs({a_X, a_Y, a_Z}, a_Reward);
	}

	// tolua_begin

	// DEPRECATED, use the vector-parametered version instead.
	UInt32 SpawnPrimedTNT(double a_X, double a_Y, double a_Z, int a_FuseTimeInSec = 80, double a_InitialVelocityCoeff = 1, bool a_ShouldPlayFuseSound = true)
	{
		LOGWARNING("cWorld::SpawnPrimedTNT(double, double, double) is deprecated, use cWorld::SpawnPrimedTNT(Vector3d) instead.");
		return SpawnPrimedTNT({a_X, a_Y, a_Z}, a_FuseTimeInSec, a_InitialVelocityCoeff, a_ShouldPlayFuseSound);
	}

	/** Spawns a new primed TNT entity at the specified block coords and specified fuse duration.
	Initial velocity is given based on the relative coefficient provided.
	Returns the UniqueID of the created entity, or cEntity::INVALID_ID on failure. */
	UInt32 SpawnPrimedTNT(Vector3d a_Pos, int a_FuseTimeInSec = 80, double a_InitialVelocityCoeff = 1, bool a_ShouldPlayFuseSound = true);

	/** Spawns a new ender crystal at the specified block coords.
	Returns the UniqueID of the created entity, or cEntity::INVALID_ID on failure. */
	UInt32 SpawnEnderCrystal(Vector3d a_Pos, bool a_ShowBottom);

	// tolua_end

	/** Replaces the specified block with another, and calls the OnPlaced block handler.
	The OnBroken block handler is called for the replaced block. Wakes up the simulators.
	If the chunk for any of the blocks is not loaded, the set operation is ignored silently. */
	void PlaceBlock(const Vector3i a_Position, const BlockState a_Block);

	/** Retrieves block types of the specified blocks. If a chunk is not loaded, doesn't modify the block. Returns true if all blocks were read. */
	bool GetBlocks(sSetBlockVector & a_Blocks, bool a_ContinueOnFailure);

	using cWorldInterface::SendBlockTo;

	// tolua_begin

	/** Replaces the specified block with air, and calls the OnBroken block handler.
	Wakes up the simulators. Doesn't produce pickups, use DropBlockAsPickups() for that instead.
	Returns true on success, false if the chunk is not loaded. */
	bool DigBlock(Vector3i a_BlockPos, const cEntity * a_Digger = nullptr);

	/** OBSOLETE, use the Vector3-based overload instead.
	Replaces the specified block with air, and calls the apropriate block handlers (OnBreaking(), OnBroken()).
	Wakes up the simulators.
	Doesn't produce pickups, use DropBlockAsPickups() for that instead.
	Returns true on success, false if the chunk is not loaded. */
	bool DigBlock(int a_X, int a_Y, int a_Z, cEntity * a_Digger = nullptr)
	{
		return DigBlock({a_X, a_Y, a_Z}, a_Digger);
	}

	/** Digs the specified block, and spawns the appropriate pickups for it.
	a_Digger is an optional entity causing the digging, usually the player.
	a_Tool is an optional item used to dig up the block, used by the handlers (empty hand vs shears produce different pickups from leaves).
	An empty hand is assumed if a_Tool is nullptr.
	Returns true on success, false if the chunk is not loaded. */
	bool DropBlockAsPickups(Vector3i a_BlockPos, const cEntity * a_Digger = nullptr, const cItem * a_Tool = nullptr);

	/** Returns all the pickups that would result if the a_Digger dug up the block at a_BlockPos using a_Tool
	a_Digger is usually a player, but can be nullptr for natural causes.
	a_Tool is an optional item used to dig up the block, used by the handlers (empty hand vs shears produce different pickups from leaves).
	An empty hand is assumed if a_Tool is nullptr.
	Returns an empty cItems object if the chunk is not present. */
	cItems PickupsFromBlock(Vector3i a_BlockPos, const cEntity * a_Digger = nullptr, const cItem * a_Tool = nullptr);

	/** Sends the block at the specified coords to the player.
	Used mainly when plugins disable block-placing or block-breaking, to restore the previous block. */
	virtual void SendBlockTo(int a_X, int a_Y, int a_Z, const cPlayer & a_Player) override;

	/** Set default spawn at the given coordinates.
	Returns false if the new spawn couldn't be stored in the INI file. */
	bool SetSpawn(int a_X, int a_Y, int a_Z);

	int GetSpawnX(void) const { return m_SpawnX; }
	int GetSpawnY(void) const { return m_SpawnY; }
	int GetSpawnZ(void) const { return m_SpawnZ; }
	Vector3i GetSpawnPos() const
	{
		return {m_SpawnX, m_SpawnY, m_SpawnZ};
	}

	/** Wakes up the simulators for the specified block */
	virtual void WakeUpSimulators(Vector3i a_Block) override;

	/** Wakes up the simulators for the specified area of blocks */
	void WakeUpSimulatorsInArea(const cCuboid & a_Area);

	// tolua_end

	inline cSimulatorManager * GetSimulatorManager(void) { return m_SimulatorManager.get(); }

	inline cFluidSimulator * GetWaterSimulator(void) { return m_WaterSimulator; }
	inline cFluidSimulator * GetLavaSimulator (void) { return m_LavaSimulator; }
	inline cRedstoneSimulator * GetRedstoneSimulator(void) { return m_RedstoneSimulator; }

	/** Calls the callback for each block entity in the specified chunk; returns true if all block entities processed, false if the callback aborted by returning true */
	bool ForEachBlockEntityInChunk(int a_ChunkX, int a_ChunkZ, cBlockEntityCallback a_Callback);  // Exported in ManualBindings.cpp

	/** Does an explosion with the specified strength at the specified coordinates.
	Executes the HOOK_EXPLODING and HOOK_EXPLODED hooks as part of the processing.
	a_SourceData exact type depends on the a_Source, see the declaration of the esXXX constants in Defines.h for details.
	Exported to Lua manually in ManualBindings_World.cpp in order to support the variable a_SourceData param. */
	virtual void DoExplosionAt(double a_ExplosionSize, double a_BlockX, double a_BlockY, double a_BlockZ, bool a_CanCauseFire, eExplosionSource a_Source, void * a_SourceData) override;

	/** Calls the callback for the block entity at the specified coords; returns false if there's no block entity at those coords, and whatever the callback returns if found. */
	virtual bool DoWithBlockEntityAt(Vector3i a_Position, cBlockEntityCallback a_Callback) override;  // Exported in ManualBindings.cpp

	/** Retrieves the test on the sign at the specified coords; returns false if there's no sign at those coords, true if found */
	bool GetSignLines (int a_BlockX, int a_BlockY, int a_BlockZ, AString & a_Line1, AString & a_Line2, AString & a_Line3, AString & a_Line4);  // Exported in ManualBindings.cpp

	/** a_Player is using block entity at [x, y, z], handle that: */
	void UseBlockEntity(cPlayer * a_Player, int a_BlockX, int a_BlockY, int a_BlockZ) { m_ChunkMap.UseBlockEntity(a_Player, a_BlockX, a_BlockY, a_BlockZ); }  // tolua_export

	/** Calls the callback for the chunk specified, with ChunkMapCS locked.
	Returns false if the chunk doesn't exist, otherwise returns the same value as the callback */
	bool DoWithChunk(int a_ChunkX, int a_ChunkZ, cChunkCallback a_Callback);

	/** Calls the callback for the chunk at the block position specified, with ChunkMapCS locked.
	Returns false if the chunk isn't loaded, otherwise returns the same value as the callback */
	bool DoWithChunkAt(Vector3i a_BlockPos, cChunkCallback a_Callback);

	/** Imprints the specified blocks into the world, as long as each log block replaces only allowed blocks.
	a_Blocks specifies the logs, leaves, vines and possibly other blocks that comprise a single tree.
	Returns true if the tree is imprinted successfully, false otherwise. */
	bool GrowTreeImage(const sSetBlockVector & a_Blocks);

	/** Grows a tree at the specified coords.
	If the specified block is a sapling, the tree is grown from that sapling.
	Otherwise a tree is grown based on the biome.
	Returns true if the tree was grown, false if not (invalid chunk, insufficient space).
	Exported in DeprecatedBindings due to the obsolete int-based overload. */
	bool GrowTree(Vector3i a_BlockPos);

	/** Grows a tree from the sapling at the specified coords.
	If the sapling is a part of a large-tree sapling (2x2), a large tree growth is attempted.
	Returns true if the tree was grown, false if not (invalid chunk, insufficient space).
	Exported in DeprecatedBindings due to the obsolete int-based overload and obsolete additional SaplingMeta param. */
	bool GrowTreeFromSapling(Vector3i a_BlockPos);

	/** Grows a tree at the specified coords, based on the biome in the place.
	Returns true if the tree was grown, false if not (invalid chunk, insufficient space).
	Exported in DeprecatedBindings due to the obsolete int-based overload. */
	bool GrowTreeByBiome(Vector3i a_BlockPos);

	// tolua_begin

	/** Grows the plant at the specified position by at most a_NumStages.
	The block's Grow handler is invoked.
	Returns the number of stages the plant has grown, 0 if not a plant. */
	int GrowPlantAt(Vector3i a_BlockPos, char a_NumStages = 1);

	/** Grows the plant at the specified block to its ripe stage.
	Returns true if grown, false if not (invalid chunk, non-growable block, already ripe). */
	bool GrowRipePlant(Vector3i a_BlockPos);

	/** OBSOLETE, use the Vector3-based overload instead.
	Grows the plant at the specified block to its ripe stage.
	a_IsByBonemeal is obsolete, do not use.
	Returns true if grown, false if not (invalid chunk, non-growable block, already ripe). */
	bool GrowRipePlant(int a_BlockX, int a_BlockY, int a_BlockZ, bool a_IsByBonemeal = false)
	{
		UNUSED(a_IsByBonemeal);
		LOGWARNING("Warning: cWorld:GrowRipePlant function expects Vector3i-based coords rather than int-based coords. Emulating old-style call.");
		return GrowRipePlant({ a_BlockX, a_BlockY, a_BlockZ });
	}

	/** Returns the biome at the specified coords. Reads the biome from the chunk, if loaded, otherwise uses the world generator to provide the biome value */
	EMCSBiome GetBiomeAt(int a_BlockX, int a_BlockZ);

	/** Sets the biome at the specified coords. Returns true if successful, false if not (chunk not loaded).
	Doesn't resend the chunk to clients, use ForceSendChunkTo() for that. */
	bool SetBiomeAt(int a_BlockX, int a_BlockZ, EMCSBiome a_Biome);

	/** Sets the biome at the area. Returns true if successful, false if any subarea failed (chunk not loaded).
	(Re)sends the chunks to their relevant clients if successful. */
	bool SetAreaBiome(int a_MinX, int a_MaxX, int a_MinZ, int a_MaxZ, EMCSBiome a_Biome);

	/** Sets the biome at the area. Returns true if successful, false if any subarea failed (chunk not loaded).
	(Re)sends the chunks to their relevant clients if successful.
	The cuboid needn't be sorted. */
	bool SetAreaBiome(const cCuboid & a_Area, EMCSBiome a_Biome);

	/** Returns the name of the world */
	const AString & GetName(void) const { return m_WorldName; }

	/** Returns the data path to the world data */
	const AString & GetDataPath(void) const { return m_DataPath; }

	/** Returns the name of the world.ini file used by this world */
	const AString & GetIniFileName(void) const {return m_IniFileName; }

	/** Returns the associated scoreboard instance. */
	cScoreboard & GetScoreBoard(void) { return m_Scoreboard; }

	/** Returns the associated map manager instance. */
	cMapManager & GetMapManager(void) { return m_MapManager; }

	bool AreCommandBlocksEnabled(void) const { return m_bCommandBlocksEnabled; }
	void SetCommandBlocksEnabled(bool a_Flag) { m_bCommandBlocksEnabled = a_Flag; }

	eShrapnelLevel GetTNTShrapnelLevel(void) const { return m_TNTShrapnelLevel; }
	void SetTNTShrapnelLevel(eShrapnelLevel a_Flag) { m_TNTShrapnelLevel = a_Flag; }

	int GetMaxViewDistance(void) const { return m_MaxViewDistance; }
	void SetMaxViewDistance(int a_MaxViewDistance);

	bool ShouldUseChatPrefixes(void) const { return m_bUseChatPrefixes; }
	void SetShouldUseChatPrefixes(bool a_Flag) { m_bUseChatPrefixes = a_Flag; }

	bool ShouldBroadcastDeathMessages(void) const { return m_BroadcastDeathMessages; }
	bool ShouldBroadcastAchievementMessages(void) const { return m_BroadcastAchievementMessages; }


	AString GetLinkedNetherWorldName(void) const { return m_LinkedNetherWorldName; }
	void SetLinkedNetherWorldName(const AString & a_Name) { m_LinkedNetherWorldName = a_Name; }

	AString GetLinkedEndWorldName(void) const { return m_LinkedEndWorldName; }
	void SetLinkedEndWorldName(const AString & a_Name) { m_LinkedEndWorldName = a_Name; }

	AString GetLinkedOverworldName(void) const { return m_LinkedOverworldName; }
	void SetLinkedOverworldName(const AString & a_Name) { m_LinkedOverworldName = a_Name; }

	/** Returns or sets the minumim or maximum netherportal width */
	virtual int GetMinNetherPortalWidth(void) const override { return m_MinNetherPortalWidth; }
	virtual int GetMaxNetherPortalWidth(void) const override { return m_MaxNetherPortalWidth; }
	virtual void SetMinNetherPortalWidth(int a_NewMinWidth) override { m_MinNetherPortalWidth = a_NewMinWidth; }
	virtual void SetMaxNetherPortalWidth(int a_NewMaxWidth) override { m_MaxNetherPortalWidth = a_NewMaxWidth; }

	/** Returns or sets the minumim or maximum netherportal height */
	virtual int GetMinNetherPortalHeight(void) const override { return m_MinNetherPortalHeight; }
	virtual int GetMaxNetherPortalHeight(void) const override { return m_MaxNetherPortalHeight; }
	virtual void SetMinNetherPortalHeight(int a_NewMinHeight) override { m_MinNetherPortalHeight = a_NewMinHeight; }
	virtual void SetMaxNetherPortalHeight(int a_NewMaxHeight) override { m_MaxNetherPortalHeight = a_NewMaxHeight; }

	// tolua_end

	/** Saves all chunks immediately. Dangerous interface, may deadlock, use QueueSaveAllChunks() instead */
	void SaveAllChunks(void);

	/** Queues a task to save all chunks onto the tick thread. The prefferred way of saving chunks from external sources */
	void QueueSaveAllChunks(void);  // tolua_export

	/** Queues a task onto the tick thread. The task object will be deleted once the task is finished */
	void QueueTask(std::function<void(cWorld &)> a_Task);  // Exported in ManualBindings.cpp

	/** Queues a lambda task onto the tick thread, with the specified delay. */
	void ScheduleTask(cTickTime a_DelayTicks, std::function<void(cWorld &)> a_Task);

	/** Returns the number of chunks loaded	 */
	size_t GetNumChunks() const;  // tolua_export

	/** Returns the number of unused dirty chunks. That's the number of chunks that we can save and then unload. */
	size_t GetNumUnusedDirtyChunks(void) const;  // tolua_export

	/** Returns the number of chunks loaded and dirty, and in the lighting queue */
	void GetChunkStats(int & a_NumValid, int & a_NumDirty, int & a_NumInLightingQueue);

	// Various queues length queries (cannot be const, they lock their CS):
	inline size_t GetGeneratorQueueLength  (void) { return m_Generator.GetQueueLength();   }    // tolua_export
	inline size_t GetLightingQueueLength   (void) { return m_Lighting.GetQueueLength();    }    // tolua_export
	inline size_t GetStorageLoadQueueLength(void) { return m_Storage.GetLoadQueueLength(); }    // tolua_export
	inline size_t GetStorageSaveQueueLength(void) { return m_Storage.GetSaveQueueLength(); }    // tolua_export

	cLightingThread & GetLightingThread(void) { return m_Lighting; }

	void InitializeSpawn(void);

	/** Starts threads that belong to this world. */
	void Start();

	/** Stops threads that belong to this world (part of deinit).
	a_DeadlockDetect is used for tracking this world's age, detecting a possible deadlock. */
	void Stop(cDeadlockDetect & a_DeadlockDetect);

	/** Processes the blocks queued for ticking with a delay (m_BlockTickQueue[]) */
	void TickQueuedBlocks(void);

	struct BlockTickQueueItem
	{
		int X;
		int Y;
		int Z;
		int TicksToWait;
	};

	/** Queues the block to be ticked after the specified number of game ticks */
	void QueueBlockForTick(int a_BlockX, int a_BlockY, int a_BlockZ, int a_TicksToWait);  // tolua_export

	// tolua_begin
	/** Casts a thunderbolt at the specified coords */
	void CastThunderbolt(Vector3i a_Block);
	void CastThunderbolt(int a_BlockX, int a_BlockY, int a_BlockZ);  // DEPRECATED, use vector-parametered version instead

	/** Sets the specified weather; resets weather interval; asks and notifies plugins of the change */
	void SetWeather(eWeather a_NewWeather);

	/** Forces a weather change in the next game tick */
	void ChangeWeather(void);

	/** Returns the current weather. Instead of comparing values directly to the weather constants, use IsWeatherXXX() functions, if possible */
	eWeather GetWeather(void) const { return m_Weather; }

	/** Returns true if the current weather is sunny. */
	bool IsWeatherSunny(void) const { return (m_Weather == wSunny); }

	/** Returns true if it is sunny at the specified location. This takes into account biomes. */
	bool IsWeatherSunnyAt(int a_BlockX, int a_BlockZ) const;

	/** Returns true if the current weather is rainy. */
	bool IsWeatherRain(void) const { return (m_Weather == wRain); }

	/** Returns true if it is raining at the specified location. This takes into account biomes. */
	bool IsWeatherRainAt(int a_BlockX, int a_BlockZ)
	{
		return (IsWeatherRain() && !IsBiomeNoDownfall(GetBiomeAt(a_BlockX, a_BlockZ)));
	}

	/** Returns true if the current weather is stormy. */
	bool IsWeatherStorm(void) const { return (m_Weather == wStorm); }

	/** Returns true if the weather is stormy at the specified location. This takes into account biomes. */
	bool IsWeatherStormAt(int a_BlockX, int a_BlockZ)
	{
		return (IsWeatherStorm() && !IsBiomeNoDownfall(GetBiomeAt(a_BlockX, a_BlockZ)));
	}

	/** Returns true if the world currently has any precipitation - rain, storm or snow. */
	bool IsWeatherWet(void) const { return !IsWeatherSunny(); }

	/** Returns true if it is raining or storming at the specified location.
	This takes into account biomes. */
	virtual bool IsWeatherWetAt(int a_BlockX, int a_BlockZ) override;

	/** Returns true if it is raining or storming at the specified location,
	and the rain reaches (the bottom of) the specified block position. */
	virtual bool IsWeatherWetAtXYZ(Vector3i a_Position) override;

	/** Returns the seed of the world. */
	int GetSeed(void) const { return m_Generator.GetSeed(); }

	// tolua_end

	cChunkGeneratorThread & GetGenerator(void) { return m_Generator; }
	cWorldStorage &   GetStorage  (void) { return m_Storage; }
	cChunkMap *       GetChunkMap (void) { return &m_ChunkMap; }

	/** Causes the specified block to be ticked on the next Tick() call.
	Only one block coord per chunk may be set, a second call overwrites the first call */
	void SetNextBlockToTick(const Vector3i a_BlockPos);  // tolua_export

	int GetMaxSugarcaneHeight(void) const { return m_MaxSugarcaneHeight; }  // tolua_export
	int GetMaxCactusHeight   (void) const { return m_MaxCactusHeight; }     // tolua_export

	bool IsBlockDirectlyWatered(int a_BlockX, int a_BlockY, int a_BlockZ);  // tolua_export

	/** Spawns a mob of the specified type. Returns the mob's UniqueID if recognized and spawned, cEntity::INVALID_ID otherwise */
	virtual UInt32 SpawnMob(double a_PosX, double a_PosY, double a_PosZ, eEntityType a_MonsterType, bool a_Baby = false) override;  // tolua_export

	/** Wraps cEntity::Initialize, doing Monster-specific things before spawning the monster.
	Takes ownership of the given Monster reference. */
	UInt32 SpawnMobFinalize(std::unique_ptr<cMonster> a_Monster);

	/** Creates a projectile of the specified type. Returns the projectile's UniqueID if successful, cEntity::INVALID_ID otherwise
	Item parameter is currently used for Fireworks to correctly set entity metadata based on item metadata. */
	UInt32 CreateProjectile(Vector3d a_Pos, eEntityType a_Kind, cEntity * a_Creator, const cItem * a_Item, const Vector3d * a_Speed = nullptr);  // tolua_export

	/** OBSOLETE, use the Vector3d-based overload instead.
	Creates a projectile of the specified type. Returns the projectile's UniqueID if successful, cEntity::INVALID_ID otherwise
	Item parameter is currently used for Fireworks to correctly set entity metadata based on item metadata. */
	UInt32 CreateProjectile(double a_PosX, double a_PosY, double a_PosZ, eEntityType a_Kind, cEntity * a_Creator, const cItem * a_Item, const Vector3d * a_Speed = nullptr);  // tolua_export

	/** Returns a random number in range [0 .. a_Range]. */
	int GetTickRandomNumber(int a_Range);

	/** Appends all usernames starting with a_Text (case-insensitive) into Results */
	void TabCompleteUserName(const AString & a_Text, AStringVector & a_Results);

	/** Get the current darkness level based on the time */
	LIGHTTYPE GetSkyDarkness() { return m_SkyDarkness; }

	/** Increments (a_AlwaysTicked == true) or decrements (false) the m_AlwaysTicked counter for the specified chunk.
	If the m_AlwaysTicked counter is greater than zero, the chunk is ticked in the tick-thread regardless of
	whether it has any clients or not.
	This function allows nesting and task-concurrency (multiple separate tasks can request ticking and as long
	as at least one requests is active the chunk will be ticked). */
	void SetChunkAlwaysTicked(int a_ChunkX, int a_ChunkZ, bool a_AlwaysTicked = true);  // tolua_export

	/** Returns true if slimes should spawn in the chunk. */
	bool IsSlimeChunk(int a_ChunkX, int a_ChunkZ) const;  // tolua_export
private:

	class cTickThread:
		public cIsThread
	{
		using Super = cIsThread;

	public:

		cTickThread(cWorld & a_World);

	protected:
		cWorld & m_World;

		// cIsThread overrides:
		virtual void Execute(void) override;
	} ;



	/** Implementation of the callbacks that the ChunkGenerator uses to store new chunks and interface to plugins */
	class cChunkGeneratorCallbacks :
		public cChunkGeneratorThread::cChunkSink,
		public cChunkGeneratorThread::cPluginInterface
	{
		cWorld * m_World;

		// cChunkSink overrides:
		virtual void OnChunkGenerated  (cChunkDesc & a_ChunkDesc) override;
		virtual bool IsChunkValid      (cChunkCoords a_Coords) override;
		virtual bool HasChunkAnyClients(cChunkCoords a_Coords) override;
		virtual bool IsChunkQueued     (cChunkCoords a_Coords) override;

		// cPluginInterface overrides:
		virtual void CallHookChunkGenerating(cChunkDesc & a_ChunkDesc) override;
		virtual void CallHookChunkGenerated (cChunkDesc & a_ChunkDesc) override;

	public:
		cChunkGeneratorCallbacks(cWorld & a_World);
	} ;


	/** The maximum number of allowed unused dirty chunks for this world.
	Loaded from config, enforced every 10 seconds by freeing some unused dirty chunks
	if this was exceeded. */
	size_t m_UnusedDirtyChunksCap;

	AString m_WorldName;

	/** The path to the root directory for the world files. Does not including trailing path specifier. */
	AString m_DataPath;

	/** The name of the overworld that portals in this world should link to.
	Only has effect if this world is a Nether or End world. */
	AString m_LinkedOverworldName;

	AString m_IniFileName;

	/** Name of the storage schema used to load and save chunks */
	AString m_StorageSchema;

	int m_StorageCompressionFactor;

	/** Whether or not writing chunks to disk is currently enabled */
	std::atomic<bool> m_IsSavingEnabled;

	/** The dimension of the world, used by the client to provide correct lighting scheme */
	eDimension m_Dimension;

	bool m_IsSpawnExplicitlySet;
	int m_SpawnX;
	int m_SpawnY;
	int m_SpawnZ;

	// Variables defining the minimum and maximum size for a nether portal
	int m_MinNetherPortalWidth;
	int m_MaxNetherPortalWidth;
	int m_MinNetherPortalHeight;
	int m_MaxNetherPortalHeight;

	bool m_BroadcastDeathMessages;
	bool m_BroadcastAchievementMessages;

	bool   m_IsDaylightCycleEnabled;

	/** The age of the world.
	Monotonic, always increasing each game tick, persistent across server restart.
	We need sub-tick precision here, that's why we store the time in milliseconds and calculate ticks off of it. */
	std::chrono::milliseconds m_WorldAge;

	/** The fully controllable age of the world.
	A value used to calculate the current day, and time of day. Settable by plugins and players, and persistent.
	We need sub-tick precision here, that's why we store the time in milliseconds and calculate ticks off of it. */
	std::chrono::milliseconds m_WorldDate;

	/** The time since this world began, in ticks.
	Monotonic, but does not persist across restarts.
	Used for less important but heavy tasks that run periodically. These tasks don't need to follow wallclock time, and slowing their rate down if TPS drops is desirable. */
	cTickTimeLong m_WorldTickAge;

	std::chrono::milliseconds m_LastChunkCheck;  // The last WorldAge in which unloading and possibly saving was triggered.
	std::chrono::milliseconds m_LastSave;  // The last WorldAge in which save-all was triggerred.
	std::map<cMonster::eFamily, cTickTimeLong> m_LastSpawnMonster;  // The last WorldAge (in ticks) in which a monster was spawned (for each megatype of monster)  // MG TODO : find a way to optimize without creating unmaintenability (if mob IDs are becoming unrowed)

	LIGHTTYPE m_SkyDarkness;

	eGameMode m_GameMode;
	bool m_bEnabledPVP;
	bool m_bFarmlandTramplingEnabled;
	bool m_IsDeepSnowEnabled;
	bool m_ShouldLavaSpawnFire;
	bool m_VillagersShouldHarvestCrops;

	std::vector<BlockTickQueueItem *> m_BlockTickQueue;
	std::vector<BlockTickQueueItem *> m_BlockTickQueueCopy;  // Second is for safely removing the objects from the queue

	std::unique_ptr<cSimulatorManager>   m_SimulatorManager;
	std::unique_ptr<cSandSimulator>      m_SandSimulator;
	cFluidSimulator *                    m_WaterSimulator;
	cFluidSimulator *                    m_LavaSimulator;
	std::unique_ptr<cFireSimulator>      m_FireSimulator;
	cRedstoneSimulator * m_RedstoneSimulator;

	// Protect with chunk map CS
	std::vector<cPlayer *> m_Players;

	cWorldStorage m_Storage;

	unsigned int m_MaxPlayers;

	cChunkMap m_ChunkMap;

	bool m_bAnimals;
	std::set<eEntityType> m_AllowedMobs;

	eWeather m_Weather;
	int m_WeatherInterval;
	int m_MaxSunnyTicks, m_MinSunnyTicks;
	int m_MaxRainTicks,  m_MinRainTicks;
	int m_MaxThunderStormTicks, m_MinThunderStormTicks;
	float m_RainGradient, m_ThunderGradient;

	int  m_MaxCactusHeight;
	int  m_MaxSugarcaneHeight;
	/* TODO: Enable when functionality exists again
	bool m_IsBeetrootsBonemealable;
	bool m_IsCactusBonemealable;
	bool m_IsCarrotsBonemealable;
	bool m_IsCropsBonemealable;
	bool m_IsGrassBonemealable;
	bool m_IsMelonStemBonemealable;
	bool m_IsMelonBonemealable;
	bool m_IsPotatoesBonemealable;
	bool m_IsPumpkinStemBonemealable;
	bool m_IsPumpkinBonemealable;
	bool m_IsSaplingBonemealable;
	bool m_IsSugarcaneBonemealable;
	bool m_IsBigFlowerBonemealable;
	bool m_IsTallGrassBonemealable;
	*/

	/** Whether command blocks are enabled or not */
	bool m_bCommandBlocksEnabled;

	/** Whether prefixes such as [INFO] are prepended to SendMessageXXX() / BroadcastChatXXX() functions */
	bool m_bUseChatPrefixes;

	/** The level of DoExplosionAt() projecting random affected blocks as FallingBlock entities
	See the eShrapnelLevel enumeration for details
	*/
	eShrapnelLevel m_TNTShrapnelLevel;

	/** The maximum view distance that a player can have in this world. */
	int m_MaxViewDistance;

	/** Name of the nether world - where Nether portals should teleport.
	Only used when this world is an Overworld. */
	AString m_LinkedNetherWorldName;

	/** Name of the End world - where End portals should teleport.
	Only used when this world is an Overworld. */
	AString m_LinkedEndWorldName;

	/** The thread responsible for generating chunks. */
	cChunkGeneratorThread m_Generator;

	cScoreboard      m_Scoreboard;
	cMapManager      m_MapManager;

	/** The callbacks that the ChunkGenerator uses to store new chunks and interface to plugins */
	cChunkGeneratorCallbacks m_GeneratorCallbacks;

	cChunkSender     m_ChunkSender;
	cLightingThread  m_Lighting;
	cTickThread      m_TickThread;

	/** Guards the m_Tasks */
	cCriticalSection m_CSTasks;

	/** Tasks that have been queued onto the tick thread, possibly to be executed at target tick in the future; guarded by m_CSTasks */
	std::vector<std::pair<std::chrono::milliseconds, std::function<void(cWorld &)>>> m_Tasks;

	/** Guards m_EntitiesToAdd */
	cCriticalSection m_CSEntitiesToAdd;

	/** List of entities that are scheduled for adding, waiting for the Tick thread to add them. */
	std::vector<std::pair<OwnedEntity, cWorld *>> m_EntitiesToAdd;

	/** CS protecting m_SetChunkDataQueue. */
	cCriticalSection m_CSSetChunkDataQueue;

	/** Queue for the chunk data to be set into m_ChunkMap by the tick thread. Protected by m_CSSetChunkDataQueue */
	std::vector<SetChunkData> m_SetChunkDataQueue;

	void Tick(std::chrono::milliseconds a_Dt, std::chrono::milliseconds a_LastTickDurationMSec);

	/** Ticks all clients that are in this world. */
	void TickClients(std::chrono::milliseconds a_Dt);

	/** Handles the weather in each tick */
	void TickWeather(float a_Dt);

	/** Handles the mob spawning / moving / destroying each tick */
	void TickMobs(std::chrono::milliseconds a_Dt);

	/** Sets the chunk data queued in the m_SetChunkDataQueue queue into their chunk. */
	void TickQueuedChunkDataSets();

	/** Adds the entities queued in the m_EntitiesToAdd queue into their chunk.
	If the entity was a player, he is also added to the m_Players list. */
	void TickQueuedEntityAdditions(void);

	/** Executes all tasks queued onto the tick thread */
	void TickQueuedTasks(void);

	/** Unloads all chunks immediately. */
	void UnloadUnusedChunks(void);

	void UpdateSkyDarkness(void);

	/** Generates a random spawnpoint on solid land by walking chunks and finding their biomes */
	void GenerateRandomSpawn(int a_MaxSpawnRadius);

	/** Can the specified coordinates be used as a spawn point?
	Returns true if spawn position is valid and sets a_Y to the valid spawn height */
	bool CanSpawnAt(int a_X, int & a_Y, int a_Z);

	/** Check if player starting point is acceptable */
	bool CheckPlayerSpawnPoint(int a_PosX, int a_PosY, int a_PosZ)
	{
		return CheckPlayerSpawnPoint({a_PosX, a_PosY, a_PosZ});
	}
	bool CheckPlayerSpawnPoint(Vector3i a_Pos);

	/** Chooses a reasonable transition from the current weather to a new weather */
	eWeather ChooseNewWeather(void);

	/** Creates a new fluid simulator, loads its settings from the inifile (a_FluidName section) */
	cFluidSimulator * InitializeFluidSimulator(cIniFile & a_IniFile, const char * a_FluidName, BlockType a_Block);

	/** Creates a new redstone simulator. */
	cRedstoneSimulator * InitializeRedstoneSimulator(cIniFile & a_IniFile);

	/** Sets mob spawning values if nonexistant to their dimension specific defaults */
	void InitializeAndLoadMobSpawningValues(cIniFile & a_IniFile);

	/** Checks if the sapling at the specified block coord is a part of a large-tree sapling (2x2).
	If so, adjusts the coords so that they point to the northwest (XM ZM) corner of the sapling area and returns true.
	Returns false if not a part of large-tree sapling. */
	bool GetLargeTreeAdjustment(Vector3i & a_BlockPos, BlockState a_Block);
};  // tolua_export
