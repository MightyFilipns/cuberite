
#include "Globals.h"  // NOTE: MSVC stupidness requires this to be the same across all modules

#include "Player.h"
#include "../Mobs/Wolf.h"
#include "../Mobs/Horse.h"
#include "../BoundingBox.h"
#include "../ChatColor.h"
#include "../Server.h"
#include "../UI/InventoryWindow.h"
#include "../UI/WindowOwner.h"
#include "../Bindings/PluginManager.h"
#include "../BlockEntities/BlockEntity.h"
#include "../BlockEntities/EnderChestEntity.h"
#include "../Root.h"
#include "../Chunk.h"
#include "../Items/ItemHandler.h"
#include "../FastRandom.h"
#include "../ClientHandle.h"

#include "../WorldStorage/StatisticsSerializer.h"
#include "../CompositeChat.h"

#include "Blocks/BlockHandler.h"
#include "Blocks/BlockBed.h"
#include "Blocks/BlockSlab.h"
#include "Blocks/ChunkInterface.h"

#include "IniFile.h"
#include "JsonUtils.h"
#include "json/json.h"

#include "../Blocks/BlockAir.h"
#include "../Blocks/BlockFluid.h"

#include "../CraftingRecipes.h"





namespace
{

/** Returns the folder for the player data based on the UUID given.
This can be used both for online and offline UUIDs. */
AString GetUUIDFolderName(const cUUID & a_Uuid)
{
	AString UUID = a_Uuid.ToShortString();

	AString res("players/");
	res.append(UUID, 0, 2);
	res.push_back('/');
	return res;
}

}  // namespace (anonymous)





const int cPlayer::MAX_HEALTH = 20;

const int cPlayer::MAX_FOOD_LEVEL = 20;

// Number of ticks it takes to eat an item.
#define EATING_TICKS 30_tick

// 6000 ticks or 5 minutes
#define PLAYER_INVENTORY_SAVE_INTERVAL 6000

// Food saturation for newly-joined or just-respawned players.
#define RESPAWN_FOOD_SATURATION 5

#define XP_TO_LEVEL15 255
#define XP_PER_LEVEL_TO15 17
#define XP_TO_LEVEL30 825





cPlayer::BodyStanceCrouching::BodyStanceCrouching(cPlayer & a_Player)
{
	a_Player.SetSize(0.6f, 1.65f);
}





cPlayer::BodyStanceSleeping::BodyStanceSleeping(cPlayer & a_Player)
{
	a_Player.SetSize(0.2f, 0.2f);
}





cPlayer::BodyStanceStanding::BodyStanceStanding(cPlayer & a_Player)
{
	a_Player.SetSize(0.6f, 1.8f);
}





cPlayer::BodyStanceGliding::BodyStanceGliding(cPlayer & a_Player) :
	TicksElytraFlying(0)
{
	a_Player.SetSize(0.6f, 0.6f);
}





cPlayer::cPlayer(const std::shared_ptr<cClientHandle> & a_Client) :
	Super(etPlayer, 0.6f, 1.8f),
	m_BodyStance(BodyStanceStanding(*this)),
	m_Inventory(*this),
	m_EnderChestContents(9, 3),
	m_DefaultWorldPath(cRoot::Get()->GetDefaultWorld()->GetDataPath()),
	m_ClientHandle(a_Client),
	m_NormalMaxSpeed(1.0),
	m_SprintingMaxSpeed(1.3),
	m_FlyingMaxSpeed(1.0),
	m_IsChargingBow(false),
	m_IsFishing(false),
	m_IsFrozen(false),
	m_IsLeftHanded(false),
	m_IsTeleporting(false),
	m_EatingFinishTick(-1),
	m_BowCharge(0),
	m_FloaterID(cEntity::INVALID_ID),
	m_Team(nullptr),
	m_Spectating(nullptr),
	m_TicksUntilNextSave(PLAYER_INVENTORY_SAVE_INTERVAL),
	m_SkinParts(0)
{
	ASSERT(GetName().length() <= 16);  // Otherwise this player could crash many clients...

	m_InventoryWindow = new cInventoryWindow(*this);
	m_CurrentWindow = m_InventoryWindow;

	LoadFromDisk();
	SetMaxHealth(MAX_HEALTH);
	UpdateCapabilities();  // Only required for plugins listening to HOOK_SPAWNING_ENTITY to not read uninitialised variables.
}





cPlayer::~cPlayer(void)
{
	LOGD("Deleting cPlayer \"%s\" at %p, ID %d", GetName().c_str(), static_cast<void *>(this), GetUniqueID());

	// "Times ragequit":
	m_Stats.Custom[CustomStatistic::LeaveGame]++;

	SaveToDisk();

	delete m_InventoryWindow;

	LOGD("Player %p deleted", static_cast<void *>(this));
}





void cPlayer::OnLoseSpectated()
{
	m_ClientHandle->SendCameraSetTo(*this);
	m_ClientHandle->SendPlayerMoveLook();
	m_Spectating = nullptr;
}





void cPlayer::SendPlayerInventoryJoin()
{
	m_InventoryWindow->OpenedByPlayer(*this);
}





int cPlayer::CalcLevelFromXp(int a_XpTotal)
{
	// level 0 to 15
	if (a_XpTotal <= XP_TO_LEVEL15)
	{
		return a_XpTotal / XP_PER_LEVEL_TO15;
	}

	// level 30+
	if (a_XpTotal > XP_TO_LEVEL30)
	{
		return static_cast<int>((151.5 + sqrt( 22952.25 - (14 * (2220 - a_XpTotal)))) / 7);
	}

	// level 16 to 30
	return static_cast<int>((29.5 + sqrt( 870.25 - (6 * ( 360 - a_XpTotal)))) / 3);
}





const std::set<UInt32> & cPlayer::GetKnownRecipes() const
{
	return m_KnownRecipes;
}





int cPlayer::XpForLevel(int a_Level)
{
	// level 0 to 15
	if (a_Level <= 15)
	{
		return a_Level * XP_PER_LEVEL_TO15;
	}

	// level 30+
	if (a_Level >= 31)
	{
		return static_cast<int>((3.5 * a_Level * a_Level) - (151.5 * a_Level) + 2220);
	}

	// level 16 to 30
	return static_cast<int>((1.5 * a_Level * a_Level) - (29.5 * a_Level) + 360);
}





int cPlayer::GetXpLevel() const
{
	return CalcLevelFromXp(m_CurrentXp);
}





float cPlayer::GetXpPercentage() const
{
	int currentLevel = CalcLevelFromXp(m_CurrentXp);
	int currentLevel_XpBase = XpForLevel(currentLevel);

	return static_cast<float>(m_CurrentXp - currentLevel_XpBase) /
		static_cast<float>(XpForLevel(1 + currentLevel) - currentLevel_XpBase);
}





bool cPlayer::SetCurrentExperience(int a_CurrentXp)
{
	if (!(a_CurrentXp >= 0) || (a_CurrentXp > (std::numeric_limits<int>::max() - m_LifetimeTotalXp)))
	{
		LOGWARNING("Tried to update experiece with an invalid Xp value: %d", a_CurrentXp);
		return false;  // oops, they gave us a dodgey number
	}

	m_CurrentXp = a_CurrentXp;

	// Update experience:
	m_ClientHandle->SendExperience();

	return true;
}





void cPlayer::ResendRenderDistanceCenter()
{
	auto pos = GetPosition();
	m_ClientHandle->SendRenderDistanceCenter(cChunkDef::BlockToChunk({static_cast<int>(round(pos.x)), 0, static_cast<int>(round(pos.z))}));
}





int cPlayer::DeltaExperience(int a_Xp_delta)
{
	if (a_Xp_delta > (std::numeric_limits<int>().max() - m_CurrentXp))
	{
		// Value was bad, abort and report
		LOGWARNING("Attempt was made to increment Xp by %d, which overflowed the int datatype. Ignoring.", a_Xp_delta);
		return -1;  // Should we instead just return the current Xp?
	}

	m_CurrentXp += a_Xp_delta;

	// Make sure they didn't subtract too much
	m_CurrentXp = std::max(m_CurrentXp, 0);


	// Update total for score calculation
	if (a_Xp_delta > 0)
	{
		m_LifetimeTotalXp += a_Xp_delta;
	}

	LOGD("Player \"%s\" gained / lost %d experience, total is now: %d", GetName().c_str(), a_Xp_delta, m_CurrentXp);

	// Set experience to be updated:
	m_ClientHandle->SendExperience();

	return m_CurrentXp;
}





void cPlayer::StartChargingBow(void)
{
	LOGD("Player \"%s\" started charging their bow", GetName().c_str());
	m_IsChargingBow = true;
	m_BowCharge = 0;
	m_World->BroadcastEntityMetadata(*this, m_ClientHandle.get());
}





int cPlayer::FinishChargingBow(void)
{
	LOGD("Player \"%s\" finished charging their bow at a charge of %d", GetName().c_str(), m_BowCharge);
	int res = m_BowCharge;
	m_IsChargingBow = false;
	m_BowCharge = 0;
	m_World->BroadcastEntityMetadata(*this, m_ClientHandle.get());

	return res;
}





void cPlayer::CancelChargingBow(void)
{
	LOGD("Player \"%s\" cancelled charging their bow at a charge of %d", GetName().c_str(), m_BowCharge);
	m_IsChargingBow = false;
	m_BowCharge = 0;
	m_World->BroadcastEntityMetadata(*this, m_ClientHandle.get());
}





void cPlayer::SetTouchGround(bool a_bTouchGround)
{
	if (IsGameModeSpectator())  // You can fly through the ground in Spectator
	{
		return;
	}

	UNUSED(a_bTouchGround);
	/* Unfortunately whatever the reason, there are still desyncs in on-ground status between the client and server. For example:
		1. Walking off a ledge (whatever height)
		2. Initial login
	Thus, it is too risky to compare their value against ours and kick them for hacking */
}





void cPlayer::Heal(int a_Health)
{
	Super::Heal(a_Health);
	m_ClientHandle->SendHealth();
}





void cPlayer::SetFoodLevel(int a_FoodLevel)
{
	int FoodLevel = Clamp(a_FoodLevel, 0, MAX_FOOD_LEVEL);

	if (cRoot::Get()->GetPluginManager()->CallHookPlayerFoodLevelChange(*this, FoodLevel))
	{
		return;
	}

	m_FoodLevel = FoodLevel;
	m_ClientHandle->SendHealth();
}





void cPlayer::SetFoodSaturationLevel(double a_FoodSaturationLevel)
{
	m_FoodSaturationLevel = Clamp(a_FoodSaturationLevel, 0.0, static_cast<double>(m_FoodLevel));
}





void cPlayer::SetFoodTickTimer(int a_FoodTickTimer)
{
	m_FoodTickTimer = a_FoodTickTimer;
}





