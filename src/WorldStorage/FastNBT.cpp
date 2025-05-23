
// FastNBT.cpp

// Implements the fast NBT parser and writer

#include "Globals.h"
#include "FastNBT.h"
#include "ByteBuffer.h"





// The number of NBT tags that are reserved when an NBT parsing is started.
// You can override this by using a cmdline define
#ifndef NBT_RESERVE_SIZE
	#define NBT_RESERVE_SIZE 200
#endif  // NBT_RESERVE_SIZE

#ifdef _MSC_VER
	// Dodge a C4127 (conditional expression is constant) for this specific macro usage
	#define PROPAGATE_ERROR(X) do { auto Err = (X); if (Err != eNBTParseError::npSuccess) return Err; } while ((false, false))
#else
	#define PROPAGATE_ERROR(X) do { auto Err = (X); if (Err != eNBTParseError::npSuccess) return Err; } while (false)
#endif



////////////////////////////////////////////////////////////////////////////////
// cNBTParseErrorCategory:

namespace
{

class cNBTParseErrorCategory final :
	public std::error_category
{
	cNBTParseErrorCategory() = default;
public:
	/** Category name */
	virtual const char * name() const noexcept override
	{
		return "NBT parse error";
	}

	/** Maps a parse error code to an error message */
	virtual AString message(int a_Condition) const override;

	/** Returns the canonical error category instance. */
	static const cNBTParseErrorCategory & Get() noexcept
	{
		static cNBTParseErrorCategory Category;
		return Category;
	}
};





AString cNBTParseErrorCategory::message(int a_Condition) const
{
	switch (static_cast<eNBTParseError>(a_Condition))
	{
		case eNBTParseError::npSuccess:
		{
			return "Parsing succeded";
		}
		case eNBTParseError::npNeedBytes:
		{
			return "Expected more data";
		}
		case eNBTParseError::npNoTopLevelCompound:
		{
			return "No top level compound tag";
		}
		case eNBTParseError::npStringMissingLength:
		{
			return "Expected a string length but had insufficient data";
		}
		case eNBTParseError::npStringInvalidLength:
		{
			return "String length invalid";
		}
		case eNBTParseError::npCompoundImbalancedTag:
		{
			return "Compound tag was unmatched at end of file";
		}
		case eNBTParseError::npListMissingType:
		{
			return "Expected a list type but had insuffiecient data";
		}
		case eNBTParseError::npListMissingLength:
		{
			return "Expected a list length but had insufficient data";
		}
		case eNBTParseError::npListInvalidLength:
		{
			return "List length invalid";
		}
		case eNBTParseError::npSimpleMissing:
		{
			return "Expected a numeric type but had insufficient data";
		}
		case eNBTParseError::npArrayMissingLength:
		{
			return "Expected an array length but had insufficient data";
		}
		case eNBTParseError::npArrayInvalidLength:
		{
			return "Array length invalid";
		}
		case eNBTParseError::npUnknownTag:
		{
			return "Unknown tag";
		}
	}
	UNREACHABLE("Unsupported nbt parse error");
}

}  // namespace (anonymous)





std::error_code make_error_code(eNBTParseError a_Err) noexcept
{
	return { static_cast<int>(a_Err), cNBTParseErrorCategory::Get() };
}





////////////////////////////////////////////////////////////////////////////////
// cParsedNBT:

#define NEEDBYTES(N, ERR) \
	do { \
		if (m_Data.size() - m_Pos < static_cast<size_t>(N)) \
		{ \
			return ERR; \
		} \
	} while (false)





cParsedNBT::cParsedNBT(const ContiguousByteBufferView a_Data) :
	m_Data(a_Data),
	m_Pos(0)
{
	m_Error = Parse(false);
}





cParsedNBT::cParsedNBT(cByteBuffer & a_Data, ContiguousByteBuffer & a_Bfr, bool a_Network_1_21) :
	m_Data(a_Bfr),
	m_Pos(0)
{
	// Hacky workaround
	size_t pos = a_Data.GetReadPos();
	a_Data.ReadAll(a_Bfr);
	m_Data = a_Bfr;
	m_Error = Parse(a_Network_1_21);
	a_Data.ResetRead();
	a_Data.SkipRead(pos + (m_Pos == 0 ? 1 : m_Pos));
}





