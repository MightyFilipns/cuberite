#include "Globals.h"
#include "CommandManager.h"
#include "CommandException.h"
#include "CompositeChat.h"


////////////////////////////////////////////////////////////////////////////////
// eCommandNode:

cCommandManager::cCommandNode::cCommandNode() :
	m_IsExecutable(false),
	m_Executioner([](cCommandExecutionContext& a_Ctx) -> bool { return true; }),
	m_Type(eNodeType::Root),
	m_ChildrenNodes({}),
	m_RedirectNode(nullptr),
	m_Argument(nullptr),
	m_Name(""),
	m_SuggestionType(eCommandSuggestionType::None)
{

}





cCommandManager::cCommandNode *
cCommandManager::cCommandNode::Then(const cCommandNode * a_ChildNode)
{
	this->m_ChildrenNodes.push_back(*a_ChildNode);
	return this;
}





cCommandManager::cCommandNode
cCommandManager::cCommandNode::Literal(const AString& a_Name)
{
	return cCommandNode(
		eNodeType::Literal, {}, nullptr,
		nullptr, a_Name,
		eCommandSuggestionType::None, false, [](cCommandExecutionContext& a_Ctx) -> bool { return true; });
}





cCommandManager::cCommandNode cCommandManager::cCommandNode::Argument(
	const AString& a_Name, CmdArgPtr a_Argument)
{
	return cCommandNode(
		 eNodeType::Argument, {}, nullptr, a_Argument, a_Name,
		eCommandSuggestionType::None, false, [](cCommandExecutionContext& a_Ctx) -> bool { return true; });
}





cCommandManager::cCommandNode * cCommandManager::cCommandNode::Executable(const CommandExecutor a_Executioner)
{
	this->m_IsExecutable = true;
	this->m_Executioner = a_Executioner;
	return this;
}





bool cCommandManager::cCommandNode::Parse(BasicStringReader& a_Command, cCommandExecutionContext& a_Ctx)
{
	if (this->m_Type == eNodeType::Root)
	{
		auto tocheck = a_Command.ReadStringUntilWhiteSpace();
		auto cmd = this->GetLiteralCommandNode(tocheck);
		if (cmd == nullptr)
		{
			return false;
		}
		else
		{
			try
			{
				return cmd->Parse(a_Command,a_Ctx);
			}
			catch (cCommandParseException ex)
			{
				UNREACHABLE("exceptions should never be caught here");
			}
		}
	}
	else
	{
		auto ret = this->GetNextPotentialNode(a_Command, a_Ctx);
		if (ret != nullptr)
		{
			return ret->Parse(a_Command, a_Ctx);
		}
	}
	if (this->m_IsExecutable && a_Command.IsDone())
	{
		return this->m_Executioner(a_Ctx);
	}
	else
	{
		AString str = a_Command.GetString();
		if (a_Command.IsDone())
		{
			cRoot::Get()->BroadcastChat("Incomplete command");
		}
		else
		{
			//TODO: handle errors properly
		}

	}

	return false;
}





void cCommandManager::cCommandNode::WriteCommandTree(cPacketizer& a_Packet)
{
	if (this->m_Type != eNodeType::Root)
	{
		UNREACHABLE("WriteCommandTree shouldn't be called on non-root nodes");
	}
	CommandNodeList list;
	list.push_back(*this);
	auto map = ComputeChildrenIds(*this);
	a_Packet.WriteVarInt32(static_cast<UInt32>(map.size()));
	this->WriteCommandTreeInternal(a_Packet,map);
	a_Packet.WriteVarInt32(0); // root node
}





cCommandManager::cCommandNode::cCommandNode(
	eNodeType a_Type,
	CommandNodeList a_ChildrenNodes, cCommandNode * a_RedirectNode,
	CmdArgPtr a_ParserArgument, AString a_Name,
	eCommandSuggestionType a_SuggestionType, bool a_IsExecutable, CommandExecutor a_Executioner) :
	m_Type(a_Type),
	m_ChildrenNodes(a_ChildrenNodes),
	m_RedirectNode(a_RedirectNode),
	m_Argument(a_ParserArgument),
	m_Name(a_Name),
	m_SuggestionType(a_SuggestionType),
	m_Executioner(a_Executioner),
	m_IsExecutable(a_IsExecutable)
{
}


  // TODO: implement binary search instead of linear
