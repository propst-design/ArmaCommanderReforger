// In-game debug and test command executor (host/session admins only).
// Accessible anywhere via chat (/acr <command>) and in the map debug panel.
class ACR_DebugCommand : Managed
{
	static void Register()
	{
		SCR_ChatPanelManager chat = SCR_ChatPanelManager.GetInstance();
		if (!chat) return;
		ChatCommandInvoker command = chat.GetCommandInvoker("acr");
		if (!command) return;
		command.Remove(OnAcrChatCommand);
		command.Insert(OnAcrChatCommand);
	}

	static void OnAcrChatCommand(SCR_ChatPanel panel, string data)
	{
		// "think ...": the AC:A Think card is client side; the server checks every request (ACR_Think).
		array<string> words = {};
		string line = data.Trim();
		line.Split(" ", words, true);
		if (!words.IsEmpty() && words[0] == "think")
		{
			words.RemoveOrdered(0);
			ACR_Think.Command(words);
			return;
		}
		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (pc) pc.ACR_RequestDebugCommand(data.Trim());
	}

	static string Execute(SCR_PlayerController pc, string commandLine, string baseName = "", RplId groupId = RplId.Invalid(), vector cursorWorldPos = vector.Zero)
	{
		if (!pc) return "No player controller";
		int playerId = pc.GetPlayerId();
		if (!ACR_Manager.IsHostOrAdmin(playerId)) return "Debug commands are restricted to host and session admins";
		ACR_Manager manager = ACR_Manager.GetInstance();
		if (!manager) return "No manager instance";

		commandLine = commandLine.Trim();
		if (commandLine.IsEmpty() || commandLine == "help")
		{
			return "Commands: rp <amt> [side], time <0-24>, heal [all], kill [all], fob supply <amt>, fob finish, fob build <slot>, capture <base> [side], attack <base>, reveal, pause [enemy/us], tp [base], defences [base|clear], side <west|east> [player], unbound, think [west|east|off] [log]";
		}

		array<string> tokens = {};
		commandLine.Split(" ", tokens, true);
		if (tokens.IsEmpty()) return "Empty command";

		string cmd = tokens[0];
		cmd.ToLower();

		Faction faction = SCR_FactionManager.SGetPlayerFaction(playerId);
		string ownKey = "";
		if (faction) ownKey = faction.GetFactionKey();
		string enemyKey = ACR_Debug.EnemyOf(ownKey);

		// Unbound RTS Camera
		if (cmd == "unbound" || cmd == "cam" || cmd == "camera")
		{
			// The camera is the caller's, on his machine: toggled there, not on the server (a remote
			// admin's stayed leashed and a listen host's came loose instead; audit 31 Oct).
			pc.m_ACR_AdminUnbound = !pc.m_ACR_AdminUnbound;
			pc.ACR_SendUnbound(pc.m_ACR_AdminUnbound);
			return "RTS camera unbound leash: " + pc.m_ACR_AdminUnbound;
		}

		// 1. RP <amount> [side]
		if (cmd == "rp")
		{
			if (tokens.Count() < 2) return "Usage: rp <amount> [faction]";
			int amount = tokens[1].ToInt();
			string targetSide = ownKey;
			if (tokens.Count() >= 3)
			{
				string sideArg = tokens[2];
				sideArg.ToLower();
				if (sideArg == "enemy") targetSide = enemyKey;
				else if (sideArg == "us" || sideArg == "west") targetSide = ACR_FactionSelection.West();
				else if (sideArg == "ussr" || sideArg == "east") targetSide = ACR_FactionSelection.East();
				else targetSide = tokens[2];
			}
			if (targetSide.IsEmpty()) return "Invalid faction";
			ACR_FactionData data = manager.m_Bases.GetFaction(targetSide);
			if (!data) return "Faction data not found for " + targetSide;
			data.m_iRP = Math.Clamp(data.m_iRP + amount, 0, ACR_BaseSystem.POINTS_CAP);
			return string.Format("[ACR] %1 now has %2 RP (cap %3)", targetSide, data.m_iRP, ACR_BaseSystem.POINTS_CAP);
		}

		// 2. TIME <hours>
		if (cmd == "time" || cmd == "tod")
		{
			if (tokens.Count() < 2) return "Usage: time <0-24>";
			float hours = tokens[1].ToFloat();
			ChimeraWorld world = ChimeraWorld.CastFrom(GetGame().GetWorld());
			if (!world) return "No world available";
			TimeAndWeatherManagerEntity timeMgr = world.GetTimeAndWeatherManager();
			if (!timeMgr) return "No time manager available";
			int h = Math.Clamp((int)hours, 0, 23);
			timeMgr.SetHoursMinutesSeconds(h, 0, 0, true);
			return string.Format("[ACR] Time set to %1:00", h);
		}

		// 3. HEAL [all]
		if (cmd == "heal")
		{
			bool healAll = false;
			if (tokens.Count() >= 2)
			{
				string healArg = tokens[1];
				healArg.ToLower();
				if (healArg == "all") healAll = true;
			}
			if (healAll)
			{
				int squadsHealed = 0;
				foreach (ACR_Squad s : manager.m_Squads)
				{
					if (s && s.GetFaction() == faction)
					{
						HealSquad(s);
						squadsHealed++;
					}
				}
				return string.Format("[ACR] Healed and rearmed %1 friendly squads", squadsHealed);
			}
			else
			{
				ACR_Squad targetSquad = null;
				if (groupId.IsValid()) targetSquad = manager.FindSquadByGroup(SCR_AIGroup.Cast(SCR_EntityHelper.RplIdToEntity(groupId)));
				if (!targetSquad) manager.m_LastSquad.Find(playerId, targetSquad);
				if (!targetSquad)
				{
					IEntity body = pc.GetControlledEntity();
					if (body)
					{
						HealEntity(body);
						return "[ACR] Healed your character";
					}
					return "Select a squad on the map or possess a soldier first";
				}
				HealSquad(targetSquad);
				return string.Format("[ACR] Healed and rearmed squad %1", targetSquad.GetName());
			}
		}

		// 4. KILL [all]
		if (cmd == "kill")
		{
			bool killAll = false;
			if (tokens.Count() >= 2)
			{
				string killArg = tokens[1];
				killArg.ToLower();
				if (killArg == "all") killAll = true;
			}
			if (killAll)
			{
				int killed = 0;
				foreach (ACR_Squad s : manager.m_Squads)
				{
					if (s && s.GetFaction() != faction)
					{
						KillSquad(s);
						killed++;
					}
				}
				return string.Format("[ACR] Eliminated %1 enemy squads", killed);
			}
			else
			{
				ACR_Squad targetSquad = null;
				if (groupId.IsValid()) targetSquad = manager.FindSquadByGroup(SCR_AIGroup.Cast(SCR_EntityHelper.RplIdToEntity(groupId)));
				if (targetSquad)
				{
					KillSquad(targetSquad);
					return string.Format("[ACR] Eliminated squad %1", targetSquad.GetName());
				}
				IEntity body = pc.GetControlledEntity();
				if (body)
				{
					KillEntity(body);
					return "[ACR] Eliminated character";
				}
				return "Select a squad on the map first";
			}
		}

		// 5. FOB <supply|finish|build>
		if (cmd == "fob")
		{
			if (tokens.Count() < 2) return "Usage: fob <supply <amt> | finish | build <slot>>";
			string sub = tokens[1];
			sub.ToLower();
			ACR_Base base = ACR_Debug.FindBase(baseName);
			if (!base || !base.m_bBuilt)
			{
				vector pos = vector.Zero;
				IEntity pcBody = pc.GetControlledEntity();
				if (pcBody) pos = pcBody.GetOrigin();
				float bestDist = 200.0;
				foreach (ACR_Base b : ACR_Base.s_All)
				{
					if (b && b.m_bBuilt && !b.m_bRetired)
					{
						float d = vector.DistanceXZ(b.GetOrigin(), pos);
						if (d < bestDist) { bestDist = d; base = b; }
					}
				}
			}
			if (!base || !base.m_bBuilt || !base.m_FobSupply) return "Select a player-built FOB on the map first";

			if (sub == "supply")
			{
				int amt = 500;
				if (tokens.Count() >= 3) amt = tokens[2].ToInt();
				SCR_ResourceContainer container = ACR_Logistics.SupplyContainer(base.m_FobSupply);
				if (!container) return "FOB has no supply container";
				int maxSupply = ACR_FobSupply.CapacityOf(base);
				int currentSupply = ACR_FobSupply.Stock(base.m_FobSupply);
				int newSupply = Math.Clamp(currentSupply + amt, 0, maxSupply);
				container.SetResourceValue(newSupply);
				return string.Format("[ACR] %1 now has %2 / %3 supplies", base.m_sName, newSupply, maxSupply);
			}
			if (sub == "finish")
			{
				ACR_FobSlot inProgress = ACR_FobSlots.BuildingSlot(base);
				if (!inProgress) return "[ACR] No upgrade is currently under construction at " + base.m_sName;
				inProgress.m_fProgress = inProgress.m_fDuration;
				return string.Format("[ACR] Completed %1 at %2", inProgress.m_sName, base.m_sName);
			}
			if (sub == "build")
			{
				if (tokens.Count() < 3) return "Usage: fob build <barracks|repair|ammo|fuel|hospital|helipad|relay|defences>";
				string slotStr = tokens[2];
				slotStr.ToLower();
				int slotIdx = SlotIndexFromName(slotStr);
				if (slotIdx < 0) return "Unknown slot. Options: barracks, repair, ammo, fuel, hospital, helipad, relay, defences";
				string refusal;
				string startRes = ACR_FobSlots.OrderUpgradeBase(manager, ownKey, base, slotIdx, refusal);
				if (!startRes.IsEmpty()) return "[ACR] " + startRes;
				return "Cannot build: " + refusal;
			}
			return "Unknown fob subcommand. Options: supply, finish, build";
		}

		// 6. CAPTURE <baseName> [faction]
		if (cmd == "capture")
		{
			string bName = baseName;
			string targetSide = ownKey;
			if (tokens.Count() >= 2) bName = tokens[1];
			if (tokens.Count() >= 3)
			{
				string sideArg = tokens[2];
				sideArg.ToLower();
				if (sideArg == "enemy") targetSide = enemyKey;
				else targetSide = tokens[2];
			}
			ACR_Base b = ACR_Debug.FindBase(bName);
			if (!b) return "Base not found: " + bName;
			manager.m_Bases.DebugCapture(manager, b, targetSide);
			return string.Format("[ACR] %1 captured for %2", b.m_sName, targetSide);
		}

		// 7. ATTACK <baseName>
		if (cmd == "attack")
		{
			string bName = baseName;
			if (tokens.Count() >= 2) bName = tokens[1];
			ACR_Base b = ACR_Debug.FindBase(bName);
			ACR_AICommander ai = manager.AICommanderFor(enemyKey);
			if (!ai) return "No enemy AI commander";
			return "[ACR] " + ai.ForceAttack(b);
		}

		// 8. REVEAL [on/off]
		if (cmd == "reveal")
		{
			return "[ACR] " + manager.m_Debug.Run(manager, ACR_EDebug.REVEAL, ownKey, "");
		}

		// 9. PAUSE [enemy/us]
		if (cmd == "pause")
		{
			string target = "enemy";
			if (tokens.Count() >= 2)
			{
				target = tokens[1];
				target.ToLower();
			}
			if (target == "us" || target == "own") return "[ACR] " + manager.m_Debug.Run(manager, ACR_EDebug.PAUSE_ENEMY, enemyKey, "");
			return "[ACR] " + manager.m_Debug.Run(manager, ACR_EDebug.PAUSE_ENEMY, ownKey, "");
		}

		// DEFENCES [base | clear]: preview where base defences would put sandbags (ACR_BaseDefences)
		if (cmd == "defences")
		{
			if (tokens.Count() >= 2 && tokens[1] == "clear")
				return string.Format("[ACR] Removed %1 preview sandbag pieces", ACR_BaseDefences.ClearPreview());
			ACR_Base fortBase = ACR_Debug.FindBase(baseName);
			if (tokens.Count() >= 2)
			{
				string named = tokens[1];
				for (int ti = 2; ti < tokens.Count(); ti++) named += " " + tokens[ti];
				fortBase = ACR_Debug.FindBase(named);
				if (!fortBase) return "Base not found: " + named;
			}
			if (!fortBase)
			{
				vector near = cursorWorldPos;
				if (near == vector.Zero && pc.GetControlledEntity()) near = pc.GetControlledEntity().GetOrigin();
				float nearDist = 600;
				foreach (ACR_Base candidate : ACR_Base.s_All)
				{
					if (!candidate || candidate.m_bRetired || near == vector.Zero) continue;
					float baseDist = vector.DistanceXZ(candidate.GetOrigin(), near);
					if (baseDist < nearDist) { nearDist = baseDist; fortBase = candidate; }
				}
			}
			return ACR_BaseDefences.Preview(fortBase, ownKey);
		}

		// 10. TELEPORT / TP [baseName]
		if (cmd == "tp" || cmd == "teleport")
		{
			IEntity playerBody = pc.GetControlledEntity();
			if (!playerBody) return "No controlled body to teleport";
			vector targetPos = cursorWorldPos;
			if (tokens.Count() >= 2)
			{
				ACR_Base b = ACR_Debug.FindBase(tokens[1]);
				if (b) targetPos = b.GetOrigin();
			}
			if (targetPos == vector.Zero) return "Specify a base name or hover cursor on map";
			targetPos[1] = GetGame().GetWorld().GetSurfaceY(targetPos[0], targetPos[2]) + 0.2;
			playerBody.SetOrigin(targetPos);
			Physics phys = playerBody.GetPhysics();
			if (phys) phys.SetVelocity(vector.Zero);
			return string.Format("[ACR] Teleported to %1 %2", (int)targetPos[0], (int)targetPos[2]);
		}

		// SIDE <west|east> [player name]: move a player (default: yourself) to a side, past the
		// match-long side lock (ACR_SpawnPoints). Someone alive in the field moves on redeploying.
		if (cmd == "side")
		{
			if (tokens.Count() < 2) return "Usage: side <west|east> [player name]";
			string wanted = tokens[1];
			wanted.ToLower();
			string sideKey = ACR_FactionSelection.West();
			if (wanted == "east") sideKey = ACR_FactionSelection.East();
			else if (wanted != "west") return "Usage: side <west|east> [player name]";
			int targetId = playerId;
			if (tokens.Count() >= 3)
			{
				string namePart = tokens[2];
				namePart.ToLower();
				targetId = 0;
				array<int> ids = {};
				GetGame().GetPlayerManager().GetPlayers(ids);
				foreach (int id : ids)
				{
					string playerName = GetGame().GetPlayerManager().GetPlayerName(id);
					playerName.ToLower();
					if (playerName.Contains(namePart)) { targetId = id; break; }
				}
				if (targetId == 0) return "No player named " + tokens[2];
			}
			string who = GetGame().GetPlayerManager().GetPlayerName(targetId);
			string forced = ACR_PlayerSide.Key();
			if (forced != "" && forced != sideKey) return "The host option -acrPlayers keeps everyone on " + forced;
			ACR_SpawnPoints.Assign(targetId, sideKey);
			SCR_PlayerController movedPc = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(targetId));
			if (movedPc && !ACR_Respawn.IsAlive(movedPc.GetMainEntity())) return string.Format("[ACR] %1 moved to %2", who, ACR_FactionSelection.DisplayName(sideKey));
			return string.Format("[ACR] %1 joins %2 when they next redeploy", who, ACR_FactionSelection.DisplayName(sideKey));
		}

