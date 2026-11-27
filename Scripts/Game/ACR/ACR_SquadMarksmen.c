// One DMR carrier per full UA/Russian rifle squad. Keep character templates and
// the logical roster unchanged, so reinforcement and support roles still work.
class ACR_MarksmanSwap : Managed
{
	ACR_Squad m_Squad;
	SCR_AIGroup m_Group;
	string m_Name;
	IEntity m_Soldier;
	IEntity m_Old;
	IEntity m_New;
	float m_Deadline;
	ref array<typename> m_OldWells = {};

	void Poll()
	{
		if (!m_Squad || !m_Squad.m_Group || !ACR_Respawn.IsAlive(m_Soldier) || !m_New) { Finish(false); return; }
		SCR_InventoryStorageManagerComponent inventory = SCR_InventoryStorageManagerComponent.Cast(m_Soldier.FindComponent(SCR_InventoryStorageManagerComponent));
		ChimeraCharacter ch = ChimeraCharacter.Cast(m_Soldier);
		CharacterControllerComponent controller;
		if (ch) controller = ch.GetCharacterController();
		if (!controller) { Finish(false); return; }
		BaseWeaponManagerComponent weapons = controller.GetWeaponManagerComponent();
		array<IEntity> carried = {};
		if (weapons) weapons.GetWeaponsList(carried);
		if (inventory && carried.Contains(m_New) && !controller.IsChangingItem())
		{
			// Do not delete a weapon while the controller still has it in hand.
			if (controller.GetCurrentItemInHands() == m_Old)
			{
				if (!controller.IsPlayerControlled()) controller.TryEquipRightHandItem(m_New, EEquipItemType.EEquipTypeWeapon, false);
			}
			else
			{
				array<IEntity> items = {};
				inventory.GetItems(items);
				foreach (IEntity item : items)
				{
					if (!item) continue;
					BaseMagazineComponent magazine = BaseMagazineComponent.Cast(item.FindComponent(BaseMagazineComponent));
					if (!magazine || !magazine.GetMagazineWell()) continue;
					bool obsolete;
					foreach (typename well : m_OldWells)
						if (magazine.GetMagazineWell().Type().IsInherited(well)) obsolete = true;
					if (obsolete) inventory.TryDeleteItem(item);
				}
				if (m_Old && (inventory.Contains(m_Old) || ACR_SquadMarksmen.Loose(m_Old))) SCR_EntityHelper.DeleteEntityAndChildren(m_Old);
				BaseWeaponComponent primary = BaseWeaponComponent.Cast(m_New.FindComponent(BaseWeaponComponent));
				array<BaseMuzzleComponent> muzzles = {};
				if (primary) primary.GetMuzzlesList(muzzles);
				if (!muzzles.IsEmpty())
				{
					ResourceName ammo = muzzles[0].GetDefaultMagazineOrProjectileName();
					int count = ACR_UALoadout.CountPrefab(inventory, ammo);
					for (int n = count; n < 5; n++)
						if (!ACR_UALoadout.SpawnCarried(inventory, ammo)) break;
				}
				Finish(true);
				return;
			}
		}
		if (GetGame().GetWorld().GetWorldTime() >= m_Deadline) { Finish(false); return; }
		GetGame().GetCallqueue().CallLater(Poll, 250);
	}

	void Finish(bool success)
	{
		if (!success && m_New && ACR_SquadMarksmen.Loose(m_New)) SCR_EntityHelper.DeleteEntityAndChildren(m_New);
		if (success && m_Squad && m_Squad.m_Group)
		{
			ACR_SquadMarksmen.s_Assigned.Set(m_Squad.m_Group, m_Soldier);
			PrintFormat("[ACR] MARKSMAN %1: %2 carries %3", m_Name, ACR_UALoadout.Leaf(m_Soldier), ACR_UALoadout.Leaf(m_New));
		}
		else PrintFormat("[ACR] MARKSMAN %1: replacement not confirmed; old weapon not deleted", m_Name);
		ACR_SquadMarksmen.s_Pending.Remove(m_Group);
	}
}

class ACR_SquadMarksmen
{
	static ref map<SCR_AIGroup, IEntity> s_Assigned;
	static ref map<SCR_AIGroup, ref ACR_MarksmanSwap> s_Pending;
	protected static ref map<SCR_AIGroup, float> s_Retry;
	static bool s_bOn = true;
	protected static bool s_bRead;
	protected static ref set<SCR_AIGroup> s_Diagnosed;
	// Called by ACR_StaticInit at game start (static initializer budget: HANDBOOK Pitfalls).
	static void ACR_InitStatics()
	{
		s_Assigned = new map<SCR_AIGroup, IEntity>();
		s_Pending = new map<SCR_AIGroup, ref ACR_MarksmanSwap>();
		s_Retry = new map<SCR_AIGroup, float>();
		s_Diagnosed = new set<SCR_AIGroup>();
	}