void cPlayer::SetFoodExhaustionLevel(double a_FoodExhaustionLevel)
{
	m_FoodExhaustionLevel = Clamp(a_FoodExhaustionLevel, 0.0, 40.0);
}





bool cPlayer::Feed(int a_Food, double a_Saturation)
{
	if (IsSatiated())
	{
		return false;
	}

	SetFoodSaturationLevel(m_FoodSaturationLevel + a_Saturation);
	SetFoodLevel(m_FoodLevel + a_Food);
	return true;
}





void cPlayer::AddFoodExhaustion(double a_Exhaustion)
{
	if (!(IsGameModeCreative() || IsGameModeSpectator()))
	{
		m_FoodExhaustionLevel = std::min(m_FoodExhaustionLevel + a_Exhaustion, 40.0);
	}
}





bool cPlayer::IsInBed(void) const
{
	return std::holds_alternative<BodyStanceSleeping>(m_BodyStance);
}





bool cPlayer::IsLeftHanded() const
{
	return m_IsLeftHanded;
}





bool cPlayer::IsStanding() const
{
	return std::holds_alternative<BodyStanceStanding>(m_BodyStance);
}





void cPlayer::TossItems(const cItems & a_Items)
{
	if (IsGameModeSpectator())  // Players can't toss items in spectator
	{
		return;
	}

	m_Stats.Custom[CustomStatistic::Drop] += static_cast<StatisticsManager::StatValue>(a_Items.Size());

	const auto Speed = (GetLookVector() + Vector3d(0, 0.2, 0)) * 6;  // A dash of height and a dollop of speed
	const auto Position = GetEyePosition() - Vector3d(0, 0.2, 0);  // Correct for eye-height weirdness
	m_World->SpawnItemPickups(a_Items, Position, Speed, true);  // 'true' because created by player
}





void cPlayer::SetIsInBed(const bool a_GoToBed)
{
	if (a_GoToBed && IsStanding())
	{
		m_BodyStance = BodyStanceSleeping(*this);
		m_World->BroadcastEntityAnimation(*this, EntityAnimation::PlayerEntersBed);
	}
	else if (!a_GoToBed && IsInBed())
	{
		m_BodyStance = BodyStanceStanding(*this);
		m_World->BroadcastEntityAnimation(*this, EntityAnimation::PlayerLeavesBed);
	}
}





void cPlayer::StartEating(void)
{
	// Set the timer:
	m_EatingFinishTick = m_World->GetWorldAge() + EATING_TICKS;

	// Send the packet:
	m_World->BroadcastEntityMetadata(*this);
}





void cPlayer::FinishEating(void)
{
	// Reset the timer:
	m_EatingFinishTick = -1_tick;

	// Send the packets:
	m_ClientHandle->SendEntityAnimation(*this, EntityAnimation::PlayerFinishesEating);
	m_World->BroadcastEntityMetadata(*this);

	// consume the item:
	cItem Item(GetEquippedItem());
	Item.m_ItemCount = 1;
	auto & ItemHandler = Item.GetHandler();
	if (!ItemHandler.EatItem(this, &Item))
	{
		return;
	}
	ItemHandler.OnFoodEaten(m_World, this, &Item);
}





void cPlayer::AbortEating(void)
{
	m_EatingFinishTick = -1_tick;
	m_World->BroadcastEntityMetadata(*this);
}





void cPlayer::ClearInventoryPaintSlots(void)
{
	// Clear the list of slots that are being inventory-painted. Used by cWindow only
	m_InventoryPaintSlots.clear();
}





void cPlayer::AddInventoryPaintSlot(int a_SlotNum)
{
	// Add a slot to the list for inventory painting. Used by cWindow only
	m_InventoryPaintSlots.push_back(a_SlotNum);
}





const cSlotNums & cPlayer::GetInventoryPaintSlots(void) const
{
	// Return the list of slots currently stored for inventory painting. Used by cWindow only
	return m_InventoryPaintSlots;
}





double cPlayer::GetMaxSpeed(void) const
{
	if (IsFlying())
	{
		return m_FlyingMaxSpeed;
	}
	else if (IsSprinting())
	{
		return m_SprintingMaxSpeed;
	}
	else
	{
		return m_NormalMaxSpeed;
	}
}





void cPlayer::SetNormalMaxSpeed(double a_Speed)
{
	m_NormalMaxSpeed = a_Speed;

	if (!m_IsFrozen)
	{
		// If we are frozen, we do not send this yet. We send when unfreeze() is called
		m_World->BroadcastEntityProperties(*this);
	}
}





void cPlayer::SetSprintingMaxSpeed(double a_Speed)
{
	m_SprintingMaxSpeed = a_Speed;

	if (!m_IsFrozen)
	{
		// If we are frozen, we do not send this yet. We send when unfreeze() is called
		m_World->BroadcastEntityProperties(*this);
	}
}





void cPlayer::SetFlyingMaxSpeed(double a_Speed)
{
	m_FlyingMaxSpeed = a_Speed;

	if (!m_IsFrozen)
	{
		// If we are frozen, we do not send this yet. We send when unfreeze() is called
		m_ClientHandle->SendPlayerAbilities();
	}
}





void cPlayer::SetCrouch(const bool a_ShouldCrouch)
{
	if (a_ShouldCrouch && IsStanding())
	{
		m_BodyStance = BodyStanceCrouching(*this);

		// Handle spectator mode detach:
		if (IsGameModeSpectator())
		{
			SpectateEntity(nullptr);
		}

		cRoot::Get()->GetPluginManager()->CallHookPlayerCrouched(*this);
	}
	else if (!a_ShouldCrouch && IsCrouched())
	{
		m_BodyStance = BodyStanceStanding(*this);
	}

	m_World->BroadcastEntityMetadata(*this);
}





void cPlayer::SetElytraFlight(const bool a_ShouldElytraFly)
{
	if (a_ShouldElytraFly && IsStanding() && !IsOnGround() && !IsInWater() && !IsRiding() && (GetEquippedChestplate().m_ItemType == Item::Elytra))
	{
		m_BodyStance = BodyStanceGliding(*this);
	}
	else if (!a_ShouldElytraFly && IsElytraFlying())
	{
		m_BodyStance = BodyStanceStanding(*this);
	}

	m_World->BroadcastEntityMetadata(*this);
}





void cPlayer::SetFlying(const bool a_ShouldFly)
{
	if (a_ShouldFly == m_IsFlying)
	{
		return;
	}

	m_IsFlying = a_ShouldFly;
	if (!m_IsFrozen)
	{
		// If we are frozen, we do not send this yet. We send when unfreeze() is called
		m_ClientHandle->SendPlayerAbilities();
	}
}





void cPlayer::SetLeftHanded(const bool a_IsLeftHanded)
{
	m_IsLeftHanded = a_IsLeftHanded;
	m_World->BroadcastEntityMetadata(*this);
}





void cPlayer::SetSprint(const bool a_ShouldSprint)
{
	if (a_ShouldSprint && IsStanding())
	{
		m_BodyStance = BodyStanceSprinting();
	}
	else if (!a_ShouldSprint && IsSprinting())
	{
		m_BodyStance = BodyStanceStanding(*this);
	}

	m_World->BroadcastEntityMetadata(*this);
	m_World->BroadcastEntityProperties(*this);
}





void cPlayer::SetCanFly(bool a_CanFly)
{
	if (a_CanFly == m_IsFlightCapable)
	{
		return;
	}

	m_IsFlightCapable = a_CanFly;
	m_ClientHandle->SendPlayerAbilities();
}





void cPlayer::SetCustomName(const AString & a_CustomName)
{
	if (m_CustomName == a_CustomName)
	{
		return;
	}

	m_World->BroadcastPlayerListRemovePlayer(*this);

	m_CustomName = a_CustomName;
	if (m_CustomName.length() > 16)
	{
		m_CustomName = m_CustomName.substr(0, 16);
	}

	m_World->BroadcastPlayerListAddPlayer(*this);
	m_World->BroadcastSpawnEntity(*this, m_ClientHandle.get());
}





void cPlayer::SetBedPos(const Vector3i a_Position)
{
	SetBedPos(a_Position, *m_World);
}





void cPlayer::SetBedPos(const Vector3i a_Position, const cWorld & a_World)
{
	m_RespawnPosition = a_Position;
	m_IsRespawnPointForced = false;
	m_SpawnWorldName = a_World.GetName();
}





void cPlayer::SetRespawnPosition(const Vector3i a_Position, const cWorld & a_World)
{
	m_RespawnPosition = a_Position;
	m_IsRespawnPointForced = true;
	m_SpawnWorldName = a_World.GetName();
}





cWorld * cPlayer::GetRespawnWorld()
{
	if (const auto World = cRoot::Get()->GetWorld(m_SpawnWorldName); World != nullptr)
	{
		return World;
	}

	return cRoot::Get()->GetDefaultWorld();
}





void cPlayer::NotifyNearbyWolves(cPawn * a_Opponent, bool a_IsPlayerInvolved)
{
	ASSERT(a_Opponent != nullptr);

	m_World->ForEachEntityInBox(cBoundingBox(GetPosition(), 16), [&] (cEntity & a_Entity)
		{
			if (a_Entity.IsMob())
			{
				auto & Mob = static_cast<cMonster&>(a_Entity);
				if (Mob.GetEntityType() == etWolf)
				{
					auto & Wolf = static_cast<cWolf&>(Mob);
					Wolf.ReceiveNearbyFightInfo(GetUUID(), a_Opponent, a_IsPlayerInvolved);
				}
			}
			return false;
		}
	);
}





void cPlayer::KilledBy(TakeDamageInfo & a_TDI)
{
	Super::KilledBy(a_TDI);

	if (m_Health > 0)
	{
		return;  //  not dead yet =]
	}

	m_IsVisible = false;  // So new clients don't see the player

	// Detach player from object / entity. If the player dies, the server still says
	// that the player is attached to the entity / object
	Detach();

	// Puke out all the items
	cItems Pickups;
	m_Inventory.CopyToItems(Pickups);
	m_Inventory.Clear();

	if (GetName() == "Notch")
	{
		Pickups.Add(cItem(Item::Apple));
	}
	m_Stats.Custom[CustomStatistic::Drop] +=  static_cast<StatisticsManager::StatValue>(Pickups.Size());

	m_World->SpawnItemPickups(Pickups, GetPosX(), GetPosY(), GetPosZ(), 10);
	SaveToDisk();  // Save it, yeah the world is a tough place !

	BroadcastDeathMessage(a_TDI);

	m_Stats.Custom[CustomStatistic::Deaths]++;
	m_Stats.Custom[CustomStatistic::TimeSinceDeath] = 0;

	m_World->GetScoreBoard().AddPlayerScore(GetName(), cObjective::otDeathCount, 1);
}





