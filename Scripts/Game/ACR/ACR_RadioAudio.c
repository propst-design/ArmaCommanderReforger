// Diegetic command & battlefield radio audio cues: plays verified vanilla Reforger UI/radio sound
// events locally on each player's machine when key operational events occur (contacts, base/FOB
// attacks, captures/losses, radio intercepts, counter-battery fixes, intel discoveries, fire
// missions, and incoming battalion artillery warnings).
//
// Server decides who hears what (by faction or proximity) and sends a lightweight owner-only RPC
// (ACR_PlayerController.ACR_SendRadioCue); contact reports ride the existing ACR_RadioOwner RPC
// with zero extra traffic. Per-cue cooldowns and a short global spacing prevent overlapping spam
// in heavy fights. Players toggle it under More > "Radio audio: on/off" (saved in ACR_UiPrefs).
enum ACR_ERadioCue
{
	CONTACT,
	BASE_ATTACK,
	BASE_CAPTURED,
	BASE_LOST,
	INTERCEPT,
	COUNTER_BATTERY,
	COUNTER_BATTERY_PINNED,
	COUNTER_BATTERY_INCOMING,
	INTEL_FOUND,
	FIRE_MISSION,
	INCOMING_ARTY
}

class ACR_RadioAudio
{
	static const float MIN_GAP_MS = 1200; // minimum spacing between routine cues on a client
	static const float INCOMING_ARTY_MARGIN_M = 100; // extra warning reach outside the battery radius

	// Client-side rate limiting & diagnostics.
	protected static ref map<int, float> s_LastCueAt;
	protected static float s_fLastAnyAt = -10000;
	static int s_iPlayedCount;
	static int s_iLastCue = -1;

	// Server-side base-attack edge tracking (avoids touching ACR_BaseSystem capture internals).
	protected static ref map<ACR_Base, bool> s_WasContested;
	protected static ref map<ACR_Base, float> s_AttackCueAt;
	static const float BASE_ATTACK_REPEAT_MS = 30000;

	// Called by ACR_StaticInit at game start.
	static void ACR_InitStatics()
	{
		s_LastCueAt = new map<int, float>();
		s_WasContested = new map<ACR_Base, bool>();
		s_AttackCueAt = new map<ACR_Base, float>();
	}

	static void Reset()
	{
		if (s_LastCueAt) s_LastCueAt.Clear();
		if (s_WasContested) s_WasContested.Clear();
		if (s_AttackCueAt) s_AttackCueAt.Clear();
		s_fLastAnyAt = -10000;
		s_iPlayedCount = 0;
		s_iLastCue = -1;
	}

	// Verified sound events defined in vanilla UISoundEntity.et banks
	// (UI.acp, UI_Gadgets.acp, UI_Actions.acp, UI_Map_Interactions.acp).
	static string SoundFor(int cue)
	{
		switch (cue)
		{
			case ACR_ERadioCue.CONTACT: return "SOUND_RADIO_ROGERBEEP";
			case ACR_ERadioCue.BASE_ATTACK: return SCR_SoundEvent.TASK_CREATED;
			case ACR_ERadioCue.BASE_CAPTURED: return SCR_SoundEvent.TASK_SUCCEED;
			case ACR_ERadioCue.BASE_LOST: return SCR_SoundEvent.TASK_FAILED;
			case ACR_ERadioCue.INTERCEPT: return SCR_SoundEvent.SOUND_RADIO_SIGNAL_GAIN;
			case ACR_ERadioCue.COUNTER_BATTERY: return SCR_SoundEvent.SOUND_RADIO_CHANGEFREQUENCY;
			case ACR_ERadioCue.COUNTER_BATTERY_PINNED: return SCR_SoundEvent.SOUND_MAP_HOVER_ENEMY;
			case ACR_ERadioCue.COUNTER_BATTERY_INCOMING: return SCR_SoundEvent.TASK_FAILED;
			case ACR_ERadioCue.INTEL_FOUND: return "SOUND_DEPOSITINTEL";
			case ACR_ERadioCue.FIRE_MISSION: return "SOUND_MAP_TRANSMISSION_CONFIRMATION";
			case ACR_ERadioCue.INCOMING_ARTY: return SCR_SoundEvent.TASK_FAILED;
		}
		return "";
	}

	static float CooldownMs(int cue)
	{
		switch (cue)
		{
			case ACR_ERadioCue.CONTACT: return 4000;
			case ACR_ERadioCue.BASE_ATTACK: return 8000;
			case ACR_ERadioCue.BASE_CAPTURED: return 3000;
			case ACR_ERadioCue.BASE_LOST: return 3000;
			case ACR_ERadioCue.INTERCEPT: return 6000;
			case ACR_ERadioCue.COUNTER_BATTERY: return 6000;
			case ACR_ERadioCue.COUNTER_BATTERY_PINNED: return 6000;
			case ACR_ERadioCue.COUNTER_BATTERY_INCOMING: return 6000;
			case ACR_ERadioCue.INTEL_FOUND: return 5000;
			case ACR_ERadioCue.FIRE_MISSION: return 5000;
			case ACR_ERadioCue.INCOMING_ARTY: return 8000;
		}
		return 4000;
	}

