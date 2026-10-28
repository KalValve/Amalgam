#include "../SDK/SDK.h"

#include "../Features/Misc/Misc.h"
#include "../Features/Output/Output.h"
#include "../Features/NoSpread/NoSpreadHitscan/NoSpreadHitscan.h"
#include "../Features/Misc/AutoVote/AutoVote.h"
#include "../Features/Aimbot/AutoHeal/AutoHeal.h"

//#define DEBUG_VISUALS
#ifdef DEBUG_VISUALS
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
#endif

MAKE_HOOK(CHLClient_DispatchUserMessage, U::Memory.GetVirtual(I::Client, 36), bool,
	void* rcx, UserMessageType type, bf_read& msgData)
{
	DEBUG_RETURN(CHLClient_DispatchUserMessage, rcx, type, msgData);

	auto bufData = reinterpret_cast<const char*>(msgData.m_pData);
	msgData.SetAssertOnOverflow(false);
	msgData.Seek(0);

	switch (type)
	{
	case VotePass:
	case VoteFailed:
	{
		msgData.SeekRelative(4);
		const int iVoteID = msgData.ReadLong();
		F::AutoVote.OnVoteEnd(iVoteID);
		break;
	}
	case CallVoteFailed:
	{
		if (Vars::Misc::Automation::AutoVote.Value & Vars::Misc::Automation::AutoVoteEnum::Kick)
		{
			const int iReason = msgData.ReadByte();
			if (iReason == VOTE_FAILED_ON_COOLDOWN || iReason == VOTE_FAILED_RATE_EXCEEDED)
				F::AutoVote.OnCallVoteFail(msgData.ReadShort());
			msgData.Seek(0);
		}
		break;
	}
	case VoteStart:
	{
		const int iTeam = msgData.ReadByte();
		const int iVoteID = msgData.ReadLong();
		const int iCaller = msgData.ReadByte();
		char sReason[256]; msgData.ReadString(sReason, sizeof(sReason));
		char sTarget[256]; msgData.ReadString(sTarget, sizeof(sTarget));
		const int iTarget = msgData.ReadByte() >> 1;
		msgData.Seek(0);

		F::Output.UserMessage(msgData);
		if (std::string(sReason).find("kick") != std::string::npos)
		{
			F::AutoVote.OnVoteStart(iTeam, iVoteID, iCaller, iTarget);
			F::Misc.OnVoteStart(iCaller, iTarget, sTarget);
		}
		if (!Vars::Misc::Automation::AutoVote.Value)
			F::AutoVote.UserMessage(msgData);
		break;
	}
	case VoiceSubtitle:
	{
		int iEntityID = msgData.ReadByte();
		int iVoiceMenu = msgData.ReadByte();
		int iCommandID = msgData.ReadByte();
		if (iVoiceMenu == 1 && iCommandID == 6)
			F::AutoHeal.m_mMedicCallers[iEntityID];

		break;
	}
	case SayText2:
	{
		const int iEntityID = msgData.ReadByte();
		msgData.ReadByte();
		char sMessageName[256]{}; msgData.ReadString(sMessageName, sizeof(sMessageName));
		char sName[256]{}; msgData.ReadString(sName, sizeof(sName));
		char sMessage[256]{}; msgData.ReadString(sMessage, sizeof(sMessage));
		F::Misc.OnChatMessage(iEntityID, sName, sMessage);
		break;
	}
	case TextMsg:
	{
		char rawMsg[256]; msgData.ReadString(rawMsg, sizeof(rawMsg), true);
		msgData.Seek(0);
		std::string sMsg = rawMsg;
		if (!sMsg.empty())
		{
			sMsg.erase(sMsg.begin());

			if (F::NoSpreadHitscan.ParsePlayerPerf(sMsg))
				return true;

#ifdef DEBUG_VISUALS
			if (sMsg.find("[Box] ") == 0)
			{
				try
				{
					sMsg.replace(0, strlen("[Box] "), "");
					std::vector<std::string> vValues = {};
					boost::split(vValues, sMsg, boost::is_any_of(" "));
					if (vValues.size() != 17)
						return true;

					Vec3 vOrigin = { std::stof(vValues[0]), std::stof(vValues[1]), std::stof(vValues[2]) };
					Vec3 vMins = { std::stof(vValues[3]), std::stof(vValues[4]), std::stof(vValues[5]) };
					Vec3 vMaxs = { std::stof(vValues[6]), std::stof(vValues[7]), std::stof(vValues[8]) };
					Vec3 vAngles = { std::stof(vValues[9]), std::stof(vValues[10]), std::stof(vValues[11]) };
					Color_t tColor = { byte(std::stoi(vValues[12])), byte(std::stoi(vValues[13])), byte(std::stoi(vValues[14])), byte(255 - std::stoi(vValues[15])) };
					float flDuration = std::stof(vValues[16]);

					G::BoxStorage.emplace_back(vOrigin, vMins, vMaxs, vAngles, I::GlobalVars->curtime + flDuration, tColor, Color_t(0, 0, 0, 0), true);
				}
				catch (...) {}

				return true;
			}
			if (sMsg.find("[Line] ") == 0)
			{
				try
				{
					sMsg.replace(0, strlen("[Line] "), "");
					std::vector<std::string> vValues = {};
					boost::split(vValues, sMsg, boost::is_any_of(" "));
					if (vValues.size() != 11)
						return true;

					Vec3 vStart = { std::stof(vValues[0]), std::stof(vValues[1]), std::stof(vValues[2]) };
					Vec3 vEnd = { std::stof(vValues[3]), std::stof(vValues[4]), std::stof(vValues[5]) };
					Color_t tColor = { byte(std::stoi(vValues[6])), byte(std::stoi(vValues[7])), byte(std::stoi(vValues[8])), byte(255 - std::stoi(vValues[9])) };
					float flDuration = std::stof(vValues[10]);

					G::LineStorage.emplace_back(std::pair<Vec3, Vec3>(vStart, vEnd), I::GlobalVars->curtime + flDuration, tColor, true);
				}
				catch (...) {}

				return true;
			}
#endif

			if (FNV1A::Hash32(sMsg.c_str()) == FNV1A::Hash32Const("#TF_Autobalance_TeamChangePending")
				&& Vars::Misc::Automation::AntiAutobalance.Value
				&& Vars::Misc::Automation::AntiAutobalanceMode.Value != Vars::Misc::Automation::AntiAutobalanceModeEnum::Off
				&& !I::EngineClient->IsPlayingDemo())
			{
				const int iMode = Vars::Misc::Automation::AntiAutobalanceMode.Value;
				if (iMode == Vars::Misc::Automation::AntiAutobalanceModeEnum::OnDeath
					|| iMode == Vars::Misc::Automation::AntiAutobalanceModeEnum::Both)
					F::Misc.SetAutoBalanceTeamChangePending(true);

				if (iMode == Vars::Misc::Automation::AntiAutobalanceModeEnum::OnPrompt
					|| iMode == Vars::Misc::Automation::AntiAutobalanceModeEnum::Both)
					I::EngineClient->ClientCmd_Unrestricted("retry");
			}
		}
		break;
	}
	case VGUIMenu:
		if (Vars::Visuals::Removals::MOTD.Value && bufData
			&& FNV1A::Hash32(bufData) == FNV1A::Hash32Const("info"))
		{
			I::EngineClient->ClientCmd_Unrestricted("closedwelcomemenu");
			return true;
		}
		break;
	case ForcePlayerViewAngles:
		return Vars::Visuals::Removals::AngleForcing.Value ? true : CALL_ORIGINAL(rcx, type, msgData);
	case SpawnFlyingBird:
	case PlayerGodRayEffect:
	case PlayerTauntSoundLoopStart:
	case PlayerTauntSoundLoopEnd:
		return Vars::Visuals::Removals::Taunts.Value ? true : CALL_ORIGINAL(rcx, type, msgData);
	case Shake:
	case Fade:
	case Rumble:
		return Vars::Visuals::Removals::ScreenEffects.Value ? true : CALL_ORIGINAL(rcx, type, msgData);
	}

	msgData.Seek(0);
	return CALL_ORIGINAL(rcx, type, msgData);
}
