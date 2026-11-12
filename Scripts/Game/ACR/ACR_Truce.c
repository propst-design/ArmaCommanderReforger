// Truce (#107, TvTvZ with zombies, Jacob 7 Nov 2026): the two living sides can stop fighting each other
// while the dead are the bigger problem. Breakable, and never a timer on the match.
//  - A commander offers it (command panel, More: "Truce: offer / accept"); the other side's commander
//    accepts with the same button or refuses with "Refuse / break truce". An offer nobody answers
//    lapses after OFFER_MS.
//  - While it holds the game's two factions are friendly (SCR_FactionManager, as FIA's ally in
//    ACR_Insurgency), neither side's soldiers capture or contest the other's bases (ACR_BaseSystem),
//    neither steals the other's supply pallets (ACR_Airdrop.MayTake) and AI commanders plan no attack on
//    the other's bases.
//  - Breaking it: everyone is told at once and shooting is allowed BREAK_WARN_MS later (the factions turn
//    hostile then). A new offer waits COOLDOWN_MS after a break or refusal.
//  - AI commanders (ACR_AICommander.ManageTruce) accept or offer when the dead threaten their bases or they
//    are losing, and may break it once it has held MIN_HOLD_MS, the dead are under control and they are ahead.
enum ACR_ETruce
{
 NONE,
 OFFERED,
 HOLDS,
 BREAKING
}

class ACR_Truce
{
 static const float OFFER_MS = 180000;      // an offer lapses after 3 min
 static const float BREAK_WARN_MS = 60000;  // public warning before shooting is allowed
 static const float COOLDOWN_MS = 300000;   // no new offer for 5 min after a break or refusal
 static const float MIN_HOLD_MS = 600000;   // an AI keeps a truce at least 10 min

 protected static int s_iState;
 protected static string s_sBy;             // who offered (OFFERED) or is breaking it (BREAKING)
 protected static float s_fSince;           // when the state began
 protected static float s_fCooldownUntil;
 static int s_iHeld;                        // truces made this match (tests)
 static int s_iBroken;                      // truces broken (tests)

 static void Reset()
 {
  if (s_iState == ACR_ETruce.HOLDS || s_iState == ACR_ETruce.BREAKING) Hostile();
  s_iState = ACR_ETruce.NONE;
  s_sBy = "";
  s_fSince = 0;
  s_fCooldownUntil = 0;
  s_iHeld = 0;
  s_iBroken = 0;
 }

 static int State() { return s_iState; }
 static string By() { return s_sBy; }

 // The sides are at peace: no capture, no theft, friendly factions (a breaking truce still holds).
 static bool Holds() { return s_iState == ACR_ETruce.HOLDS || s_iState == ACR_ETruce.BREAKING; }

 // Between these two sides (both living sides, not the same).
 static bool Between(string a, string b)
 {
  if (!Holds() || a.IsEmpty() || b.IsEmpty() || a == b) return false;
  array<string> sides = ACR_Containment.Sides();
  return sides.Contains(a) && sides.Contains(b);
 }

 // A truce can be had: zombies on and two living sides.
 static bool Possible()
 {
  return ACR_Outbreak.On() && ACR_Containment.Sides().Count() == 2;
 }

 protected static string Other(string side)
 {
  array<string> sides = ACR_Containment.Sides();
  foreach (string s : sides) if (s != side) return s;
  return "";
 }

 // ---------------------------------------------------------------- the commanders' buttons

 // "Truce: offer / accept": offers one, or accepts the other side's offer. The reply for the commander.
 static string Offer(string side)
 {
  if (!Possible()) return "A truce needs zombies and two sides (TvTvZ)";
  float now = GetGame().GetWorld().GetWorldTime();
  Expire(now);
  if (s_iState == ACR_ETruce.HOLDS) return "The truce already holds";
  if (s_iState == ACR_ETruce.BREAKING) return "The truce is being broken";
  if (s_iState == ACR_ETruce.OFFERED)
  {
   if (s_sBy == side) return "Our offer stands: waiting for their answer";
   Accept(side);
   return "Truce accepted";
  }
  if (now < s_fCooldownUntil) return string.Format("No new offer for %1 s", Math.Ceil((s_fCooldownUntil - now) / 1000));
  s_iState = ACR_ETruce.OFFERED;
  s_sBy = side;
  s_fSince = now;
  PrintFormat("[ACR] TRUCE %1 offers a truce", side);
  ACR_Containment.Feed(side, "We offered the enemy a truce against the dead", vector.Zero);
  ACR_Containment.Feed(Other(side), "The enemy offers a truce against the dead: accept with Truce (command panel, More) or refuse", vector.Zero);
  return "Truce offered";
 }

 // "Refuse / break truce": refuses the other side's offer, withdraws ours, or breaks the truce.
 static string Refuse(string side)
 {
  float now = GetGame().GetWorld().GetWorldTime();
  Expire(now);
  if (s_iState == ACR_ETruce.OFFERED)
  {
   bool ours = s_sBy == side;
   string other = Other(side);
   s_iState = ACR_ETruce.NONE;
   s_sBy = "";
   s_fCooldownUntil = now + COOLDOWN_MS;
   PrintFormat("[ACR] TRUCE %1 %2 the offer", side, ours);
   if (ours) return "Offer withdrawn";
   ACR_Containment.Feed(other, "The enemy refused our truce", vector.Zero);
   return "Truce refused";
  }
  if (s_iState == ACR_ETruce.HOLDS)
  {
   Break(side);
   return string.Format("Breaking the truce: everyone is told, shooting is allowed in %1 s", BREAK_WARN_MS / 1000);
  }
  if (s_iState == ACR_ETruce.BREAKING) return "The truce is already being broken";
  return "No truce or offer to refuse";
 }

