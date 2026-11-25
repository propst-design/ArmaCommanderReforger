// Per-match squad battle history & earned veterancy (no roster files: server-only match state).
// Squads on every side (player- and AI-commanded alike) earn XP from kills, vehicle kills, and
// base captures they participate in, unlocking modest reaction/skill gains (capped at Expert,
// never aimbot Perfect, and never overriding mission-pinned soldiers). Taking heavy green
// replacements dilutes XP proportionally, making veteran squads worth extracting and refitting.
class ACR_SquadRecord : Managed
{
	float m_fXP;
	int m_iKills;
	int m_iVehicleKills;
	int m_iCaptures;
	int m_iLastAlive;
	int m_iPeakTier; // highest tier announced in the side's intel feed
	ref array<string> m_aHonors;

	void ACR_SquadRecord()
	{
		m_aHonors = {};
	}
}

class ACR_Veterancy
{
	static const float XP_KILL = 10;
	static const float XP_VEHICLE_KILL = 25;
	static const float XP_CAPTURE = 30;
	static const float TIER_BLOODED = 30;
	static const float TIER_VETERAN = 80;
	static const float TIER_HARDENED = 160;
	static const float CAPTURE_MARGIN_M = 40;
	static const int MAX_HONORS = 3;

	protected static ref map<ACR_Squad, ref ACR_SquadRecord> s_Records;

	// Called by ACR_StaticInit at game start.
	static void ACR_InitStatics()
	{
		s_Records = new map<ACR_Squad, ref ACR_SquadRecord>();
	}

	static void Reset()
	{
		if (s_Records) s_Records.Clear();
	}

	static void Prune()
	{
		if (!s_Records) return;
		for (int i = s_Records.Count() - 1; i >= 0; i--)
		{
			if (!s_Records.GetKey(i)) s_Records.RemoveElement(i);
		}
	}

	static ACR_SquadRecord Get(ACR_Squad squad, bool create = false)
	{
		if (!squad || !s_Records) return null;
		ACR_SquadRecord rec;
		if (s_Records.Find(squad, rec)) return rec;
		if (!create) return null;
		rec = new ACR_SquadRecord();
		s_Records.Insert(squad, rec);
		return rec;
	}

	static int TierOfXP(float xp)
	{
		if (xp >= TIER_HARDENED) return 3;
		if (xp >= TIER_VETERAN) return 2;
		if (xp >= TIER_BLOODED) return 1;
		return 0;
	}

	static int Tier(ACR_Squad squad)
	{
		ACR_SquadRecord rec = Get(squad);
		if (!rec) return 0;
		return TierOfXP(rec.m_fXP);
	}

	static string TierName(int tier)
	{
		if (tier >= 3) return "Hardened";
		if (tier == 2) return "Veteran";
		if (tier == 1) return "Blooded";
		return "";
	}

	// Skill steps above base (capped at +1 step / Expert in ACR_AISkill.Apply).
	static int SkillSteps(ACR_Squad squad)
	{
		if (Tier(squad) >= 2) return 1;
		return 0;
	}

	// Reaction / spotting multiplier by tier.
	static float ReactionScale(ACR_Squad squad)
	{
		int tier = Tier(squad);
		if (tier >= 3) return 1.22;
		if (tier == 2) return 1.15;
		if (tier == 1) return 1.08;
		return 1.0;
	}

	// ---------------------------------------------------------------- events

	static void OnKill(IEntity killer, bool isVehicle)
	{
		if (!Replication.IsServer() || !killer) return;
		ACR_Manager manager = ACR_Manager.GetInstance();
		if (!manager) return;
		ACR_Squad squad = manager.FindSquadBySoldier(killer);
		if (!squad) return;
		ACR_SquadRecord rec = Get(squad, true);
		if (isVehicle)
		{
			rec.m_iVehicleKills++;
			Award(squad, rec, XP_VEHICLE_KILL, "");
		}
		else
		{
			rec.m_iKills++;
			Award(squad, rec, XP_KILL, "");
		}
	}