eNBTParseError cParsedNBT::Parse(bool a_Network_1_21)
{
	if (m_Data.size() < 3)
	{
		// Data too short
		return eNBTParseError::npNeedBytes;
	}
	if (m_Data[0] != std::byte(TAG_Compound))
	{
		// The top-level tag must be a Compound
		return eNBTParseError::npNoTopLevelCompound;
	}

	m_Tags.reserve(NBT_RESERVE_SIZE);

	m_Tags.emplace_back(TAG_Compound, -1);

	m_Pos = 1;

	if (!a_Network_1_21)
	{
		PROPAGATE_ERROR(ReadString(m_Tags.back().m_NameStart, m_Tags.back().m_NameLength));
	}
	return ReadCompound();
}





eNBTParseError cParsedNBT::ReadString(size_t & a_StringStart, size_t & a_StringLen)
{
	NEEDBYTES(2, eNBTParseError::npStringMissingLength);
	a_StringStart = m_Pos + 2;
	a_StringLen = static_cast<size_t>(NetworkBufToHost<UInt16>(m_Data.data() + m_Pos));
	NEEDBYTES(2 + a_StringLen, eNBTParseError::npStringInvalidLength);
	m_Pos += 2 + a_StringLen;
	return eNBTParseError::npSuccess;
}





eNBTParseError cParsedNBT::ReadCompound(void)
{
	ASSERT(m_Tags.size() > 0);

	// Reads the latest tag as a compound
	size_t ParentIdx = m_Tags.size() - 1;
	int PrevSibling = -1;
	for (;;)
	{
		NEEDBYTES(1, eNBTParseError::npCompoundImbalancedTag);
		const auto TagTypeNum = m_Data[m_Pos];
		if ((TagTypeNum < std::byte(TAG_Min)) || (TagTypeNum > std::byte(TAG_Max)))
		{
			return eNBTParseError::npUnknownTag;
		}
		eTagType TagType = static_cast<eTagType>(TagTypeNum);
		m_Pos++;
		if (TagType == TAG_End)
		{
			break;
		}
		m_Tags.emplace_back(TagType, static_cast<int>(ParentIdx), PrevSibling);
		if (PrevSibling >= 0)
		{
			m_Tags[static_cast<size_t>(PrevSibling)].m_NextSibling = static_cast<int>(m_Tags.size()) - 1;
		}
		else
		{
			m_Tags[ParentIdx].m_FirstChild = static_cast<int>(m_Tags.size()) - 1;
		}
		PrevSibling = static_cast<int>(m_Tags.size()) - 1;
		PROPAGATE_ERROR(ReadString(m_Tags.back().m_NameStart, m_Tags.back().m_NameLength));
		PROPAGATE_ERROR(ReadTag());
	}  // while (true)
	m_Tags[ParentIdx].m_LastChild = PrevSibling;
	return eNBTParseError::npSuccess;
}





eNBTParseError cParsedNBT::ReadList(eTagType a_ChildrenType)
{
	// Reads the latest tag as a list of items of type a_ChildrenType

	// Read the count:
	NEEDBYTES(4, eNBTParseError::npListMissingLength);
	int Count = NetworkBufToHost<int>(m_Data.data() + m_Pos);
	m_Pos += 4;
	auto MinChildSize = GetMinTagSize(a_ChildrenType);
	if ((Count < 0) || (Count > static_cast<int>((m_Data.size() - m_Pos) / MinChildSize)))
	{
		return eNBTParseError::npListInvalidLength;
	}

	// Read items:
	ASSERT(m_Tags.size() > 0);
	size_t ParentIdx = m_Tags.size() - 1;
	int PrevSibling = -1;
	for (int i = 0; i < Count; i++)
	{
		m_Tags.emplace_back(a_ChildrenType, static_cast<int>(ParentIdx), PrevSibling);
		if (PrevSibling >= 0)
		{
			m_Tags[static_cast<size_t>(PrevSibling)].m_NextSibling = static_cast<int>(m_Tags.size()) - 1;
		}
		else
		{
			m_Tags[ParentIdx].m_FirstChild = static_cast<int>(m_Tags.size()) - 1;
		}
		PrevSibling = static_cast<int>(m_Tags.size()) - 1;
		PROPAGATE_ERROR(ReadTag());
	}  // for (i)
	m_Tags[ParentIdx].m_LastChild = PrevSibling;
	return eNBTParseError::npSuccess;
}





