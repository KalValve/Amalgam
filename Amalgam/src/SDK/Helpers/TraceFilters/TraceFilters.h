#pragma once
#include "../../Definitions/Interfaces/IEngineTrace.h"
#include "../../Definitions/Definitions.h"

class CBaseEntity;

enum
{
	SKIP_CHECK,
	FORCE_PASS,
	FORCE_HIT
};

enum
{
	WEAPON_INCLUDE,
	WEAPON_EXCLUDE
};
enum
{
	PLAYER_DEFAULT,
	PLAYER_NONE,
	PLAYER_ALL
};
enum
{
	OBJECT_DEFAULT,
	OBJECT_NONE,
	OBJECT_ALL
};

class CTraceFilterHitscan : public ITraceFilter
{
public:
	CTraceFilterHitscan() = default;
	CTraceFilterHitscan(CBaseEntity* pPassEntity) : pSkip(pPassEntity) {}
	bool ShouldHitEntity(IHandleEntity* pHandleEntity, int nContentsMask) override;
	TraceType_t GetTraceType() const override;
	CBaseEntity* pSkip = nullptr;

	int iTeam = -1;
	std::vector<int> vWeapons = { TF_WEAPON_SNIPERRIFLE, TF_WEAPON_SNIPERRIFLE_CLASSIC, TF_WEAPON_SNIPERRIFLE_DECAP };
	int iType = FORCE_HIT;
	int iWeapon = WEAPON_EXCLUDE;
	bool bWeapon = false;
};

class CTraceFilterCollideable : public ITraceFilter
{
public:
	bool ShouldHitEntity(IHandleEntity* pHandleEntity, int nContentsMask) override;
	TraceType_t GetTraceType() const override;
	CBaseEntity* pSkip = nullptr;

	int iTeam = -1;
	std::vector<int> vWeapons = { TF_WEAPON_CROSSBOW, TF_WEAPON_LUNCHBOX };
	int iType = FORCE_HIT;
	int iWeapon = WEAPON_INCLUDE;
	bool bWeapon = false;
	int iPlayer = PLAYER_DEFAULT;
	int iObject = OBJECT_ALL;
	bool bMisc = false;
};

class CTraceFilterWorldAndPropsOnly : public ITraceFilter
{
public:
	CTraceFilterWorldAndPropsOnly() = default;
	CTraceFilterWorldAndPropsOnly(const IHandleEntity* pPassEntity) : pSkip(const_cast<CBaseEntity*>(reinterpret_cast<const CBaseEntity*>(pPassEntity))) {}
	bool ShouldHitEntity(IHandleEntity* pHandleEntity, int nContentsMask) override;
	TraceType_t GetTraceType() const override;
	CBaseEntity* pSkip = nullptr;

	int iTeam = -1;
};

class CTraceFilterNavigation : public ITraceFilter
{
public:
	CTraceFilterNavigation() = default;
	CTraceFilterNavigation(const IHandleEntity* pPassEntity) : m_pSkip(const_cast<CBaseEntity*>(reinterpret_cast<const CBaseEntity*>(pPassEntity))) {}
	bool ShouldHitEntity(IHandleEntity* pHandleEntity, int nContentsMask) override;
	TraceType_t GetTraceType() const override;
	CBaseEntity* m_pSkip = nullptr;
	int m_iTeam = -1;
	int m_iPlayer = PLAYER_NONE;
	int m_iObject = OBJECT_NONE;
};
