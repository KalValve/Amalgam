#include "Misc.h"
#include "../Configs/Configs.h"
#include "AutoItem/AutoItem.h"
#include <fstream>
#include <filesystem>

#ifdef TEXTMODE
#include "NamedPipe/NamedPipe.h"
#endif
#include "../Backtrack/Backtrack.h"
#include "../Ticks/Ticks.h"
#include "../Players/PlayerUtils.h"
#include "../Aimbot/AutoRocketJump/AutoRocketJump.h"
#include "../AntiCheatCompatibility/AntiCheatCompatibility.h"

MAKE_SIGNATURE(Voice_IsRecording, "engine.dll", "80 3D ? ? ? ? ? 74 ? 80 3D ? ? ? ? ? 75", 0x0);
MAKE_SIGNATURE(ReportPlayerAccount, "client.dll", "48 89 5C 24 ? 57 48 83 EC ? 48 8B D9 8B FA 48 C1 E9", 0x0);

void CMisc::RunPre(CTFPlayer* pLocal, CUserCmd* pCmd)
{
	EnsureChatUtilsDoc();
	F::AutoItem.Run(pLocal);
	CheatsBypass();
	WeaponSway();
	AntiAFK(pLocal, pCmd);
	InstantRespawnMVM(pLocal);
	NoisemakerSpam(pLocal);
	VoiceCommandSpam(pLocal);
	ChatSpam(pLocal);
	AchievementSpam(pLocal);
	CallVoteSpam(pLocal);
	AutoReport();
	AutoRetry(pLocal);
	AutoDisguise(pLocal);
	if (!pLocal->IsAlive() || pLocal->IsAGhost() || pLocal->m_MoveType() != MOVETYPE_WALK || pLocal->IsSwimming()
		|| pLocal->IsTaunting() || pLocal->InCond(TF_COND_SHIELD_CHARGE))
		return;

	AutoJump(pLocal, pCmd);
	EdgeJump(pLocal, pCmd);
	if (pLocal->InCond(TF_COND_HALLOWEEN_KART))
		return;

	AutoJumpbug(pLocal, pCmd);
	AutoFaNJump(pLocal, pCmd);
	AutoRevJump(pLocal, pCmd);
	AutoStrafe(pLocal, pCmd);
	AutoPeek(pLocal, pCmd);
	MovementLock(pLocal, pCmd);
	BreakJump(pLocal, pCmd);
}

void CMisc::RunPost(CTFPlayer* pLocal, CUserCmd* pCmd)
{
	if (!pLocal->IsAlive() || pLocal->IsAGhost() || pLocal->m_MoveType() != MOVETYPE_WALK || pLocal->IsSwimming()
		|| pLocal->InCond(TF_COND_SHIELD_CHARGE))
		return;

	if (pLocal->IsTaunting() || pLocal->InCond(TF_COND_HALLOWEEN_KART))
		TauntKartControl(pLocal, pCmd);
	else
	{
		EdgeJump(pLocal, pCmd, true);
		AutoPeek(pLocal, pCmd, true);
		FastMovement(pLocal, pCmd);
	}
}



void CMisc::AutoJump(CTFPlayer* pLocal, CUserCmd* pCmd)
{
	if (!Vars::Misc::Movement::Bunnyhop.Value)
		return;

	if (auto pWeapon = H::Entities.GetWeapon(); pWeapon && pWeapon->GetWeaponID() == TF_WEAPON_GRAPPLINGHOOK && pWeapon->As<CTFGrapplingHook>()->m_hProjectile())
		return;

	static bool bStaticAttempted = false, bStaticValid = false;
	bool bLastAttempted = bStaticAttempted, bLastValid = bStaticValid;
	bool bCurrAttempted = bStaticAttempted = G::OriginalCmd.buttons & IN_JUMP, bCurrValid = bStaticValid = pLocal->m_hGroundEntity() && !pLocal->IsDucking();
	if (!bCurrValid || bCurrValid && G::LastUserCmd->buttons & IN_JUMP)
		pCmd->buttons &= ~IN_JUMP;

	static float flLastAttempt = 0.f;
	bool bPressed = bCurrAttempted && !bLastAttempted;
	bool bParachute = SDK::AttribHookValue(0, "parachute_attribute", pLocal) && !pLocal->InCond(TF_COND_PARACHUTE_ACTIVE);
	bool bAllow = !bParachute || G::OriginalCmd.buttons & IN_DUCK; // evil we don't want to manual
	bool bManual = bAllow && (bPressed || bParachute && I::GlobalVars->curtime < flLastAttempt + 0.1f);
	if (bPressed && !bCurrValid)
		flLastAttempt = I::GlobalVars->curtime;
	if (bManual)
		pCmd->buttons |= IN_JUMP;

	F::AntiCheatCompatibility.BunnyHop(pCmd, bCurrValid, bLastValid);
}

void CMisc::AutoJumpbug(CTFPlayer* pLocal, CUserCmd* pCmd)
{
	if (!Vars::Misc::Movement::AutoJumpbug.Value || !(pCmd->buttons & IN_DUCK) || pLocal->m_hGroundEntity() || pLocal->m_vecVelocity().z > -650.f)
		return;

	float flUnduckHeight = 20 * pLocal->m_flModelScale();
	float flTraceDistance = flUnduckHeight + 2;

	CGameTrace trace = {};
	CTraceFilterWorldAndPropsOnly filter = {};

	Vec3 vOrigin = pLocal->m_vecOrigin();
	SDK::TraceHull(vOrigin, vOrigin - Vec3(0, 0, flTraceDistance), pLocal->m_vecMins(), pLocal->m_vecMaxs(), pLocal->SolidMask(), &filter, &trace);
	if (!trace.DidHit() || trace.fraction * flTraceDistance < flUnduckHeight) // don't try if we aren't in range to unduck or are too low
		return;

	pCmd->buttons &= ~IN_DUCK;
	pCmd->buttons |= IN_JUMP;
}

void CMisc::AutoFaNJump(CTFPlayer* pLocal, CUserCmd* pCmd)
{
	if (!Vars::Misc::Movement::AutoFaNJump.Value || G::Attacking == 1 || pLocal->m_bScattergunJump() || pLocal->m_vecVelocity().To2D().IsZero())
		return;

	if (auto pWeapon = H::Entities.GetWeapon(); SDK::AttribHookValue(0, "set_scattergun_has_knockback", pWeapon) != 1)
		return;

	Vec3 vAngles = { 45.f, Math::VectorAngles(pLocal->m_vecVelocity()).y };
	SDK::FixMovement(pCmd, vAngles);
	pCmd->viewangles = vAngles;
	pCmd->buttons |= IN_ATTACK;
	if (pLocal->m_hGroundEntity())
		pCmd->buttons |= IN_JUMP;
}

void CMisc::AutoRevJump(CTFPlayer* pLocal, CUserCmd* pCmd)
{
	if (!Vars::Misc::Movement::AutoRevJump.Value || !pLocal->m_hGroundEntity())
		return;

	if (auto pWeapon = H::Entities.GetWeapon(); !pWeapon || pWeapon->GetWeaponID() != TF_WEAPON_MINIGUN || pWeapon->As<CTFMinigun>()->m_iWeaponState() != AC_STATE_IDLE)
		return;

	if (!(pCmd->buttons & IN_ATTACK2) || G::LastUserCmd->buttons & IN_ATTACK2)
		return;

	pCmd->buttons |= IN_JUMP;
}