	static bool IsUrgent(int cue)
	{
		return cue == ACR_ERadioCue.BASE_LOST || cue == ACR_ERadioCue.COUNTER_BATTERY_INCOMING || cue == ACR_ERadioCue.INCOMING_ARTY;
	}

	// ---------------------------------------------------------------- server dispatch

	static void NotifyFaction(string factionKey, int cue)
	{
		if (!Replication.IsServer() || factionKey.IsEmpty()) return;
		array<int> players = {};
		GetGame().GetPlayerManager().GetPlayers(players);
		foreach (int id : players)
		{
			Faction faction = SCR_FactionManager.SGetPlayerFaction(id);
			if (faction && faction.GetFactionKey() == factionKey) NotifyPlayer(id, cue);
		}
	}

	static void NotifyPlayer(int playerId, int cue)
	{
		if (!Replication.IsServer() || playerId <= 0) return;
		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
		if (pc) pc.ACR_SendRadioCue(cue);
	}

	// Called once when a battalion howitzer barrage starts falling (3 s before impact): warn any
	// living player inside or near the impact zone (either side) so they can dive for cover.
	static void WarnIncomingArtillery(vector target, float radius)
	{
		if (!Replication.IsServer()) return;
		float reach = radius + INCOMING_ARTY_MARGIN_M;
		array<int> players = {};
		GetGame().GetPlayerManager().GetPlayers(players);
		foreach (int id : players)
		{
			IEntity body = GetGame().GetPlayerManager().GetPlayerControlledEntity(id);
			if (!body || !ACR_Respawn.IsAlive(body)) continue;
			if (vector.DistanceXZ(body.GetOrigin(), target) > reach) continue;
			NotifyPlayer(id, ACR_ERadioCue.INCOMING_ARTY);
		}
	}

	// Server, every second (ACR_Manager.Tick): detect owned bases/FOBs entering a contested state.
	static void Tick()
	{
		if (!Replication.IsServer() || !s_WasContested) return;
		float now = GetGame().GetWorld().GetWorldTime();
		foreach (ACR_Base base : ACR_Base.s_All)
		{
			if (!base || base.m_bRetired) continue;
			bool was = false;
			s_WasContested.Find(base, was);
			bool nowContested = base.m_bContested && !base.m_sOwner.IsEmpty();
			s_WasContested.Set(base, nowContested);
			if (!nowContested || was) continue;
			float lastAt = -BASE_ATTACK_REPEAT_MS;
			s_AttackCueAt.Find(base, lastAt);
			if (now - lastAt < BASE_ATTACK_REPEAT_MS) continue;
			s_AttackCueAt.Set(base, now);
			NotifyFaction(base.m_sOwner, ACR_ERadioCue.BASE_ATTACK);
		}
		// Prune deleted bases.
		for (int i = s_WasContested.Count() - 1; i >= 0; i--)
		{
			if (!s_WasContested.GetKey(i)) s_WasContested.RemoveElement(i);
		}
		for (int j = s_AttackCueAt.Count() - 1; j >= 0; j--)
		{
			if (!s_AttackCueAt.GetKey(j)) s_AttackCueAt.RemoveElement(j);
		}
	}

	// ---------------------------------------------------------------- client playback

	static void Toggle()
	{
		ACR_UiPrefs.Load();
		ACR_UiPrefs.s_bRadioAudio = !ACR_UiPrefs.s_bRadioAudio;
		ACR_UiPrefs.Save();
		if (ACR_UiPrefs.s_bRadioAudio) SCR_UISoundEntity.SoundEvent(SCR_SoundEvent.SOUND_RADIO_TURN_ON);
		else SCR_UISoundEntity.SoundEvent(SCR_SoundEvent.SOUND_RADIO_TURN_OFF);
	}

	static bool Play(int cue)
	{
		ACR_UiPrefs.Load();
		if (!ACR_UiPrefs.s_bRadioAudio) return false;
		string sound = SoundFor(cue);
		if (sound.IsEmpty()) return false;
		float now = GetGame().GetWorld().GetWorldTime();
		// Client-side match restart / reconnect guard: world time resets to 0 on a new match.
		if (now < s_fLastAnyAt)
		{
			s_fLastAnyAt = -10000;
			if (s_LastCueAt) s_LastCueAt.Clear();
		}
		if (!IsUrgent(cue) && now - s_fLastAnyAt < MIN_GAP_MS) return false;
		float last = -60000;
		if (s_LastCueAt && s_LastCueAt.Find(cue, last) && now >= last && now - last < CooldownMs(cue)) return false;
		if (s_LastCueAt) s_LastCueAt.Set(cue, now);
		s_fLastAnyAt = now;
		s_iPlayedCount++;
		s_iLastCue = cue;
		SCR_UISoundEntity.SoundEvent(sound);
		if (cue == ACR_ERadioCue.INCOMING_ARTY)
			SCR_HintManagerComponent.ShowCustomHint("Incoming artillery — take cover!", "INCOMING", 4, true);
		return true;
	}
}
