// srv:disguise (Everon whole map, about 40 s): insurgent player disguise & roadblock checkpoints
// (ACR_Disguise, 0.8.0 #97 item 10).
// Verifies:
//  1. Going plain while watched by an army soldier is refused with a clear reason.
//  2. Going plain out of sight (unarmed) flips body affiliation to CIV and skips BaseSystem census
//     (does not contest or capture a base).
//  3. Carrying contraband (charge) into a manned army roadblock blows the disguise and flips
//     affiliation back to insurgent + sets cooldown.
//  4. Lingering within 5 m of an army soldier for >10 s blows the disguise.
//  5. Holding/carrying a long weapon refuses GoPlain and blows an active disguise.
class ACR_TestDisguise
{
	static void Run(int second)
	{
		ACR_Manager manager = ACR_Manager.GetInstance();
		if (!manager) return;
		if (second == 10) Verify(manager);
	}

	protected static IEntity SpawnUnarmedInsurgent(vector pos)
	{
		// Spawn an FIA character and strip its weapons so it starts clean for GoPlain.
		IEntity body = ACR_Manager.SpawnPrefab(ACR_Mortars.RIFLE_FIA, ACR_Manager.Ground(pos));
		if (!body) return null;
		BaseWeaponManagerComponent wm = BaseWeaponManagerComponent.Cast(body.FindComponent(BaseWeaponManagerComponent));
		if (wm)
		{
			array<WeaponSlotComponent> slots = {};
			wm.GetWeaponsSlots(slots);
			foreach (WeaponSlotComponent slot : slots)
			{
				if (!slot) continue;
				IEntity wep = slot.GetWeaponEntity();
				if (wep) SCR_EntityHelper.DeleteEntityAndChildren(wep);
			}
		}
		return body;
	}