void CMisc::AutoStrafe(CTFPlayer* pLocal, CUserCmd* pCmd)
{
	if (!Vars::Misc::Movement::AutoStrafe.Value || pLocal->m_hGroundEntity() || !(pLocal->m_afButtonLast() & IN_JUMP) && (pCmd->buttons & IN_JUMP))
		return;

	switch (Vars::Misc::Movement::AutoStrafe.Value)
	{
	case Vars::Misc::Movement::AutoStrafeEnum::Legit:
	{
		static auto cl_sidespeed = H::ConVars.FindVar("cl_sidespeed");
		const float flSideSpeed = cl_sidespeed->GetFloat();

		if (pCmd->mousedx)
		{
			pCmd->forwardmove = 0.f;
			pCmd->sidemove = pCmd->mousedx > 0 ? flSideSpeed : -flSideSpeed;
		}
		break;
	}
	case Vars::Misc::Movement::AutoStrafeEnum::Directional:
	{
		// credits: KGB
		if (!(pCmd->buttons & (IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT)))
			break;

		float flForward = pCmd->forwardmove, flSide = pCmd->sidemove;
		Vec3 vForward, vRight; Math::AngleVectors(pCmd->viewangles, &vForward, &vRight, nullptr);
		vForward.Normalize2D(), vRight.Normalize2D();

		Vec3 vWishDir = Math::VectorAngles({ vForward.x * flForward + vRight.x * flSide, vForward.y * flForward + vRight.y * flSide, 0.f });
		Vec3 vCurDir = Math::VectorAngles(pLocal->m_vecVelocity());
		float flDirDelta = Math::NormalizeAngle(vWishDir.y - vCurDir.y);
		if (fabsf(flDirDelta) > Vars::Misc::Movement::AutoStrafeMaxDelta.Value)
			break;

		float flTurnScale = Math::RemapVal(Vars::Misc::Movement::AutoStrafeTurnScale.Value, 0.f, 1.f, 0.9f, 1.f);
		float flRotation = Math::Deg2Rad((flDirDelta > 0.f ? -90.f : 90.f) + flDirDelta * flTurnScale);
		float flCosRot = cosf(flRotation), flSinRot = sinf(flRotation);

		pCmd->forwardmove = flCosRot * flForward - flSinRot * flSide;
		pCmd->sidemove = flSinRot * flForward + flCosRot * flSide;
	}
	}
}

void CMisc::MovementLock(CTFPlayer* pLocal, CUserCmd* pCmd)
{
	static bool bLock = false;

	if (!Vars::Misc::Movement::MovementLock.Value)
	{
		bLock = false;
		return;
	}

	static Vec3 vMove = {}, vView = {};
	if (!bLock)
	{
		bLock = true;
		vMove = { pCmd->forwardmove, pCmd->sidemove, pCmd->upmove };
		vView = pCmd->viewangles;
	}

	pCmd->forwardmove = vMove.x, pCmd->sidemove = vMove.y, pCmd->upmove = vMove.z;
	SDK::FixMovement(pCmd, vView, pCmd->viewangles);
}

void CMisc::BreakJump(CTFPlayer* pLocal, CUserCmd* pCmd)
{
	if (!Vars::Misc::Movement::BreakJump.Value || F::AutoRocketJump.IsRunning())
		return;

	static bool bStaticJump = false;
	const bool bLastJump = bStaticJump;
	const bool bCurrJump = bStaticJump = pCmd->buttons & IN_JUMP;

	static int iTickSinceGrounded = -1;
	if (pLocal->m_hGroundEntity().Get())
		iTickSinceGrounded = -1;
	iTickSinceGrounded++;

	switch (iTickSinceGrounded)
	{
	case 0:
		if (bLastJump || !bCurrJump || pLocal->IsDucking())
			return;
		break;
	case 1:
		break;
	default:
		return;
	}

	pCmd->buttons |= IN_DUCK;
}

void CMisc::AntiAFK(CTFPlayer* pLocal, CUserCmd* pCmd)
{
	static Timer tTimer = {};
	m_bAntiAFK = false;

	if (pCmd->buttons & (IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT) || !pLocal->IsAlive())
		tTimer.Update();
	else if (Vars::Misc::Automation::AntiAFK.Value && tTimer.Run(25.f))
	{
		pCmd->buttons |= IN_FORWARD;
		m_bAntiAFK = true;
	}
}

void CMisc::InstantRespawnMVM(CTFPlayer* pLocal)
{
	if (!Vars::Misc::MannVsMachine::InstantRespawn.Value || pLocal->IsAlive())
		return;

	KeyValues* kv = new KeyValues("MVM_Revive_Response");
	kv->SetBool("accepted", true);
	I::EngineClient->ServerCmdKeyValues(kv);
}

void CMisc::NoisemakerSpam(CTFPlayer* pLocal)
{
	if (!Vars::Misc::Exploits::NoisemakerSpam.Value || !pLocal->IsAlive() || pLocal->IsAGhost()
		|| pLocal->m_bUsingActionSlot() || pLocal->m_flNextNoiseMakerTime() > I::GlobalVars->curtime)
		return;

	KeyValues* kv = new KeyValues("use_action_slot_item_server");
	I::EngineClient->ServerCmdKeyValues(kv);
}

void CMisc::CheatsBypass()
{
	static bool bCheatSet = false;
	static auto sv_cheats = H::ConVars.FindVar("sv_cheats");
	if (Vars::Misc::Exploits::CheatsBypass.Value)
	{
		sv_cheats->m_nValue = 1;
		bCheatSet = true;
	}
	else if (bCheatSet)
	{
		sv_cheats->m_nValue = 0;
		bCheatSet = false;
	}
}

void CMisc::WeaponSway()
{
	static auto cl_wpn_sway_interp = H::ConVars.FindVar("cl_wpn_sway_interp");
	static auto cl_wpn_sway_scale = H::ConVars.FindVar("cl_wpn_sway_scale");

	bool bSway = Vars::Visuals::Viewmodel::SwayInterp.Value || Vars::Visuals::Viewmodel::SwayScale.Value;
	cl_wpn_sway_interp->SetValue(bSway ? Vars::Visuals::Viewmodel::SwayInterp.Value : 0.f);
	cl_wpn_sway_scale->SetValue(bSway ? Vars::Visuals::Viewmodel::SwayScale.Value : 0.f);
}



void CMisc::TauntKartControl(CTFPlayer* pLocal, CUserCmd* pCmd)
{
	if (Vars::Misc::Automation::TauntControl.Value && pLocal->IsTaunting() && pLocal->m_bAllowMoveDuringTaunt())
	{
		if (pLocal->m_bTauntForceMoveForward())
		{
			if (pCmd->buttons & IN_BACK)
				pCmd->viewangles.x = 91.f;
			else if (!(pCmd->buttons & IN_FORWARD))
				pCmd->viewangles.x = 90.f;
		}
		if (pCmd->buttons & IN_MOVELEFT)
			pCmd->sidemove = pCmd->viewangles.x == 90.f ? -450.f : -pLocal->m_flTauntForceMoveForwardSpeed();
		else if (pCmd->buttons & IN_MOVERIGHT)
			pCmd->sidemove = pCmd->viewangles.x == 90.f ? 450.f : pLocal->m_flTauntForceMoveForwardSpeed();
	}
	else if (Vars::Misc::Automation::KartControl.Value && pLocal->InCond(TF_COND_HALLOWEEN_KART))
	{
		bool bChoke = I::ClientState->chokedcommands < 3 && F::Ticks.CanChoke(true);
		float flForward = fabsf(pCmd->forwardmove), flSide = pCmd->sidemove * (!bChoke ? 0.f : pCmd->forwardmove < 0.f ? -1 : 1);

		Vec3 vForward, vRight; Math::AngleVectors(pCmd->viewangles, &vForward, &vRight, nullptr);
		vForward.Normalize2D(), vRight.Normalize2D();

		pCmd->viewangles.x = 90.f;
		G::SilentAngles = true;

		if (!(pCmd->buttons & (IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT)))
			return;

		if (pCmd->forwardmove < 0.f)
			pCmd->viewangles.x = 91.f;
		else if (pCmd->forwardmove > 0.f || flSide)
			pCmd->viewangles.x = 10.f;
		pCmd->forwardmove = 0.f;

		if (!flForward && !flSide)
			return;

		pCmd->forwardmove = 450.f;
		if (flSide)
		{
			Vec3 vWishDir = Math::VectorAngles({ vForward.x * flForward + vRight.x * flSide, vForward.y * flForward + vRight.y * flSide, 0.f });
			pCmd->viewangles.y = vWishDir.y;
			G::PSilentAngles = true;
		}
	}
}