	static bool Loose(IEntity item)
	{
		if (!item || item.GetRootParent() != item) return false;
		InventoryItemComponent component = InventoryItemComponent.Cast(item.FindComponent(InventoryItemComponent));
		return component && !component.GetParentSlot();
	}

	static void Reset()
	{
		for (int i = 0; i < s_Pending.Count(); i++)
		{
			ACR_MarksmanSwap pending = s_Pending.GetElement(i);
			if (pending) GetGame().GetCallqueue().Remove(pending.Poll);
		}
		s_Pending.Clear(); s_Assigned.Clear(); s_Retry.Clear(); s_Diagnosed.Clear();
		// -acrMarksmen is read again: a scenario that turned them off no longer did so for every later one.
		s_bRead = false; s_bOn = true;
	}

	static bool IsDMR(IEntity item)
	{
		string name = ACR_UALoadout.Leaf(item);
		return name.Contains("SVD") || name.Contains("M21") || name.Contains("M38SDMR") || name.Contains("M110");
	}

	static IEntity Primary(IEntity soldier)
	{
		BaseWeaponManagerComponent weapons = BaseWeaponManagerComponent.Cast(soldier.FindComponent(BaseWeaponManagerComponent));
		if (!weapons) return null;
		array<IEntity> carried = {};
		weapons.GetWeaponsList(carried);
		foreach (IEntity item : carried)
		{
			BaseWeaponComponent weapon = BaseWeaponComponent.Cast(item.FindComponent(BaseWeaponComponent));
			if (!weapon) continue;
			if (weapon.GetWeaponType() != EWeaponType.WT_RIFLE && weapon.GetWeaponType() != EWeaponType.WT_SNIPERRIFLE) continue;
			array<BaseMuzzleComponent> muzzles = {};
			weapon.GetMuzzlesList(muzzles);
			bool grenade;
			foreach (BaseMuzzleComponent muzzle : muzzles)
				if (muzzle.GetMuzzleType() == EMuzzleType.MT_UGLMuzzle) grenade = true;
			if (!grenade) return item;
		}
		return null;
	}

