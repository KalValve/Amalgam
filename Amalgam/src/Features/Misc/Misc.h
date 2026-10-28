#pragma once
#include "../../SDK/SDK.h"

class CMisc
{
private:
	void AutoJump(CTFPlayer* pLocal, CUserCmd* pCmd);
	void AutoJumpbug(CTFPlayer* pLocal, CUserCmd* pCmd);
	void AutoFaNJump(CTFPlayer* pLocal, CUserCmd* pCmd);
	void AutoRevJump(CTFPlayer* pLocal, CUserCmd* pCmd);
	void AutoStrafe(CTFPlayer* pLocal, CUserCmd* pCmd);
	void MovementLock(CTFPlayer* pLocal, CUserCmd* pCmd);
	void BreakJump(CTFPlayer* pLocal, CUserCmd* pCmd);
	void AntiAFK(CTFPlayer* pLocal, CUserCmd* pCmd);
	void InstantRespawnMVM(CTFPlayer* pLocal);
	void NoisemakerSpam(CTFPlayer* pLocal);
	void VoiceCommandSpam(CTFPlayer* pLocal);
	void CallVoteSpam(CTFPlayer* pLocal);
	void AchievementSpam(CTFPlayer* pLocal);
	void AutoReport();
	void ChatSpam(CTFPlayer* pLocal);
	void AutoRetry(CTFPlayer* pLocal);
	void AutoDisguise(CTFPlayer* pLocal);
	void DoKillSay(int iVictim);
	void EnsureChatUtilsDoc();
	bool LoadLines(const char* szFileName, std::vector<std::string>& vLines, const char* szDefaultContent = nullptr);
	std::vector<std::string> ParseTokens(std::string str, char delimiter);
	struct AutoReply_t { std::vector<std::string> vTriggers; std::vector<std::string> vReplies; };
	std::vector<std::string> m_vChatSpamLines;
	std::vector<std::string> m_vKillSayLines;
	std::vector<std::string> m_vF1Messages;
	std::vector<std::string> m_vF2Messages;
	std::vector<AutoReply_t> m_vAutoReplies;
	Timer m_tChatSpamTimer;
	Timer m_tCallVoteSpamTimer;
	int m_iCurrentChatSpamIndex = 0;
	std::string m_sLastKilledName;
	enum class AchievementSpamState { IDLE, CLEARING, WAITING, AWARDING };
	AchievementSpamState m_eAchievementSpamState = AchievementSpamState::IDLE;
	Timer m_tAchievementSpamTimer;
	Timer m_tAchievementDelayTimer;
	int m_iAchievementSpamID = 0;
	std::string m_sAchievementSpamName;

	void CheatsBypass();
	void WeaponSway();

	void TauntKartControl(CTFPlayer* pLocal, CUserCmd* pCmd);
	void FastMovement(CTFPlayer* pLocal, CUserCmd* pCmd);

	void AutoPeek(CTFPlayer* pLocal, CUserCmd* pCmd, bool bPost = false);
	void EdgeJump(CTFPlayer* pLocal, CUserCmd* pCmd, bool bPost = false);

	bool m_bPeekPlaced = false;
	Vec3 m_vPeekReturnPos = {};
	bool m_bAutoBalanceTeamChangePending = false;

	//bool bSteamCleared = false;

public:
	void RunPre(CTFPlayer* pLocal, CUserCmd* pCmd);
	void RunPost(CTFPlayer* pLocal, CUserCmd* pCmd);

	void Event(IGameEvent* pEvent, uint32_t uNameHash);
	void MicSpam();
	void OnVoteStart(int iCaller, int iTarget, const std::string& sTarget);
	void OnChatMessage(int iEntIndex, const std::string& sName, const std::string& sMsg);
	std::string ReplaceTags(std::string sMsg, std::string sTarget = "", std::string sInitiator = "", std::string sKiller = "", std::string sVictim = "");
	void SetAutoBalanceTeamChangePending(bool bPending) { m_bAutoBalanceTeamChangePending = bPending; }
	int AntiBackstab(CTFPlayer* pLocal, CUserCmd* pCmd, bool bSendPacket);

	void PingReducer();
	void UnlockAchievements();
	void LockAchievements();

	int m_iWishCmdrate = -1;
	int m_iWishUpdaterate = -1;
	bool m_bAntiAFK = false;
};

ADD_FEATURE(CMisc, Misc);
