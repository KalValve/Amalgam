#pragma once
#include "CBaseEntity.h"

class CFuncPasstimeGoal : public CBaseEntity
{
public:
	enum GoalType
	{
		TYPE_HOOP,
		TYPE_ENDZONE,
		TYPE_TOWER
	};

	NETVAR(m_bTriggerDisabled, bool, "CFuncPasstimeGoal", "m_bTriggerDisabled");
	NETVAR(m_iGoalType, int, "CFuncPasstimeGoal", "m_iGoalType");
};
