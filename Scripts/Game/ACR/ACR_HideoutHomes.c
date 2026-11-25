// Hideouts as homes (0.8.0 #97 item 7) + guerrilla site dressing:
//  - Each standing hideout (ACR_DefenderPost with m_Cell.m_bHideout) is dressed with subtle
//    guerrilla props tucked against its buildings (Soviet ammo/shell crates, fuel barrel,
//    command table/map/radio/kerosene lamp/chair, FIA camo net, tires/woodpile).
//  - When the insurgent seat is open to players (ACR_InsurgentSide.PlayersAllowed), each
//    standing hideout gets a deploy spawn point (disabled while enemies are within
//    HIDEOUT_CLOSE unless it is the side's last open deploy point), and appears in the
//    insurgent commander's map feed with garrison, prisoners, stashed vehicles and
//    cold/warm/hot/found intel heat.
//  - Bands can buy/reinforce at hideouts (ACR_Requisition), set up new hideouts at isolated
//    building clusters (OrderSetup), and the commander can order a hideout to go to ground
//    (GoToGround: empties, refunds RP, removes spawn point, packs up active gear, and lets
//    army intel go stale).
class ACR_HideoutSetup : Managed
{
	ACR_Squad m_Squad;
	string m_sFaction;
	vector m_vSite;
	int m_iPaid;
	float m_fArrivedAt;
	bool m_bMoved;
}

class ACR_HideoutHomes
{
	static const int HIDEOUT_COST = 25;
	static const int GO_TO_GROUND_REFUND = 15;
	static const int MAX_HIDEOUTS = 6;
	static const float ARMY_CLEAR = 400;         // m from army soldiers when siting a new hideout
	static const float GO_TO_GROUND_CLEAR = 250; // m from army soldiers to pack up cleanly
	static const float SETUP_REACH = 40;         // m from site to begin setting up
	static const float SETUP_S = 15;             // s on site to finish setting up
	// Light guerrilla cache (FARP-lite): infantry rearm, basic medical, and light fuel/turret rearm
	// for returning bands and stashed vehicles. No full arsenal and no heavy armour repair.
	static const float CACHE_CAPACITY = 20;            // service loads per hideout
	static const float CACHE_RESTOCK_PER_TICK = 0.067; // ~1 load per 30 s while uncontested (~10 min full)
	static const float CACHE_SERVICE_RANGE = 35;       // m: covers buildings and vehicle stash (STASH_OFFSET 25 m)
	static const int CACHE_SERVICES = ACR_EService.AMMO | ACR_EService.FUEL | ACR_EService.MEDICAL;

	protected static ref map<ACR_DefenderPost, SCR_SpawnPoint> s_Points;
	protected static ref map<SCR_SpawnPoint, ACR_DefenderPost> s_ByPoint;
	protected static ref map<ACR_DefenderPost, ref array<IEntity>> s_Dressing;
	protected static ref map<ACR_DefenderPost, IEntity> s_CachePoint;
	protected static ref map<IEntity, ACR_DefenderPost> s_ByCachePoint;
	protected static ref map<ACR_DefenderPost, float> s_CacheStock;
	protected static ref array<ref ACR_HideoutSetup> s_Setups;

	// Called by ACR_StaticInit at game start.
	static void ACR_InitStatics()
	{
		s_Points = new map<ACR_DefenderPost, SCR_SpawnPoint>();
		s_ByPoint = new map<SCR_SpawnPoint, ACR_DefenderPost>();
		s_Dressing = new map<ACR_DefenderPost, ref array<IEntity>>();
		s_CachePoint = new map<ACR_DefenderPost, IEntity>();
		s_ByCachePoint = new map<IEntity, ACR_DefenderPost>();
		s_CacheStock = new map<ACR_DefenderPost, float>();
		s_Setups = {};
	}