void cPlayer::Killed(const cEntity & a_Victim, eDamageType a_DamageType)
{
	cScoreboard & ScoreBoard = m_World->GetScoreBoard();

	if (a_Victim.IsPlayer())
	{
		m_Stats.Custom[CustomStatistic::PlayerKills]++;

		ScoreBoard.AddPlayerScore(GetName(), cObjective::otPlayerKillCount, 1);
	}
	else if (a_Victim.IsMob())
	{
		const auto & Monster = static_cast<const cMonster &>(a_Victim);

		if (Monster.GetMobFamily() == cMonster::mfHostile)
		{
			AwardAchievement(CustomStatistic::AchKillEnemy);
		}

		if ((Monster.GetEntityType() == etSkeleton) && (a_DamageType == eDamageType::dtRangedAttack))
		{
			const double DistX = GetPosX() - Monster.GetPosX();
			const double DistZ = GetPosZ() - Monster.GetPosZ();

			if ((DistX * DistX + DistZ * DistZ) >= 2500.0)
			{
				AwardAchievement(CustomStatistic::AchSnipeSkeleton);
			}
		}

		m_Stats.Custom[CustomStatistic::MobKills]++;
	}

	ScoreBoard.AddPlayerScore(GetName(), cObjective::otTotalKillCount, 1);
}





void cPlayer::Respawn(void)
{
	ASSERT(m_World != nullptr);

	m_Health = GetMaxHealth();
	SetInvulnerableTicks(20);

	// Reset food level:
	m_FoodLevel = MAX_FOOD_LEVEL;
	m_FoodSaturationLevel = RESPAWN_FOOD_SATURATION;
	m_FoodExhaustionLevel = 0;

	// Reset Experience
	m_CurrentXp = 0;
	m_LifetimeTotalXp = 0;
	// ToDo: send score to client? How?

	// Extinguish the fire:
	StopBurning();

	// Disable flying:
	SetFlying(false);

	if (!m_IsRespawnPointForced)
	{
		// Check if the bed is still present:
		if (!cBlockBedHandler::IsBlockBed(GetRespawnWorld()->GetBlock(m_RespawnPosition)))
		{
			const auto & DefaultWorld = *cRoot::Get()->GetDefaultWorld();

			// If not, reset spawn to default and inform:
			SetRespawnPosition(Vector3i(DefaultWorld.GetSpawnX(), DefaultWorld.GetSpawnY(), DefaultWorld.GetSpawnZ()), DefaultWorld);
			SendAboveActionBarMessage("Your home bed was missing or obstructed");
		}

		// TODO: bed obstruction check here
	}


	if (const auto RespawnWorld = GetRespawnWorld(); m_World != RespawnWorld)
	{
		MoveToWorld(*RespawnWorld, m_RespawnPosition, false, false);
	}
	else
	{
		m_ClientHandle->SendRespawn(m_World->GetDimension(), true);
		TeleportToCoords(m_RespawnPosition.x, m_RespawnPosition.y, m_RespawnPosition.z);
	}

	// The Notchian client enters a weird glitched state when trying to "resurrect" dead players
	// To prevent that, destroy the existing client-side entity, and create a new one with the same ID
	// This does not make any difference to more modern clients
	m_World->BroadcastDestroyEntity(*this, &*m_ClientHandle);
	m_World->BroadcastSpawnEntity(*this, &*m_ClientHandle);


	SetVisible(true);
}





double cPlayer::GetEyeHeight(void) const
{
	return GetEyePosition().y - GetPosY();
}





Vector3d cPlayer::GetEyePosition(void) const
{
	if (IsCrouched())
	{
		return GetPosition().addedY(1.54);
	}

	if (IsElytraFlying())
	{
		return GetPosition().addedY(0.4);
	}

	if (IsInBed())
	{
		return GetPosition().addedY(0.2);
	}

	return GetPosition().addedY(1.6);
}





bool cPlayer::IsGameModeCreative(void) const
{
	return (GetEffectiveGameMode() == gmCreative);
}





bool cPlayer::IsGameModeSurvival(void) const
{
	return (GetEffectiveGameMode() == gmSurvival);
}





bool cPlayer::IsGameModeAdventure(void) const
{
	return (GetEffectiveGameMode() == gmAdventure);
}





bool cPlayer::IsGameModeSpectator(void) const
{
	return (GetEffectiveGameMode() == gmSpectator);
}





bool cPlayer::CanMobsTarget(void) const
{
	return (IsGameModeSurvival() || IsGameModeAdventure()) && (m_Health > 0);
}





AString cPlayer::GetIP(void) const
{
	return m_ClientHandle->GetIPString();
}





void cPlayer::SetTeam(cTeam * a_Team)
{
	if (m_Team == a_Team)
	{
		return;
	}

	if (m_Team)
	{
		m_Team->RemovePlayer(GetName());
	}

	m_Team = a_Team;

	if (m_Team)
	{
		m_Team->AddPlayer(GetName());
	}
}





cTeam * cPlayer::UpdateTeam(void)
{
	if (m_World == nullptr)
	{
		SetTeam(nullptr);
	}
	else
	{
		cScoreboard & Scoreboard = m_World->GetScoreBoard();

		SetTeam(Scoreboard.QueryPlayerTeam(GetName()));
	}

	return m_Team;
}





void cPlayer::OpenWindow(cWindow & a_Window)
{
	if (cRoot::Get()->GetPluginManager()->CallHookPlayerOpeningWindow(*this, a_Window))
	{
		return;
	}

	if (&a_Window != m_CurrentWindow)
	{
		CloseWindow(false);
	}

	a_Window.OpenedByPlayer(*this);
	m_CurrentWindow = &a_Window;
	a_Window.SendWholeWindow(*GetClientHandle());
}





void cPlayer::CloseWindow(bool a_CanRefuse)
{
	if (m_CurrentWindow == nullptr)
	{
		m_CurrentWindow = m_InventoryWindow;
		return;
	}

	if (m_CurrentWindow->ClosedByPlayer(*this, a_CanRefuse) || !a_CanRefuse)
	{
		// Close accepted, go back to inventory window (the default):
		m_CurrentWindow = m_InventoryWindow;
	}
	else
	{
		// Re-open the window
		m_CurrentWindow->OpenedByPlayer(*this);
		m_CurrentWindow->SendWholeWindow(*GetClientHandle());
	}
}





void cPlayer::CloseWindowIfID(char a_WindowID, bool a_CanRefuse)
{
	if ((m_CurrentWindow == nullptr) || (m_CurrentWindow->GetWindowID() != a_WindowID))
	{
		return;
	}
	CloseWindow();
}





void cPlayer::SendMessage(const AString & a_Message)
{
	m_ClientHandle->SendChat(a_Message, mtCustom);
}





void cPlayer::SendMessageInfo(const AString & a_Message)
{
	m_ClientHandle->SendChat(a_Message, mtInformation);
}





void cPlayer::SendMessageFailure(const AString & a_Message)
{
	m_ClientHandle->SendChat(a_Message, mtFailure);
}





void cPlayer::SendMessageSuccess(const AString & a_Message)
{
	m_ClientHandle->SendChat(a_Message, mtSuccess);
}





void cPlayer::SendMessageWarning(const AString & a_Message)
{
	m_ClientHandle->SendChat(a_Message, mtWarning);
}





void cPlayer::SendMessageFatal(const AString & a_Message)
{
	m_ClientHandle->SendChat(a_Message, mtFailure);
}





void cPlayer::SendMessagePrivateMsg(const AString & a_Message, const AString & a_Sender)
{
	m_ClientHandle->SendChat(a_Message, mtPrivateMessage, a_Sender);
}





void cPlayer::SendMessage(const cCompositeChat & a_Message)
{
	m_ClientHandle->SendChat(a_Message);
}





void cPlayer::SendMessageRaw(const AString & a_MessageRaw, eChatType a_Type)
{
	m_ClientHandle->SendChatRaw(a_MessageRaw, a_Type);
}





void cPlayer::SendSystemMessage(const AString & a_Message)
{
	m_ClientHandle->SendChatSystem(a_Message, mtCustom);
}





void cPlayer::SendAboveActionBarMessage(const AString & a_Message)
{
	m_ClientHandle->SendChatAboveActionBar(a_Message, mtCustom);
}





void cPlayer::SendSystemMessage(const cCompositeChat & a_Message)
{
	m_ClientHandle->SendChatSystem(a_Message);
}





void cPlayer::SendAboveActionBarMessage(const cCompositeChat & a_Message)
{
	m_ClientHandle->SendChatAboveActionBar(a_Message);
}





const AString & cPlayer::GetName(void) const
{
	return m_ClientHandle->GetUsername();
}





void cPlayer::SetGameMode(eGameMode a_GameMode)
{
	if ((a_GameMode < gmMin) || (a_GameMode >= gmMax))
	{
		LOGWARNING("%s: Setting invalid gamemode: %d", GetName().c_str(), static_cast<UInt32>(a_GameMode));
		return;
	}

	if (m_GameMode == a_GameMode)
	{
		// New gamemode unchanged, we're done:
		return;
	}

	m_GameMode = a_GameMode;
	UpdateCapabilities();

	if (IsGameModeSpectator())
	{
		// Spectators cannot ride entities:
		Detach();
	}
	else
	{
		// Non-spectators may not spectate:
		SpectateEntity(nullptr);
	}

	m_ClientHandle->SendGameMode(a_GameMode);
	m_ClientHandle->SendInventorySlot(-1, -1, m_DraggingItem);
	m_World->BroadcastPlayerListUpdateGameMode(*this);
	m_World->BroadcastEntityMetadata(*this);
}





void cPlayer::UpdateCapabilities()
{
	if (IsGameModeCreative())
	{
		m_IsFlightCapable = true;
		m_IsVisible = true;
	}
	else if (IsGameModeSpectator())
	{
		m_DraggingItem.Empty();  // Clear the current dragging item of spectators.
		m_IsFlightCapable = true;
		m_IsFlying = true;  // Spectators are always in flight mode.
		m_IsVisible = false;  // Spectators are invisible.
	}
	else
	{
		m_IsFlightCapable = false;
		m_IsFlying = false;
		m_IsVisible = true;
	}
}





void cPlayer::AwardAchievement(const CustomStatistic a_Ach)
{
	// Check if the prerequisites are met:
	if (!m_Stats.SatisfiesPrerequisite(a_Ach))
	{
		return;
	}

	// Increment the statistic and check if we already have it:
	if (m_Stats.Custom[a_Ach]++ != 1)
	{
		return;
	}

	if (m_World->ShouldBroadcastAchievementMessages())
	{
		cCompositeChat Msg;
		Msg.SetMessageType(mtSuccess);
		// TODO: cCompositeChat should not use protocol-specific strings
		// Msg.AddShowAchievementPart(GetName(), nameNew);
		Msg.AddTextPart("Achievement get!");
		m_World->BroadcastChat(Msg);
	}

	// Achievement Get!
	m_ClientHandle->SendStatistics(m_Stats);
}