#define CASE_SIMPLE_TAG(TAGTYPE, LEN) \
	case TAG_##TAGTYPE: \
	{ \
		NEEDBYTES(LEN, eNBTParseError::npSimpleMissing); \
		Tag.m_DataStart = m_Pos; \
		Tag.m_DataLength = LEN; \
		m_Pos += LEN; \
		return eNBTParseError::npSuccess; \
	}

eNBTParseError cParsedNBT::ReadTag(void)
{
	cFastNBTTag & Tag = m_Tags.back();
	switch (Tag.m_Type)
	{
		CASE_SIMPLE_TAG(Byte,   1)
		CASE_SIMPLE_TAG(Short,  2)
		CASE_SIMPLE_TAG(Int,    4)
		CASE_SIMPLE_TAG(Long,   8)
		CASE_SIMPLE_TAG(Float,  4)
		CASE_SIMPLE_TAG(Double, 8)

		case TAG_String:
		{
			return ReadString(Tag.m_DataStart, Tag.m_DataLength);
		}

		case TAG_ByteArray:
		{
			NEEDBYTES(4, eNBTParseError::npArrayMissingLength);
			int len = NetworkBufToHost<int>(m_Data.data() + m_Pos);
			m_Pos += 4;
			if (len < 0)
			{
				// Invalid length
				return eNBTParseError::npArrayInvalidLength;
			}
			NEEDBYTES(len, eNBTParseError::npArrayInvalidLength);
			Tag.m_DataLength = static_cast<size_t>(len);
			Tag.m_DataStart = m_Pos;
			m_Pos += static_cast<size_t>(len);
			return eNBTParseError::npSuccess;
		}

		case TAG_List:
		{
			NEEDBYTES(1, eNBTParseError::npListMissingType);
			eTagType ItemType = static_cast<eTagType>(m_Data[m_Pos]);
			m_Pos++;
			PROPAGATE_ERROR(ReadList(ItemType));
			return eNBTParseError::npSuccess;
		}

		case TAG_Compound:
		{
			PROPAGATE_ERROR(ReadCompound());
			return eNBTParseError::npSuccess;
		}

		case TAG_IntArray:
		{
			NEEDBYTES(4, eNBTParseError::npArrayMissingLength);
			int len = NetworkBufToHost<int>(m_Data.data() + m_Pos);
			m_Pos += 4;
			if (len < 0)
			{
				// Invalid length
				return eNBTParseError::npArrayInvalidLength;
			}
			len *= 4;
			NEEDBYTES(len, eNBTParseError::npArrayInvalidLength);
			Tag.m_DataLength = static_cast<size_t>(len);
			Tag.m_DataStart = m_Pos;
			m_Pos += static_cast<size_t>(len);
			return eNBTParseError::npSuccess;
		}

		case TAG_LongArray:
		{
			NEEDBYTES(4, eNBTParseError::npArrayMissingLength);
			int len = NetworkBufToHost<int>(m_Data.data() + m_Pos);
			m_Pos += 4;
			if (len < 0)
			{
				// Invalid length
				return eNBTParseError::npArrayInvalidLength;
			}
			len *= 8;
			NEEDBYTES(len, eNBTParseError::npArrayInvalidLength);
			Tag.m_DataLength = static_cast<size_t>(len);
			Tag.m_DataStart = m_Pos;
			m_Pos += static_cast<size_t>(len);
			return eNBTParseError::npSuccess;
		}

		case TAG_Min:
		{
			return eNBTParseError::npUnknownTag;
		}
	}  // switch (iType)
	UNREACHABLE("Unsupported nbt tag type");
}

#undef CASE_SIMPLE_TAG





int cParsedNBT::FindChildByName(int a_Tag, const char * a_Name, size_t a_NameLength) const
{
	if (a_Tag < 0)
	{
		return -1;
	}
	if (m_Tags[static_cast<size_t>(a_Tag)].m_Type != TAG_Compound)
	{
		return -1;
	}

	if (a_NameLength == 0)
	{
		a_NameLength = strlen(a_Name);
	}
	for (int Child = m_Tags[static_cast<size_t>(a_Tag)].m_FirstChild; Child != -1; Child = m_Tags[static_cast<size_t>(Child)].m_NextSibling)
	{
		if (
			(m_Tags[static_cast<size_t>(Child)].m_NameLength == a_NameLength) &&
			(memcmp(m_Data.data() + m_Tags[static_cast<size_t>(Child)].m_NameStart, a_Name, a_NameLength) == 0)
		)
		{
			return Child;
		}
	}  // for Child - children of a_Tag
	return -1;
}