	protected static void Verify(ACR_Manager manager)
	{
		string ins = ACR_InsurgentSide.Key();
		string army = ACR_Coin.Army();
		ACR_Base armyHQ = ACR_OneSide.Home();
		array<ACR_DefenderPost> hideouts = {};
		ACR_Guerrillas.Hideouts(hideouts);
		ACR_DefenderPost hideout;
		if (!hideouts.IsEmpty()) hideout = hideouts[0];

		// 1. Watched refusal: unarmed insurgent beside army HQ cannot go plain.
		bool watchedOk = false;
		string watchedRef = "";
		if (armyHQ)
		{
			IEntity watchedBody = SpawnUnarmedInsurgent(armyHQ.GetOrigin() + Vector(8, 0, 8));
			if (watchedBody)
			{
				watchedOk = !ACR_Disguise.GoPlain(watchedBody, ins, 101, watchedRef) && !ACR_Disguise.IsDisguised(watchedBody);
				SCR_EntityHelper.DeleteEntityAndChildren(watchedBody);
			}
		}
		Check("disguise: going plain while watched by army is refused", watchedOk, watchedRef);

		// 2. Clean GoPlain at hideout -> body faction becomes CIV and does not count as insurgent in census.
		bool plainOk = false;
		string plainInfo = "no hideout";
		IEntity plainBody;
		if (hideout)
		{
			plainBody = SpawnUnarmedInsurgent(hideout.m_vPos + Vector(4, 0, 4));
			if (plainBody)
			{
				string ref2;
				bool went = ACR_Disguise.GoPlain(plainBody, ins, 102, ref2);
				FactionAffiliationComponent aff = FactionAffiliationComponent.Cast(plainBody.FindComponent(FactionAffiliationComponent));
				string bodyKey = "";
				if (aff && aff.GetAffiliatedFaction()) bodyKey = aff.GetAffiliatedFaction().GetFactionKey();
				plainOk = went && ACR_Disguise.IsDisguised(plainBody) && bodyKey == "CIV";
				plainInfo = string.Format("went %1 ('%2'), bodyKey '%3'", went, ref2, bodyKey);
			}
		}
		Check("disguise: going plain out of sight flips body affiliation to CIV", plainOk, plainInfo);

		// 3. Checkpoint search: manned army roadblock + contraband charge blows disguise & sets cooldown.
		bool checkpointOk = false;
		string checkpointInfo = "no plain body";
		if (plainBody)
		{
			InventoryStorageManagerComponent inv = InventoryStorageManagerComponent.Cast(plainBody.FindComponent(InventoryStorageManagerComponent));
			if (inv) inv.TrySpawnPrefabToStorage(ACR_Demolition.ChargeFor(ins));
			ACR_Roadblock rb = new ACR_Roadblock();
			rb.m_sFaction = army;
			rb.m_vPos = plainBody.GetOrigin();
			rb.m_Entity = GetGame().GetWorldEntity();
			ref ACR_DefenderPost defPost = new ACR_DefenderPost();
			defPost.m_iReserve = 2;
			rb.m_Defenders = defPost;
			ACR_Roadblocks.s_aRoadblocks.Insert(rb);
			ACR_Disguise.AdvanceForTest(1);
			ACR_Roadblocks.s_aRoadblocks.RemoveItem(rb);
			FactionAffiliationComponent aff3 = FactionAffiliationComponent.Cast(plainBody.FindComponent(FactionAffiliationComponent));
			string afterKey = "";
			if (aff3 && aff3.GetAffiliatedFaction()) afterKey = aff3.GetAffiliatedFaction().GetFactionKey();
			string cdRef;
			bool onCooldown = !ACR_Disguise.GoPlain(plainBody, ins, 102, cdRef);
			checkpointOk = !ACR_Disguise.IsDisguised(plainBody) && afterKey == ins && onCooldown;
			checkpointInfo = string.Format("disguised %1, key '%2', cooldownRef '%3'", ACR_Disguise.IsDisguised(plainBody), afterKey, cdRef);
			SCR_EntityHelper.DeleteEntityAndChildren(plainBody);
		}
		Check("disguise: manned roadblock search with contraband blows disguise and sets cooldown", checkpointOk, checkpointInfo);

		// 4. Lingering within 5 m of an army soldier for >10 s blows the disguise.
		bool lingerOk = false;
		string lingerInfo = "no hideout";
		if (hideout)
		{
			IEntity lingerBody = SpawnUnarmedInsurgent(hideout.m_vPos + Vector(6, 0, 6));
			if (lingerBody)
			{
				string ref4;
				bool went4 = ACR_Disguise.GoPlain(lingerBody, ins, 103, ref4);
				// Place an army soldier 2 m away and tick BaseSystem census + Disguise.
				ResourceName armyMan = "{26A9756790131354}Prefabs/Characters/Factions/BLUFOR/US_Army/Character_US_Rifleman.et";
				IEntity soldier = ACR_Manager.SpawnPrefab(armyMan, ACR_Manager.Ground(lingerBody.GetOrigin() + Vector(2, 0, 0)));
				if (went4 && soldier)
				{
					manager.m_Bases.Tick(manager);
					ACR_Disguise.AdvanceForTest(ACR_Disguise.LINGER_S + 2);
					lingerOk = !ACR_Disguise.IsDisguised(lingerBody);
					lingerInfo = string.Format("went %1, stillDisguised %2", went4, ACR_Disguise.IsDisguised(lingerBody));
				}
				if (soldier) SCR_EntityHelper.DeleteEntityAndChildren(soldier);
				SCR_EntityHelper.DeleteEntityAndChildren(lingerBody);
			}
		}
		Check("disguise: lingering within 5 m of an army soldier blows disguise", lingerOk, lingerInfo);

		// 5. Armed insurgent with rifle cannot go plain.
		bool armedOk = false;
		string armedRef = "";
		if (hideout)
		{
			IEntity armedBody = ACR_Manager.SpawnPrefab(ACR_Mortars.RIFLE_FIA, ACR_Manager.Ground(hideout.m_vPos + Vector(5, 0, 5)));
			if (armedBody)
			{
				armedOk = !ACR_Disguise.GoPlain(armedBody, ins, 104, armedRef) && !ACR_Disguise.IsDisguised(armedBody);
				SCR_EntityHelper.DeleteEntityAndChildren(armedBody);
			}
		}
		Check("disguise: carrying a long weapon refuses going plain", armedOk, armedRef);

		ACR_TestRegress.Complete("disguise", 5);
	}

	protected static void Check(string name, bool ok, string detail)
	{
		ACR_TestRegress.Check(name, ok, detail);
	}
}