		return "Unknown command '" + cmd + "'. Type '/acr help' for available commands.";
	}

	static int SlotIndexFromName(string name)
	{
		if (name == "barracks") return ACR_FobSlots.SLOT_BARRACKS;
		if (name == "repair") return ACR_FobSlots.SLOT_REPAIR;
		if (name == "ammo") return ACR_FobSlots.SLOT_AMMO;
		if (name == "fuel") return ACR_FobSlots.SLOT_FUEL;
		if (name == "hospital" || name == "med") return ACR_FobSlots.SLOT_HOSPITAL;
		if (name == "helipad" || name == "heli") return ACR_FobSlots.SLOT_HELIPAD;
		if (name == "relay" || name == "radio") return ACR_FobSlots.SLOT_RELAY;
		if (name == "defences" || name == "defense" || name == "def") return ACR_FobSlots.SLOT_DEFENCES;
		return -1;
	}

	static void HealEntity(IEntity entity)
	{
		if (!entity) return;
		SCR_DamageManagerComponent damage = SCR_DamageManagerComponent.Cast(entity.FindComponent(SCR_DamageManagerComponent));
		if (damage)
		{
			damage.FullHeal();
		}
	}

	static void HealSquad(ACR_Squad squad)
	{
		if (!squad) return;
		array<AIAgent> agents = {};
		squad.GetAgents(agents);
		foreach (AIAgent agent : agents)
		{
			IEntity soldier = agent.GetControlledEntity();
			if (soldier && ACR_Respawn.IsAlive(soldier)) HealEntity(soldier);
		}
		IEntity vehicle = ACR_Transport.CrewedVehicle(squad);
		if (vehicle)
		{
			HealEntity(vehicle);
			SCR_FuelManagerComponent fuel = SCR_FuelManagerComponent.Cast(vehicle.FindComponent(SCR_FuelManagerComponent));
			if (fuel) fuel.SetTotalFuelPercentage(1.0);
		}
	}

	static void KillEntity(IEntity entity)
	{
		if (!entity) return;
		SCR_DamageManagerComponent damage = SCR_DamageManagerComponent.Cast(entity.FindComponent(SCR_DamageManagerComponent));
		if (damage)
		{
			HitZone defaultZone = damage.GetDefaultHitZone();
			vector hit[3];
			hit[0] = entity.GetOrigin();
			hit[1] = "0 1 0";
			hit[2] = "0 -1 0";
			SCR_DamageContext context = new SCR_DamageContext(EDamageType.EXPLOSIVE, 99999, hit, entity, defaultZone, Instigator.CreateInstigator(null), null, -1, -1);
			damage.HandleDamage(context);
		}
	}

	static void KillSquad(ACR_Squad squad)
	{
		if (!squad) return;
		array<AIAgent> agents = {};
		squad.GetAgents(agents);
		foreach (AIAgent agent : agents)
		{
			IEntity soldier = agent.GetControlledEntity();
			if (soldier && ACR_Respawn.IsAlive(soldier)) KillEntity(soldier);
		}
	}
}