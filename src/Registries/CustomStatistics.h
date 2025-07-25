#pragma once

// tolua_begin
enum class CustomStatistic
{
	// tolua_end

	/* Achievements */
	AchOpenInventory,     /* Taking Inventory     */
	AchMineWood,          /* Getting Wood         */
	AchBuildWorkBench,    /* Benchmarking         */
	AchBuildPickaxe,      /* Time to Mine!        */
	AchBuildFurnace,      /* Hot Topic            */
	AchAcquireIron,       /* Acquire Hardware     */
	AchBuildHoe,          /* Time to Farm!        */
	AchMakeBread,         /* Bake Bread           */
	AchBakeCake,          /* The Lie              */
	AchBuildBetterPickaxe, /* Getting an Upgrade   */
	AchCookFish,          /* Delicious Fish       */
	AchOnARail,           /* On A Rail            */
	AchBuildSword,        /* Time to Strike!      */
	AchKillEnemy,         /* Monster Hunter       */
	AchKillCow,           /* Cow Tipper           */
	AchFlyPig,            /* When Pigs Fly        */
	AchSnipeSkeleton,     /* Sniper Duel          */
	AchDiamonds,          /* DIAMONDS!            */
	AchPortal,            /* We Need to Go Deeper */
	AchGhast,             /* Return to Sender     */
	AchBlazeRod,          /* Into Fire            */
	AchPotion,            /* Local Brewery        */
	AchTheEnd,            /* The End?             */
	AchTheEnd2,           /* The End.             */
	AchEnchantments,      /* Enchanter            */
	AchOverkill,          /* Overkill             */
	AchBookcase,          /* Librarian            */
	AchExploreAllBiomes,  /* Adventuring Time     */
	AchSpawnWither,       /* The Beginning?       */
	AchKillWither,        /* The Beginning.       */
	AchFullBeacon,        /* Beaconator           */
	AchBreedCow,          /* Repopulation         */
	AchDiamondsToYou,     /* Diamonds to you!     */

	// tolua_begin

	/* Statistics */
	AnimalsBred,
	AviateOneCm,
	BellRing,
	BoatOneCm,
	CleanArmor,
	CleanBanner,
	CleanShulkerBox,
	ClimbOneCm,
	CrouchOneCm,
	DamageAbsorbed,
	DamageBlockedByShield,
	DamageDealt,
	DamageDealtAbsorbed,
	DamageDealtResisted,
	DamageResisted,
	DamageTaken,
	Deaths,
	Drop,
	EatCakeSlice,
	EnchantItem,
	FallOneCm,
	FillCauldron,
	FishCaught,
	FlyOneCm,
	HappyGhastOneCm,
	HorseOneCm,
	InspectDispenser,
	InspectDropper,
	InspectHopper,
	InteractWithAnvil,
	InteractWithBeacon,
	InteractWithBlastFurnace,
	InteractWithBrewingstand,
	InteractWithCampfire,
	InteractWithCartographyTable,
	InteractWithCraftingTable,
	InteractWithFurnace,
	InteractWithGrindstone,
	InteractWithLectern,
	InteractWithLoom,
	InteractWithSmithingTable,
	InteractWithSmoker,
	InteractWithStonecutter,
	Jump,
	LeaveGame,
	MinecartOneCm,
	MobKills,
	OpenBarrel,
	OpenChest,
	OpenEnderchest,
	OpenShulkerBox,
	PigOneCm,
	PlayNoteblock,
	PlayRecord,
	PlayTime,
	PlayerKills,
	PotFlower,
	RaidTrigger,
	RaidWin,
	SleepInBed,
	SneakTime,
	SprintOneCm,
	StriderOneCm,
	SwimOneCm,
	TalkedToVillager,
	TargetHit,
	TimeSinceDeath,
	TimeSinceRest,
	TotalWorldTime,
	TradedWithVillager,
	TriggerTrappedChest,
	TuneNoteblock,
	UseCauldron,
	WalkOnWaterOneCm,
	WalkOneCm,
	WalkUnderWaterOneCm,

	// Deprecated
	PlayOneMinute = PlayTime,

	// Old ones just for compatibility
	JunkFished = 110,
	TreasureFished = 109,
};
// tolua_end
