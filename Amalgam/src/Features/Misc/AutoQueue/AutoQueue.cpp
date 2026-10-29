#include "AutoQueue.h"
#include "../../Players/PlayerUtils.h"
#ifdef TEXTMODE
#include "../NamedPipe/NamedPipe.h"
#endif

void CAutoQueue::Run()
{
	static float flLastRequeueTime = 0.0f;
	static bool bWasInGame = false;
	static bool bQueuedFromRequeueIf = false;

	const bool bInGame = I::EngineClient->IsInGame();
	const bool bIsLoadingMap = I::EngineClient->IsDrawingLoadingImage();
	const bool bIsConnected = I::EngineClient->IsConnected();
	const bool bHasNetChannel = I::ClientState && I::ClientState->m_NetChannel;
	const bool bStillAttachedToServer = bInGame || bIsConnected || bHasNetChannel;
	const float flCurrentTime = I::GlobalVars->realtime;

	if (!Vars::Misc::Queueing::AutoCasualQueue.Value)
	{
		bWasInGame = false;
		bQueuedFromRequeueIf = false;
		flLastRequeueTime = 0.0f;
		return;
	}

	if (bIsLoadingMap && Vars::Misc::Queueing::RQLTM.Value)
	{
		if (I::TFPartyClient->BInQueueForMatchGroup(k_eTFMatchGroup_Casual_Default))
			I::TFPartyClient->CancelMatchQueueRequest(k_eTFMatchGroup_Casual_Default);
		bQueuedFromRequeueIf = false;
		flLastRequeueTime = flCurrentTime;
		return;
	}

	int nPlayerCount = 0;
	bool bRequeueConditionMet = false;
	if (bInGame && Vars::Misc::Queueing::RQif.Value)
	{
		if (auto pResource = H::Entities.GetResource())
		{
			for (int i = 1; i <= I::EngineClient->GetMaxClients(); i++)
			{
				if (!pResource->m_bValid(i) || !pResource->m_bConnected(i) || pResource->m_iUserID(i) == -1 || pResource->IsFakePlayer(i))
					continue;

				bool bShouldCount = true;
				const uint32_t uAccountID = pResource->m_iAccountID(i);
				if (Vars::Misc::Queueing::RQIgnoreFriends.Value)
				{
#ifdef TEXTMODE
					if (uAccountID && F::NamedPipe.IsLocalBot(uAccountID))
						bShouldCount = false;
#endif
					if (bShouldCount && (H::Entities.IsFriend(uAccountID) || H::Entities.InParty(uAccountID) ||
						F::PlayerUtils.HasTag(uAccountID, F::PlayerUtils.TagToIndex(FRIEND_TAG)) ||
						F::PlayerUtils.HasTag(uAccountID, F::PlayerUtils.TagToIndex(IGNORED_TAG)) ||
						F::PlayerUtils.HasTag(uAccountID, F::PlayerUtils.TagToIndex(PARTY_TAG))))
						bShouldCount = false;
				}

				if (bShouldCount)
					nPlayerCount++;
			}
		}

		const int nPlayersLT = Vars::Misc::Queueing::RQplt.Value;
		const int nPlayersGT = Vars::Misc::Queueing::RQpgt.Value;
		bRequeueConditionMet = (nPlayersLT > 0 && nPlayerCount < nPlayersLT) || (nPlayersGT > 0 && nPlayerCount > nPlayersGT);
	}

	const bool bIsQueued = I::TFPartyClient->BInQueueForMatchGroup(k_eTFMatchGroup_Casual_Default);
	if (bIsQueued && bQueuedFromRequeueIf && (!Vars::Misc::Queueing::RQif.Value || !bInGame || !bRequeueConditionMet))
	{
		I::TFPartyClient->CancelMatchQueueRequest(k_eTFMatchGroup_Casual_Default);
		bQueuedFromRequeueIf = false;
		flLastRequeueTime = flCurrentTime;
		return;
	}
	else if (bIsQueued)
	{
		bWasInGame = bInGame;
		return;
	}

	if (bWasInGame && !bInGame && !bIsLoadingMap && Vars::Misc::Queueing::RQif.Value && Vars::Misc::Queueing::RQkick.Value)
		flLastRequeueTime = 0.0f;
	bWasInGame = bInGame;

	if (bInGame && Vars::Misc::Queueing::RQif.Value && bRequeueConditionMet)
	{
		if (Vars::Misc::Queueing::RQnoAbandon.Value)
		{
			I::TFPartyClient->RequestQueueForMatch(k_eTFMatchGroup_Casual_Default);
			flLastRequeueTime = flCurrentTime;
			bQueuedFromRequeueIf = true;
			return;
		}

		I::TFGCClientSystem->AbandonCurrentMatch();
		bWasInGame = false;
		flLastRequeueTime = 0.0f;
		bQueuedFromRequeueIf = false;
		return;
	}

	if (Vars::Misc::Queueing::AutoCasualQueue.Value && !bIsQueued &&
		(!bIsLoadingMap || !Vars::Misc::Queueing::RQLTM.Value) && !bStillAttachedToServer)
	{
		if (!I::TFPartyClient->AnySelected())
			I::TFPartyClient->LoadSavedCasualCriteria();
		I::TFPartyClient->RequestQueueForMatch(k_eTFMatchGroup_Casual_Default);
	}
}
