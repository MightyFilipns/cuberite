

// Item.h

// Declares the cItem class representing an item (in the inventory sense)





#pragma once

#include "Defines.h"
#include "Enchantments.h"
#include "WorldStorage/FireworksSerializer.h"
#include "Color.h"
#include "DataComponents/DataComponents.h"
#include "Registries/Items.h"
#include "Protocol/Palettes/Upgrade.h"





// fwd:
class cItemHandler;
class cItemGrid;
class cColor;

namespace Json
{
	class Value;
}





// tolua_begin
class cItem
{
public:
	/** Creates an empty item */
	cItem(void);

	/** Creates an item of the specified type, by default 1 piece with no damage and no enchantments */
	cItem(
		enum Item a_ItemType,
		char a_ItemCount = 1,
		short a_ItemDamage = 0,
		const AString & a_Enchantments = "",
		const AStringVector & a_LoreTable = {},
		const DataComponents::DataComponentMap & a_DataComponents = DataComponents::DataComponentMap()
	);

	// The constructor is disabled in code, because the compiler generates it anyway,
	// but it needs to stay because ToLua needs to generate the binding for it
	#ifdef TOLUA_EXPOSITION

	/** Creates an exact copy of the item */
	cItem(const cItem & a_CopyFrom);

	#endif

	/** Empties the item and frees up any dynamic storage used by the internals. */
	void Empty(void);

	// Deprecated - for compatibility with old plugins
	void Clear(void) { Empty(); }

	/** Returns true if the item represents an empty stack - either the type is invalid, or count is zero. */
	bool IsEmpty(void) const
	{
		return ((m_ItemType == Item::Air) || (m_ItemCount <= 0));
	}

	/* Returns true if this itemstack can stack with the specified stack (types match, enchantments etc.)
	ItemCounts are ignored. */
	bool IsEqual(const cItem & a_Item) const
	{
		return (
			IsSameType(a_Item) &&
			// (m_ItemDamage == a_Item.m_ItemDamage) &&
			(m_Enchantments == a_Item.m_Enchantments) &&
			// (m_CustomName == a_Item.m_CustomName) &&
			(m_LoreTable == a_Item.m_LoreTable) &&
			m_FireworkItem.IsEqualTo(a_Item.m_FireworkItem) &&
			m_ItemComponents.m_data == a_Item.m_ItemComponents.m_data
		);
	}


	bool IsSameType(const cItem & a_Item) const
	{
		return (m_ItemType == a_Item.m_ItemType) || (IsEmpty() && a_Item.IsEmpty());
	}


	bool IsBothNameAndLoreEmpty(void) const
	{
		return (IsCustomNameEmpty() && IsLoreEmpty());
	}


	bool IsCustomNameEmpty(void) const { return (!HasComponent<DataComponents::CustomNameComponent>()); }
	bool IsLoreEmpty(void) const { return (m_LoreTable.empty()); }

	/** Returns a copy of this item with m_ItemCount set to 1. Useful to preserve enchantments etc. on stacked items */
	cItem CopyOne(void) const;

	/** Adds the specified count to this object and returns the reference to self (useful for chaining) */
	cItem & AddCount(char a_AmountToAdd);

	/** Returns the maximum damage value that this item can have; zero if damage is not applied */
	UInt32 GetMaxDamage(void) const;

	/** Damages a weapon / tool. Returns true when damage reaches max value and the item should be destroyed */
	bool DamageItem(short a_Amount = 1);

	inline bool IsDamageable(void) const { return (GetMaxDamage() > 0); }

	/** Returns true if the item is stacked up to its maximum stacking. */
	bool IsFullStack(void) const;

	/** Returns the maximum amount of stacked items of this type. */
	char GetMaxStackSize(void) const;

	// tolua_end

	bool operator == (const cItem & rhs) const { return this->IsEqual(rhs); }

	/** Returns the cItemHandler responsible for this item type */
	const cItemHandler & GetHandler(void) const;

	/** Saves the item data into JSON representation */
	void GetJson(Json::Value & a_OutValue) const;

	/** Loads the item data from JSON representation */
	void FromJson(const Json::Value & a_Value);

	/** Returns true if the specified item type is enchantable.
	If FromBook is true, the function is used in the anvil inventory with book enchantments.
	So it checks the "only book enchantments" too. Example: You can only enchant a hoe with a book. */
	static bool IsEnchantable(Item a_ItemType, bool a_FromBook = false);  // tolua_export

	/** Returns the enchantability of the item. When the item hasn't a enchantability, it will returns 0 */
	unsigned GetEnchantability();  // tolua_export

	/** Randomly enchants the item using the specified number of XP levels.
	Returns true if the item was enchanted, false if not (not enchantable / too many enchantments already).
	Randomness is derived from the provided PRNG. */
	bool EnchantByXPLevels(unsigned a_NumXPLevels, MTRand & a_Random);  // Exported in ManualBindings.cpp

	/** Adds this specific enchantment to this item, returning the cost.
	FromBook specifies whether the enchantment should be treated as coming
	from a book. If true, then the cost returned uses the book values, if
	false it uses the normal item multipliers. */
	int AddEnchantment(int a_EnchantmentID, unsigned int a_Level, bool a_FromBook);  // tolua_export

	/** Adds the enchantments on a_Other to this item, returning the
	XP cost of the transfer. */
	int AddEnchantmentsFromItem(const cItem & a_Other);  // tolua_export

	/** Returns whether or not this item is allowed to have the given enchantment. Note: Does not check whether the enchantment is exclusive with the current enchantments on the item. */
	bool CanHaveEnchantment(int a_EnchantmentID);

	// tolua_begin