void CMisc::FastMovement(CTFPlayer* pLocal, CUserCmd* pCmd)
{
	if (!pLocal->m_hGroundEntity() || pLocal->InCond(TF_COND_HALLOWEEN_KART))
		return;

	const float flSpeed = pLocal->m_vecVelocity().Length2D();
	const int flMaxSpeed = std::min(pLocal->m_flMaxspeed() * 0.9f, 520.f) - 10.f;
	const int iRun = !pCmd->forwardmove && !pCmd->sidemove ? 0 : flSpeed < flMaxSpeed ? 1 : 2;

	switch (iRun)
	{
	case 0:
	{
		if (!Vars::Misc::Movement::FastStop.Value || !flSpeed)
			return;

		Vec3 vDirection = pLocal->m_vecVelocity().ToAngle();
		vDirection.y = pCmd->viewangles.y - vDirection.y;
		Vec3 vNegatedDirection = vDirection.FromAngle() * -flSpeed;
		pCmd->forwardmove = vNegatedDirection.x;
		pCmd->sidemove = vNegatedDirection.y;

		break;
	}
	case 1:
	{
		if ((pLocal->IsDucking() ? !Vars::Misc::Movement::DuckSpeed.Value : !Vars::Misc::Movement::FastAccelerate.Value)
			|| F::AntiCheatCompatibility.Active()
			|| G::Attacking == 1 || F::Ticks.m_bDoubletap || F::Ticks.m_bSpeedhack || F::Ticks.m_bRecharge || G::AntiAim)
			return;

		if (!(pCmd->buttons & (IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT)))
			return;

		bool bChoke = !I::ClientState->chokedcommands && F::Ticks.CanChoke(true);
		if (!bChoke)
			return;

		Vec3 vMove = { pCmd->forwardmove, pCmd->sidemove, 0.f };
		Vec3 vAngMoveReverse = Math::VectorAngles(-vMove);
		pCmd->forwardmove = -vMove.Length();
		pCmd->sidemove = 0.f;
		pCmd->viewangles.y = fmodf(pCmd->viewangles.y - vAngMoveReverse.y, 360.f);
		pCmd->viewangles.z = 270.f;
		G::PSilentAngles = true;

		break;
	}
	}
}

void CMisc::AutoPeek(CTFPlayer* pLocal, CUserCmd* pCmd, bool bPost)
{
	static bool bReturning = false;

	if (!bPost)
	{
		if (Vars::AutoPeek::Enabled.Value)
		{
			Vec3 vLocalPos = pLocal->m_vecOrigin();

			if (bReturning)
			{
				if (vLocalPos.DistTo2D(m_vPeekReturnPos) < 8.f)
				{
					bReturning = false;
					return;
				}

				SDK::WalkTo(pCmd, pLocal, m_vPeekReturnPos);
				pCmd->buttons &= ~IN_JUMP;
			}
			else if (!pLocal->m_hGroundEntity())
				m_bPeekPlaced = false;

			if (!m_bPeekPlaced)
			{
				m_vPeekReturnPos = vLocalPos;
				m_bPeekPlaced = true;
			}
			else
			{
				static Timer tTimer = {};
				if (tTimer.Run(0.7f))
					H::Particles.DispatchParticleEffect("ping_circle", m_vPeekReturnPos, {});
			}
		}
		else
			m_bPeekPlaced = bReturning = false;
	}
	else if (G::Attacking && m_bPeekPlaced)
		bReturning = true;
}

void CMisc::EdgeJump(CTFPlayer* pLocal, CUserCmd* pCmd, bool bPost)
{
	if (!Vars::Misc::Movement::EdgeJump.Value)
		return;

	static bool bStaticGround = false;
	if (!bPost)
		bStaticGround = pLocal->m_hGroundEntity();
	else if (bStaticGround && !pLocal->m_hGroundEntity())
		pCmd->buttons |= IN_JUMP;
}



void CMisc::Event(IGameEvent* pEvent, uint32_t uHash)
{
	switch (uHash)
	{
	case FNV1A::Hash32Const("client_disconnect"):
	case FNV1A::Hash32Const("client_beginconnect"):
	case FNV1A::Hash32Const("game_newmap"):
		m_vChatSpamLines.clear();
		m_vKillSayLines.clear();
		m_vF1Messages.clear();
		m_vF2Messages.clear();
		m_vAutoReplies.clear();
		m_iCurrentChatSpamIndex = 0;
		m_bAutoBalanceTeamChangePending = false;
		break;
	case FNV1A::Hash32Const("player_spawn"):
	{
		if (I::EngineClient->GetPlayerForUserID(pEvent->GetInt("userid")) != I::EngineClient->GetLocalPlayer())
			return;

		m_bPeekPlaced = false;
		break;
	}
	case FNV1A::Hash32Const("player_death"):
	{
		const int iLocalPlayer = I::EngineClient->GetLocalPlayer();
		const int iAttacker = I::EngineClient->GetPlayerForUserID(pEvent->GetInt("attacker"));
		const int iVictim = I::EngineClient->GetPlayerForUserID(pEvent->GetInt("userid"));
		if (iVictim == iLocalPlayer && m_bAutoBalanceTeamChangePending
			&& Vars::Misc::Automation::AntiAutobalance.Value
			&& (Vars::Misc::Automation::AntiAutobalanceMode.Value == Vars::Misc::Automation::AntiAutobalanceModeEnum::OnDeath
				|| Vars::Misc::Automation::AntiAutobalanceMode.Value == Vars::Misc::Automation::AntiAutobalanceModeEnum::Both))
		{
			auto pResource = H::Entities.GetResource();
			if (pResource)
			{
				int iRedPlayers = 0, iBluePlayers = 0;
				for (int i = 1; i <= I::EngineClient->GetMaxClients(); ++i)
				{
					if (!pResource->m_bValid(i) || !pResource->m_bConnected(i))
						continue;
					switch (pResource->m_iTeam(i))
					{
					case TF_TEAM_RED: ++iRedPlayers; break;
					case TF_TEAM_BLUE: ++iBluePlayers; break;
					}
				}

				const int iLocalTeam = pResource->m_iTeam(iLocalPlayer);
				if (iLocalTeam == TF_TEAM_RED || iLocalTeam == TF_TEAM_BLUE)
				{
					const int iOurPlayers = iLocalTeam == TF_TEAM_RED ? iRedPlayers : iBluePlayers;
					const int iEnemyPlayers = iLocalTeam == TF_TEAM_RED ? iBluePlayers : iRedPlayers;
					if (iOurPlayers - iEnemyPlayers > 2)
						I::EngineClient->ClientCmd_Unrestricted("retry");
				}
			}
			m_bAutoBalanceTeamChangePending = false;
		}

		if (iAttacker == iLocalPlayer && iAttacker != iVictim && Vars::Misc::Automation::AutoTaunt.Value)
		{
			const int iChance = std::clamp(Vars::Misc::Automation::AutoTauntChance.Value, 0, 100);
			auto pLocal = H::Entities.GetLocal();
			if (iChance > 0 && pLocal && pLocal->IsAlive() && !pLocal->IsTaunting()
				&& !pLocal->InCond(TF_COND_HALLOWEEN_KART) && SDK::RandomInt(1, 100) <= iChance)
			{
				const int iSlot = Vars::Misc::Automation::AutoTauntSlot.Value;
				I::EngineClient->ClientCmd_Unrestricted(iSlot > 0
					? std::format("taunt {}", iSlot).c_str() : "taunt");
			}
		}
		if (iAttacker == iLocalPlayer && iAttacker != iVictim)
		{
			player_info_t pi{};
			if (I::EngineClient->GetPlayerInfo(iVictim, &pi))
				m_sLastKilledName = pi.name;
			DoKillSay(iVictim);
		}
		break;
	}
	case FNV1A::Hash32Const("vote_maps_changed"):
		if (Vars::Misc::Automation::AutoVoteMap.Value)
			I::EngineClient->ClientCmd_Unrestricted(std::format("next_map_vote {}", Vars::Misc::Automation::AutoVoteMapOption.Value).c_str());
		break;
	}
}