cCommandManager::cCommandNode *
cCommandManager::cCommandNode::GetLiteralCommandNode(const AString& a_NodeName)
{
	for (auto& var : this->m_ChildrenNodes)
	{
		if (var.m_Name.compare(a_NodeName) == 0 && var.m_Type == eNodeType::Literal)
		{
			return &var;
		}
	}
	return nullptr;
}

void cCommandManager::cCommandNode::WriteCommandTreeInternal(cPacketizer & a_Packet, std::map<cCommandNode *, UInt32> & a_Map)
{
	Byte flags = static_cast<Byte>(this->m_Type) | (this->m_IsExecutable << 2) | ((this->m_RedirectNode != nullptr) << 3) | ((this->m_SuggestionType != eCommandSuggestionType::None) << 4);
	a_Packet.WriteBEInt8(flags);
	a_Packet.WriteVarInt32(static_cast<UInt32>(this->m_ChildrenNodes.size()));

	for (auto& var : this->m_ChildrenNodes)
	{
		auto ptr = &var;
		auto towrite = a_Map[&var];
		a_Packet.WriteVarInt32(towrite);
	}
	
	if (this->m_RedirectNode != nullptr)
	{
		// TODO: write redirect node
	}
	if (this->m_Type != eNodeType::Root)
	{
		a_Packet.WriteString(this->m_Name);
		if (this->m_Type == eNodeType::Argument)
		{
			this->m_Argument->WriteParserID(a_Packet);
			this->m_Argument->WriteProperties(a_Packet);
		}
	}

	switch (this->m_SuggestionType)
	{
		case eCommandSuggestionType::None: /* Do nothing */ break;
		case eCommandSuggestionType::AskServer: a_Packet.WriteString("minecraft:ask_server"); break;
		case eCommandSuggestionType::AllRecipes: a_Packet.WriteString("minecraft:all_recipes"); break;
		case eCommandSuggestionType::AvailableSounds: a_Packet.WriteString("minecraft:available_sounds"); break;
		case eCommandSuggestionType::SummonableEntities: a_Packet.WriteString("minecraft:summonable_entities"); break;
	}
	for (auto & command_node : this->m_ChildrenNodes)
	{
		command_node.WriteCommandTreeInternal(a_Packet,a_Map);
	}
}

std::map<cCommandManager::cCommandNode *, UInt32>
cCommandManager::cCommandNode::ComputeChildrenIds(cCommandNode & a_node)
{
	std::map<cCommandNode *, UInt32> childmap;
	std::vector<cCommandNode*> list;
	CollectChildren(list, a_node);
	childmap[&a_node] = 0;
	int i = 1;
	for each (cCommandNode* var in list)
	{
		childmap[var] = i;
		i++;
	}
	return childmap;
}

std::vector<cCommandManager::cCommandNode*>
cCommandManager::cCommandNode::CollectChildren(std::vector<cCommandNode*>& a_list, cCommandNode& a_node)
{
	for (auto& var : a_node.m_ChildrenNodes)
	{
		auto ptr = &var;
		a_list.push_back(&var);
		CollectChildren(a_list, var);
	}
	return a_list;
}

cCommandManager::cCommandNode *
cCommandManager::cCommandNode::GetNextPotentialNode(BasicStringReader& a_Reader, cCommandExecutionContext& a_Ctx)
{
	const int oldc = a_Reader.GetCursor();
	const auto lit = this->GetLiteralCommandNode(a_Reader.ReadStringUntilWhiteSpace());
	if (lit != nullptr)
	{
		return lit;
	}
	a_Reader.SetCursor(oldc);
	for (auto& var : this->m_ChildrenNodes)
	{
		if (var.m_Type == eNodeType::Argument)
		{
			try
			{
				var.m_Argument->Parse(a_Reader,a_Ctx,var.m_Name);
				return &var;
			}
			catch (const cCommandParseException & ex)
			{
				//  TODO: put exceptions somewhere in case every attempt fails. for now just silently fail
				a_Reader.SetCursor(oldc);
			}
		}
	}
	return nullptr;
}
