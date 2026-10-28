#include "AutoVote.h"

#include "../../Misc/NamedPipe/NamedPipe.h"
#include "../../Players/PlayerUtils.h"

void CAutoVote::UserMessage(bf_read& msgData)
{
	msgData.ReadByte();
	const int iVoteID = msgData.ReadLong();
	msgData.ReadByte();
	char sReason[256]; msgData.ReadString(sReason, sizeof(sReason));
	char sTarget[256]; msgData.ReadString(sTarget, sizeof(sTarget));
	const int iTarget = msgData.ReadByte() >> 1;
	msgData.Seek(0);

	if (Vars::Misc::Automation::AutoF2Ignored.Value
		&& (F::PlayerUtils.IsIgnored(iTarget) || H::Entities.IsFriend(iTarget) || H::Entities.InParty(iTarget)))
		I::ClientState->SendStringCmd(std::format("vote {} option2", iVoteID).c_str());
	else if (Vars::Misc::Automation::AutoF1Priority.Value && F::PlayerUtils.IsPrioritized(iTarget)
		&& !H::Entities.IsFriend(iTarget) && !H::Entities.InParty(iTarget))
		I::ClientState->SendStringCmd(std::format("vote {} option1", iVoteID).c_str());
}

static void HandleVote(CTFPlayerResource* pResource, const int iVoteID, const int iCaller, const int iTarget)
{
	auto uTargetAccountID = pResource->m_iAccountID(iTarget);
	const int iTargetVotePriority = F::PlayerUtils.GetVotePriority(uTargetAccountID);
	const bool bDefendTarget = (Vars::Misc::Automation::AutoVote.Value & Vars::Misc::Automation::AutoVoteEnum::Defend)
		&& iTargetVotePriority < 0;
	const bool bKickTarget = (Vars::Misc::Automation::AutoVote.Value & Vars::Misc::Automation::AutoVoteEnum::Kick)
		&& iTargetVotePriority > 0;
	if (bDefendTarget)
	{
		I::ClientState->SendStringCmd(std::format("vote {} option2", iVoteID).c_str());
		return;
	}
	if (pResource->m_bValid(iCaller)
		&& (H::Entities.IsFriend(pResource->m_iAccountID(iCaller))
			|| H::Entities.InParty(pResource->m_iAccountID(iCaller))))
	{
		I::ClientState->SendStringCmd(std::format("vote {} option1", iVoteID).c_str());
		return;
	}
	if (bKickTarget)
	{
		I::ClientState->SendStringCmd(std::format("vote {} option1", iVoteID).c_str());
		return;
	}
	if (pResource->m_bValid(iCaller)
		&& (Vars::Misc::Automation::AutoVote.Value & Vars::Misc::Automation::AutoVoteEnum::Assist)
		&& F::PlayerUtils.GetVotePriority(pResource->m_iAccountID(iCaller)) > 0)
	{
		I::ClientState->SendStringCmd(std::format("vote {} option1", iVoteID).c_str());
		return;
	}
	if (iTargetVotePriority == 0)
		return;

}

void CAutoVote::OnVoteStart(const int iTeam, const int iVoteID, const int iCaller, const int iTarget)
{
	int iLocalIdx = I::EngineClient->GetLocalPlayer();
	if (iCaller == iLocalIdx || iTarget == iLocalIdx)
	{
		m_bActiveVote = true;
		return;
	}

	auto pLocal = H::Entities.GetLocal();
	if (!pLocal)
	{
		m_mPendingVotes[iVoteID] = Vote_t{ iTeam, iCaller, iTarget, I::GlobalVars->curtime, 0.f, false };
		return;
	}

	if (!pLocal->IsInValidTeam() || iTeam != pLocal->m_iTeamNum())
		return;

	auto pResource = H::Entities.GetResource();
	if (!pResource)
	{
		m_mPendingVotes[iVoteID] = Vote_t{ iTeam, iCaller, iTarget, I::GlobalVars->curtime, 0.f, false };
		return;
	}

	if (!pResource->m_bValid(iTarget))
		return;

	if (Vars::Misc::Automation::AutoVoteDelay.Value
		|| !(Vars::Misc::Automation::AutoVote.Value & Vars::Misc::Automation::AutoVoteEnum::Defend)
		&& !(Vars::Misc::Automation::AutoVote.Value & Vars::Misc::Automation::AutoVoteEnum::Assist)
		&& !(Vars::Misc::Automation::AutoVote.Value & Vars::Misc::Automation::AutoVoteEnum::Kick)
		&& !(Vars::Misc::Automation::AutoVote.Value & Vars::Misc::Automation::AutoVoteEnum::KickAll))
	{
		m_mPendingVotes[iVoteID] = Vote_t{ iTeam, iCaller, iTarget, I::GlobalVars->curtime, 0.f, false };
		return;
	}

	HandleVote(pResource, iVoteID, iCaller, iTarget);
}

void CAutoVote::OnVoteEnd(int iVoteID)
{
	m_mPendingVotes.erase(iVoteID);
	m_bActiveVote = false;
}

void CAutoVote::OnCallVoteFail(int iTimeLeft)
{
	m_flCallCooldownExpireTime = I::GlobalVars->curtime + iTimeLeft;
}