	static void Reset()
	{
		if (s_Points)
		{
			foreach (ACR_DefenderPost post, SCR_SpawnPoint point : s_Points)
				if (point) SCR_EntityHelper.DeleteEntityAndChildren(point);
			s_Points.Clear();
		}
		if (s_ByPoint) s_ByPoint.Clear();
		ACR_Manager manager = ACR_Manager.GetInstance();
		if (s_CachePoint)
		{
			if (manager && manager.m_Logistics)
			{
				foreach (ACR_DefenderPost cpPost, IEntity cpEnt : s_CachePoint)
					if (cpEnt) manager.m_Logistics.RemovePoint(cpEnt);
			}
			s_CachePoint.Clear();
		}
		if (s_ByCachePoint) s_ByCachePoint.Clear();
		if (s_CacheStock) s_CacheStock.Clear();
		if (s_Dressing)
		{
			foreach (ACR_DefenderPost dp, array<IEntity> props : s_Dressing)
			{
				if (!props) continue;
				foreach (IEntity prop : props) if (prop) SCR_EntityHelper.DeleteEntityAndChildren(prop);
			}
			s_Dressing.Clear();
		}
		if (s_Setups) s_Setups.Clear();
	}

	static bool IsHideoutPoint(SCR_SpawnPoint point)
	{
		return point && s_ByPoint && s_ByPoint.Contains(point);
	}

	static int MaxHideouts()
	{
		return Math.Round(MAX_HIDEOUTS * ACR_Coin.CapScale());
	}

	// ---------------------------------------------------------------- site dressing

	// Subtle guerrilla site dressing tucked beside the hideout's buildings (off roads):
	// ammo/shell crates, fuel barrel, command table + radio + map + lamp + chair, FIA camo net,
	// and a small tire/wood pile. Stored per hideout so GoToGround can pack up the active gear.
	static void DressHideout(ACR_DefenderPost post)
	{
		if (!Replication.IsServer() || !post || post.m_bGone) return;
		if (s_Dressing.Contains(post)) return;
		array<IEntity> spawned = {};
		array<ResourceName> activeGear = {
			ACR_BaseDefences.CRATES_EAST,
			ACR_BaseDefences.SHELLS,
			ACR_BaseDefences.BARREL,
			ACR_BaseDefences.TABLE_EAST,
			ACR_BaseDefences.CHAIR_EAST,
			ACR_BaseDefences.RADIO_EAST,
			ACR_BaseDefences.MAP_EAST,
			ACR_BaseDefences.LAMP_EAST,
			ACR_BaseDefences.NET_FIA
		};
		array<ResourceName> clutter = {
			ACR_BaseDefences.PALLET,
			ACR_BaseDefences.TIRES,
			ACR_BaseDefences.LOG
		};
		// Anchor props around the central building cluster, clear of roads and water.
		vector center = post.m_vPos;
		int placed;
		IEntity cacheAnchor;
		foreach (int i, ResourceName prefab : activeGear)
		{
			int ring = i % 3;
			IEntity prop = SpawnPropNear(prefab, center, 4 + ring * 4, 18, i * 0.7);
			if (prop)
			{
				spawned.Insert(prop);
				placed++;
				if (!cacheAnchor) cacheAnchor = prop;
			}
		}
		foreach (int j, ResourceName cPrefab : clutter)
		{
			if (j >= 2) break;
			IEntity cProp = SpawnPropNear(cPrefab, center, 10 + j * 5, 24, 2.0 + j * 1.4);
			if (cProp) { spawned.Insert(cProp); placed++; }
		}
		s_Dressing.Set(post, spawned);
		if (cacheAnchor) RegisterCache(post, cacheAnchor);
		PrintFormat("[ACR] HIDEOUT DRESS %1: %2 props placed", post.m_sName, placed);
	}

	static void CleanDressing(ACR_DefenderPost post, bool leaveColdClutter = false)
	{
		if (!post) return;
		UnregisterCache(post);
		if (!s_Dressing) return;
		array<IEntity> props;
		if (!s_Dressing.Find(post, props)) return;
		int keep = 0;
		if (leaveColdClutter && props && props.Count() > 2) keep = 1;
		if (props)
		{
			for (int i = 0; i < props.Count() - keep; i++)
				if (props[i]) SCR_EntityHelper.DeleteEntityAndChildren(props[i]);
		}
		s_Dressing.Remove(post);
	}

	// ---------------------------------------------------------------- light supply cache