void CMisc::VoiceCommandSpam(CTFPlayer* pLocal)
{
	const int iVoiceCommand = Vars::Misc::Automation::VoiceCommandSpam.Value;
	if (!iVoiceCommand || !pLocal || !pLocal->IsAlive())
		return;

	static float flLastVoiceTime = 0.f;
	const float flCurrentTime = SDK::PlatFloatTime();
	if (flCurrentTime - flLastVoiceTime < 6.5f)
		return;
	flLastVoiceTime = flCurrentTime;

	const auto RunCommand = [](int iMenu, int iCommand)
	{
		I::EngineClient->ClientCmd_Unrestricted(std::format("voicemenu {} {}", iMenu, iCommand).c_str());
	};
	switch (iVoiceCommand)
	{
	case Vars::Misc::Automation::VoiceCommandSpamEnum::Random: RunCommand(SDK::RandomInt(0, 2), SDK::RandomInt(0, 8)); break;
	case Vars::Misc::Automation::VoiceCommandSpamEnum::Medic: RunCommand(0, 0); break;
	case Vars::Misc::Automation::VoiceCommandSpamEnum::Thanks: RunCommand(0, 1); break;
	case Vars::Misc::Automation::VoiceCommandSpamEnum::NiceShot: RunCommand(2, 6); break;
	case Vars::Misc::Automation::VoiceCommandSpamEnum::Cheers: RunCommand(2, 2); break;
	case Vars::Misc::Automation::VoiceCommandSpamEnum::Jeers: RunCommand(2, 3); break;
	case Vars::Misc::Automation::VoiceCommandSpamEnum::GoGoGo: RunCommand(0, 2); break;
	case Vars::Misc::Automation::VoiceCommandSpamEnum::MoveUp: RunCommand(0, 3); break;
	case Vars::Misc::Automation::VoiceCommandSpamEnum::GoLeft: RunCommand(0, 4); break;
	case Vars::Misc::Automation::VoiceCommandSpamEnum::GoRight: RunCommand(0, 5); break;
	case Vars::Misc::Automation::VoiceCommandSpamEnum::Yes: RunCommand(0, 6); break;
	case Vars::Misc::Automation::VoiceCommandSpamEnum::No: RunCommand(0, 7); break;
	case Vars::Misc::Automation::VoiceCommandSpamEnum::Incoming: RunCommand(1, 0); break;
	case Vars::Misc::Automation::VoiceCommandSpamEnum::Spy: RunCommand(1, 1); break;
	case Vars::Misc::Automation::VoiceCommandSpamEnum::Sentry: RunCommand(1, 2); break;
	case Vars::Misc::Automation::VoiceCommandSpamEnum::NeedTeleporter: RunCommand(1, 3); break;
	case Vars::Misc::Automation::VoiceCommandSpamEnum::Pootis: RunCommand(1, 4); break;
	case Vars::Misc::Automation::VoiceCommandSpamEnum::NeedSentry: RunCommand(1, 5); break;
	case Vars::Misc::Automation::VoiceCommandSpamEnum::ActivateCharge: RunCommand(1, 6); break;
	case Vars::Misc::Automation::VoiceCommandSpamEnum::Help: RunCommand(2, 0); break;
	case Vars::Misc::Automation::VoiceCommandSpamEnum::BattleCry: RunCommand(2, 1); break;
	case Vars::Misc::Automation::VoiceCommandSpamEnum::GoodJob: RunCommand(2, 7); break;
	}
}

void CMisc::MicSpam()
{
	static bool bShouldRestore = false;
	static bool bWasEnabled = false;
	static Timer tRecordTimer{};

	if (Vars::Misc::Automation::Micspam.Value)
	{
		if (I::EngineClient->IsInGame() && (!bWasEnabled || tRecordTimer.Run(10.0f)))
		{
			I::EngineClient->ClientCmd_Unrestricted("+voicerecord");
			I::EngineClient->ClientCmd_Unrestricted("voice_avggain 1");
#ifdef TEXTMODE
			I::EngineClient->ClientCmd_Unrestricted("voice_enable 1");
			I::EngineClient->ClientCmd_Unrestricted("voice_loopback 0");
#endif
			bWasEnabled = true;
		}
		bShouldRestore = true;
	}
	else if (bShouldRestore)
	{
		if (S::Voice_IsRecording.Call<bool>())
		{
			I::EngineClient->ClientCmd_Unrestricted("-voicerecord");
#ifdef TEXTMODE
			I::EngineClient->ClientCmd_Unrestricted("voice_enable 0");
			I::EngineClient->ClientCmd_Unrestricted("voice_loopback 0");
#endif
		}
		bShouldRestore = false;
		bWasEnabled = false;
	}
}

void CMisc::AutoRetry(CTFPlayer* pLocal)
{
	if (!Vars::Misc::Automation::AutoRetry.Value || !pLocal || !pLocal->IsAlive())
		return;

	const int iMaxHealth = pLocal->GetMaxHealth();
	const float flHealthPercent = iMaxHealth > 0
		? static_cast<float>(pLocal->m_iHealth()) * 100.f / iMaxHealth
		: 100.f;
	if (flHealthPercent >= Vars::Misc::Automation::AutoRetryHealth.Value)
		return;

	static Timer tRetryTimer{};
	if (tRetryTimer.Run(5.0f))
		I::EngineClient->ClientCmd_Unrestricted("retry");
}

void CMisc::AutoDisguise(CTFPlayer* pLocal)
{
	if (!Vars::Misc::Automation::AutoDisguise.Value || !pLocal || !pLocal->IsAlive()
		|| !pLocal->IsInValidTeam() || pLocal->m_iClass() != TF_CLASS_SPY
		|| pLocal->InCond(TF_COND_DISGUISING) || pLocal->InCond(TF_COND_DISGUISED)
		|| pLocal->InCond(TF_COND_DISGUISE_WEARINGOFF))
		return;

	static Timer tDisguiseTimer{};
	if (!tDisguiseTimer.Run(0.75f))
		return;

	auto pResource = H::Entities.GetResource();
	if (!pResource)
		return;

	const int iEnemyTeam = pLocal->m_iTeamNum() == TF_TEAM_RED ? TF_TEAM_BLUE : TF_TEAM_RED;
	std::array<int, TF_CLASS_COUNT> aClassCounts = {};
	int iTotalClasses = 0;
	for (int i = 1; i <= I::EngineClient->GetMaxClients(); ++i)
	{
		if (!pResource->m_bValid(i) || !pResource->m_bConnected(i) || pResource->m_iTeam(i) != iEnemyTeam)
			continue;

		const int iEnemyClass = pResource->m_iPlayerClass(i);
		if (iEnemyClass <= TF_CLASS_UNDEFINED || iEnemyClass >= TF_CLASS_COUNT || iEnemyClass == TF_CLASS_HEAVY)
			continue;

		++aClassCounts[iEnemyClass];
		++iTotalClasses;
	}
	if (!iTotalClasses)
		return;

	int iRandomClass = SDK::RandomInt(1, iTotalClasses);
	for (int iClass = TF_CLASS_SCOUT; iClass < TF_CLASS_COUNT; ++iClass)
	{
		if (iRandomClass > aClassCounts[iClass])
		{
			iRandomClass -= aClassCounts[iClass];
			continue;
		}

		I::EngineClient->ClientCmd_Unrestricted(std::format("disguise {} -1", iClass).c_str());
		return;
	}
}