void cPlayer::TeleportToCoords(double a_PosX, double a_PosY, double a_PosZ)
{
	//  ask plugins to allow teleport to the new position.
	if (!cRoot::Get()->GetPluginManager()->CallHookEntityTeleport(*this, m_LastPosition, Vector3d(a_PosX, a_PosY, a_PosZ)))
	{
		m_IsTeleporting = true;

		Detach();
		SetPosition({a_PosX, a_PosY, a_PosZ});
		FreezeInternal(GetPosition(), false);

		m_ClientHandle->SendPlayerMoveLook();
	}
}





void cPlayer::Freeze(const Vector3d & a_Location)
{
	FreezeInternal(a_Location, true);
}





bool cPlayer::IsFrozen()
{
	return m_IsFrozen;
}





void cPlayer::Unfreeze()
{
	if (IsElytraFlying())
	{
		m_World->BroadcastEntityMetadata(*this);
	}

	m_ClientHandle->SendPlayerAbilities();
	m_World->BroadcastEntityProperties(*this);

	m_IsFrozen = false;
	BroadcastMovementUpdate(m_ClientHandle.get());
	GetClientHandle()->SendPlayerPosition();
}





void cPlayer::SendRotation(double a_YawDegrees, double a_PitchDegrees)
{
	SetYaw(a_YawDegrees);
	SetPitch(a_PitchDegrees);
	m_ClientHandle->SendPlayerMoveLook();
}





void cPlayer::SpectateEntity(cEntity * a_Target)
{
	if (a_Target == this)
	{
		// Canonicalise self-pointers:
		a_Target = nullptr;
	}

	if (m_Spectating == a_Target)
	{
		// Already spectating requested target:
		return;
	}

	if (a_Target == nullptr)
	{
		m_Spectating->OnLoseSpectator(*this);
		OnLoseSpectated();
		return;
	}

	m_Spectating = a_Target;
	a_Target->OnAcquireSpectator(*this);
	m_ClientHandle->SendCameraSetTo(*a_Target);
}





Vector3d cPlayer::GetThrowStartPos(void) const
{
	Vector3d res = GetEyePosition();

	// Adjust the position to be just outside the player's bounding box:
	res.x += 0.16 * cos(GetPitch());
	res.y += -0.1;
	res.z += 0.16 * sin(GetPitch());

	return res;
}





Vector3d cPlayer::GetThrowSpeed(double a_SpeedCoeff) const
{
	Vector3d res = GetLookVector();
	res.Normalize();

	// TODO: Add a slight random change (+-0.0075 in each direction)

	return res * a_SpeedCoeff;
}





eGameMode cPlayer::GetEffectiveGameMode(void) const
{
	return (m_GameMode == gmNotSet) ? m_World->GetGameMode() : m_GameMode;
}





void cPlayer::ForceSetSpeed(const Vector3d & a_Speed)
{
	SetSpeed(a_Speed);
}





void cPlayer::SetVisible(bool a_bVisible)
{
	if (a_bVisible && !m_IsVisible)
	{
		m_IsVisible = true;
	}
	if (!a_bVisible && m_IsVisible)
	{
		m_IsVisible = false;
	}

	m_World->BroadcastEntityMetadata(*this);
}





MTRand cPlayer::GetEnchantmentRandomProvider()
{
	return m_EnchantmentSeed;
}





void cPlayer::PermuteEnchantmentSeed()
{
	// Get a new random integer and save that as the seed:
	m_EnchantmentSeed = GetRandomProvider().RandInt<unsigned int>();
}





bool cPlayer::HasPermission(const AString & a_Permission) const
{
	if (a_Permission.empty())
	{
		// Empty permission request is always granted
		return true;
	}

	AStringVector Split = StringSplit(a_Permission, ".");

	// Iterate over all restrictions; if any matches, then return failure:
	for (auto & Restriction: m_SplitRestrictions)
	{
		if (PermissionMatches(Split, Restriction))
		{
			return false;
		}
	}  // for Restriction - m_SplitRestrictions[]

	// Iterate over all granted permissions; if any matches, then return success:
	for (auto & Permission: m_SplitPermissions)
	{
		if (PermissionMatches(Split, Permission))
		{
			return true;
		}
	}  // for Permission - m_SplitPermissions[]

	// No granted permission matches
	return false;
}





bool cPlayer::PermissionMatches(const AStringVector & a_Permission, const AStringVector & a_Template)
{
	// Check the sub-items if they are the same or there's a wildcard:
	size_t lenP = a_Permission.size();
	size_t lenT = a_Template.size();
	size_t minLen = std::min(lenP, lenT);
	for (size_t i = 0; i < minLen; i++)
	{
		if (a_Template[i] == "*")
		{
			// Has matched so far and now there's a wildcard in the template, so the permission matches:
			return true;
		}
		if (a_Permission[i] != a_Template[i])
		{
			// Found a mismatch
			return false;
		}
	}

	// So far all the sub-items have matched
	// If the sub-item count is the same, then the permission matches
	return (lenP == lenT);
}





AString cPlayer::GetColor(void) const
{
	if (m_MsgNameColorCode.empty() || (m_MsgNameColorCode == "-"))
	{
		// Color has not been assigned, return an empty string:
		return AString();
	}

	// Return the color, including the delimiter:
	return cChatColor::Delimiter + m_MsgNameColorCode;
}





AString cPlayer::GetPrefix(void) const
{
	return m_MsgPrefix;
}





AString cPlayer::GetSuffix(void) const
{
	return m_MsgSuffix;
}





AString cPlayer::GetPlayerListName() const
{
	const AString & Color = GetColor();

	if (HasCustomName())
	{
		return m_CustomName;
	}
	else if ((GetName().length() <= 14) && !Color.empty())
	{
		return fmt::format(FMT_STRING("{}{}"), Color, GetName());
	}
	else
	{
		return GetName();
	}
}





void cPlayer::SetDraggingItem(const cItem & a_Item)
{
	if (GetWindow() != nullptr)
	{
		m_DraggingItem = a_Item;
		GetClientHandle()->SendInventorySlot(-1, -1, m_DraggingItem);
	}
}





void cPlayer::TossEquippedItem(char a_Amount)
{
	cItems Drops;
	cItem DroppedItem(GetInventory().GetEquippedItem());
	if (!DroppedItem.IsEmpty())
	{
		char NewAmount = a_Amount;
		if (NewAmount > GetInventory().GetEquippedItem().m_ItemCount)
		{
			NewAmount = GetInventory().GetEquippedItem().m_ItemCount;  // Drop only what's there
		}

		GetInventory().GetHotbarGrid().ChangeSlotCount(GetInventory().GetEquippedSlotNum() /* Returns hotbar subslot, which HotbarGrid takes */, -a_Amount);

		DroppedItem.m_ItemCount = NewAmount;
		Drops.push_back(DroppedItem);
	}

	TossItems(Drops);
}





void cPlayer::ReplaceOneEquippedItemTossRest(const cItem & a_Item)
{
	auto PlacedCount = GetInventory().ReplaceOneEquippedItem(a_Item);
	char ItemCountToToss = a_Item.m_ItemCount - static_cast<char>(PlacedCount);

	if (ItemCountToToss == 0)
	{
		return;
	}

	cItem Pickup = a_Item;
	Pickup.m_ItemCount = ItemCountToToss;
	TossPickup(Pickup);
}





void cPlayer::TossHeldItem(char a_Amount)
{
	cItems Drops;
	cItem & Item = GetDraggingItem();
	if (!Item.IsEmpty())
	{
		char OriginalItemAmount = Item.m_ItemCount;
		Item.m_ItemCount = std::min(OriginalItemAmount, a_Amount);
		Drops.push_back(Item);

		if (OriginalItemAmount > a_Amount)
		{
			Item.m_ItemCount = OriginalItemAmount - a_Amount;
		}
		else
		{
			Item.Empty();
		}
	}

	TossItems(Drops);
}





void cPlayer::TossPickup(const cItem & a_Item)
{
	cItems Drops;
	Drops.push_back(a_Item);

	TossItems(Drops);
}





void cPlayer::LoadFromDisk()
{
	RefreshRank();

	Json::Value Root;
	const auto & UUID = GetUUID();
	const auto & FileName = GetUUIDFileName(UUID);

	try
	{
		// Load the data from the save file and parse:
		InputFileStream(FileName) >> Root;

		// Load the player stats.
		// We use the default world name (like bukkit) because stats are shared between dimensions / worlds.
		StatisticsSerializer::Load(m_Stats, m_DefaultWorldPath, UUID.ToLongString());
	}
	catch (const InputFileStream::failure &)
	{
		if (errno != ENOENT)
		{
			// Save file exists but unreadable:
			throw;
		}

		// This is a new player whom we haven't seen yet with no save file, let them have the defaults:
		LOG("Player \"%s\" (%s) save or statistics file not found, resetting to defaults", GetName().c_str(), UUID.ToShortString().c_str());
	}

	m_CurrentWorldName = Root.get("world", cRoot::Get()->GetDefaultWorld()->GetName()).asString();
	m_World = cRoot::Get()->GetWorld(m_CurrentWorldName);

	if (const auto & PlayerPosition = Root["position"]; PlayerPosition.size() == 3)
	{
		SetPosition(PlayerPosition[0].asDouble(), PlayerPosition[1].asDouble(), PlayerPosition[2].asDouble());
	}
	else
	{
		SetPosition(Vector3d(0.5, 0.5, 0.5) + Vector3i(m_World->GetSpawnX(), m_World->GetSpawnY(), m_World->GetSpawnZ()));
	}

	if (const auto & PlayerRotation = Root["rotation"]; PlayerRotation.size() == 3)
	{
		SetYaw  (PlayerRotation[0].asDouble());
		SetPitch(PlayerRotation[1].asDouble());
		SetRoll (PlayerRotation[2].asDouble());
	}

	m_Health              = Root.get("health",         MAX_HEALTH).asFloat();
	m_AirLevel            = Root.get("air",            MAX_AIR_LEVEL).asInt();
	m_FoodLevel           = Root.get("food",           MAX_FOOD_LEVEL).asInt();
	m_FoodSaturationLevel = Root.get("foodSaturation", RESPAWN_FOOD_SATURATION).asDouble();
	m_FoodTickTimer       = Root.get("foodTickTimer",  0).asInt();
	m_FoodExhaustionLevel = Root.get("foodExhaustion", 0).asDouble();
	m_LifetimeTotalXp     = Root.get("xpTotal",        0).asInt();
	m_CurrentXp           = Root.get("xpCurrent",      0).asInt();
	m_IsFlying            = Root.get("isflying",       0).asBool();
	m_EnchantmentSeed     = Root.get("enchantmentSeed", GetRandomProvider().RandInt<unsigned int>()).asUInt();

	Json::Value & JSON_KnownItems = Root["knownItems"];
	for (UInt32 i = 0; i < JSON_KnownItems.size(); i++)
	{
		cItem Item;
		Item.FromJson(JSON_KnownItems[i]);
		m_KnownItems.insert(Item);
	}

	const auto & RecipeNameMap = cRoot::Get()->GetCraftingRecipes()->GetRecipeNameMap();

	Json::Value & JSON_KnownRecipes = Root["knownRecipes"];
	for (UInt32 i = 0; i < JSON_KnownRecipes.size(); i++)
	{
		auto RecipeId = RecipeNameMap.find(JSON_KnownRecipes[i].asString());
		if (RecipeId != RecipeNameMap.end())
		{
			m_KnownRecipes.insert(RecipeId->second);
		}
	}

	m_GameMode = static_cast<eGameMode>(Root.get("gamemode", eGameMode_NotSet).asInt());

	m_Inventory.LoadFromJson(Root["inventory"]);
	m_Inventory.SetEquippedSlotNum(Root.get("equippedItemSlot", 0).asInt());

	cEnderChestEntity::LoadFromJson(Root["enderchestinventory"], m_EnderChestContents);

	m_RespawnPosition.x = Root.get("SpawnX", m_World->GetSpawnX()).asInt();
	m_RespawnPosition.y = Root.get("SpawnY", m_World->GetSpawnY()).asInt();
	m_RespawnPosition.z = Root.get("SpawnZ", m_World->GetSpawnZ()).asInt();
	m_IsRespawnPointForced = Root.get("SpawnForced", true).asBool();
	m_SpawnWorldName = Root.get("SpawnWorld", m_World->GetName()).asString();

	FLOGD("Player \"{0}\" with save file \"{1}\" is spawning at {2:.2f} in world \"{3}\"",
		GetName(), FileName, GetPosition(), m_World->GetName()
	);
}





