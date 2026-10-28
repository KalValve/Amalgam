#include "AutoJoin.h"

// Doesnt work with custom huds!1!!
void CAutoJoin::Run(CTFPlayer* pLocal)
{
	static Timer tJoinTimer{}, tRandomTimer{};
	static Timer tStatusTimer{};
	static int iRandomClass = 0;
	auto SetStatus = [&](std::string sStatus, bool bRepeat = false)
	{
		if (m_sStatus != sStatus || bRepeat && tStatusTimer.Run(5.f))
		{
			m_sStatus = std::move(sStatus);
			SDK::BotDiagnostic("AutoJoin", m_sStatus);
			tStatusTimer.Update();
		}
	};
	if (!pLocal)
	{
		SetStatus("No local player; AutoJoin did not run");
		return;
	}

	int iDesiredClass = Vars::Misc::Automation::ForceClass.Value;
	if (Vars::Misc::Automation::RandomClass.Value)
	{
		static float flRandomInterval = 0.f;
		if (!iRandomClass || tRandomTimer.Run(flRandomInterval))
		{
			int iExclude = Vars::Misc::Automation::RandomClassExclude.Value;
			if ((iExclude & 0x1FF) == 0x1FF)
			{
				m_iDesiredClass = 0;
				SetStatus("Random class is on, but every class is excluded", true);
				return;
			}
			do { iRandomClass = SDK::RandomInt(1, 9); }
			while (iExclude & (1 << (iRandomClass - 1)));
			flRandomInterval = SDK::RandomFloat(Vars::Misc::Automation::RandomClassInterval.Value.Min, Vars::Misc::Automation::RandomClassInterval.Value.Max) * 60.f;
		}
		iDesiredClass = iRandomClass;
	}
	else
		iRandomClass = 0;
	m_iDesiredClass = iDesiredClass;

	if (iDesiredClass < 0 || iDesiredClass > static_cast<int>(m_aClassNames.size()))
	{
		SetStatus(std::format("Invalid class ID {} (expected 0-9)", iDesiredClass), true);
		return;
	}
	if (!iDesiredClass)
	{
		SetStatus("Off: no class selected and random class is disabled");
		return;
	}

	const int iCurrentClass = pLocal->m_iClass();
	if (pLocal->IsInValidTeam() && pLocal->IsAlive() && iCurrentClass == iDesiredClass)
	{
		SetStatus(std::format("Already class {} (ID {})", m_aClassNames[iDesiredClass - 1], iDesiredClass));
		return;
	}

	if (!tJoinTimer.Run(1.f))
	{
		SetStatus(std::format("Waiting to request class {} (ID {})", m_aClassNames[iDesiredClass - 1], iDesiredClass));
		return;
	}

	if (pLocal->IsInValidTeam())
	{
		const char* sClassName = m_aClassNames[iDesiredClass - 1].c_str();
		I::EngineClient->ClientCmd_Unrestricted(std::format("joinclass {}", sClassName).c_str());
		I::EngineClient->ClientCmd_Unrestricted("menuclosed");
		SetStatus(std::format("Sent joinclass {} (ID {}); current class is {} (alive={}). Waiting for TF2 to change it",
			sClassName, iDesiredClass, iCurrentClass, pLocal->IsAlive()), true);
	}
	else
	{
		I::EngineClient->ClientCmd_Unrestricted("team_ui_setup");
		I::EngineClient->ClientCmd_Unrestricted("menuopen");
		I::EngineClient->ClientCmd_Unrestricted("autoteam");
		I::EngineClient->ClientCmd_Unrestricted("menuclosed");
		SetStatus(std::format("Team is {} (not RED/BLU); sent autoteam before class {}", pLocal->m_iTeamNum(), iDesiredClass), true);
	}
}