	static void OnBaseCaptured(string baseName, string byFaction)
	{
		if (!Replication.IsServer() || baseName.IsEmpty() || byFaction.IsEmpty()) return;
		ACR_Manager manager = ACR_Manager.GetInstance();
		if (!manager) return;
		ACR_Base target;
		foreach (ACR_Base base : ACR_Base.s_All)
		{
			if (base && !base.m_bRetired && base.m_sName == baseName) { target = base; break; }
		}
		if (!target) return;
		float reach = target.m_fRadius + CAPTURE_MARGIN_M;
		vector centre = target.GetOrigin();
		foreach (ACR_Squad squad : manager.m_Squads)
		{
			if (!squad || !squad.GetFaction() || squad.GetFaction().GetFactionKey() != byFaction) continue;
			IEntity anchor = squad.GetAnchor();
			if (!anchor || vector.DistanceXZ(anchor.GetOrigin(), centre) > reach) continue;
			ACR_SquadRecord rec = Get(squad, true);
			rec.m_iCaptures++;
			string honor = "took " + baseName;
			if (!rec.m_aHonors.Contains(honor))
			{
				rec.m_aHonors.Insert(honor);
				while (rec.m_aHonors.Count() > MAX_HONORS) rec.m_aHonors.RemoveOrdered(0);
			}
			Award(squad, rec, XP_CAPTURE, honor);
		}
	}

	// Called from ACR_Squad.UpdateStatus with current living count: if green replacements arrived
	// while the squad held XP, dilute XP proportionally to the surviving core.
	static void NoteStrength(ACR_Squad squad, int alive)
	{
		if (!squad || alive <= 0) return;
		ACR_SquadRecord rec = Get(squad);
		if (!rec) return;
		if (rec.m_iLastAlive > 0 && alive > rec.m_iLastAlive && rec.m_fXP > 0)
		{
			float keep = rec.m_iLastAlive / (float)alive;
			rec.m_fXP *= keep;
		}
		rec.m_iLastAlive = alive;
	}

	protected static void Award(ACR_Squad squad, ACR_SquadRecord rec, float xp, string note)
	{
		if (!squad || !rec || xp <= 0) return;
		int alive = squad.CountAlive();
		if (alive > 0 && rec.m_iLastAlive <= 0) rec.m_iLastAlive = alive;
		rec.m_fXP += xp;
		int tier = TierOfXP(rec.m_fXP);
		if (tier >= 2 && tier > rec.m_iPeakTier)
		{
			rec.m_iPeakTier = tier;
			ACR_Manager manager = ACR_Manager.GetInstance();
			if (manager && squad.GetFaction())
			{
				ACR_Intel intel = manager.IntelFor(squad.GetFaction().GetFactionKey());
				string detail = "";
				if (!note.IsEmpty()) detail = " (" + note + ")";
				else if (!rec.m_aHonors.IsEmpty()) detail = " (" + rec.m_aHonors[rec.m_aHonors.Count() - 1] + ")";
				vector at = vector.Zero;
				if (squad.GetAnchor()) at = squad.GetAnchor().GetOrigin();
				if (intel) intel.AddFeed(GetGame().GetWorld().GetWorldTime(), string.Format("%1 earned %2 status%3", squad.GetName(), TierName(tier), detail), at);
			}
		}
	}

	// Short tag for ACR_Squad.Describe(), e.g. "Veteran (6 kills, took Montignac)" or "" if green.
	static string Tag(ACR_Squad squad)
	{
		ACR_SquadRecord rec = Get(squad);
		if (!rec) return "";
		int tier = TierOfXP(rec.m_fXP);
		if (tier <= 0) return "";
		string parts = "";
		if (rec.m_iKills > 0) parts += string.Format("%1 kills", rec.m_iKills);
		if (rec.m_iVehicleKills > 0)
		{
			if (!parts.IsEmpty()) parts += ", ";
			parts += string.Format("%1 veh", rec.m_iVehicleKills);
		}
		if (!rec.m_aHonors.IsEmpty())
		{
			if (!parts.IsEmpty()) parts += ", ";
			parts += rec.m_aHonors[rec.m_aHonors.Count() - 1];
		}
		if (parts.IsEmpty()) return TierName(tier);
		return string.Format("%1 (%2)", TierName(tier), parts);
	}
}