void cPlayer::OpenHorseInventory()
{
	if (
		(m_AttachedTo == nullptr) ||
		!m_AttachedTo->IsMob()
	)
	{
		return;
	}

	auto & Mob = static_cast<cMonster &>(*m_AttachedTo);

	if (Mob.GetEntityType() != etHorse)
	{
		return;
	}

	auto & Horse = static_cast<cHorse &>(Mob);
	// The client sends requests for untame horses as well but shouldn't actually open
	if (Horse.IsTame())
	{
		Horse.PlayerOpenWindow(*this);
	}
}





void cPlayer::SaveToDisk()
{
	const auto & UUID = GetUUID();
	cFile::CreateFolderRecursive(GetUUIDFolderName(UUID));

	// create the JSON data
	Json::Value JSON_PlayerPosition;
	JSON_PlayerPosition.append(Json::Value(GetPosX()));
	JSON_PlayerPosition.append(Json::Value(GetPosY()));
	JSON_PlayerPosition.append(Json::Value(GetPosZ()));

	Json::Value JSON_PlayerRotation;
	JSON_PlayerRotation.append(Json::Value(GetYaw()));
	JSON_PlayerRotation.append(Json::Value(GetPitch()));
	JSON_PlayerRotation.append(Json::Value(GetRoll()));

	Json::Value JSON_Inventory;
	m_Inventory.SaveToJson(JSON_Inventory);

	Json::Value JSON_EnderChestInventory;
	cEnderChestEntity::SaveToJson(JSON_EnderChestInventory, m_EnderChestContents);

	Json::Value JSON_KnownItems;
	for (const auto & KnownItem : m_KnownItems)
	{
		Json::Value JSON_Item;
		KnownItem.GetJson(JSON_Item);
		JSON_KnownItems.append(JSON_Item);
	}

	Json::Value JSON_KnownRecipes;
	for (auto KnownRecipe : m_KnownRecipes)
	{
		auto Recipe = cRoot::Get()->GetCraftingRecipes()->GetRecipeById(KnownRecipe);
		JSON_KnownRecipes.append(Recipe->m_RecipeName);
	}

	Json::Value root;
	root["position"]            = JSON_PlayerPosition;
	root["rotation"]            = JSON_PlayerRotation;
	root["inventory"]           = JSON_Inventory;
	root["knownItems"]          = JSON_KnownItems;
	root["knownRecipes"]        = JSON_KnownRecipes;
	root["equippedItemSlot"]    = m_Inventory.GetEquippedSlotNum();
	root["enderchestinventory"] = JSON_EnderChestInventory;
	root["health"]              = m_Health;
	root["xpTotal"]             = m_LifetimeTotalXp;
	root["xpCurrent"]           = m_CurrentXp;
	root["air"]                 = m_AirLevel;
	root["food"]                = m_FoodLevel;
	root["foodSaturation"]      = m_FoodSaturationLevel;
	root["foodTickTimer"]       = m_FoodTickTimer;
	root["foodExhaustion"]      = m_FoodExhaustionLevel;
	root["isflying"]            = IsFlying();
	root["lastknownname"]       = GetName();
	root["SpawnX"]              = GetLastBedPos().x;
	root["SpawnY"]              = GetLastBedPos().y;
	root["SpawnZ"]              = GetLastBedPos().z;
	root["SpawnForced"]         = m_IsRespawnPointForced;
	root["SpawnWorld"]          = m_SpawnWorldName;
	root["enchantmentSeed"]     = m_EnchantmentSeed;
	root["world"]               = m_CurrentWorldName;
	root["gamemode"]            = static_cast<int>(m_GameMode);

	auto JsonData = JsonUtils::WriteStyledString(root);
	AString SourceFile = GetUUIDFileName(UUID);

	cFile f;
	if (!f.Open(SourceFile, cFile::fmWrite))
	{
		LOGWARNING("Error writing player \"%s\" to file \"%s\": cannot open file. Player will lose their progress",
			GetName().c_str(), SourceFile.c_str()
		);
		return;
	}
	if (f.Write(JsonData.c_str(), JsonData.size()) != static_cast<int>(JsonData.size()))
	{
		LOGWARNING("Error writing player \"%s\" to file \"%s\": cannot save data. Player will lose their progress",
			GetName().c_str(), SourceFile.c_str()
		);
		return;
	}

	try
	{
		// Save the player stats.
		// We use the default world name (like bukkit) because stats are shared between dimensions / worlds.
		// TODO: save together with player.dat, not in some other place.
		StatisticsSerializer::Save(m_Stats, m_DefaultWorldPath, GetUUID().ToLongString());
	}
	catch (...)
	{
		LOGWARNING("Error writing player \"%s\" statistics to file", GetName().c_str());
	}
}





void cPlayer::UseEquippedItem(short a_Damage)
{
	// No durability loss in creative or spectator modes:
	if (IsGameModeCreative() || IsGameModeSpectator())
	{
		return;
	}

	UseItem(cInventory::invHotbarOffset + m_Inventory.GetEquippedSlotNum(), a_Damage);
}





void cPlayer::UseEquippedItem(cItemHandler::eDurabilityLostAction a_Action)
{
	// Get item being used:
	cItem Item = GetEquippedItem();

	// Get base damage for action type:
	short Dmg = Item.GetHandler().GetDurabilityLossByAction(a_Action);

	UseEquippedItem(Dmg);
}





void cPlayer::UseItem(int a_SlotNumber, short a_Damage)
{
	const cItem & Item = m_Inventory.GetSlot(a_SlotNumber);

	if (Item.IsEmpty())
	{
		return;
	}

	// Ref: https://minecraft.wiki/w/Enchanting#Unbreaking
	unsigned int UnbreakingLevel = Item.m_Enchantments.GetLevel(cEnchantments::enchUnbreaking);
	double chance = ItemCategory::IsArmor(Item.m_ItemType)
		? (0.6 + (0.4 / (UnbreakingLevel + 1))) : (1.0 / (UnbreakingLevel + 1));

	// When durability is reduced by multiple points
	// Unbreaking is applied for each point of reduction.
	std::binomial_distribution<short> Dist(a_Damage, chance);
	short ReducedDamage = Dist(GetRandomProvider().Engine());

	if (m_Inventory.DamageItem(a_SlotNumber, ReducedDamage))
	{
		// The item broke. Broadcast the correct animation:
		if (Item.m_ItemType == Item::Shield)
		{
			m_World->BroadcastEntityAnimation(*this, EntityAnimation::PawnShieldBreaks);
		}
		else if (a_SlotNumber == (cInventory::invHotbarOffset + m_Inventory.GetEquippedSlotNum()))
		{
			m_World->BroadcastEntityAnimation(*this, EntityAnimation::PawnMainHandEquipmentBreaks);
		}
		else
		{
			switch (a_SlotNumber)
			{
				case cInventory::invArmorOffset:     return m_World->BroadcastEntityAnimation(*this, EntityAnimation::PawnHeadEquipmentBreaks);
				case cInventory::invArmorOffset + 1: return m_World->BroadcastEntityAnimation(*this, EntityAnimation::PawnChestEquipmentBreaks);
				case cInventory::invArmorOffset + 2: return m_World->BroadcastEntityAnimation(*this, EntityAnimation::PawnLegsEquipmentBreaks);
				case cInventory::invArmorOffset + 3: return m_World->BroadcastEntityAnimation(*this, EntityAnimation::PawnFeetEquipmentBreaks);
				case cInventory::invShieldOffset:    return m_World->BroadcastEntityAnimation(*this, EntityAnimation::PawnOffHandEquipmentBreaks);
			}
		}
	}
	else if (Item.m_ItemType == Item::Shield)
	{
		// The item survived. Special case for shield blocking:
		m_World->BroadcastEntityAnimation(*this, EntityAnimation::PawnShieldBlocks);
	}
}





void cPlayer::HandleFood(void)
{
	// Ref.: https://minecraft.wiki/w/Hunger

	if (IsGameModeCreative() || IsGameModeSpectator())
	{
		// Hunger is disabled for Creative and Spectator
		return;
	}

	// Apply food exhaustion that has accumulated:
	if (m_FoodExhaustionLevel > 4.0)
	{
		m_FoodExhaustionLevel -= 4.0;

		if (m_FoodSaturationLevel > 0.0)
		{
			m_FoodSaturationLevel = std::max(m_FoodSaturationLevel - 1.0, 0.0);
		}
		else
		{
			SetFoodLevel(m_FoodLevel - 1);
		}
	}

	// Heal or damage, based on the food level, using the m_FoodTickTimer:
	if ((m_FoodLevel >= 18) || (m_FoodLevel <= 0))
	{
		m_FoodTickTimer++;
		if (m_FoodTickTimer >= 80)
		{
			m_FoodTickTimer = 0;

			if ((m_FoodLevel >= 18) && (GetHealth() < GetMaxHealth()))
			{
				// Regenerate health from food, incur 3 pts of food exhaustion:
				Heal(1);
				AddFoodExhaustion(3.0);
			}
			else if ((m_FoodLevel <= 0) && (m_Health > 1))
			{
				// Damage from starving
				TakeDamage(dtStarving, nullptr, 1, 1, 0);
			}
		}
	}
	else
	{
		m_FoodTickTimer = 0;
	}
}