void CAutoVote::Run(CTFPlayer* pLocal)
{
	if (!(Vars::Misc::Automation::AutoVote.Value & Vars::Misc::Automation::AutoVoteEnum::Defend)
		&& !(Vars::Misc::Automation::AutoVote.Value & Vars::Misc::Automation::AutoVoteEnum::Assist)
		&& !(Vars::Misc::Automation::AutoVote.Value & Vars::Misc::Automation::AutoVoteEnum::Kick)
		&& !(Vars::Misc::Automation::AutoVote.Value & Vars::Misc::Automation::AutoVoteEnum::KickAll)
		|| !pLocal->IsInValidTeam())
	{
		Reset();
		return;
	}

	auto pResource = H::Entities.GetResource();
	if (!pResource)
		return;

	int iLocalTeam = pLocal->m_iTeamNum();
	if (!m_mPendingVotes.empty())
	{
		static auto sv_vote_timer_duration = H::ConVars.FindVar("sv_vote_timer_duration");

		float flCurtime = I::GlobalVars->curtime;
		float flMaxVoteDuration = sv_vote_timer_duration->GetFloat() + 1.f;

		std::vector<int> vErase = {};
		for (auto& [iVoteID, tVoteInfo] : m_mPendingVotes)
		{
			bool bOtherTeam = tVoteInfo.m_iTeam != iLocalTeam;
			if (bOtherTeam || !pResource->m_bValid(tVoteInfo.m_iTarget)
				|| tVoteInfo.m_flStartTime + flMaxVoteDuration <= flCurtime)
			{
				if (!bOtherTeam || tVoteInfo.m_bVoted)
					m_bActiveVote = false;
				vErase.push_back(iVoteID);
				continue;
			}

			// Already checked
			if (tVoteInfo.m_bVoted)
				continue;

			m_bActiveVote = true;

			if (!(Vars::Misc::Automation::AutoVote.Value & Vars::Misc::Automation::AutoVoteEnum::Defend)
				&& !(Vars::Misc::Automation::AutoVote.Value & Vars::Misc::Automation::AutoVoteEnum::Assist)
				&& !(Vars::Misc::Automation::AutoVote.Value & Vars::Misc::Automation::AutoVoteEnum::Kick)
				&& !(Vars::Misc::Automation::AutoVote.Value & Vars::Misc::Automation::AutoVoteEnum::KickAll))
			{
				tVoteInfo.m_bVoted = true;
				continue;
			}

			if (Vars::Misc::Automation::AutoVoteDelay.Value)
			{
				if (tVoteInfo.m_flVoteTime == 0.f)
					tVoteInfo.m_flVoteTime = tVoteInfo.m_flStartTime + SDK::RandomFloat(Vars::Misc::Automation::AutoVoteDelayInterval.Value.Min, Vars::Misc::Automation::AutoVoteDelayInterval.Value.Max);
				if (tVoteInfo.m_flVoteTime >= flCurtime)
				{
					m_bActiveVote = true;
					break;
				}
			}

			// Mark so we ignore it later
			tVoteInfo.m_bVoted = true;

			HandleVote(pResource, iVoteID, tVoteInfo.m_iCaller, tVoteInfo.m_iTarget);
			break;
		}
		if (!vErase.empty())
		{
			for (auto iEraseID : vErase)
				m_mPendingVotes.erase(iEraseID);
		}
		if (m_bActiveVote)
			return;
	}

	if (!(Vars::Misc::Automation::AutoVote.Value & Vars::Misc::Automation::AutoVoteEnum::Kick)
		&& !(Vars::Misc::Automation::AutoVote.Value & Vars::Misc::Automation::AutoVoteEnum::KickAll))
		return;

	static Timer tAutoVotekickTimer = {};
	if (Vars::Misc::Automation::AutoVote.Value & Vars::Misc::Automation::AutoVoteEnum::Cooldown)
	{
		if (m_bActiveVote || (int)(m_flCallCooldownExpireTime - I::GlobalVars->curtime) > 0)
			return;
	}
	else if (!tAutoVotekickTimer.Run(1.f))
		return;

	int iLocalIdx = pLocal->entindex();
	std::vector<int> vPotentialTargets;
	for (int i = 1; i <= I::EngineClient->GetMaxClients(); i++)
	{
		if (i == iLocalIdx)
			continue;

		if (!pResource->m_bValid(i)
			|| pResource->IsFakePlayer(i)
			|| pResource->m_iTeam(i) != iLocalTeam)
			continue;

		auto uAccountID = pResource->m_iAccountID(i);
#ifdef TEXTMODE
		if (F::NamedPipe.IsLocalBot(uAccountID))
			continue;
#endif
		const int iVotePriority = F::PlayerUtils.GetVotePriority(uAccountID);
		const bool bKickAll = Vars::Misc::Automation::AutoVote.Value & Vars::Misc::Automation::AutoVoteEnum::KickAll;
		if (iVotePriority < 0 || (!bKickAll && iVotePriority <= 0))
			continue;

		if (F::PlayerUtils.IsIgnored(uAccountID)
			|| H::Entities.IsFriend(uAccountID)
			|| H::Entities.InParty(uAccountID))
			continue;

		vPotentialTargets.push_back(i);
	}

	if (vPotentialTargets.empty())
		return;

	int iRandom = SDK::RandomInt(0, static_cast<int>(vPotentialTargets.size()) - 1);
	int iTarget = vPotentialTargets[iRandom];

	static auto sv_vote_creation_timer = H::ConVars.FindVar("sv_vote_creation_timer");
	m_flCallCooldownExpireTime = I::GlobalVars->curtime + sv_vote_creation_timer->GetFloat();
	I::ClientState->SendStringCmd(std::format("callvote Kick \"{} other\"", pResource->m_iUserID(iTarget)).c_str());
}

void CAutoVote::Reset()
{
	m_flCallCooldownExpireTime = 0.f;
	m_bActiveVote = false;
	m_mPendingVotes.clear();
}