 protected static void Accept(string side)
 {
  s_iState = ACR_ETruce.HOLDS;
  s_sBy = "";
  s_fSince = GetGame().GetWorld().GetWorldTime();
  s_iHeld++;
  Friendly();
  PrintFormat("[ACR] TRUCE holds (%1 accepted)", side);
  array<string> sides = ACR_Containment.Sides();
  foreach (string s : sides) ACR_Containment.Feed(s, "Truce: the two sides hold fire on each other. No base captures and no taking their pallets while it holds", vector.Zero);
 }

 static void Break(string side)
 {
  if (s_iState != ACR_ETruce.HOLDS) return;
  s_iState = ACR_ETruce.BREAKING;
  s_sBy = side;
  s_fSince = GetGame().GetWorld().GetWorldTime();
  PrintFormat("[ACR] TRUCE %1 breaks the truce: hostile in %2 s", side, BREAK_WARN_MS / 1000);
  array<string> sides = ACR_Containment.Sides();
  foreach (string s : sides)
  {
   if (s == side) ACR_Containment.Feed(s, string.Format("We are breaking the truce: weapons free in %1 s", BREAK_WARN_MS / 1000), vector.Zero);
   else ACR_Containment.Feed(s, string.Format("The enemy is breaking the truce: expect fire in %1 s", BREAK_WARN_MS / 1000), vector.Zero);
  }
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  foreach (string told : sides) manager.NotifyFaction(told, string.Format("The truce is being broken: shooting is allowed in %1 s", BREAK_WARN_MS / 1000));
 }

 // ---------------------------------------------------------------- every second (ACR_Containment.Tick / ACR_Manager)

 static void Tick()
 {
  if (s_iState == ACR_ETruce.NONE) return;
  Expire(GetGame().GetWorld().GetWorldTime());
 }

 protected static void Expire(float now)
 {
  if (s_iState == ACR_ETruce.OFFERED && now - s_fSince > OFFER_MS)
  {
   PrintFormat("[ACR] TRUCE %1's offer lapsed", s_sBy);
   ACR_Containment.Feed(s_sBy, "Our truce offer went unanswered", vector.Zero);
   s_iState = ACR_ETruce.NONE;
   s_sBy = "";
   s_fCooldownUntil = now + COOLDOWN_MS;
   return;
  }
  if (s_iState == ACR_ETruce.BREAKING && now - s_fSince >= BREAK_WARN_MS)
  {
   string breaker = s_sBy;
   s_iState = ACR_ETruce.NONE;
   s_sBy = "";
   s_fCooldownUntil = now + COOLDOWN_MS;
   s_iBroken++;
   Hostile();
   PrintFormat("[ACR] TRUCE over (%1 broke it): hostile", breaker);
   array<string> sides = ACR_Containment.Sides();
   foreach (string s : sides) ACR_Containment.Feed(s, "The truce is over: weapons free", vector.Zero);
  }
 }

 // How long the truce has held (0 when it doesn't).
 static float HeldMs()
 {
  if (s_iState != ACR_ETruce.HOLDS) return 0;
  return GetGame().GetWorld().GetWorldTime() - s_fSince;
 }

 // ---------------------------------------------------------------- factions

 protected static void Friendly() { SetFriendly(true); }
 protected static void Hostile() { SetFriendly(false); }

 protected static void SetFriendly(bool friendly)
 {
  ApplyFactions(friendly, true);
  // Vanilla replicates friendliness only through SCR_DelegateFactionManagerComponent, which a
  // world may not have (0.5.13 review): clients apply it themselves from the game mode's flag.
  SCR_BaseGameMode mode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
  if (mode) mode.ACR_SetTruceFriendly(friendly);
 }

 // This machine's faction relation between the two sides ('updateAIs': the server's AI targets).
 static void ApplyFactions(bool friendly, bool updateAIs)
 {
  SCR_FactionManager factions = SCR_FactionManager.Cast(GetGame().GetFactionManager());
  if (!factions) return;
  SCR_Faction west = SCR_Faction.Cast(factions.GetFactionByKey(ACR_FactionSelection.West()));
  SCR_Faction east = SCR_Faction.Cast(factions.GetFactionByKey(ACR_FactionSelection.East()));
  if (!west || !east || west == east) return;
  if (friendly) factions.SetFactionsFriendly(west, east, -1, updateAIs);
  else factions.SetFactionsHostile(west, east, -1, updateAIs);
 }

 // Tests: run the break's warning out now.
 static void EndWarningForTest()
 {
  if (s_iState == ACR_ETruce.BREAKING) s_fSince = -BREAK_WARN_MS;
  Expire(GetGame().GetWorld().GetWorldTime());
 }
}

// The truce's faction relation for clients (ACR_Truce.SetFriendly), joiners too.
modded class SCR_BaseGameMode
{
 [RplProp(onRplName: "ACR_OnTruceFriendly")]
 protected bool m_bACRTruceFriendly;

 void ACR_SetTruceFriendly(bool friendly)
 {
  if (m_bACRTruceFriendly == friendly) return;
  m_bACRTruceFriendly = friendly;
  Replication.BumpMe();
 }

 protected void ACR_OnTruceFriendly()
 {
  if (!Replication.IsServer()) ACR_Truce.ApplyFactions(m_bACRTruceFriendly, false);
 }
}