	protected static void RegisterCache(ACR_DefenderPost post, IEntity anchor)
	{
		if (!post || !anchor || !s_CachePoint) return;
		s_CachePoint.Set(post, anchor);
		s_ByCachePoint.Set(anchor, post);
		s_CacheStock.Set(post, CACHE_CAPACITY);
		ACR_Manager manager = ACR_Manager.GetInstance();
		if (manager && manager.m_Logistics)
			manager.m_Logistics.AddPoint(anchor, CACHE_SERVICES, post.m_sFaction);
	}

	static void UnregisterCache(ACR_DefenderPost post)
	{
		if (!post || !s_CachePoint) return;
		IEntity anchor;
		if (s_CachePoint.Find(post, anchor))
		{
			ACR_Manager manager = ACR_Manager.GetInstance();
			if (anchor && manager && manager.m_Logistics) manager.m_Logistics.RemovePoint(anchor);
			if (anchor && s_ByCachePoint) s_ByCachePoint.Remove(anchor);
			s_CachePoint.Remove(post);
		}
		if (s_CacheStock) s_CacheStock.Remove(post);
	}

	// Called every second from TickSetups: registers starting hideouts once m_Logistics is
	// created on tick 1, and prunes destroyed hideouts.
	static void SyncCachePoints(ACR_Manager manager)
	{
		if (!Replication.IsServer() || !manager || !manager.m_Logistics || !s_CachePoint) return;
		array<ACR_DefenderPost> gone = {};
		foreach (ACR_DefenderPost post, IEntity anchor : s_CachePoint)
		{
			if (!post || post.m_bGone || !anchor) { gone.Insert(post); continue; }
			if (!manager.m_Logistics.IsPoint(anchor))
				manager.m_Logistics.AddPoint(anchor, CACHE_SERVICES, post.m_sFaction);
		}
		foreach (ACR_DefenderPost dead : gone) UnregisterCache(dead);
	}

	static IEntity CachePointOf(ACR_DefenderPost post)
	{
		if (!post || !s_CachePoint) return null;
		IEntity anchor;
		s_CachePoint.Find(post, anchor);
		return anchor;
	}

	static ACR_DefenderPost HideoutOfPoint(IEntity point)
	{
		if (!point || !s_ByCachePoint) return null;
		ACR_DefenderPost post;
		s_ByCachePoint.Find(point, post);
		return post;
	}

	static float CacheStock(ACR_DefenderPost post)
	{
		if (!post || post.m_bGone || Contested(post) || !s_CacheStock) return 0;
		float stock;
		if (!s_CacheStock.Find(post, stock)) return 0;
		return stock;
	}

	static bool UseCache(ACR_DefenderPost post, float amount)
	{
		if (amount <= 0) return true;
		float stock = CacheStock(post);
		if (stock < amount) return false;
		s_CacheStock.Set(post, Math.Max(0, stock - amount));
		return true;
	}

	static void RestockCache(ACR_DefenderPost post, float amount = CACHE_RESTOCK_PER_TICK)
	{
		if (!post || post.m_bGone || Contested(post) || amount <= 0 || !s_CacheStock) return;
		float stock;
		if (!s_CacheStock.Find(post, stock)) return;
		if (stock >= CACHE_CAPACITY) return;
		s_CacheStock.Set(post, Math.Min(CACHE_CAPACITY, stock + amount));
	}

	static int DressingCount(ACR_DefenderPost post)
	{
		if (!post || !s_Dressing) return 0;
		array<IEntity> props;
		if (!s_Dressing.Find(post, props) || !props) return 0;
		int count;
		foreach (IEntity p : props) if (p) count++;
		return count;
	}

