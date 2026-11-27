// Insurgent player disguise (0.8.0 #97 item 10): players only (AI fighters never disguise).
//  - Going plain: at a hideout (within 40 m, clear of army) or anywhere not watched by the army
//    (ACR_Muster.WatchedByEnemy), with no weapon in hands and no long weapon slung (a pistol or
//    explosive charge stowed out of sight is allowed). Flips the body's faction affiliation to CIV
//    while keeping the player's own faction on the insurgent side.
//  - While disguised: army AI treats him as a civilian, he never shows on the army map, and
//    ACR_BaseSystem.CollectBodies skips him so he never contests or captures a base. Driving an
//    unarmed civilian vehicle keeps the disguise; a military or armed vehicle blows it.
//  - Blown by: drawing/holding a weapon, being searched at an army base/FOB/manned roadblock
//    while carrying contraband (weapon/pistol/charge), or lingering within 5 m of an army soldier
//    for 10 s. In a Loyal town, locals tip off the army with a rough "Stranger seen" feed line.
//  - Once blown: body flips back to insurgent immediately and a 120 s cooldown applies.
class ACR_DisguiseState : Managed
{
	IEntity m_Body;
	string m_sSide;
	int m_iPlayerId;
	bool m_bWasControlled;
	float m_fLingerMs;
	float m_fStrangerAt;
}

class ACR_Disguise
{
	static const float HIDEOUT_REACH = 40;
	static const float HIDEOUT_ENEMY_CLEAR = 150;
	static const float CHECKPOINT_RADIUS = 25;   // base, FOB or manned roadblock search radius
	static const float LINGER_RADIUS = 5;        // m from an army soldier
	static const float LINGER_S = 10;            // s up close before recognised
	static const float BLOWN_COOLDOWN_S = 120;   // s before going plain again after blown
	static const float STRANGER_GAP_MS = 180000; // 3 min between Loyal-town "stranger seen" tips

	static ref array<ref ACR_DisguiseState> s_States;
	protected static ref map<int, float> s_CooldownUntil;

	// Called by ACR_StaticInit at game start.
	static void ACR_InitStatics()
	{
		s_States = {};
		s_CooldownUntil = new map<int, float>();
	}

	static void Reset()
	{
		if (s_States) s_States.Clear();
		if (s_CooldownUntil) s_CooldownUntil.Clear();
	}

	static ACR_DisguiseState Find(IEntity body)
	{
		if (!body || !s_States) return null;
		foreach (ACR_DisguiseState s : s_States) if (s && s.m_Body == body) return s;
		return null;
	}

	static bool IsDisguised(IEntity body)
	{
		return Find(body) != null;
	}

	// True if holding a weapon in hands (raised or selected in weapon manager).
	static bool HasWeaponInHands(IEntity body)
	{
		if (!body) return false;
		BaseWeaponManagerComponent wm = BaseWeaponManagerComponent.Cast(body.FindComponent(BaseWeaponManagerComponent));
		if (!wm) return false;
		BaseWeaponComponent cur = wm.GetCurrentWeapon();
		if (!cur) return false;
		EWeaponType wt = cur.GetWeaponType();
		return wt != EWeaponType.WT_NONE;
	}

	// True if carrying a rifle, machine gun, sniper or launcher (pistols and stowed charges are allowed).
	static bool HasLongWeapon(IEntity body)
	{
		if (!body) return false;
		BaseWeaponManagerComponent wm = BaseWeaponManagerComponent.Cast(body.FindComponent(BaseWeaponManagerComponent));
		if (!wm) return false;
		array<WeaponSlotComponent> slots = {};
		wm.GetWeaponsSlots(slots);
		foreach (WeaponSlotComponent slot : slots)
		{
			if (!slot) continue;
			IEntity wep = slot.GetWeaponEntity();
			if (!wep) continue;
			BaseWeaponComponent comp = BaseWeaponComponent.Cast(wep.FindComponent(BaseWeaponComponent));
			if (!comp) continue;
			EWeaponType wt = comp.GetWeaponType();
			if (wt == EWeaponType.WT_HANDGUN || wt == EWeaponType.WT_FRAGGRENADE || wt == EWeaponType.WT_SMOKEGRENADE || wt == EWeaponType.WT_NONE) continue;
			return true;
		}
		return false;
	}