void cPlayer::HandleFloater()
{
	if (GetEquippedItem().m_ItemType == Item::FishingRod)
	{
		return;
	}
	m_World->DoWithEntityByID(m_FloaterID, [](cEntity & a_Entity)
		{
			a_Entity.Destroy();
			return true;
		}
	);
	SetIsFishing(false);
}





bool cPlayer::IsClimbing(void) const
{
	const auto Position = GetPosition().Floor();

	if (!cChunkDef::IsValidHeight(Position))
	{
		return false;
	}

	auto Block = m_World->GetBlock(Position);
	switch (Block.Type())
	{
		case BlockType::Ladder:
		case BlockType::Vine:
		{
			return true;
		}
		default: return false;
	}
}





void cPlayer::UpdateMovementStats(const Vector3d & a_DeltaPos, bool a_PreviousIsOnGround)
{
	if (m_IsTeleporting)
	{
		m_IsTeleporting = false;
		return;
	}

	const auto Value = FloorC<StatisticsManager::StatValue>(a_DeltaPos.Length() * 100 + 0.5);
	if (m_AttachedTo == nullptr)
	{
		if (IsFlying())
		{
			m_Stats.Custom[CustomStatistic::FlyOneCm] += Value;
			// May be flying and doing any of the following:
		}

		if (IsClimbing())
		{
			if (a_DeltaPos.y > 0.0)  // Going up
			{
				m_Stats.Custom[CustomStatistic::ClimbOneCm] += FloorC<StatisticsManager::StatValue>(a_DeltaPos.y * 100 + 0.5);
			}
		}
		else if (IsInWater())
		{
			if (m_IsHeadInWater)
			{
				m_Stats.Custom[CustomStatistic::WalkUnderWaterOneCm] += Value;
			}
			else
			{
				m_Stats.Custom[CustomStatistic::WalkOnWaterOneCm] += Value;
			}
			AddFoodExhaustion(0.00015 * static_cast<double>(Value));
		}
		else if (IsOnGround())
		{
			if (IsCrouched())
			{
				m_Stats.Custom[CustomStatistic::CrouchOneCm] += Value;
				AddFoodExhaustion(0.0001 * static_cast<double>(Value));
			}
			if (IsSprinting())
			{
				m_Stats.Custom[CustomStatistic::SprintOneCm] += Value;
				AddFoodExhaustion(0.001 * static_cast<double>(Value));
			}
			else
			{
				m_Stats.Custom[CustomStatistic::WalkOneCm] += Value;
				AddFoodExhaustion(0.0001 * static_cast<double>(Value));
			}
		}
		else
		{
			// If a jump just started, process food exhaustion:
			if ((a_DeltaPos.y > 0.0) && a_PreviousIsOnGround)
			{
				m_Stats.Custom[CustomStatistic::Jump]++;
				AddFoodExhaustion((IsSprinting() ? 0.008 : 0.002) * static_cast<double>(Value));
			}
			else if (a_DeltaPos.y < 0.0)
			{
				// Increment statistic
				m_Stats.Custom[CustomStatistic::FallOneCm] += static_cast<StatisticsManager::StatValue>(-a_DeltaPos.y * 100 + 0.5);
			}
			// TODO: good opportunity to detect illegal flight (check for falling tho)
		}
	}
	else
	{
		switch (m_AttachedTo->GetEntityType())
		{
			case etMinecart: m_Stats.Custom[CustomStatistic::MinecartOneCm] += Value; break;
			case etOakBoat:  m_Stats.Custom[CustomStatistic::BoatOneCm]     += Value; break;
			default: break;
		}
		if (m_AttachedTo->IsMob())
		{
			cMonster * Monster = static_cast<cMonster *>(m_AttachedTo);
			switch (Monster->GetEntityType())
			{
				case etPig:   m_Stats.Custom[CustomStatistic::PigOneCm]   += Value; break;
				case etHorse: m_Stats.Custom[CustomStatistic::HorseOneCm] += Value; break;
				default: break;
			}
		}
	}
}





void cPlayer::LoadRank(void)
{
	// Update our permissions:
	RefreshRank();

	// Send a permission level update:
	m_ClientHandle->SendPlayerPermissionLevel();
}





void cPlayer::SendBlocksAround(Vector3i a_BlockPos, int a_Range)
{
	// Collect the coords of all the blocks to send:
	sSetBlockVector blks;
	for (int y = a_BlockPos.y - a_Range + 1; y < a_BlockPos.y + a_Range; y++)
	{
		for (int z = a_BlockPos.z - a_Range + 1; z < a_BlockPos.z + a_Range; z++)
		{
			for (int x = a_BlockPos.x - a_Range + 1; x < a_BlockPos.x + a_Range; x++)
			{
				blks.emplace_back(x, y, z, Block::Air::Air());  // Use fake blocktype, it will get set later on.
			}
		}
	}  // for y

	// Get the values of all the blocks:
	if (!m_World->GetBlocks(blks, false))
	{
		LOGD("%s: Cannot query all blocks, not sending an update", __FUNCTION__);
		return;
	}

	// Divide the block changes by their respective chunks:
	std::unordered_map<cChunkCoords, sSetBlockVector, cChunkCoordsHash> Changes;
	for (const auto & blk: blks)
	{
		Changes[cChunkCoords(blk.m_ChunkX, blk.m_ChunkZ)].push_back(blk);
	}  // for blk - blks[]
	blks.clear();

	// Send the blocks for each affected chunk:
	for (auto itr = Changes.cbegin(), end = Changes.cend(); itr != end; ++itr)
	{
		m_ClientHandle->SendBlockChanges(itr->first.m_ChunkX, itr->first.m_ChunkZ, itr->second);
	}
}





bool cPlayer::DoesPlacingBlocksIntersectEntity(const std::initializer_list<sSetBlock> a_Blocks) const
{
	// Compute the bounding box for each block to be placed
	std::vector<cBoundingBox> PlacementBoxes;
	cBoundingBox PlacingBounds(0, 0, 0, 0, 0, 0);
	bool HasInitializedBounds = false;
	for (auto blk: a_Blocks)
	{
		int x = blk.GetX();
		int y = blk.GetY();
		int z = blk.GetZ();
		cBoundingBox BlockBox = cBlockHandler::For(blk.m_Block.Type()).GetPlacementCollisionBox(
			m_World->GetBlock({ x - 1, y, z }),
			m_World->GetBlock({ x + 1, y, z }),
			(y == 0) ? Block::Air::Air() : m_World->GetBlock({ x, y - 1, z }),
			(y == cChunkDef::Height - 1) ? Block::Air::Air() : m_World->GetBlock({ x, y + 1, z }),
			m_World->GetBlock({ x, y, z - 1 }),
			m_World->GetBlock({ x, y, z + 1 })
		);
		BlockBox.Move(x, y, z);

		PlacementBoxes.push_back(BlockBox);

		if (HasInitializedBounds)
		{
			PlacingBounds = PlacingBounds.Union(BlockBox);
		}
		else
		{
			PlacingBounds = BlockBox;
			HasInitializedBounds = true;
		}
	}

	cWorld * World = GetWorld();

	// Check to see if any entity intersects any block being placed
	return !World->ForEachEntityInBox(PlacingBounds, [&](cEntity & a_Entity)
		{
			// The distance inside the block the entity can still be.
			const double EPSILON = 0.0005;

			if (!a_Entity.DoesPreventBlockPlacement())
			{
				return false;
			}
			auto EntBox = a_Entity.GetBoundingBox();
			for (auto BlockBox : PlacementBoxes)
			{
				// Put in a little bit of wiggle room
				BlockBox.Expand(-EPSILON, -EPSILON, -EPSILON);
				if (EntBox.DoesIntersect(BlockBox))
				{
					return true;
				}
			}
			return false;
		}
	);
}





const cUUID & cPlayer::GetUUID(void) const
{
	return m_ClientHandle->GetUUID();
}





bool cPlayer::PlaceBlock(const Vector3i a_Position, BlockState a_Block)
{
	return PlaceBlocks({ { a_Position, a_Block } });
}





bool cPlayer::PlaceBlocks(const std::initializer_list<sSetBlock> a_Blocks)
{
	if (DoesPlacingBlocksIntersectEntity(a_Blocks))
	{
		// Abort - re-send all the current blocks in the a_Blocks' coords to the client:
		for (const auto & ResendBlock : a_Blocks)
		{
			m_World->SendBlockTo(ResendBlock.GetX(), ResendBlock.GetY(), ResendBlock.GetZ(), *this);
		}
		return false;
	}

	cPluginManager * pm = cPluginManager::Get();

	// Check the blocks CanBeAt, and call the "placing" hooks; if any fail, abort:
	for (const auto & Block : a_Blocks)
	{
		if (
			!m_World->DoWithChunkAt(Block.GetAbsolutePos(), [&Block](cChunk & a_Chunk)
			{
				return cBlockHandler::For(Block.m_Block.Type()).CanBeAt(a_Chunk, Block.GetRelativePos(), Block.m_Block);
			})
		)
		{
			return false;
		}

		if (pm->CallHookPlayerPlacingBlock(*this, Block))
		{
			// Abort - re-send all the current blocks in the a_Blocks' coords to the client:
			for (const auto & ResendBlock : a_Blocks)
			{
				m_World->SendBlockTo(ResendBlock.GetX(), ResendBlock.GetY(), ResendBlock.GetZ(), *this);
			}
			return false;
		}
	}

	cChunkInterface ChunkInterface(m_World->GetChunkMap());
	for (const auto & Block : a_Blocks)
	{
		// Set the blocks:
		m_World->PlaceBlock(Block.GetAbsolutePos(), Block.m_Block);

		// Call the "placed" hooks:
		pm->CallHookPlayerPlacedBlock(*this, Block);
	}

	return true;
}





void cPlayer::SetSkinParts(int a_Parts)
{
	m_SkinParts = a_Parts & spMask;
	m_World->BroadcastEntityMetadata(*this, m_ClientHandle.get());
}





AString cPlayer::GetUUIDFileName(const cUUID & a_UUID)
{
	AString UUID = a_UUID.ToLongString();

	AString res("players/");
	res.append(UUID, 0, 2);
	res.push_back('/');
	res.append(UUID, 2, AString::npos);
	res.append(".json");
	return res;
}





