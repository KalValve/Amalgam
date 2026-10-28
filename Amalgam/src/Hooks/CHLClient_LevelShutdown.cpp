#include "../SDK/SDK.h"

#include "../Features/EnginePrediction/EnginePrediction.h"
#include "../Features/Spectate/Spectate.h"
#include "../Features/NavBot/Objectives.h"
#include "../Features/NavBot/NavEngine.h"
#include "../Features/NavBot/NavBotCore.h"
#include "../Features/NavBot/BotUtils.h"
#include "../Features/FollowBot/FollowBot.h"
#include "../Features/Misc/AutoVote/AutoVote.h"

MAKE_HOOK(CHLClient_LevelShutdown, U::Memory.GetVirtual(I::Client, 7), void,
	void* rcx)
{
	DEBUG_RETURN(CHLClient_LevelShutdown, rcx);

	H::Entities.Clear(true);
	F::EnginePrediction.Unload();
	F::Spectate.Reset();
	F::GameObjectiveController.Reset();
	F::NavEngine.ClearRespawnRooms();
	F::NavBotCore.Reset();
	F::BotUtils.Reset();
	F::FollowBot.Reset();
	F::AutoVote.Reset();

	CALL_ORIGINAL(rcx);
}