int cParsedNBT::FindTagByPath(int a_Tag, const AString & a_Path) const
{
	if (a_Tag < 0)
	{
		return -1;
	}
	size_t Begin = 0;
	size_t Length = a_Path.length();
	int Tag = a_Tag;
	for (size_t i = 0; i < Length; i++)
	{
		if (a_Path[i] != '\\')
		{
			continue;
		}
		Tag = FindChildByName(Tag, a_Path.c_str() + Begin, i - Begin);
		if (Tag < 0)
		{
			return -1;
		}
		Begin = i + 1;
	}  // for i - a_Path[]

	if (Begin < Length)
	{
		Tag = FindChildByName(Tag, a_Path.c_str() + Begin, Length - Begin);
	}
	return Tag;
}





size_t cParsedNBT::GetMinTagSize(eTagType a_TagType)
{
	switch (a_TagType)
	{
		case TAG_End:       return 1;
		case TAG_Byte:      return 1;
		case TAG_Short:     return 2;
		case TAG_Int:       return 4;
		case TAG_Long:      return 8;
		case TAG_Float:     return 4;
		case TAG_Double:    return 8;
		case TAG_String:    return 2;  // 2 bytes for the string length
		case TAG_ByteArray: return 4;  // 4 bytes for the count
		case TAG_List:      return 5;  // 1 byte list type + 4 bytes count
		case TAG_Compound:  return 1;  // Single TAG_End byte
		case TAG_IntArray:  return 4;  // 4 bytes for the count
		case TAG_LongArray: return 4;  // 4 bytes for the count
	}
	UNREACHABLE("Unsupported nbt tag type");
}





////////////////////////////////////////////////////////////////////////////////
// cFastNBTWriter:

cFastNBTWriter::cFastNBTWriter(const AString & a_RootTagName) :
	m_CurrentStack(0)
{
	m_Stack[0].m_Type = TAG_Compound;
	m_Result.reserve(100 KiB);
	m_Result.push_back(std::byte(TAG_Compound));
	WriteString(a_RootTagName);
}





cFastNBTWriter::cFastNBTWriter(bool Network1_21) :
	m_CurrentStack(0)
{
	m_Stack[0].m_Type = TAG_Compound;
	m_Result.reserve(100 KiB);
	m_Result.push_back(std::byte(TAG_Compound));
	if (!Network1_21)
	{
		WriteString("");
	}
}





void cFastNBTWriter::BeginCompound(const AString & a_Name)
{
	if (m_CurrentStack >= MAX_STACK - 1)
	{
		ASSERT(!"Stack overflow");
		return;
	}

	TagCommon(a_Name, TAG_Compound);

	++m_CurrentStack;
	m_Stack[m_CurrentStack].m_Type = TAG_Compound;
}





void cFastNBTWriter::EndCompound(void)
{
	ASSERT(m_CurrentStack > 0);
	ASSERT(IsStackTopCompound());

	m_Result.push_back(std::byte(TAG_End));
	--m_CurrentStack;
}





void cFastNBTWriter::BeginList(const AString & a_Name, eTagType a_ChildrenType)
{
	if (m_CurrentStack >= MAX_STACK - 1)
	{
		ASSERT(!"Stack overflow");
		return;
	}

	TagCommon(a_Name, TAG_List);

	m_Result.push_back(std::byte(a_ChildrenType));
	m_Result.append(4, std::byte(0));

	++m_CurrentStack;
	m_Stack[m_CurrentStack].m_Type     = TAG_List;
	m_Stack[m_CurrentStack].m_Pos      = static_cast<int>(m_Result.size()) - 4;
	m_Stack[m_CurrentStack].m_Count    = 0;
	m_Stack[m_CurrentStack].m_ItemType = a_ChildrenType;
}





void cFastNBTWriter::EndList(void)
{
	ASSERT(m_CurrentStack > 0);
	ASSERT(m_Stack[m_CurrentStack].m_Type == TAG_List);

	// Update the list count:
	auto Value = HostToNetwork(m_Stack[m_CurrentStack].m_Count);
	std::copy(Value.begin(), Value.end(), m_Result.data() + m_Stack[m_CurrentStack].m_Pos);

	--m_CurrentStack;
}





void cFastNBTWriter::AddByte(const AString & a_Name, unsigned char a_Value)
{
	TagCommon(a_Name, TAG_Byte);
	m_Result.push_back(std::byte(a_Value));
}