	Item           m_ItemType;
	char           m_ItemCount;
	// short          m_ItemDamage;
	cEnchantments  m_Enchantments;
	// AString        m_CustomName;

	// tolua_end

	AStringVector  m_LoreTable;  // Exported in ManualBindings.cpp

	/**
	Compares two items for the same type or category. Type of item is defined
	via `m_ItemType` and `m_ItemDamage`. Some items (e.g. planks) have the same
	`m_ItemType` and the wood kind is defined via `m_ItemDamage`. `-1` is used
	as placeholder for all kinds (e.g. all kind of planks).

	Items are different when the `ItemType` is different or the `ItemDamage`
	is different and unequal -1.
	*/
	struct sItemCompare
	{
		bool operator() (const cItem & a_Lhs, const cItem & a_Rhs) const
		{
			if (a_Lhs.m_ItemType != a_Rhs.m_ItemType)
			{
				return (a_Lhs.m_ItemType < a_Rhs.m_ItemType);
			}
			return true;  // TODO: compare components
		}
	};

	const std::map<size_t, DataComponents::DataComponent> & GetDefaultItemComponents(void) const;

	template <typename TypeToFind> TypeToFind & GetOrAdd()
	{
		if (HasComponent<TypeToFind>())
		{
			return std::get<TypeToFind>(m_ItemComponents.m_data[DataComponents::GetIndexOfDataComponent<TypeToFind, 0>()]);
		}
		DataComponents::DataComponent def;
		def.emplace<DataComponents::GetIndexOfDataComponent<TypeToFind>()>();
		SetComponent(def);
		return std::get<TypeToFind>(m_ItemComponents.m_data[DataComponents::GetIndexOfDataComponent<TypeToFind, 0>()]);
	}

	void SetComponent(const DataComponents::DataComponent & a_comp)
	{
		auto comps = GetDefaultItemComponents();

		size_t ind = a_comp.index();
		if (comps.contains(ind) && (a_comp == comps.at(ind)))
		{
			auto rez = m_ItemComponents.m_data.find(ind);
			if (rez != m_ItemComponents.m_data.end())
			{
				m_ItemComponents.m_data.erase(rez);
			}
			return;
		}

		m_ItemComponents.AddComp(a_comp);
	}

	template <typename TypeToFind> const TypeToFind & GetComponentOrDefault() const
	{
		const auto & comps = GetDefaultItemComponents();
		if (m_ItemComponents.HasComponent<TypeToFind>())
		{
			return m_ItemComponents.GetComp<TypeToFind>();
		}
		if (comps.contains(DataComponents::GetIndexOfDataComponent<TypeToFind>()))
		{
			return std::get<TypeToFind>(comps.at(DataComponents::GetIndexOfDataComponent<TypeToFind>()));
		}
		VERIFY("Component not found on item");
		throw std::runtime_error("Component not found on item");
	}

	// Check if the given component is present or is a default component
	template <typename TypeToFind> bool HasComponent() const
	{
		auto comps = GetDefaultItemComponents();
		return m_ItemComponents.HasComponent<TypeToFind>() || comps.contains(DataComponents::GetIndexOfDataComponent<TypeToFind>());
	}

	template <typename TypeToFind> void RemoveComp()
	{
		if (m_ItemComponents.HasComponent<TypeToFind>())
		{
			m_ItemComponents.RemoveComp<TypeToFind>();
		}
		// TODO: default item component removal
	}

	const DataComponents::DataComponentMap & GetDataComponents(void) const { return m_ItemComponents; }

private:
	DataComponents::DataComponentMap m_ItemComponents;
public:
	// tolua_begin

	// int            m_RepairCost;
	cFireworkItem  m_FireworkItem;
	cColor         m_ItemColor;
};
// tolua_end





/** This class bridges a vector of cItem for safe access via Lua. It checks boundaries for all accesses
Note that this class is zero-indexed!
*/
class cItems  // tolua_export
	: public std::vector<cItem>
{  // tolua_export
public:

	cItems(const cItems &) = default;
	cItems(cItems &&) = default;
	cItems & operator = (const cItems &) = default;
	cItems & operator = (cItems &&) = default;

	/** Constructs a new instance containing the specified item. */
	cItems(cItem && a_InitialItem);

	// tolua_begin

	/** Need a Lua-accessible constructor */
	cItems(void) {}

	cItem * Get   (int a_Idx);
	void    Set   (int a_Idx, const cItem & a_Item);
	void    Add   (const cItem & a_Item) { push_back(a_Item); }
	void    Add   (enum Item a_ItemType) { emplace_back(a_ItemType); }
	void    Add   (enum Item a_ItemType, char a_ItemCount) { emplace_back(a_ItemType, a_ItemCount); }
	void    Delete(int a_Idx);
	void    Clear (void) {clear(); }
	size_t  Size  (void) const { return size(); }
	void    Set   (int a_Idx, Item a_Item, char a_ItemCount, short a_ItemDamage);
	bool    Contains(const cItem & a_Item);
	bool    ContainsType(const cItem & a_Item);

	/** Adds a copy of all items in a_ItemGrid. */
	void AddItemGrid(const cItemGrid & a_ItemGrid);

	// tolua_end
} ;  // tolua_export





/** Used to store loot probability tables */
class cLootProbab
{
public:
	cItem m_Item;
	int   m_MinAmount;
	int   m_MaxAmount;
	int   m_Weight;
} ;

template<> class fmt::formatter<Item> : public fmt::formatter<std::string_view>
{
public:
	template <typename FormatContext>
	auto format(const Item & a_Item, FormatContext & a_Ctx) const
	{
		return fmt::format_to(a_Ctx.out(), "{}", NamespaceSerializer::From(a_Item));
	}
};