void cPlayer::FreezeInternal(const Vector3d & a_Location, bool a_ManuallyFrozen)
{
	SetSpeed(0, 0, 0);
	SetPosition(a_Location);
	m_IsFrozen = true;
	m_IsManuallyFrozen = a_ManuallyFrozen;

	double NormalMaxSpeed = GetNormalMaxSpeed();
	double SprintMaxSpeed = GetSprintingMaxSpeed();
	double FlyingMaxpeed = GetFlyingMaxSpeed();
	bool IsFlying = m_IsFlying;

	// Set the client-side speed to 0
	m_NormalMaxSpeed = 0;
	m_SprintingMaxSpeed = 0;
	m_FlyingMaxSpeed = 0;
	m_IsFlying = true;

	// Send the client its fake speed and max speed of 0
	m_ClientHandle->SendPlayerMoveLook();
	m_ClientHandle->SendPlayerAbilities();
	m_ClientHandle->SendEntityVelocity(*this);
	m_World->BroadcastEntityProperties(*this);

	// Keep the server side speed variables as they were in the first place
	m_NormalMaxSpeed = NormalMaxSpeed;
	m_SprintingMaxSpeed = SprintMaxSpeed;
	m_FlyingMaxSpeed = FlyingMaxpeed;
	m_IsFlying = IsFlying;
}





float cPlayer::GetLiquidHeightPercent(BlockState a_Block)
{
	auto Falloff = cBlockFluidHandler::GetFalloff(a_Block);
	if (Falloff >= 8)
	{
		Falloff = 0;
	}
	return static_cast<float>(Falloff + 1) / 9.0f;
}





bool cPlayer::IsInsideWater()
{
	const auto EyePos = GetEyePosition().Floor();

	if (!cChunkDef::IsValidHeight(EyePos))
	{
		// Not in water if in void.
		return false;
	}

	BlockState Block;
	if (m_World->GetBlock(EyePos, Block))
	{
		return false;
	}

	if (Block.Type() != BlockType::Water)
	{
		return false;
	}

	const auto EyeHeight = GetEyeHeight();
	float f = GetLiquidHeightPercent(Block) - 0.11111111f;
	float f1 = static_cast<float>(EyeHeight + 1) - f;
	return EyeHeight < f1;
}





float cPlayer::GetDigSpeed(BlockState a_Block)
{
	// Based on: https://minecraft.wiki/w/Breaking#Speed

	// Get the base speed multiplier of the equipped tool for the mined block
	float MiningSpeed = GetEquippedItem().GetHandler().GetBlockBreakingStrength(a_Block);

	// If we can harvest the block then we can apply material and enchantment bonuses
	if (GetEquippedItem().GetHandler().CanHarvestBlock(a_Block))
	{
		if (MiningSpeed > 1.0f)  // If the base multiplier for this block is greater than 1, now we can check enchantments
		{
			unsigned int EfficiencyModifier = GetEquippedItem().m_Enchantments.GetLevel(cEnchantments::eEnchantment::enchEfficiency);
			if (EfficiencyModifier > 0)  // If an efficiency enchantment is present, apply formula as on wiki
			{
				MiningSpeed += (EfficiencyModifier * EfficiencyModifier) + 1;
			}
		}
	}
	else  // If we can't harvest the block then no bonuses:
	{
		MiningSpeed = 1;
	}

	// Haste increases speed by 20% per level
	auto Haste = GetEntityEffect(cEntityEffect::effHaste);
	if (Haste != nullptr)
	{
		int intensity = Haste->GetIntensity() + 1;
		MiningSpeed *= 1.0f + (intensity * 0.2f);
	}

	// Mining fatigue decreases speed a lot
	auto MiningFatigue = GetEntityEffect(cEntityEffect::effMiningFatigue);
	if (MiningFatigue != nullptr)
	{
		int intensity = MiningFatigue->GetIntensity();
		switch (intensity)
		{
			case 0:  MiningSpeed *= 0.3f;     break;
			case 1:  MiningSpeed *= 0.09f;    break;
			case 2:  MiningSpeed *= 0.0027f;  break;
			default: MiningSpeed *= 0.00081f; break;

		}
	}

	// 5x speed loss for being in water
	if (IsInsideWater() && GetEquippedItem().m_Enchantments.GetLevel(cEnchantments::eEnchantment::enchAquaAffinity) <= 0)
	{
		MiningSpeed /= 5.0f;
	}

	// 5x speed loss for not touching ground
	if (!IsOnGround())
	{
		MiningSpeed /= 5.0f;
	}

	return MiningSpeed;
}





float cPlayer::GetMiningProgressPerTick(BlockState a_Block)
{
	// Based on https://minecraft.wiki/w/Breaking#Calculation
	// If we know it's instantly breakable then quit here:
	if (cBlockInfo::IsOneHitDig(a_Block))
	{
		return 1;
	}

	const bool CanHarvest = GetEquippedItem().GetHandler().CanHarvestBlock(a_Block);
	const float BlockHardness = cBlockInfo::GetHardness(a_Block) * (CanHarvest ? 1.5f : 5.0f);
	ASSERT(BlockHardness > 0);  // Can't divide by 0 or less, IsOneHitDig should have returned true

	// LOGD("Time to mine block = %f", BlockHardness/DigSpeed);
	// Number of ticks to mine = (20 * BlockHardness)/DigSpeed;
	// Therefore take inverse to get fraction mined per tick:
	return GetDigSpeed(a_Block) / (20.0f * BlockHardness);
}





bool cPlayer::CanInstantlyMine(BlockState a_Block)
{
	// Based on: https://minecraft.wiki/w/Breaking#Calculation

	// If the dig speed is greater than 30 times the hardness, then the wiki says we can instantly mine:
	return GetDigSpeed(a_Block) > (30 * cBlockInfo::GetHardness(a_Block));
}





void cPlayer::AddKnownItem(const cItem & a_Item)
{
	auto Response = m_KnownItems.insert(a_Item.CopyOne());
	if (!Response.second)
	{
		// The item was already known, bail out:
		return;
	}

	// Process the recipes that got unlocked by this newly-known item:
	auto Recipes = cRoot::Get()->GetCraftingRecipes()->FindNewRecipesForItem(a_Item, m_KnownItems);
	for (const auto & RecipeId : Recipes)
	{
		AddKnownRecipe(RecipeId);
	}
}





void cPlayer::AddKnownRecipe(UInt32 a_RecipeId)
{
	auto Response = m_KnownRecipes.insert(a_RecipeId);
	if (!Response.second)
	{
		// The recipe was already known, bail out:
		return;
	}
	m_ClientHandle->SendUnlockRecipe(a_RecipeId);
}





void cPlayer::TickFreezeCode()
{
	if (m_IsFrozen)
	{
		if ((!m_IsManuallyFrozen) && (GetClientHandle()->IsPlayerChunkSent()))
		{
			// If the player was automatically frozen, unfreeze if the chunk the player is inside is loaded and sent
			Unfreeze();

			// Pull the player out of any solids that might have loaded on them.
			PREPARE_REL_AND_CHUNK(GetPosition(), *(GetParentChunk()));
			if (RelSuccess)
			{
				int NewY = Rel.y;
				if (NewY < 0)
				{
					NewY = 0;
				}
				while (NewY < cChunkDef::Height - 2)
				{
					// If we find a position with enough space for the player
					if (
						!cBlockInfo::IsSolid(Chunk->GetBlock(Rel.x, NewY, Rel.z)) &&
						!cBlockInfo::IsSolid(Chunk->GetBlock(Rel.x, NewY + 1, Rel.z))
					)
					{
						// If the found position is not the same as the original
						if (NewY != Rel.y)
						{
							SetPosition(GetPosition().x, NewY, GetPosition().z);
							GetClientHandle()->SendPlayerPosition();
						}
						break;
					}
					++NewY;
				}
			}
		}
		else if ((GetWorld()->GetWorldTickAge() % 100_tick) == 0_tick)
		{
			// Despite the client side freeze, the player may be able to move a little by
			// Jumping or canceling flight. Re-freeze every now and then
			FreezeInternal(GetPosition(), m_IsManuallyFrozen);
		}
	}
	else
	{
		if (!GetClientHandle()->IsPlayerChunkSent() || (!GetParentChunk()->IsValid()))
		{
			FreezeInternal(GetPosition(), false);
		}
	}
}





void cPlayer::RefreshRank()
{
	const auto & UUID = GetUUID();
	cRankManager * RankMgr = cRoot::Get()->GetRankManager();

	// Load the values from cRankManager:
	m_Rank = RankMgr->GetPlayerRankName(UUID);
	if (m_Rank.empty())
	{
		m_Rank = RankMgr->GetDefaultRank();
	}
	else
	{
		// Update the name:
		RankMgr->UpdatePlayerName(UUID, GetName());
	}
	m_Permissions = RankMgr->GetPlayerPermissions(UUID);
	m_Restrictions = RankMgr->GetPlayerRestrictions(UUID);
	RankMgr->GetRankVisuals(m_Rank, m_MsgPrefix, m_MsgSuffix, m_MsgNameColorCode);

	// Break up the individual permissions on each dot, into m_SplitPermissions:
	m_SplitPermissions.clear();
	m_SplitPermissions.reserve(m_Permissions.size());
	for (auto & Permission : m_Permissions)
	{
		m_SplitPermissions.push_back(StringSplit(Permission, "."));
	}

	// Break up the individual restrictions on each dot, into m_SplitRestrictions:
	m_SplitRestrictions.clear();
	m_SplitRestrictions.reserve(m_Restrictions.size());
	for (auto & Restriction : m_Restrictions)
	{
		m_SplitRestrictions.push_back(StringSplit(Restriction, "."));
	}
}





void cPlayer::ApplyArmorDamage(int a_DamageBlocked)
{
	short ArmorDamage = static_cast<short>(std::max(a_DamageBlocked / 4, 1));

	for (int i = 0; i < 4; i++)
	{
		UseItem(cInventory::invArmorOffset + i, ArmorDamage);
	}
}





void cPlayer::BroadcastMovementUpdate(const cClientHandle * a_Exclude)
{
	if (!m_IsFrozen && m_Speed.SqrLength() > 0.001)
	{
		// If the player is not frozen, has a non-zero speed,
		// send the speed to the client so he is forced to move so:
		m_ClientHandle->SendEntityVelocity(*this);
	}

	// Since we do no physics processing for players, speed will otherwise never decrease:
	m_Speed.Set(0, 0, 0);

	Super::BroadcastMovementUpdate(a_Exclude);
}





