#include "../SDK/SDK.h"

#include "../Features/PacketManip/AntiAim/AntiAim.h"
#include "../Features/Resolver/Resolver.h"
#include "../Utils/Math/Math.h"

MAKE_HOOK(CTFPlayer_UpdateClientSideAnimation, S::CTFPlayer_UpdateClientSideAnimation(), void,
	void* rcx)
{
	DEBUG_RETURN(CTFPlayer_UpdateClientSideAnimation, rcx);

	auto pPlayer = reinterpret_cast<CTFPlayer*>(rcx);
	const bool bLocalPlayer = pPlayer->entindex() == I::EngineClient->GetLocalPlayer();
	const bool bLocalTauntSpin = bLocalPlayer
		&& Vars::AntiAim::TauntSpin.Value && pPlayer->IsTaunting();
	const bool bLocalAntiAimSpin = bLocalPlayer && !pPlayer->IsTaunting() && G::AntiAim
		&& (Vars::AntiAim::YawReal.Value == Vars::AntiAim::YawEnum::Spin
			|| Vars::AntiAim::YawFake.Value == Vars::AntiAim::YawEnum::Spin);
	if (bLocalAntiAimSpin)
	{
		CALL_ORIGINAL(rcx);

		if (auto pAnimState = pPlayer->m_PlayerAnimState())
		{
			const bool bRealSpin = Vars::AntiAim::YawReal.Value == Vars::AntiAim::YawEnum::Spin;
			const float flTargetYaw = bRealSpin ? F::AntiAim.vRealAngles.y : F::AntiAim.vFakeAngles.y;
			const float flSpinSpeed = Vars::AntiAim::SpinSpeed.Value;
			const float flTickSpinPhase = std::fmod(I::GlobalVars->tickcount * flSpinSpeed + 180.f, 360.f) - 180.f;
			const float flBaseYaw = Math::NormalizeAngle(flTargetYaw - flTickSpinPhase);
			const double flRenderTime = I::GlobalVars->curtime + I::GlobalVars->interpolation_amount * TICK_INTERVAL;
			static int s_iSpinPlayer = -1;
			static bool s_bRealSpin = false;
			static double s_flRenderSpinPhase = 0.0;
			static double s_flLastRenderTime = -1.0;
			if (s_iSpinPlayer != pPlayer->entindex() || s_bRealSpin != bRealSpin
				|| s_flLastRenderTime < 0.0 || flRenderTime < s_flLastRenderTime)
			{
				s_iSpinPlayer = pPlayer->entindex();
				s_bRealSpin = bRealSpin;
				s_flRenderSpinPhase = static_cast<double>(I::GlobalVars->tickcount) * flSpinSpeed;
			}
			else
				s_flRenderSpinPhase += flSpinSpeed * ((flRenderTime - s_flLastRenderTime) / TICK_INTERVAL);
			s_flLastRenderTime = flRenderTime;

			const float flYaw = flBaseYaw + static_cast<float>(s_flRenderSpinPhase);
			pAnimState->m_flEyeYaw = flYaw;
			pAnimState->m_flGoalFeetYaw = flYaw;
			pAnimState->m_flCurrentFeetYaw = flYaw;
			pAnimState->SetRenderAngles({ 0.f, flYaw, 0.f });
			pPlayer->InvalidateBoneCache();
		}
		return;
	}

	if (bLocalTauntSpin || ((!Vars::Visuals::Removals::Interpolation.Value && !F::Resolver.GetAngles(pPlayer) || G::UpdatingAnims)
		&& (pPlayer->entindex() != I::EngineClient->GetLocalPlayer() || I::EngineClient->IsPlayingDemo())))
		CALL_ORIGINAL(rcx);
}
