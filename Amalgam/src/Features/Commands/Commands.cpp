#include "Commands.h"

#include "../../Core/Core.h"
#include "../ImGui/Menu/Menu.h"
#include "../Configs/Configs.h"
#include "../Misc/AutoItem/AutoItem.h"
#include "../Misc/Misc.h"
#include "../NavBot/NavEngine.h"
#include "../Players/PlayerUtils.h"
#include <utility>
#include <boost/algorithm/string/replace.hpp>

#define AddCommand(sCommand, ...) \
{ \
	FNV1A::Hash32Const(sCommand), \
	[](const std::deque<const char*>& vArgs) \
		__VA_ARGS__ \
},

//struct ConVarValues_t
//{
//	int m_iValue;
//	float m_flValue;
//	const char* m_sValue;
//};
//static std::unordered_map<ConVar*, ConVarValues_t> s_mConVarValues = {};

static std::unordered_map<uint32_t, CommandCallback> s_mCommands = {
	AddCommand("setcvar",
	{
		if (vArgs.size() < 2)
		{
			SDK::Output("Usage:\n\tsetcvar <cvar> <value>");
			return;
		}

		const char* sCVar = vArgs[0];
		auto pCVar = I::CVar->FindVar(sCVar);
		if (!pCVar)
		{
			SDK::Output(std::format("Could not find {}", sCVar).c_str());
			return;
		}

		std::string sValue = "";
		for (int i = 1; i < vArgs.size(); i++)
			sValue += std::format("{} ", vArgs[i]);
		sValue.pop_back();
		boost::replace_all(sValue, "\"", "");

		pCVar->SetValue(sValue.c_str());
		SDK::Output(std::format("Set {} to {}", sCVar, sValue).c_str());
	})
	AddCommand("getcvar",
	{
		if (vArgs.size() != 1)
		{
			SDK::Output("Usage:\n\tgetcvar <cvar>");
			return;
		}

		const char* sCVar = vArgs[0];
		auto pCVar = I::CVar->FindVar(sCVar);
		if (!pCVar)
		{
			SDK::Output(std::format("Could not find {}", sCVar).c_str());
			return;
		}

		SDK::Output(std::format("Value of {} is {}", sCVar, pCVar->GetString()).c_str());
	})
	AddCommand("queue",
	{
		if (!I::TFPartyClient->AnySelected())
			I::TFPartyClient->LoadSavedCasualCriteria();
		I::TFPartyClient->RequestQueueForMatch(k_eTFMatchGroup_Casual_Default);
	})
	AddCommand("clear_chat",
	{
		I::ClientModeShared->m_pChatElement->SetText("");
	})
	AddCommand("menu",
	{
		I::MatSystemSurface->SetCursorAlwaysVisible(F::Menu.m_bIsOpen = !F::Menu.m_bIsOpen);
	})
	AddCommand("unload",
	{
		if (F::Menu.m_bIsOpen)
			I::MatSystemSurface->SetCursorAlwaysVisible(F::Menu.m_bIsOpen = false);
		U::Core.m_bUnload = true;
	})
	AddCommand("crash",
	{	// if you want to time out of a server and rejoin
		switch (vArgs.empty() ? 0 : FNV1A::Hash32(vArgs.front()))
		{
		case FNV1A::Hash32Const("true"):
		case FNV1A::Hash32Const("t"):
		case FNV1A::Hash32Const("1"):
			break;
		default:
			Vars::Debug::CrashLogging.Value = false; // we are voluntarily crashing, don't give out log if we don't want one
		}
		reinterpret_cast<void(*)()>(0)();
	})
	//AddCommand("recordcvars",
	//{
	//	for (ConCommandBase* pBase = I::CVar->GetCommands(); pBase; pBase = pBase->m_pNext)
	//	{
	//		if (pBase->IsCommand())
	//			continue;

	//		auto pCVar = reinterpret_cast<ConVar*>(pBase);
	//		s_mConVarValues[pCVar] = { pCVar->GetInt(), pCVar->GetFloat(), pCVar->GetString() };
	//	}
	//})
	//AddCommand("diffcvars",
	//{
	//	for (auto& [pCVar, tValues] : s_mConVarValues)
	//	{
	//		if (auto iOld = tValues.m_iValue, iNew = pCVar->GetInt(); iOld != iNew)
	//			SDK::Output(pCVar->GetName(), std::format("int: {} -> {}", iOld, iNew).c_str());
	//		else if (auto flOld = tValues.m_flValue, flNew = pCVar->GetFloat(); flOld != flNew)
	//			SDK::Output(pCVar->GetName(), std::format("float: {} -> {}", flOld, flNew).c_str());
	//		else if (auto sOld = tValues.m_sValue, sNew = pCVar->GetString(); sOld != sNew || FNV1A::Hash32(sOld) != FNV1A::Hash32(sNew))
	//			SDK::Output(pCVar->GetName(), std::format("string: {} -> {}", sOld, sNew).c_str());
	//	}
	//})
};