	// True if carrying any weapon, pistol, explosive charge or detonator anywhere in inventory.
	static bool HasContraband(IEntity body)
	{
		if (!body) return false;
		if (HasWeaponInHands(body) || HasLongWeapon(body)) return true;
		InventoryStorageManagerComponent inv = InventoryStorageManagerComponent.Cast(body.FindComponent(InventoryStorageManagerComponent));
		if (!inv) return false;
		array<IEntity> items = {};
		inv.GetItems(items);
		foreach (IEntity item : items)
		{
			if (!item) continue;
			if (item.FindComponent(WeaponComponent)) return true;
			if (item.FindComponent(SCR_ExplosiveChargeComponent)) return true;
			if (item.FindComponent(SCR_DetonatorGadgetComponent)) return true;
		}
		return false;
	}

	// True if inside an armed or non-civilian faction vehicle.
	static bool InMilitaryVehicle(IEntity body)
	{
		if (!body) return false;
		IEntity veh = CompartmentAccessComponent.GetVehicleIn(body);
		if (!veh) return false;
		if (veh.FindComponent(BaseWeaponManagerComponent)) return true;
		FactionAffiliationComponent aff = FactionAffiliationComponent.Cast(veh.FindComponent(FactionAffiliationComponent));
		if (aff && aff.GetAffiliatedFaction())
		{
			string key = aff.GetAffiliatedFaction().GetFactionKey();
			if (!key.IsEmpty() && key != "CIV") return true;
		}
		return false;
	}

	// True if within CHECKPOINT_RADIUS of an army base, FOB or manned roadblock.
	static bool AtCheckpoint(vector pos, string army)
	{
		if (army.IsEmpty()) return false;
		ACR_Manager manager = ACR_Manager.GetInstance();
		foreach (ACR_Base base : ACR_Base.s_All)
		{
			if (!base || base.m_bRetired || base.m_sOwner != army) continue;
			float checkRadius = Math.Max(CHECKPOINT_RADIUS, base.m_fRadius);
			if (vector.DistanceXZ(base.GetOrigin(), pos) <= checkRadius && manager && manager.m_Bases && manager.m_Bases.MenWithin(base.GetOrigin(), checkRadius, army) > 0) return true;
		}
		foreach (ACR_Roadblock rb : ACR_Roadblocks.s_aRoadblocks)
		{
			if (!rb || !rb.m_Entity || rb.m_sFaction != army) continue;
			if (vector.DistanceXZ(rb.m_vPos, pos) > CHECKPOINT_RADIUS) continue;
			bool manned = (rb.m_Defenders && (rb.m_Defenders.m_iReserve > 0 || rb.m_Defenders.m_Group)) || (manager && manager.m_Bases && manager.m_Bases.MenWithin(rb.m_vPos, CHECKPOINT_RADIUS, army) > 0);
			if (manned) return true;
		}
		return false;
	}

	protected static bool SetBodyFaction(IEntity body, string factionKey)
	{
		if (!body) return false;
		Faction faction = GetGame().GetFactionManager().GetFactionByKey(factionKey);
		if (!faction) return false;
		FactionAffiliationComponent aff = FactionAffiliationComponent.Cast(body.FindComponent(FactionAffiliationComponent));
		if (!aff) return false;
		aff.SetAffiliatedFaction(faction);
		return true;
	}

	static bool GoPlain(IEntity body, string side, int playerId, out string refusal)
	{
		refusal = "";
		if (!ACR_InsurgentSide.IsInsurgent(side)) { refusal = "only insurgents can go plain"; return false; }
		if (!body || !ACR_Respawn.IsAlive(body)) { refusal = "must be alive to go plain"; return false; }
		if (IsDisguised(body)) { refusal = "already in plain clothes"; return false; }
		float now = GetGame().GetWorld().GetWorldTime();
		float until;
		if (playerId > 0 && s_CooldownUntil.Find(playerId, until) && now < until)
		{
			refusal = string.Format("disguise blown recently — wait %1 s", Math.Ceil((until - now) / 1000));
			return false;
		}
		vector pos = body.GetOrigin();
		string army = ACR_Coin.Army();
		ACR_Manager manager = ACR_Manager.GetInstance();
		ACR_DefenderPost nearHideout = ACR_HideoutHomes.NearestHome(pos, true);
		bool atHideout = nearHideout && vector.DistanceXZ(nearHideout.m_vPos, pos) <= HIDEOUT_REACH && (!manager || !manager.m_Bases || manager.m_Bases.MenWithin(pos, HIDEOUT_ENEMY_CLEAR, army) == 0);
		if (!atHideout && ACR_Muster.WatchedByEnemy(pos, army))
		{
			refusal = "cannot change clothes while enemies are nearby or watching";
			return false;
		}
		if (HasWeaponInHands(body) || HasLongWeapon(body))
		{
			refusal = "drop or stow long weapons first (only a concealed pistol or charge is allowed)";
			return false;
		}
		if (!SetBodyFaction(body, "CIV"))
		{
			refusal = "civilian faction unavailable in this world";
			return false;
		}
		ACR_DisguiseState st = new ACR_DisguiseState();
		st.m_Body = body;
		st.m_sSide = side;
		st.m_iPlayerId = playerId;
		st.m_bWasControlled = playerId > 0 && ACR_Squad.FindController(body) == playerId;
		st.m_fStrangerAt = -STRANGER_GAP_MS;
		s_States.Insert(st);
		PrintFormat("[ACR] DISGUISE player=%1 (%2) went plain at %3", playerId, side, ACR_Manager.Grid(pos));
		return true;
	}