void cFastNBTWriter::AddShort(const AString & a_Name, Int16 a_Value)
{
	TagCommon(a_Name, TAG_Short);
	auto Value = HostToNetwork(a_Value);
	m_Result.append(Value.begin(), Value.end());
}





void cFastNBTWriter::AddInt(const AString & a_Name, Int32 a_Value)
{
	TagCommon(a_Name, TAG_Int);
	auto Value = HostToNetwork(a_Value);
	m_Result.append(Value.begin(), Value.end());
}





void cFastNBTWriter::AddLong(const AString & a_Name, Int64 a_Value)
{
	TagCommon(a_Name, TAG_Long);
	auto Value = HostToNetwork(a_Value);
	m_Result.append(Value.begin(), Value.end());
}





void cFastNBTWriter::AddFloat(const AString & a_Name, float a_Value)
{
	TagCommon(a_Name, TAG_Float);
	auto Value = HostToNetwork(a_Value);
	m_Result.append(Value.begin(), Value.end());
}





void cFastNBTWriter::AddDouble(const AString & a_Name, double a_Value)
{
	TagCommon(a_Name, TAG_Double);
	auto Value = HostToNetwork(a_Value);
	m_Result.append(Value.begin(), Value.end());
}





void cFastNBTWriter::AddString(const AString & a_Name, const std::string_view a_Value)
{
	TagCommon(a_Name, TAG_String);
	auto Length = HostToNetwork(static_cast<UInt16>(a_Value.size()));
	m_Result.append(Length.begin(), Length.end());
	m_Result.append({ reinterpret_cast<const std::byte *>(a_Value.data()), a_Value.size() });
}





void cFastNBTWriter::AddByteArray(const AString & a_Name, const char * a_Value, size_t a_NumElements)
{
	TagCommon(a_Name, TAG_ByteArray);
	auto Length = HostToNetwork(static_cast<UInt32>(a_NumElements));
	m_Result.append(Length.begin(), Length.end());
	m_Result.append(reinterpret_cast<const std::byte *>(a_Value), a_NumElements);
}





void cFastNBTWriter::AddByteArray(const AString & a_Name, size_t a_NumElements, unsigned char a_Value)
{
	TagCommon(a_Name, TAG_ByteArray);
	auto Length = HostToNetwork(static_cast<UInt32>(a_NumElements));
	m_Result.append(Length.begin(), Length.end());
	m_Result.append(a_NumElements, std::byte(a_Value));
}





void cFastNBTWriter::AddIntArray(const AString & a_Name, const Int32 * a_Value, size_t a_NumElements)
{
	TagCommon(a_Name, TAG_IntArray);
	auto Length = HostToNetwork(static_cast<UInt32>(a_NumElements));
	size_t cap = m_Result.capacity();
	size_t size = m_Result.length();
	if ((cap - size) < (4 + a_NumElements * 4))
	{
		m_Result.reserve(size + 4 + (a_NumElements * 4));
	}
	m_Result.append(Length.begin(), Length.end());
	for (size_t i = 0; i < a_NumElements; i++)
	{
		auto Element = HostToNetwork(a_Value[i]);
		m_Result.append(Element.begin(), Element.end());
	}
}





void cFastNBTWriter::AddLongArray(const AString & a_Name, const Int64 * a_Value, size_t a_NumElements)
{
	TagCommon(a_Name, TAG_LongArray);
	auto Length = HostToNetwork(static_cast<UInt32>(a_NumElements));
	size_t cap = m_Result.capacity();
	size_t size = m_Result.length();
	if ((cap - size) < (8 + a_NumElements * 8))
	{
		m_Result.reserve(size + 8 + (a_NumElements * 8));
	}
	m_Result.append(Length.begin(), Length.end());
	for (size_t i = 0; i < a_NumElements; i++)
	{
		auto Element = HostToNetwork(a_Value[i]);
		m_Result.append(Element.begin(), Element.end());
	}
}





void cFastNBTWriter::Finish(void)
{
	ASSERT(m_CurrentStack == 0);
	m_Result.push_back(std::byte(TAG_End));
}





void cFastNBTWriter::WriteString(const std::string_view a_Data)
{
	// TODO check size <= short max
	auto Length = HostToNetwork(static_cast<UInt16>(a_Data.size()));
	m_Result.append(Length.begin(), Length.end());
	m_Result.append(reinterpret_cast<const std::byte *>(a_Data.data()), a_Data.size());
}