static const bool s_bCatAliasesRegistered = []
{
	const std::pair<const char*, const char*> vAliases[] = {
		{ "cat_setcvar", "setcvar" }, { "cat_getcvar", "getcvar" },
		{ "cat_queue", "queue" }, { "cat_clearchat", "clear_chat" },
		{ "cat_menu", "menu" }, { "cat_unload", "unload" }, { "cat_crash", "crash" }
	};
	for (const auto& [sAlias, sCommand] : vAliases)
		s_mCommands.emplace(FNV1A::Hash32Const(sAlias), s_mCommands.at(FNV1A::Hash32Const(sCommand)));
	return true;
}();

static std::unordered_map<uint32_t, CommandCallback> s_mExtraCommands = {
	AddCommand("cat_kill",
	{
		I::EngineClient->ClientCmd_Unrestricted(SDK::RandomInt(0, 1) ? "kill" : "explode");
	})
	AddCommand("cat_criteria",
	{
		if (!I::TFPartyClient)
		{
			SDK::Output("cat_criteria", "TFPartyClient interface unavailable");
			return;
		}
		I::TFPartyClient->LoadSavedCasualCriteria();
		SDK::Output("Loaded saved casual criteria.");
	})
	AddCommand("cat_abandon",
	{
		if (!I::TFGCClientSystem)
		{
			SDK::Output("cat_abandon", "TFGCClientSystem interface unavailable");
			return;
		}
		I::TFGCClientSystem->AbandonCurrentMatch();
		SDK::Output("Requested match abandon.");
	})
	AddCommand("cat_load",
	{
		if (vArgs.size() != 1)
		{
			SDK::Output("Usage:\n\tcat_load <config_name>");
			return;
		}
		F::Configs.LoadConfig(vArgs.front(), true);
	})
	AddCommand("cat_path_to",
	{
		if (vArgs.size() < 3)
		{
			SDK::Output("Usage:\n\tcat_path_to <x> <y> <z>");
			return;
		}

		try
		{
			const Vec3 vDest(std::stof(vArgs[0]), std::stof(vArgs[1]), std::stof(vArgs[2]));
			if (!F::NavEngine.NavTo(vDest))
				SDK::Output("cat_path_to", "Could not start path; check that a nav mesh is loaded and the destination is reachable");
		}
		catch (...)
		{
			SDK::Output("Usage:\n\tcat_path_to <x> <y> <z>");
		}
	})
	AddCommand("cat_cancel_path",
	{
		F::NavEngine.CancelPath();
	})
	AddCommand("cat_save_nav_mesh",
	{
		if (auto pNavFile = F::NavEngine.GetNavFile())
			pNavFile->Write();
		else
			SDK::Output("cat_save_nav_mesh", "No nav mesh is loaded");
	})
	AddCommand("cat_ignore",
	{
		if (vArgs.size() < 2)
		{
			SDK::Output("Usage:\n\tcat_ignore <id32> <tag|rage>");
			return;
		}

		uint32_t uAccountID = 0;
		try
		{
			const unsigned long uParsed = std::stoul(vArgs[0]);
			if (uParsed > UINT32_MAX)
				throw std::out_of_range("id32");
			uAccountID = static_cast<uint32_t>(uParsed);
		}
		catch (...)
		{
			SDK::Output("Invalid ID32 format");
			return;
		}
		if (!uAccountID)
		{
			SDK::Output("Invalid ID32");
			return;
		}

		std::string sTag = vArgs[1];
		std::transform(sTag.begin(), sTag.end(), sTag.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		const int iTagID = sTag == "rage"
			? F::PlayerUtils.TagToIndex(CHEATER_TAG)
			: F::PlayerUtils.GetTag(sTag);
		if (iTagID == -1)
		{
			SDK::Output(std::format("Invalid tag: {}", sTag).c_str());
			return;
		}

		auto pTag = F::PlayerUtils.GetTag(iTagID);
		if (!pTag || !pTag->m_bAssignable)
		{
			SDK::Output(std::format("Tag {} is not assignable", sTag).c_str());
			return;
		}
		if (F::PlayerUtils.HasTag(uAccountID, iTagID))
		{
			SDK::Output(std::format("ID32 {} already has tag {}", uAccountID, pTag->m_sName).c_str());
			return;
		}

		F::PlayerUtils.AddTag(uAccountID, iTagID, true);
		SDK::Output(std::format("Added tag {} to ID32 {}", pTag->m_sName, uAccountID).c_str());
	})
	AddCommand("cat_rent_item",
	{
		if (vArgs.size() != 1)
		{
			SDK::Output("Usage:\n\tcat_rent_item <item_def_index>");
			return;
		}
		try
		{
			F::AutoItem.Rent(static_cast<item_definition_index_t>(std::stoi(vArgs.front())));
		}
		catch (...)
		{
			SDK::Output("Invalid item_def_index");
		}
	})
	AddCommand("cat_achievement_unlock",
	{
		if (!vArgs.empty() && FNV1A::Hash32(vArgs.front()) != FNV1A::Hash32Const("all") && FNV1A::Hash32(vArgs.front()) != FNV1A::Hash32Const("normal"))
		{
			SDK::Output("Usage:\n\tcat_achievement_unlock [all]");
			return;
		}
		F::Misc.UnlockAchievements();
	})
	AddCommand("cat_achievement_lock",
	{
		if (!vArgs.empty() && FNV1A::Hash32(vArgs.front()) != FNV1A::Hash32Const("all") && FNV1A::Hash32(vArgs.front()) != FNV1A::Hash32Const("normal"))
		{
			SDK::Output("Usage:\n\tcat_achievement_lock [all]");
			return;
		}
		F::Misc.LockAchievements();
	})
	AddCommand("cat_mvm_quit",
	{
		if (I::TFGCClientSystem)
			I::TFGCClientSystem->AbandonCurrentMatch();
		I::EngineClient->ClientCmd_Unrestricted("disconnect");
		SDK::Output("cat_mvm_quit", "Abandoned match and disconnected.");
	})
	AddCommand("cat_mvm_tele",
	{
		auto pLocal = H::Entities.GetLocal();
		if (!pLocal || !pLocal->IsAlive())
		{
			SDK::Output("cat_mvm_tele", "Local player unavailable");
			return;
		}

		CBaseObject* pBest = nullptr;
		float flBestDist = FLT_MAX;
		for (auto pEntity : H::Entities.GetGroup(EntityEnum::BuildingTeam))
		{
			if (!pEntity || pEntity->IsDormant() || pEntity->GetClassID() != ETFClassID::CObjectTeleporter)
				continue;

			auto pTeleporter = pEntity->As<CObjectTeleporter>();
			if (pTeleporter->m_iObjectMode() != 0 || pTeleporter->m_bPlacing() || pTeleporter->m_bCarried() || pTeleporter->m_bDisabled())
				continue;

			const float flDist = pLocal->GetAbsOrigin().DistToSqr(pTeleporter->GetAbsOrigin());
			if (flDist >= flBestDist)
				continue;

			flBestDist = flDist;
			pBest = pTeleporter;
		}

		if (!pBest || !F::NavEngine.NavTo(pBest->GetAbsOrigin()))
		{
			SDK::Output("cat_mvm_tele", "No reachable teleporter entrance found");
			return;
		}

		SDK::Output("cat_mvm_tele", "Pathing to closest teleporter entrance");
	})
	AddCommand("cat_mvm_rent",
	{
		F::AutoItem.MvmRent();
	})
};

static const bool s_bExtraCommandsRegistered = []
{
	for (const auto& [uCommand, fnCallback] : s_mExtraCommands)
		s_mCommands.emplace(uCommand, fnCallback);
	return true;
}();

bool CCommands::Run(const char* sCmd, std::deque<const char*>& vArgs)
{
	std::string sLower = sCmd;
	std::transform(sLower.begin(), sLower.end(), sLower.begin(), ::tolower);

	auto uHash = FNV1A::Hash32(sLower.c_str());
	if (!s_mCommands.contains(uHash))
		return false;

	s_mCommands[uHash](vArgs);
	return true;
}