	static void Check(ACR_Squad squad)
	{
		if (!s_bRead)
		{
			s_bRead = true;
			string option;
			if (ACR_Options.Get("acrMarksmen", option) && option == "0") s_bOn = false;
		}
		if (!s_bOn || !squad || !squad.m_Group || !squad.GetFaction() || !squad.m_Group.GetPrefabData()) return;
		SCR_AIGroup group = squad.m_Group;
		string prefab = FilePath.StripExtension(FilePath.StripPath(group.GetPrefabData().GetPrefabName()));
		if (System.IsCLIParam("acrFactionTest") && squad.GetName().Contains("DMR test") && !s_Diagnosed.Contains(group))
		{
			s_Diagnosed.Insert(group);
			PrintFormat("[ACR] MARKSMAN fixture %1 prefab=%2 slots=%3", squad.GetName(), prefab, group.m_aUnitPrefabSlots.Count());
		}
		if (!prefab.EndsWith("_RifleSquad") || s_Pending.Contains(group)) return;
		string faction = squad.GetFaction().GetFactionKey();
		bool ua = faction == "US" && ACR_UALoadout.Active();
		if (!ua && faction != "USSR" && faction != "RHS_AFRF") return;
		IEntity assigned;
		if (s_Assigned.Find(group, assigned) && ACR_Respawn.IsAlive(assigned)) return;
		float now = GetGame().GetWorld().GetWorldTime();
		if (s_Retry.Get(group) > now) return;
		array<AIAgent> agents = {};
		squad.GetAgents(agents);
		int full = group.m_aUnitPrefabSlots.Count();
		if (full < 6 || squad.CountAlive() < full) return;
		IEntity candidate;
		IEntity original;
		int best = -1;
		foreach (AIAgent agent : agents)
		{
			IEntity man = agent.GetControlledEntity();
			if (!man || !ACR_Respawn.IsAlive(man)) continue;
			if (ua && !ACR_UALoadout.Settled(man)) return;
			IEntity primary = Primary(man);
			if (!primary) continue;
			if (IsDMR(primary)) { s_Assigned.Set(group, man); return; }
			ChimeraCharacter manChar = ChimeraCharacter.Cast(man);
			if (!manChar) continue;
			CharacterControllerComponent controller = manChar.GetCharacterController();
			if (!controller) continue;
			if (man == squad.m_Leader || controller.IsPlayerControlled() || BBB.PlayerOf(man) || controller.IsUnconscious() || controller.IsChangingItem() || manChar.IsInVehicle()) continue;
			if (controller.GetCurrentItemInHands() && controller.GetCurrentItemInHands() != primary) continue;
			string name = ACR_UALoadout.Leaf(man);
			int rank = 1;
			if (name.Contains("Rifleman")) rank = 100;
			else if (name.Contains("_AMG") || name.Contains("_Ammo")) rank = 90;
			else if (name.Contains("_AAT")) rank = 85;
			else if (name.Contains("_TL")) rank = 80;
			else if (name.Contains("_SR")) rank = 70;
			if (rank < best) continue;
			best = rank; candidate = man; original = primary;
		}
		if (!candidate || !original) return;
		if (System.IsCLIParam("acrFactionTest")) PrintFormat("[ACR] MARKSMAN candidate %1 %2 %3", squad.GetName(), ACR_UALoadout.Leaf(candidate), ACR_UALoadout.Leaf(original));
		s_Retry.Set(group, now + 15000);
		ResourceName wanted = "{6415B7923DE28C1B}Prefabs/Weapons/Rifles/SVD/Rifle_SVD_PSO.et";
		if (ua) wanted = ACR_UALoadout.PrimaryFor(ACR_EUARole.MARKSMAN);
		Resource resource = Resource.Load(wanted);
		if (!resource || !resource.IsValid()) return;
		EntitySpawnParams parameters = new EntitySpawnParams();
		parameters.TransformMode = ETransformMode.WORLD;
		candidate.GetWorldTransform(parameters.Transform);
		IEntity replacement = GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), parameters);
		if (!replacement) return;
		InventoryItemComponent item = InventoryItemComponent.Cast(original.FindComponent(InventoryItemComponent));
		InventoryStorageSlot slot;
		if (item) slot = item.GetParentSlot();
		SCR_InventoryStorageManagerComponent inventory = SCR_InventoryStorageManagerComponent.Cast(candidate.FindComponent(SCR_InventoryStorageManagerComponent));
		ChimeraCharacter candChar = ChimeraCharacter.Cast(candidate);
		CharacterControllerComponent control;
		if (candChar) control = candChar.GetCharacterController();
		if (!control) { SCR_EntityHelper.DeleteEntityAndChildren(replacement); return; }
		bool accepted;
		if (slot && inventory)
		{
			// Use the controller's held-item replacement API, then verify inventory
			// ownership below. Its immediate return does not establish completion.
			if (control.GetCurrentItemInHands() == original)
			{
				bool result = control.ReplaceEquippedItem(replacement);
				accepted = true;
				if (System.IsCLIParam("acrFactionTest")) PrintFormat("[ACR] MARKSMAN held replacement returned=%1 owned=%2", result, inventory.Contains(replacement));
			}
			else accepted = inventory.TryReplaceItem(replacement, slot.GetStorage(), slot.GetID());
		}
		if (!accepted) { PrintFormat("[ACR] MARKSMAN replace rejected %1 slot=%2 held=%3", squad.GetName(), slot != null, control.GetCurrentItemInHands() == original); SCR_EntityHelper.DeleteEntityAndChildren(replacement); return; }
		ACR_MarksmanSwap change = new ACR_MarksmanSwap();
		change.m_Squad = squad; change.m_Soldier = candidate; change.m_Old = original; change.m_New = replacement;
		change.m_Group = group; change.m_Name = squad.GetName();
		change.m_Deadline = now + 5000;
		BaseWeaponComponent oldWeapon = BaseWeaponComponent.Cast(original.FindComponent(BaseWeaponComponent));
		array<BaseMuzzleComponent> oldMuzzles = {};
		if (oldWeapon) oldWeapon.GetMuzzlesList(oldMuzzles);
		foreach (BaseMuzzleComponent oldMuzzle : oldMuzzles)
			if (oldMuzzle.GetMagazineWell()) change.m_OldWells.Insert(oldMuzzle.GetMagazineWell().Type());
		s_Pending.Set(group, change);
		GetGame().GetCallqueue().CallLater(change.Poll, 250);
	}
}