bool cPlayer::DoTakeDamage(TakeDamageInfo & a_TDI)
{
	// Filters out damage for creative mode / friendly fire.

	if ((a_TDI.DamageType != dtInVoid) && (a_TDI.DamageType != dtPlugin))
	{
		if (IsGameModeCreative() || IsGameModeSpectator())
		{
			// No damage / health in creative or spectator mode if not void or plugin damage
			return false;
		}
	}

	if ((a_TDI.Attacker != nullptr) && (a_TDI.Attacker->IsPlayer()))
	{
		cPlayer * Attacker = static_cast<cPlayer *>(a_TDI.Attacker);

		if ((m_Team != nullptr) && (m_Team == Attacker->m_Team))
		{
			if (!m_Team->AllowsFriendlyFire())
			{
				// Friendly fire is disabled
				return false;
			}
		}
	}

	if (Super::DoTakeDamage(a_TDI))
	{
		// Any kind of damage adds food exhaustion
		AddFoodExhaustion(0.3f);
		m_ClientHandle->SendHealth();

		// Tell the wolves
		if (a_TDI.Attacker != nullptr)
		{
			if (a_TDI.Attacker->IsPawn())
			{
				NotifyNearbyWolves(static_cast<cPawn*>(a_TDI.Attacker), true);
			}
		}
		m_Stats.Custom[CustomStatistic::DamageTaken] += FloorC<StatisticsManager::StatValue>(a_TDI.FinalDamage * 10 + 0.5);
		return true;
	}
	return false;
}





float cPlayer::GetEnchantmentBlastKnockbackReduction()
{
	if (
		IsGameModeSpectator() ||
		(IsGameModeCreative() && !IsOnGround())
	)
	{
		return 1;  // No impact from explosion
	}

	return Super::GetEnchantmentBlastKnockbackReduction();
}





bool cPlayer::IsCrouched(void) const
{
	return std::holds_alternative<BodyStanceCrouching>(m_BodyStance);
}





bool cPlayer::IsSprinting(void) const
{
	return std::holds_alternative<BodyStanceSprinting>(m_BodyStance);
}





bool cPlayer::IsElytraFlying(void) const
{
	return std::holds_alternative<BodyStanceGliding>(m_BodyStance);
}





bool cPlayer::IsInvisible() const
{
	return !m_IsVisible || Super::IsInvisible();
}





void cPlayer::OnAddToWorld(cWorld & a_World)
{
	// Sends player spawn:
	Super::OnAddToWorld(a_World);

	// Update world name tracking:
	m_CurrentWorldName = m_World->GetName();

	// Fix to stop the player falling through the world, until we get serversided collision detection:
	FreezeInternal(GetPosition(), false);

	// UpdateCapabilities was called in the constructor, and in OnRemoveFromWorld, possibly changing our visibility.
	// If world is in spectator mode, invisibility will need updating. If we just connected, we might be on fire from a previous game.
	// Hence, tell the client by sending metadata:
	m_ClientHandle->SendEntityMetadata(*this);

	// Send contents of the inventory window:
	m_ClientHandle->SendWholeInventory(*m_CurrentWindow, m_DraggingItem);

	// Send health (the respawn packet, which understandably resets health, is also used for world travel...):
	m_ClientHandle->SendHealth();

	// Send experience, similar story with the respawn packet:
	m_ClientHandle->SendExperience();

	// Send hotbar active slot (also reset by respawn):
	m_ClientHandle->SendHeldItemChange(m_Inventory.GetEquippedSlotNum());

	// Update player team:
	UpdateTeam();

	// Send scoreboard data:
	m_World->GetScoreBoard().SendTo(*m_ClientHandle);

	// Update the view distance:
	m_ClientHandle->SetViewDistance(m_ClientHandle->GetRequestedViewDistance());

	// Send current weather of target world:
	m_ClientHandle->SendWeather(a_World.GetWeather());

	// Send time:
	m_ClientHandle->SendTimeUpdate(a_World.GetWorldAge(), a_World.GetWorldDate(), a_World.IsDaylightCycleEnabled());

	// Finally, deliver the notification hook:
	cRoot::Get()->GetPluginManager()->CallHookPlayerSpawned(*this);
}





void cPlayer::OnDetach()
{
	if (m_IsTeleporting)
	{
		// If they are teleporting, no need to figure out position:
		return;
	}

	int PosX = POSX_TOINT;
	int PosY = POSY_TOINT;
	int PosZ = POSZ_TOINT;

	// Search for a position within an area to teleport player after detachment
	// Position must be solid land with two air blocks above.
	// If nothing found, player remains where they are.
	for (int x = PosX - 1; x <= (PosX + 1); ++x)
	{
		for (int y = PosY; y <= (PosY + 3); ++y)
		{
			for (int z = PosZ - 1; z <= (PosZ + 1); ++z)
			{
				if (
					(cBlockAirHandler::IsBlockAir(m_World->GetBlock({ x, y, z }))) &&
					(cBlockAirHandler::IsBlockAir(m_World->GetBlock({ x, y + 1, z }))) &&
					cBlockInfo::IsSolid(m_World->GetBlock({ x, y - 1, z }))
				)
				{
					TeleportToCoords(x + 0.5, y, z + 0.5);
					return;
				}
			}
		}
	}
}





void cPlayer::OnRemoveFromWorld(cWorld & a_World)
{
	Super::OnRemoveFromWorld(a_World);

	// Remove any references to this player pointer by windows in the old world:
	CloseWindow(false);

	// Stop spectation and remove our reference from the spectated:
	SpectateEntity(nullptr);

	// Remove the client handle from the world:
	m_World->RemoveClientFromChunks(m_ClientHandle.get());

	if (m_ClientHandle->IsDestroyed())  // Note: checking IsWorldChangeScheduled not appropriate here since we can disconnecting while having a scheduled warp
	{
		// Disconnecting, do the necessary cleanup.
		// This isn't in the destructor to avoid crashing accessing destroyed objects during shutdown.

		if (!cRoot::Get()->GetPluginManager()->CallHookPlayerDestroyed(*this))
		{
			cRoot::Get()->BroadcastChatLeave(fmt::format(FMT_STRING("{} has left the game"), GetName()));
			LOGINFO("Player %s has left the game", GetName());
		}

		// Remove ourself from everyone's lists:
		cRoot::Get()->BroadcastPlayerListsRemovePlayer(*this);

		// Atomically decrement player count (in world thread):
		cRoot::Get()->GetServer()->PlayerDestroyed();

		// We're just disconnecting. The remaining code deals with going through portals, so bail:
		return;
	}

	const auto DestinationDimension = m_WorldChangeInfo.m_NewWorld->GetDimension();

	// Award relevant achievements:
	if (DestinationDimension == dimEnd)
	{
		AwardAchievement(CustomStatistic::AchTheEnd);
	}
	else if (DestinationDimension == dimNether)
	{
		AwardAchievement(CustomStatistic::AchPortal);
	}

	// Set capabilities based on new world:
	UpdateCapabilities();

	// Clientside warp start:
	m_ClientHandle->SendRespawn(DestinationDimension, false);
	m_ClientHandle->SendPlayerListUpdateGameMode(*this);
	m_World->BroadcastPlayerListUpdateGameMode(*this);

	// Clear sent chunk lists from the clienthandle:
	m_ClientHandle->RemoveFromWorld();
}





void cPlayer::SpawnOn(cClientHandle & a_Client)
{
	if (m_ClientHandle.get() == &a_Client)
	{
		return;
	}

	a_Client.SendPlayerSpawn(*this);
	a_Client.SendEntityHeadLook(*this);
	a_Client.SendEntityEquipment(*this, 0, m_Inventory.GetEquippedItem());
	a_Client.SendEntityEquipment(*this, 1, m_Inventory.GetEquippedBoots());
	a_Client.SendEntityEquipment(*this, 2, m_Inventory.GetEquippedLeggings());
	a_Client.SendEntityEquipment(*this, 3, m_Inventory.GetEquippedChestplate());
	a_Client.SendEntityEquipment(*this, 4, m_Inventory.GetEquippedHelmet());
}





void cPlayer::Tick(std::chrono::milliseconds a_Dt, cChunk & a_Chunk)
{
	if (m_ClientHandle->IsDestroyed())
	{
		Destroy();
		return;
	}

	if (!m_ClientHandle->IsPlaying())
	{
		// We're not yet in the game, ignore everything:
		return;
	}

	{
		const auto TicksElapsed = static_cast<StatisticsManager::StatValue>(std::chrono::duration_cast<cTickTime>(a_Dt).count());

		m_Stats.Custom[CustomStatistic::PlayOneMinute] += TicksElapsed;
		m_Stats.Custom[CustomStatistic::TimeSinceDeath] += TicksElapsed;

		if (IsCrouched())
		{
			m_Stats.Custom[CustomStatistic::SneakTime] += TicksElapsed;
		}
	}

	ASSERT((GetParentChunk() != nullptr) && (GetParentChunk()->IsValid()));
	ASSERT(a_Chunk.IsValid());

	// Handle a frozen player:
	TickFreezeCode();

	if (
		m_IsFrozen ||  // Don't do Tick updates if frozen
		IsWorldChangeScheduled()  // If we're about to change worlds (e.g. respawn), abort processing all world interactions (GH #3939)
	)
	{
		return;
	}

	Super::Tick(a_Dt, a_Chunk);

	// Handle charging the bow:
	if (m_IsChargingBow)
	{
		m_BowCharge += 1;
	}

	// Handle syncing our position with the entity being spectated:
	if (IsGameModeSpectator() && (m_Spectating != nullptr))
	{
		SetYaw(m_Spectating->GetYaw());
		SetPitch(m_Spectating->GetPitch());
		SetRoll(m_Spectating->GetRoll());
		SetPosition(m_Spectating->GetPosition());
	}

	if (IsElytraFlying())
	{
		// Damage elytra, once per second:
		{
			using namespace std::chrono_literals;

			auto & TicksFlying = std::get<BodyStanceGliding>(m_BodyStance).TicksElytraFlying;
			const auto TotalFlew = TicksFlying + a_Dt;
			const auto Periods = static_cast<short>(TotalFlew / 1s);
			TicksFlying = std::chrono::duration_cast<cTickTime>(TotalFlew - Periods * 1s);

			UseItem(cInventory::invArmorOffset + 1, Periods);
		}

		// Check if flight is still possible:
		if (IsOnGround() || IsInWater() || IsRiding() || (GetEquippedChestplate().m_ItemType != Item::Elytra))
		{
			SetElytraFlight(false);
		}
	}
	else if (IsInBed())
	{
		// Check if sleeping is still possible:
		if ((GetPosition().Floor() != m_RespawnPosition) || (cBlockBedHandler::IsBlockBed(m_World->GetBlock(m_RespawnPosition))))
		{
			m_ClientHandle->HandleLeaveBed();
		}
	}

	BroadcastMovementUpdate(m_ClientHandle.get());

	if (m_Health > 0)  // make sure player is alive
	{
		if ((m_EatingFinishTick >= 0_tick) && (m_EatingFinishTick <= m_World->GetWorldAge()))
		{
			FinishEating();
		}

		HandleFood();
	}

	if (m_IsFishing)
	{
		HandleFloater();
	}

	// Update items (e.g. Maps)
	m_Inventory.UpdateItems();

	if (m_TicksUntilNextSave == 0)
	{
		SaveToDisk();
		m_TicksUntilNextSave = PLAYER_INVENTORY_SAVE_INTERVAL;
	}
	else
	{
		m_TicksUntilNextSave--;
	}
}