int CMisc::AntiBackstab(CTFPlayer* pLocal, CUserCmd* pCmd, bool bSendPacket)
{
	if (!Vars::Misc::Automation::AntiBackstab.Value || !bSendPacket || G::Attacking == 1 || !pLocal || pLocal->m_MoveType() != MOVETYPE_WALK || pLocal->InCond(TF_COND_HALLOWEEN_KART))
		return 0;

	std::vector<std::pair<Vec3, CBaseEntity*>> vTargets = {};
	for (auto pEntity : H::Entities.GetGroup(EntityEnum::PlayerEnemy))
	{
		auto pPlayer = pEntity->As<CTFPlayer>();
		if (pPlayer->IsDormant() || !pPlayer->IsAlive() || pPlayer->IsAGhost() || pPlayer->InCond(TF_COND_STEALTHED))
			continue;

		auto pWeapon = pPlayer->m_hActiveWeapon()->As<CTFWeaponBase>();
		if (!pWeapon
			|| pWeapon->GetWeaponID() != TF_WEAPON_KNIFE
			&& !(G::PrimaryWeaponType == EWeaponType::MELEE && SDK::AttribHookValue(0, "crit_from_behind", pWeapon) > 0)
			&& !(pWeapon->GetWeaponID() == TF_WEAPON_FLAMETHROWER && SDK::AttribHookValue(0, "set_flamethrower_back_crit", pWeapon) == 1)
			|| F::PlayerUtils.IsIgnored(pPlayer->entindex()))
			continue;

		Vec3 vLocalPos = pLocal->GetCenter();
		Vec3 vTargetPos1 = pPlayer->GetCenter();
		Vec3 vTargetPos2 = vTargetPos1 + pPlayer->m_vecVelocity() * F::Backtrack.GetReal();
		float flDistance = std::max(std::max(SDK::MaxSpeed(pPlayer), SDK::MaxSpeed(pLocal)), pPlayer->m_vecVelocity().Length());
		float flDistanceSqr = powf(flDistance, 2);
		if ((vLocalPos.DistToSqr(vTargetPos1) > flDistanceSqr || !SDK::VisPosWorld(pLocal, pPlayer, vLocalPos, vTargetPos1))
			&& (vLocalPos.DistToSqr(vTargetPos2) > flDistanceSqr || !SDK::VisPosWorld(pLocal, pPlayer, vLocalPos, vTargetPos2)))
			continue;

		vTargets.emplace_back(vTargetPos2, pEntity);
	}
	if (vTargets.empty())
		return 0;

	std::sort(vTargets.begin(), vTargets.end(), [&](const auto& a, const auto& b) -> bool
	{
		return pLocal->GetCenter().DistToSqr(a.first) < pLocal->GetCenter().DistToSqr(b.first);
	});

	auto& pTargetPos = vTargets.front();
	switch (Vars::Misc::Automation::AntiBackstab.Value)
	{
	case Vars::Misc::Automation::AntiBackstabEnum::Yaw:
	{
		Vec3 vAngleTo = Math::CalcAngle(pLocal->m_vecOrigin(), pTargetPos.first);
		vAngleTo.x = pCmd->viewangles.x;
		SDK::FixMovement(pCmd, vAngleTo);
		pCmd->viewangles = vAngleTo;
		
		return 1;
	}
	case Vars::Misc::Automation::AntiBackstabEnum::Pitch:
	case Vars::Misc::Automation::AntiBackstabEnum::Fake:
	{
		bool bCheater = F::PlayerUtils.HasTag(pTargetPos.second->entindex(), F::PlayerUtils.TagToIndex(CHEATER_TAG));
		// if the closest spy is a cheater, assume auto stab is being used, otherwise don't do anything if target is in front
		if (!bCheater)
		{
			auto fTargetIsBehind = [&]()
			{
				const float flCompDist = PLAYER_ORIGIN_COMPRESSION / 2;
				const float flSqCompDist = 0.0884f;

				Vec3 vToTarget = (pLocal->m_vecOrigin() - pTargetPos.first).To2D();
				const float flDist = vToTarget.Normalize();
				if (flDist < flSqCompDist)
					return true;

				const float flExtra = 2.f * flCompDist / flDist; // account for origin compression
				float flPosVsTargetViewMinDot = 0.f - 0.0031f - flExtra;

				Vec3 vTargetForward; Math::AngleVectors(pCmd->viewangles, &vTargetForward);
				vTargetForward.Normalize2D();

				const float flPosVsTargetViewDot = vToTarget.Dot(vTargetForward); // Behind?

				return flPosVsTargetViewDot > flPosVsTargetViewMinDot;
			};

			if (!fTargetIsBehind())
				return 0;
		}

		if (!bCheater || Vars::Misc::Automation::AntiBackstab.Value == Vars::Misc::Automation::AntiBackstabEnum::Pitch)
		{
			pCmd->forwardmove *= -1;
			pCmd->viewangles.x = 269.f;
		}
		else
		{
			pCmd->viewangles.x = 271.f;
		}
		// may slip up some auto backstabs depending on mode, though we are still able to be stabbed

		return 2;
	}
	}

	return 0;
}

void CMisc::PingReducer()
{
	static Timer tTimer = {};
	if (!tTimer.Run(0.1f))
		return;

	auto pNetChan = reinterpret_cast<CNetChannel*>(I::EngineClient->GetNetChannelInfo());
	auto pResource = H::Entities.GetResource();
	if (!pNetChan || !pResource || !I::EngineClient->IsConnected())
		return;

	static auto cl_cmdrate = H::ConVars.FindVar("cl_cmdrate");
	if (!cl_cmdrate)
		return;

	const int iCmdRate = cl_cmdrate->GetInt();
	const int iPing = pResource->m_iPing(I::EngineClient->GetLocalPlayer());
	const int iTarget = Vars::Misc::Exploits::PingReducer.Value
		&& iPing > Vars::Misc::Exploits::PingTarget.Value ? -1 : iCmdRate;
	if (m_iWishCmdrate != iTarget)
	{
		m_iWishCmdrate = iTarget;
		NET_SetConVar tConvar = { "cl_cmdrate", std::to_string(m_iWishCmdrate).c_str() };
		pNetChan->SendNetMsg(tConvar);
	}
}

void CMisc::UnlockAchievements()
{
	const auto pAchievementMgr = U::Memory.CallVirtual<114, IAchievementMgr*>(I::EngineClient);
	if (pAchievementMgr)
	{
		I::SteamUserStats->RequestCurrentStats();
		for (int i = 0; i < pAchievementMgr->GetAchievementCount(); i++)
			pAchievementMgr->AwardAchievement(pAchievementMgr->GetAchievementByIndex(i)->GetAchievementID());
		I::SteamUserStats->StoreStats();
		I::SteamUserStats->RequestCurrentStats();
	}
}

void CMisc::LockAchievements()
{
	const auto pAchievementMgr = U::Memory.CallVirtual<114, IAchievementMgr*>(I::EngineClient);
	if (pAchievementMgr)
	{
		I::SteamUserStats->RequestCurrentStats();
		for (int i = 0; i < pAchievementMgr->GetAchievementCount(); i++)
			I::SteamUserStats->ClearAchievement(pAchievementMgr->GetAchievementByIndex(i)->GetName());
		I::SteamUserStats->StoreStats();
		I::SteamUserStats->RequestCurrentStats();
	}
}


void CMisc::CallVoteSpam(CTFPlayer* pLocal)
{
	if (!Vars::Misc::Automation::CallVoteSpam.Value || !m_tCallVoteSpamTimer.Run(1.0f))
		return;

	std::vector<std::string> vVoteOptions = {
		"callvote changelevel cp_badlands",
		"callvote changelevel cp_granary",
		"callvote changelevel cp_well",
		"callvote changelevel cp_5gorge",
		"callvote changelevel cp_freight_final1",
		"callvote changelevel cp_yukon_final",
		"callvote changelevel cp_gravelpit",
		"callvote changelevel cp_dustbowl",
		"callvote changelevel cp_egypt_final",
		"callvote changelevel cp_junction_final",
		"callvote changelevel cp_steel",
		"callvote changelevel ctf_2fort",
		"callvote changelevel ctf_well",
		"callvote changelevel ctf_sawmill",
		"callvote changelevel ctf_turbine",
		"callvote changelevel ctf_doublecross",
		"callvote changelevel pl_badwater",
		"callvote changelevel pl_goldrush",
		"callvote changelevel pl_dustbowl",
		"callvote changelevel pl_upward",
		"callvote changelevel pl_thundermountain",
		"callvote changelevel koth_harvest_final",
		"callvote changelevel koth_nucleus",
		"callvote changelevel koth_sawmill",
		"callvote changelevel koth_viaduct",
		"callvote changelevel cp_5gorge",
		"callvote changelevel cp_dustbowl",
		"callvote changelevel ctf_2fort",
		"callvote changelevel ctf_doublecross",
		"callvote changelevel ctf_turbine",
		"callvote changelevel koth_brazil",
		"callvote changelevel pl_badwater",
		"callvote changelevel pl_pheonix",
		"callvote changelevel plr_bananabay",
		"callvote changelevel plr_hightower",
		"callvote scrambleteams"
	};

	int iRandomIndex = SDK::RandomInt(0, static_cast<int>(vVoteOptions.size()) - 1);
	std::string strSelectedVote = vVoteOptions[iRandomIndex];

	I::ClientState->SendStringCmd(strSelectedVote.c_str());
}