	protected static IEntity SpawnPropNear(ResourceName prefab, vector center, float minR, float maxR, float angleHint)
	{
		Resource res = Resource.Load(prefab);
		if (!res || !res.IsValid()) return null;
		for (int attempt = 0; attempt < 5; attempt++)
		{
			float a = angleHint + attempt * 1.1;
			float d = Math.RandomFloat(minR, maxR);
			vector cand = ACR_Manager.Ground(center + Vector(Math.Cos(a) * d, 0, Math.Sin(a) * d));
			if (BBB_Terrain.IsWaterAt(cand)) continue;
			if (ACR_Minefields.RoadClearance(cand) < 8) continue;
			vector free;
			if (!ACR_BaseSystem.ClearGround(free, cand, 5, 1.2)) continue;
			EntitySpawnParams sp = new EntitySpawnParams();
			sp.TransformMode = ETransformMode.WORLD;
			Math3D.AnglesToMatrix(Vector(Math.RandomFloat(0, 360), 0, 0), sp.Transform);
			sp.Transform[3] = free;
			IEntity ent = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), sp);
			if (ent) return ent;
		}
		return null;
	}

	// ---------------------------------------------------------------- lookup & status

	static ACR_DefenderPost FindByLabel(string label)
	{
		if (label.IsEmpty()) return null;
		array<ACR_DefenderPost> hideouts = {};
		ACR_Guerrillas.Hideouts(hideouts);
		foreach (ACR_DefenderPost post : hideouts)
			if (post.m_sName == label) return post;
		return null;
	}

	static bool Contested(ACR_DefenderPost post)
	{
		if (!post || post.m_bGone) return true;
		ACR_Manager manager = ACR_Manager.GetInstance();
		if (!manager || !manager.m_Bases) return false;
		string army = ACR_Coin.Army();
		if (army.IsEmpty()) return false;
		return manager.m_Bases.MenWithin(post.m_vPos, ACR_Guerrillas.HIDEOUT_CLOSE, army) > 0;
	}

	static ACR_DefenderPost NearestHome(vector pos, bool allowContested = false)
	{
		array<ACR_DefenderPost> hideouts = {};
		ACR_Guerrillas.Hideouts(hideouts);
		ACR_DefenderPost best;
		float bestDist = float.MAX;
		foreach (ACR_DefenderPost post : hideouts)
		{
			if (!allowContested && Contested(post)) continue;
			float d = vector.DistanceXZ(post.m_vPos, pos);
			if (d < bestDist) { bestDist = d; best = post; }
		}
		return best;
	}

	// ---------------------------------------------------------------- deploy spawn points

	// Called every second from ACR_SpawnPoints.Tick.
	static void TickSpawnPoints(ACR_Manager manager)
	{
		if (!Replication.IsServer() || !ACR_InsurgentSide.PlayersAllowed()) return;
		string key = ACR_InsurgentSide.Key();
		array<ACR_DefenderPost> hideouts = {};
		ACR_Guerrillas.Hideouts(hideouts);
		// Prune spawn points for destroyed / gone-to-ground hideouts.
		array<ACR_DefenderPost> gone = {};
		foreach (ACR_DefenderPost post, SCR_SpawnPoint point : s_Points)
			if (!post || post.m_bGone || !point || !hideouts.Contains(post)) gone.Insert(post);
		foreach (ACR_DefenderPost dead : gone)
		{
			SCR_SpawnPoint sp = s_Points.Get(dead);
			if (sp) SCR_EntityHelper.DeleteEntityAndChildren(sp);
			s_Points.Remove(dead);
		}
		s_ByPoint.Clear();
		foreach (ACR_DefenderPost owner, SCR_SpawnPoint own : s_Points) s_ByPoint.Set(own, owner);

		int openCount;
		SCR_SpawnPoint fallback;
		foreach (ACR_DefenderPost h : hideouts)
		{
			SCR_SpawnPoint pt;
			s_Points.Find(h, pt);
			if (!pt)
			{
				vector at = ACR_Manager.Ground(h.m_vPos);
				vector free;
				if (ACR_BaseSystem.ClearGround(free, at, 12, 1.5)) at = free;
				pt = SCR_SpawnPoint.Cast(ACR_Manager.SpawnPrefab(ACR_SpawnPoints.PREFAB, at));
				if (!pt) continue;
				pt.SetFactionKey(key);
				pt.SetSpawnPointName(h.m_sName);
				pt.SetPriority(ACR_SpawnPoints.PRIORITY_FOB);
				s_Points.Set(h, pt);
				s_ByPoint.Set(pt, h);
				PrintFormat("[ACR] HIDEOUT SPAWN %1 for %2 at %3", h.m_sName, key, at);
			}
			if (pt.GetFactionKey() != key) pt.SetFactionKey(key);
			bool enable = !Contested(h);
			pt.SetSpawnPointEnabled_S(enable);
			if (enable) openCount++;
			if (!fallback) fallback = pt;
		}
		// Never leave insurgent players with zero open spawn points while a hideout stands.
		if (openCount == 0 && fallback) fallback.SetSpawnPointEnabled_S(true);
	}

	// ---------------------------------------------------------------- map feed

	// Called from ACR_MapFeed.BuildBases: projects standing hideouts onto the insurgent
	// commander's map as selectable green homes with cold/warm/hot/found intel readout.
	static void AppendBases(ACR_Manager manager, ACR_FeedFrame frame, inout string sig, inout int bytes)
	{
		if (!ACR_InsurgentSide.IsInsurgent(frame.m_sKey)) return;
		string army = ACR_Coin.Army();
		array<ACR_DefenderPost> hideouts = {};
		ACR_Guerrillas.Hideouts(hideouts);
		foreach (ACR_DefenderPost post : hideouts)
		{
			int men = post.m_iReserve;
			if (post.m_Group && manager && manager.m_Bases) men += manager.m_Bases.GuardsAlive(post.m_Group);
			string heat = ACR_FIAIntel.Heat(post, army);
			string detail = string.Format("Hideout | garrison: %1 | intel: %2", men, heat);
			int prisoners = ACR_FIAPrisoners.HeldAt(post);
			if (prisoners > 0) detail += string.Format(" | prisoners: %1", prisoners);
			if (s_CacheStock && s_CacheStock.Contains(post))
				detail += string.Format(" | cache: %1%%", (int)Math.Round(100.0 * s_CacheStock.Get(post) / CACHE_CAPACITY));
			bool contested = Contested(post);
			if (contested) detail += " | CONTESTED";
			frame.m_BasePos.Insert(post.m_vPos);
			frame.m_BaseNames.Insert(post.m_sName);
			frame.m_BaseOwners.Insert(frame.m_sKey);
			frame.m_BaseDetails.Insert(detail);
			frame.m_BaseRadii.Insert(ACR_Guerrillas.HIDEOUT_AREA);
			int flag = 0;
			if (contested) flag = 1;
			frame.m_BaseContested.Insert(flag);
			frame.m_BaseSupply.Insert(-1);
			frame.m_BaseSupply.Insert(0);
			frame.m_BaseSupply.Insert(0);
			for (int s = 0; s < ACR_FobSlots.UPGRADE_COUNT; s++) frame.m_BaseSupply.Insert(-1);
			sig += post.m_sName + "|" + frame.m_sKey + "|" + detail + "|" + flag.ToString() + ";";
			bytes += 12 + post.m_sName.Length() + frame.m_sKey.Length() + detail.Length() + 4 * (8 + ACR_FobSlots.UPGRADE_COUNT);
		}
	}

	// ---------------------------------------------------------------- set up hideout

	static bool OrderSetup(ACR_Squad squad, vector spot, out string refusal)
	{
		refusal = "";
		if (!squad || !squad.GetFaction() || !ACR_InsurgentSide.IsInsurgent(squad.GetFaction().GetFactionKey()))
		{
			refusal = "only an insurgent band can set up a hideout";
			return false;
		}
		if (squad.CountAlive() < 2 || !ACR_Fortify.Infantry(squad))
		{
			refusal = "needs at least 2 infantry fighters on foot";
			return false;
		}
		array<ACR_DefenderPost> existing = {};
		ACR_Guerrillas.Hideouts(existing);
		if (existing.Count() + s_Setups.Count() >= MaxHideouts())
		{
			refusal = string.Format("at the hideout limit (%1)", MaxHideouts());
			return false;
		}
		vector middle;
		if (!ACR_Guerrillas.CanSiteHideout(spot, middle, refusal, ACR_Coin.Army())) return false;
		ACR_Manager manager = ACR_Manager.GetInstance();
		string key = squad.GetFaction().GetFactionKey();
		if (!manager || !manager.m_Bases || !manager.m_Bases.Spend(key, HIDEOUT_COST))
		{
			refusal = string.Format("not enough RP (%1 needed)", HIDEOUT_COST);
			return false;
		}
		CancelSetup(squad, true);
		ACR_HideoutSetup setup = new ACR_HideoutSetup();
		setup.m_Squad = squad;
		setup.m_sFaction = key;
		setup.m_vSite = middle;
		setup.m_iPaid = HIDEOUT_COST;
		s_Setups.Insert(setup);
		squad.RouteTo(middle, ACR_Squad.WP_MOVE, -1, "Set up hideout", ACR_EOrderKind.MOVE);
		setup.m_bMoved = true;
		PrintFormat("[ACR] HIDEOUT SETUP %1 ordered by %2 at %3 (-%4 RP)", key, squad.GetName(), middle, HIDEOUT_COST);
		return true;
	}

	static void CancelSetup(ACR_Squad squad, bool refund)
	{
		if (!squad || !s_Setups) return;
		ACR_Manager manager = ACR_Manager.GetInstance();
		for (int i = s_Setups.Count() - 1; i >= 0; i--)
		{
			ACR_HideoutSetup s = s_Setups[i];
			if (s.m_Squad != squad) continue;
			if (refund && s.m_iPaid > 0 && manager && manager.m_Bases) manager.m_Bases.Refund(s.m_sFaction, s.m_iPaid);
			s_Setups.RemoveOrdered(i);
		}
	}

	static bool IsSettingUp(ACR_Squad squad)
	{
		if (!squad || !s_Setups) return false;
		foreach (ACR_HideoutSetup s : s_Setups)
			if (s && s.m_Squad == squad) return true;
		return false;
	}

	// Called every second from ACR_Manager.Tick.
	static void TickSetups(ACR_Manager manager)
	{
		if (!Replication.IsServer()) return;
		SyncCachePoints(manager);
		if (!s_Setups || s_Setups.IsEmpty()) return;
		float now = GetGame().GetWorld().GetWorldTime();
		string army = ACR_Coin.Army();
		for (int i = s_Setups.Count() - 1; i >= 0; i--)
		{
			ACR_HideoutSetup s = s_Setups[i];
			if (!s.m_Squad || s.m_Squad.CountAlive() < 2)
			{
				if (s.m_iPaid > 0 && manager && manager.m_Bases) manager.m_Bases.Refund(s.m_sFaction, s.m_iPaid);
				s_Setups.RemoveOrdered(i);
				continue;
			}
			IEntity anchor = s.m_Squad.GetAnchor();
			if (!anchor) continue;
			float dist = vector.DistanceXZ(anchor.GetOrigin(), s.m_vSite);
			bool enemiesNear = !army.IsEmpty() && manager && manager.m_Bases && manager.m_Bases.MenWithin(s.m_vSite, ACR_Guerrillas.HIDEOUT_CLOSE, army) > 0;
			if (dist > SETUP_REACH || enemiesNear)
			{
				s.m_fArrivedAt = 0;
				continue;
			}
			if (s.m_fArrivedAt <= 0) { s.m_fArrivedAt = now; continue; }
			if (now - s.m_fArrivedAt < SETUP_S * 1000) continue;
			ACR_DefenderPost created = ACR_Guerrillas.AddHideout(s.m_vSite);
			s_Setups.RemoveOrdered(i);
			if (created && manager)
			{
				string line = string.Format("New hideout set up at %1", ACR_Manager.Grid(created.m_vPos));
				manager.Command().Feed(s.m_sFaction, line);
				PrintFormat("[ACR] HIDEOUT SETUP complete: %1", created.m_sName);
			}
		}
	}

	// ---------------------------------------------------------------- go to ground

	// Empties a hideout, removes its deploy point, packs up active dressing, refunds part of
	// its cost, and lets army intel on it go stale without alerting the army.
	static bool GoToGround(string factionKey, string hideoutLabel, out string refusal)
	{
		refusal = "";
		if (!ACR_InsurgentSide.IsInsurgent(factionKey))
		{
			refusal = "only the insurgent side can go to ground";
			return false;
		}
		ACR_DefenderPost post = FindByLabel(hideoutLabel);
		if (!post || post.m_bGone)
		{
			refusal = "hideout not found";
			return false;
		}
		ACR_Manager manager = ACR_Manager.GetInstance();
		string army = ACR_Coin.Army();
		if (!army.IsEmpty() && manager && manager.m_Bases && manager.m_Bases.MenWithin(post.m_vPos, GO_TO_GROUND_CLEAR, army) > 0)
		{
			refusal = "enemies are too close to slip away";
			return false;
		}
		// Release any prisoners held here cleanly.
		ACR_FIAPrisoners.HideoutDestroyed(post);
		// Pack up any deployed garrison.
		if (post.m_Group && manager && manager.m_Bases)
		{
			manager.m_Bases.PackUpGuards(post.m_Group);
			post.m_Group = null;
		}
		post.m_iReserve = 0;
		post.m_bGone = true;
		// Remove its spawn point and active dressing immediately.
		SCR_SpawnPoint sp;
		if (s_Points.Find(post, sp))
		{
			if (sp) SCR_EntityHelper.DeleteEntityAndChildren(sp);
			s_Points.Remove(post);
			s_ByPoint.Remove(sp);
		}
		CleanDressing(post, true);
		ACR_FIAIntel.GoStale(post);
		if (manager && manager.m_Bases)
		{
			manager.m_Bases.Refund(factionKey, GO_TO_GROUND_REFUND);
			manager.Command().Feed(factionKey, string.Format("%1 went to ground (+%2 RP)", post.m_sName, GO_TO_GROUND_REFUND));
		}
		PrintFormat("[ACR] HIDEOUT %1 went to ground (+%2 RP to %3)", post.m_sName, GO_TO_GROUND_REFUND, factionKey);
		return true;
	}

	// ---------------------------------------------------------------- hand-fired IED kit (#97 / #244 ruling 5)

	static const int IED_KIT_COST = 5;
	static const float IED_KIT_REACH = 40; // m from a standing hideout

	// Gives an insurgent player a vanilla explosive charge + blasting machine from a hideout.
	static bool GiveIedKit(IEntity body, string factionKey, out string refusal, bool skipDistForTest = false)
	{
		refusal = "";
		if (!ACR_InsurgentSide.IsInsurgent(factionKey)) { refusal = "only insurgents can draw an IED kit"; return false; }
		if (!body || !ACR_Respawn.IsAlive(body)) { refusal = "must be alive to draw an IED kit"; return false; }
		if (!skipDistForTest)
		{
			ACR_DefenderPost near = NearestHome(body.GetOrigin(), true);
			if (!near || vector.DistanceXZ(near.m_vPos, body.GetOrigin()) > IED_KIT_REACH)
			{
				refusal = "stand within 40 m of a hideout to draw an IED kit";
				return false;
			}
		}
		InventoryStorageManagerComponent inv = InventoryStorageManagerComponent.Cast(body.FindComponent(InventoryStorageManagerComponent));
		if (!inv) { refusal = "no inventory available"; return false; }
		ACR_Manager manager = ACR_Manager.GetInstance();
		if (!manager || !manager.m_Bases || !manager.m_Bases.Spend(factionKey, IED_KIT_COST))
		{
			refusal = string.Format("not enough RP (%1 needed)", IED_KIT_COST);
			return false;
		}
		bool okCharge = inv.TrySpawnPrefabToStorage(ACR_Demolition.ChargeFor(factionKey));
		bool okDet = inv.TrySpawnPrefabToStorage(ACR_Demolition.DetonatorFor(factionKey));
		if (!okCharge || !okDet)
		{
			manager.m_Bases.Refund(factionKey, IED_KIT_COST);
			refusal = "not enough room in your inventory for the charge and blasting machine";
			return false;
		}
		PrintFormat("[ACR] HIDEOUT IED KIT drawn by %1 (-%2 RP)", factionKey, IED_KIT_COST);
		return true;
	}
}