	static bool Drop(IEntity body)
	{
		ACR_DisguiseState st = Find(body);
		if (!st) return false;
		SetBodyFaction(st.m_Body, st.m_sSide);
		s_States.RemoveItem(st);
		PrintFormat("[ACR] DISGUISE player=%1 dropped plain clothes", st.m_iPlayerId);
		return true;
	}

	static void Blow(ACR_DisguiseState st, string reason)
	{
		if (!st) return;
		SetBodyFaction(st.m_Body, st.m_sSide);
		float now = GetGame().GetWorld().GetWorldTime();
		if (st.m_iPlayerId > 0) s_CooldownUntil.Set(st.m_iPlayerId, now + BLOWN_COOLDOWN_S * 1000);
		ACR_Manager manager = ACR_Manager.GetInstance();
		if (manager && st.m_iPlayerId > 0) manager.Notify(st.m_iPlayerId, "Disguise blown: " + reason);
		PrintFormat("[ACR] DISGUISE blown player=%1 (%2): %3", st.m_iPlayerId, st.m_sSide, reason);
		s_States.RemoveItem(st);
	}

	// Server, every second (ACR_Manager.Tick).
	static void Tick(ACR_Manager manager, float stepMs = 1000)
	{
		if (!Replication.IsServer() || !s_States || s_States.IsEmpty()) return;
		string army = ACR_Coin.Army();
		float now = GetGame().GetWorld().GetWorldTime();
		for (int i = s_States.Count() - 1; i >= 0; i--)
		{
			ACR_DisguiseState st = s_States[i];
			if (!st || !st.m_Body)
			{
				s_States.RemoveOrdered(i);
				continue;
			}
			if (!ACR_Respawn.IsAlive(st.m_Body) || (st.m_bWasControlled && ACR_Squad.FindController(st.m_Body) != st.m_iPlayerId))
			{
				SetBodyFaction(st.m_Body, st.m_sSide);
				s_States.RemoveOrdered(i);
				continue;
			}
			if (HasWeaponInHands(st.m_Body) || HasLongWeapon(st.m_Body))
			{
				Blow(st, "weapon drawn");
				continue;
			}
			if (InMilitaryVehicle(st.m_Body))
			{
				Blow(st, "entered a military or armed vehicle");
				continue;
			}
			vector pos = st.m_Body.GetOrigin();
			if (AtCheckpoint(pos, army) && HasContraband(st.m_Body))
			{
				Blow(st, "searched at a checkpoint with contraband");
				continue;
			}
			if (!army.IsEmpty() && manager && manager.m_Bases && manager.m_Bases.MenWithin(pos, LINGER_RADIUS, army) > 0)
			{
				st.m_fLingerMs += stepMs;
				if (st.m_fLingerMs >= LINGER_S * 1000)
				{
					Blow(st, "recognised up close by a soldier");
					continue;
				}
			}
			else
			{
				st.m_fLingerMs = 0;
			}
			// In a Loyal town, locals talk: rough "Stranger seen" feed line to the army.
			ACR_Base town = ACR_Muster.TownAt(pos);
			if (town && town.m_iSupportTier == ACR_TownSupport.LOYAL && now - st.m_fStrangerAt >= STRANGER_GAP_MS)
			{
				st.m_fStrangerAt = now;
				if (manager && !army.IsEmpty())
					manager.Command().Feed(army, string.Format("Locals report a stranger asking questions near %1", town.m_sName), town.GetOrigin());
			}
		}
	}

	static void AdvanceForTest(float seconds)
	{
		ACR_Manager manager = ACR_Manager.GetInstance();
		if (manager) Tick(manager, seconds * 1000);
	}
}