void CMisc::AchievementSpam(CTFPlayer* pLocal)
{
	if (!Vars::Misc::Automation::AchievementSpam.Value || !pLocal || !pLocal->IsAlive())
	{
		m_eAchievementSpamState = AchievementSpamState::IDLE;
		return;
	}

	const auto pAchievementMgr = reinterpret_cast<IAchievementMgr * (*)()>(U::Memory.GetVirtual(I::EngineClient, 114))();
	if (!pAchievementMgr)
	{
		m_eAchievementSpamState = AchievementSpamState::IDLE;
		return;
	}

	switch (m_eAchievementSpamState)
	{
	case AchievementSpamState::IDLE:
	{
		if (!m_tAchievementSpamTimer.Run(20.0f))
			return;

		int specificAchievementID = Vars::Misc::Automation::AchievementSpamID.Value;

		IAchievement* pAchievement = nullptr;
		for (int i = 0; i < pAchievementMgr->GetAchievementCount(); i++)
		{
			IAchievement* pCurrentAchievement = pAchievementMgr->GetAchievementByIndex(i);
			if (pCurrentAchievement && pCurrentAchievement->GetAchievementID() == specificAchievementID)
			{
				pAchievement = pCurrentAchievement;
				break;
			}
		}

		if (!pAchievement || !pAchievement->GetName())
		{
			pAchievement = pAchievementMgr->GetAchievementByIndex(0);
			if (!pAchievement || !pAchievement->GetName())
				return;

			specificAchievementID = pAchievement->GetAchievementID();
			Vars::Misc::Automation::AchievementSpamID.Value = specificAchievementID;
		}

		m_iAchievementSpamID = specificAchievementID;
		m_sAchievementSpamName = pAchievement->GetName();
		m_eAchievementSpamState = AchievementSpamState::CLEARING;
		break;
	}
	case AchievementSpamState::CLEARING:
	{
		I::SteamUserStats->RequestCurrentStats();
		I::SteamUserStats->ClearAchievement(m_sAchievementSpamName.c_str());
		I::SteamUserStats->StoreStats();

		m_tAchievementDelayTimer.Update();
		m_eAchievementSpamState = AchievementSpamState::WAITING;
		break;
	}
	case AchievementSpamState::WAITING:
	{
		if (!m_tAchievementDelayTimer.Run(0.1f))
			return;

		m_eAchievementSpamState = AchievementSpamState::AWARDING;
		break;
	}
	case AchievementSpamState::AWARDING:
	{
		I::SteamUserStats->RequestCurrentStats();
		pAchievementMgr->AwardAchievement(m_iAchievementSpamID);
		I::SteamUserStats->StoreStats();

		m_eAchievementSpamState = AchievementSpamState::IDLE;
		break;
	}
	}
}


void CMisc::AutoReport()
{
	if (!Vars::Misc::Automation::AutoReport.Value)
		return;

	static Timer tReportTimer{};
	if (!tReportTimer.Run(5.0f))
		return;

	int iLocalIdx = I::EngineClient->GetLocalPlayer();
	for (int i = 1; i <= I::EngineClient->GetMaxClients(); i++)
	{
		if (i == iLocalIdx)
			continue;

		if (auto uSteamId = F::PlayerUtils.GetAccountID(i))
		{
			if (H::Entities.IsFriend(i) ||
				H::Entities.InParty(i) ||
				F::PlayerUtils.IsIgnored(i) ||
				F::PlayerUtils.HasTag(i, F::PlayerUtils.TagToIndex(IGNORED_TAG)) ||
				F::PlayerUtils.HasTag(i, F::PlayerUtils.TagToIndex(FRIEND_TAG)))
				continue;

			uint64_t uSteamID64 = ((uint64_t)1 << 56) | ((uint64_t)1 << 52) | ((uint64_t)1 << 32) | uSteamId;
			S::ReportPlayerAccount.Call<bool>(uSteamID64, 1);
		}
	}
}


void CMisc::ChatSpam(CTFPlayer* pLocal)
{
	auto ResetChatTimer = [&]()
		{
			m_tChatSpamTimer.Update();
		};

	if (!Vars::Misc::Automation::ChatSpam::Enable.Value)
	{
		ResetChatTimer();
		return;
	}

	if (!pLocal->IsAlive() || !pLocal->IsInValidTeam() || pLocal->m_iClass() == TF_CLASS_UNDEFINED)
	{
		ResetChatTimer();
		return;
	}

	static Timer tReloadTimer{};
	auto EnsureChatLinesLoaded = [&]() -> bool
		{
			size_t uSize = m_vChatSpamLines.size();
			if (uSize > 0 && !tReloadTimer.Run(5.0f))
				return true;

			static const char* szDefaultContent =
				"This is a default message from cat_chatspam.txt\n"
				"Edit this file unibox/cat_chatspam.txt\n"
				"Each line will be sent as a separate message\n"
				"[unibox] Chat Spam is working!\n"
				"Put your chat spam lines in this file\n";

			if (LoadLines("cat_chatspam.txt", m_vChatSpamLines, szDefaultContent))
			{
				// Reset index if number of lines changed
				if (uSize != m_vChatSpamLines.size())
					m_iCurrentChatSpamIndex = 0;
				return true;
			}

			m_vChatSpamLines = {
				"Put your chat spam lines in unibox/cat_chatspam.txt",
				"ChatSpam is running but couldn't find unibox/cat_chatspam.txt",
				"[unibox] Chat Spam is working!"
			};
			m_iCurrentChatSpamIndex = 0;
			return false;
		};

	if (!EnsureChatLinesLoaded())
		return;

	float flSpamInterval = Vars::Misc::Automation::ChatSpam::Interval.Value;
	if (flSpamInterval < 0.2f)
		flSpamInterval = 0.2f;

	if (!m_tChatSpamTimer.Run(flSpamInterval))
		return;

	auto FetchNextChatLine = [&]() -> std::string
		{
			const size_t uLineCount = m_vChatSpamLines.size();
			if (!uLineCount)
				return {};

			if (Vars::Misc::Automation::ChatSpam::Randomize.Value)
			{
				int iMax = static_cast<int>(uLineCount) - 1;
				if (iMax < 0)
					iMax = 0;
				const int iRandomIndex = SDK::RandomInt(0, iMax);
				if (iRandomIndex >= 0 && iRandomIndex < static_cast<int>(uLineCount))
					return m_vChatSpamLines[iRandomIndex];
				return "[ChatSpam]";
			}

			if (m_iCurrentChatSpamIndex < 0 || m_iCurrentChatSpamIndex >= static_cast<int>(uLineCount))
				m_iCurrentChatSpamIndex = 0;

			const std::string& sLine = m_vChatSpamLines[m_iCurrentChatSpamIndex];
			m_iCurrentChatSpamIndex = (m_iCurrentChatSpamIndex + 1) % static_cast<int>(uLineCount);
			return sLine;
		};

	std::string sChatLine = FetchNextChatLine();
	if (sChatLine.empty())
		return;

	sChatLine = ReplaceTags(sChatLine);

	if (sChatLine.length() > 150)
		sChatLine.resize(150);

	std::string sChatCommand;
	if (Vars::Misc::Automation::ChatSpam::TeamChat.Value)
		sChatCommand = "say_team \"" + sChatLine + "\"";
	else
		sChatCommand = "say \"" + sChatLine + "\"";

	SDK::Output("ChatSpam", std::format("Sending: {}", sChatCommand).c_str(), {}, OUTPUT_CONSOLE | OUTPUT_DEBUG);
	I::EngineClient->ClientCmd_Unrestricted(sChatCommand.c_str());
}


void CMisc::EnsureChatUtilsDoc()
{
	static bool bWritten = false;
	if (bWritten)
		return;
	bWritten = true;

	const std::string sPath = F::Configs.m_sConfigPath + "chatutils_readme.txt";
	if (std::filesystem::exists(sPath))
		return;

	static const char* szDoc =
		"================================================================\n"
		"  ChatUtils Documentation\n"
		"================================================================\n"
		"\n"
		"All chat-related text features read plain .txt files from this\n"
		"cfg folder. Each file is auto-generated with examples the first\n"
		"time the corresponding feature runs. Lines starting with // and\n"
		"empty lines are ignored. Edit, save, and the file is reloaded\n"
		"automatically (within ~5 seconds).\n"
		"\n"
		"----------------------------------------------------------------\n"
		"  Files\n"
		"----------------------------------------------------------------\n"
		"\n"
		"  cat_chatspam.txt   Lines sent on a timer by Chat Spam.\n"
		"                     One message per line.\n"
		"\n"
		"  killsay.txt        Lines sent when YOU kill another player.\n"
		"                     One message per line, picked at random.\n"
		"\n"
		"  votekick.txt       Replies fired when a vote kick starts.\n"
		"                     Prefix each line with `F1:` or `F2:`.\n"
		"                       F1: used when a friend/you call the vote\n"
		"                       F2: used when someone votes on a friend/you\n"
		"\n"
		"  autoreply.txt      Trigger-based replies in public chat.\n"
		"                     Format:  trigger1, trigger2 : reply1, reply2\n"
		"                     Triggers are matched case-insensitively as\n"
		"                     substrings; one reply is chosen at random.\n"
		"\n"
		"----------------------------------------------------------------\n"
		"  Tags\n"
		"----------------------------------------------------------------\n"
		"\n"
		"  {killer}        Your in-game name (KillSay).\n"
		"  {victim}        Name of the player you just killed (KillSay).\n"
		"  {target}        Vote-kick target / autoreply sender.\n"
		"  {triggername}   Same thing as {target}.\n"
		"  {initiator}     Player who called the current vote.\n"
		"  {enemyteam}     `RED` or `BLU` - the opposite of your team.\n"
		"  {friendlyteam}  `RED` or `BLU` - your team.\n"
		"  {lastkilled}    Name of the last player you killed. (Global)\n"
		"  {highestscore}  Name of the player with the highest score.\n"
		"  {random}        Random non-bot player on the server.\n"
		"  {friend}        Random player tagged FRIEND.\n"
		"  {ignored}       Random player tagged IGNORED.\n"
		"\n"
		"Tags only resolve when the relevant data is available - e.g.\n"
		"{killer}/{victim} are empty outside KillSay, {target} is empty\n"
		"outside vote replies / auto-replies.\n"
		"\n"
		"----------------------------------------------------------------\n"
		"  PS\n"
		"----------------------------------------------------------------\n"
		"\n"
		"  - Messages are truncated to 150 characters after tag expansion.\n"
		"  - Kill Say honours the Chance slider in the menu.\n"
		"  - Use Team Chat toggles in the menu to switch say -> say_team.\n"
		"\n"
		"================================================================\n";

	std::ofstream file(sPath);
	if (file.good())
		file << szDoc;
}


void CMisc::DoKillSay(int iVictim)
{
	if (!Vars::Misc::Automation::KillSay::Enable.Value)
		return;

	const int iChance = std::clamp(Vars::Misc::Automation::KillSay::Chance.Value, 0, 100);
	if (!iChance || SDK::RandomInt(1, 100) > iChance)
		return;

	static Timer tReloadTimer{};
	if (m_vKillSayLines.empty() || tReloadTimer.Run(5.0f))
	{
		static const char* szDefaultContent =
			"// Kill Say configuration\n"
			"// Each non-empty, non-// line is a possible message sent when you get a kill.\n"
			"// Supports tags: {killer}, {victim}, {enemyteam}, {friendlyteam}, {random}, {friend}, {ignored}, {highestscore}, {lastkilled}\n"
			"SO FUHIN EASY!!! GET GOOD, GET UNIBOX!\n"
			"{victim} YOURE SO TRASH!\n"
			"WHAT??? YOU DIED TO ME, {killer}??? HOW COULD YOU BE THIS BAD??? {enemyteam} IS CARRYING YOU SO BAD!!!\n"
			"{friendlyteam} IS SUPERIOR, JUST LIKE {killer} AND UNIBOX! UNLIKE {victim} AND {enemyteam}!\n"
			"I AM THE FUCKING SPECTRE\n";
		LoadLines("killsay.txt", m_vKillSayLines, szDefaultContent);
	}

	if (m_vKillSayLines.empty())
		return;

	std::string sVictim;
	{
		player_info_t pi;
		if (I::EngineClient->GetPlayerInfo(iVictim, &pi))
			sVictim = pi.name;
	}

	std::string sKiller;
	{
		player_info_t pi;
		if (I::EngineClient->GetPlayerInfo(I::EngineClient->GetLocalPlayer(), &pi))
			sKiller = pi.name;
	}

	const int iIndex = SDK::RandomInt(0, static_cast<int>(m_vKillSayLines.size()) - 1);
	std::string sLine = ReplaceTags(m_vKillSayLines[iIndex], "", "", sKiller, sVictim);

	if (sLine.length() > 150)
		sLine.resize(150);

	std::string sCommand = Vars::Misc::Automation::KillSay::TeamChat.Value
		? "say_team \"" + sLine + "\""
		: "say \"" + sLine + "\"";

	SDK::Output("KillSay", std::format("Sending: {}", sCommand).c_str(), {}, OUTPUT_CONSOLE | OUTPUT_DEBUG);
	I::EngineClient->ClientCmd_Unrestricted(sCommand.c_str());
}


std::string CMisc::ReplaceTags(std::string sMsg, std::string sTarget, std::string sInitiator, std::string sKiller, std::string sVictim)
{
	auto ReplaceAll = [&](std::string& str, const std::string& from, const std::string& to)
		{
			if (from.empty()) return;
			size_t start_pos = 0;
			while ((start_pos = str.find(from, start_pos)) != std::string::npos)
			{
				str.replace(start_pos, from.length(), to);
				start_pos += to.length();
			}
		};

	if (!sTarget.empty())
	{
		ReplaceAll(sMsg, "{target}", sTarget);
		ReplaceAll(sMsg, "{triggername}", sTarget);
	}

	if (!sInitiator.empty())
		ReplaceAll(sMsg, "{initiator}", sInitiator);

	if (!sKiller.empty())
		ReplaceAll(sMsg, "{killer}", sKiller);

	if (!sVictim.empty())
		ReplaceAll(sMsg, "{victim}", sVictim);

	if (sMsg.find("{enemyteam}") != std::string::npos || sMsg.find("{friendlyteam}") != std::string::npos)
	{
		auto pLocal = H::Entities.GetLocal();
		if (pLocal)
		{
			int iLocalTeam = pLocal->m_iTeamNum();
			ReplaceAll(sMsg, "{enemyteam}", iLocalTeam == TF_TEAM_RED ? "BLU" : "RED");
			ReplaceAll(sMsg, "{friendlyteam}", iLocalTeam == TF_TEAM_RED ? "RED" : "BLU");
		}
	}

	if (sMsg.find("{lastkilled}") != std::string::npos)
		ReplaceAll(sMsg, "{lastkilled}", m_sLastKilledName.empty() ? "unknown" : m_sLastKilledName);

	if (sMsg.find("{highestscore}") != std::string::npos)
	{
		int iHighestScore = -1;
		std::string sHighestScoreName = "unknown";
		auto pResource = H::Entities.GetResource();
		if (pResource)
		{
			for (int i = 1; i <= I::EngineClient->GetMaxClients(); i++)
			{
				if (!pResource->m_bValid(i)) continue;
				int iScore = pResource->m_iTotalScore(i);
				if (iScore > iHighestScore)
				{
					iHighestScore = iScore;
					sHighestScoreName = pResource->GetName(i);
				}
			}
		}
		ReplaceAll(sMsg, "{highestscore}", sHighestScoreName);
	}

	auto GetRandomPlayer = [&](bool bFriend, bool bIgnored) -> std::string
		{
			std::vector<std::string> vCandidates;
			auto pResource = H::Entities.GetResource();
			if (pResource)
			{
				for (int i = 1; i <= I::EngineClient->GetMaxClients(); i++)
				{
					if (!pResource->m_bValid(i) || pResource->IsFakePlayer(i)) continue;

					bool isFriend = F::PlayerUtils.HasTag(i, F::PlayerUtils.TagToIndex(FRIEND_TAG));
					bool isIgnored = F::PlayerUtils.HasTag(i, F::PlayerUtils.TagToIndex(IGNORED_TAG));

					if (bFriend && !isFriend) continue;
					if (bIgnored && !isIgnored) continue;

					vCandidates.push_back(pResource->GetName(i));
				}
			}
			if (vCandidates.empty()) return "unknown";
			return vCandidates[SDK::RandomInt(0, static_cast<int>(vCandidates.size()) - 1)];
		};

	if (sMsg.find("{random}") != std::string::npos)
		ReplaceAll(sMsg, "{random}", GetRandomPlayer(false, false));

	if (sMsg.find("{friend}") != std::string::npos)
		ReplaceAll(sMsg, "{friend}", GetRandomPlayer(true, false));

	if (sMsg.find("{ignored}") != std::string::npos)
		ReplaceAll(sMsg, "{ignored}", GetRandomPlayer(false, true));

	return sMsg;
}


void CMisc::OnVoteStart(int iCaller, int iTarget, const std::string& sTarget)
{
	if (!Vars::Misc::Automation::ChatSpam::VoteKickReply.Value)
		return;

	static Timer tReloadTimer{};
	if ((m_vF1Messages.empty() && m_vF2Messages.empty()) || tReloadTimer.Run(5.0f))
	{
		m_vF1Messages.clear();
		m_vF2Messages.clear();

		static const char* szDefaultContent =
			"// Vote Kick Reply Configuration\n"
			"// Format: F1: message (supports {target}, {initiator}, {enemyteam}, {friendlyteam})\n"
			"// Format: F2: message (supports {target}, {initiator}, {enemyteam}, {friendlyteam})\n"
			"F1: {initiator} called a vote on {target}! Go {friendlyteam}!\n"
			"F2: {initiator} is trying to kick {target}! Don't let {enemyteam} win!\n";

		std::vector<std::string> vLines;
		if (LoadLines("votekick.txt", vLines, szDefaultContent))
		{
			for (const auto& line : vLines)
			{
				if (line.find("F1:") == 0)
				{
					std::string msg = line.substr(3);
					if (msg.find_first_not_of(" \t") != std::string::npos)
						msg.erase(0, msg.find_first_not_of(" \t"));
					if (!msg.empty()) m_vF1Messages.push_back(msg);
				}
				else if (line.find("F2:") == 0)
				{
					std::string msg = line.substr(3);
					if (msg.find_first_not_of(" \t") != std::string::npos)
						msg.erase(0, msg.find_first_not_of(" \t"));
					if (!msg.empty()) m_vF2Messages.push_back(msg);
				}
			}
		}
	}

	bool bTargetIsFriend = H::Entities.IsFriend(iTarget) || H::Entities.InParty(iTarget) || F::PlayerUtils.IsIgnored(iTarget);
	bool bCallerIsFriend = H::Entities.IsFriend(iCaller) || H::Entities.InParty(iCaller) || F::PlayerUtils.IsIgnored(iCaller);
	bool bTargetIsLocal = iTarget == I::EngineClient->GetLocalPlayer();
	bool bCallerIsLocal = iCaller == I::EngineClient->GetLocalPlayer();

	std::string sReply = "";
	std::string sInitiator = "";
	auto pResource = H::Entities.GetResource();
	if (pResource && pResource->m_bValid(iCaller))
		sInitiator = pResource->GetName(iCaller);

	if ((bTargetIsFriend || bTargetIsLocal) && !bCallerIsLocal)
	{
		if (!m_vF2Messages.empty())
		{
			int index = SDK::RandomInt(0, static_cast<int>(m_vF2Messages.size()) - 1);
			sReply = m_vF2Messages[index];
		}
	}
	else if ((bCallerIsFriend || bCallerIsLocal) && !bTargetIsLocal)
	{
		if (!m_vF1Messages.empty())
		{
			int index = SDK::RandomInt(0, static_cast<int>(m_vF1Messages.size()) - 1);
			sReply = m_vF1Messages[index];
		}
	}

	if (!sReply.empty())
	{
		sReply = ReplaceTags(sReply, sTarget, sInitiator);
		I::EngineClient->ClientCmd_Unrestricted(std::format("say {}", sReply).c_str());
	}
}


bool CMisc::LoadLines(const char* szFileName, std::vector<std::string>& vLines, const char* szDefaultContent)
{
	vLines.clear();

	std::string sPath = F::Configs.m_sConfigPath + szFileName;

	if (!std::filesystem::exists(sPath) && szDefaultContent)
	{
		std::ofstream newFile(sPath);
		if (newFile.good())
			newFile << szDefaultContent;
	}

	std::ifstream file(sPath);
	if (!file.good())
		return false;

	std::string line;
	while (std::getline(file, line))
	{
		if (line.empty() || line.find("//") == 0)
			continue;

		vLines.push_back(line);
	}

	return !vLines.empty();
}


std::vector<std::string> CMisc::ParseTokens(std::string str, char delimiter)
{
	std::vector<std::string> tokens;
	size_t pos = 0;
	std::string token;
	while ((pos = str.find(delimiter)) != std::string::npos)
	{
		token = str.substr(0, pos);
		if (token.find_first_not_of(" \t") != std::string::npos)
		{
			token.erase(0, token.find_first_not_of(" \t"));
			token.erase(token.find_last_not_of(" \t") + 1);
			if (!token.empty())
				tokens.push_back(token);
		}
		str.erase(0, pos + 1);
	}
	token = str;
	if (token.find_first_not_of(" \t") != std::string::npos)
	{
		token.erase(0, token.find_first_not_of(" \t"));
		token.erase(token.find_last_not_of(" \t") + 1);
		if (!token.empty())
			tokens.push_back(token);
	}
	return tokens;
}


void CMisc::OnChatMessage(int iEntIndex, const std::string& sName, const std::string& sMsg)
{
	if (iEntIndex == I::EngineClient->GetLocalPlayer())
		return;

	if (Vars::Misc::Automation::ChatSpam::AutoReply.Value)
	{
		static Timer tReloadTimer{};
		if (m_vAutoReplies.empty() || tReloadTimer.Run(5.0f))
		{
			m_vAutoReplies.clear();

			static const char* szDefaultContent =
				"// Auto Reply Configuration\n"
				"// Format: trigger1, trigger2 : response1, response2\n"
				"// Example:\n"
				"hello, hi : hi there, hello!\n"
				"bot, hacker : I am not a bot, I am just good\n";

			std::vector<std::string> vLines;
			if (LoadLines("autoreply.txt", vLines, szDefaultContent))
			{
				for (const auto& line : vLines)
				{
					size_t delimiterPos = line.find(':');
					if (delimiterPos != std::string::npos)
					{
						std::string triggersStr = line.substr(0, delimiterPos);
						std::string repliesStr = line.substr(delimiterPos + 1);

						AutoReply_t entry;
						entry.vTriggers = ParseTokens(triggersStr, ',');
						entry.vReplies = ParseTokens(repliesStr, ',');

						if (!entry.vTriggers.empty() && !entry.vReplies.empty())
							m_vAutoReplies.push_back(entry);
					}
				}
			}
		}

		auto StripColors = [](std::string str) -> std::string
			{
				std::string out = "";
				for (size_t i = 0; i < str.length(); i++)
				{
					if (str[i] > 0 && str[i] < 32) continue;
					out += str[i];
				}
				return out;
			};

		std::string sCleanMsg = StripColors(sMsg);
		std::transform(sCleanMsg.begin(), sCleanMsg.end(), sCleanMsg.begin(), ::tolower);

		for (const auto& entry : m_vAutoReplies)
		{
			bool bTriggered = false;
			for (const auto& trigger : entry.vTriggers)
			{
				std::string sLowTrigger = trigger;
				std::transform(sLowTrigger.begin(), sLowTrigger.end(), sLowTrigger.begin(), ::tolower);

				if (sCleanMsg.find(sLowTrigger) != std::string::npos)
				{
					bTriggered = true;
					break;
				}
			}

			if (bTriggered)
			{
				if (!entry.vReplies.empty())
				{
					int index = SDK::RandomInt(0, static_cast<int>(entry.vReplies.size()) - 1);
					std::string sReply = ReplaceTags(entry.vReplies[index], sName);
					I::EngineClient->ClientCmd_Unrestricted(std::format("say {}", sReply).c_str());
					break;
				}
			}
		}
	}

}

