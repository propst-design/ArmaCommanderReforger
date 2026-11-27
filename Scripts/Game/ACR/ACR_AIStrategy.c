// Periodic doctrine review, adaptation rules, and doctrine switching for
// ACR_AICommander. Extracted as a standalone helper; ACR_AICommander keeps
// thin forwarding wrappers for existing callers and tests.
class ACR_AIStrategy
{
 static void SnapshotReview(ACR_AICommander cmd)
 {
  ACR_SideStats s = ACR_Report.Of(cmd.m_sFaction);
  if (!s) return;
  cmd.m_iReviewLost = s.m_iLost;
  cmd.m_iReviewKills = s.m_iKills;
  cmd.m_iReviewWon = s.m_iBasesWon;
  cmd.m_iReviewBasesLost = s.m_iBasesLost;
 }

 static void ReviewDoctrine(ACR_AICommander cmd)
 {
  if (!ACR_AIDoctrine.AdaptAllowed()) return;
  float now = GetGame().GetWorld().GetWorldTime();
  if (cmd.m_fNextDoctrineReview < 0)
  {
   cmd.m_fNextDoctrineReview = now + ACR_AICommander.DOCTRINE_FIRST_REVIEW_MIN * 60000;
   SnapshotReview(cmd);
   return;
  }
  if (now < cmd.m_fNextDoctrineReview) return;
  cmd.m_fNextDoctrineReview = now + ACR_AICommander.DOCTRINE_REVIEW_MIN * 60000;
  ACR_SideStats s = ACR_Report.Of(cmd.m_sFaction);
  if (!s) return;
  // A new doctrine is judged on what it does once settled in, not on the purchases and plans
  // of the old one still arriving (Everon night soak 26 Oct: Balanced -> Firepower ->
  // Fortress, ten minutes apart, both for losses).
  if (cmd.m_bDoctrineSettling)
  {
   cmd.m_bDoctrineSettling = false;
   SnapshotReview(cmd);
   cmd.Say(string.Format("[ACR] AI commander %1 doctrine review: %2 settling in, judged from now", cmd.m_sFaction, cmd.m_Doctrine.m_sName));
   return;
  }
  int lost = s.m_iLost - cmd.m_iReviewLost;
  int kills = s.m_iKills - cmd.m_iReviewKills;
  int won = s.m_iBasesWon - cmd.m_iReviewWon;
  int basesLost = s.m_iBasesLost - cmd.m_iReviewBasesLost;
  SnapshotReview(cmd);
  string enemyKnown;
  ACR_Intel intel = cmd.m_Manager.IntelFor(cmd.m_sFaction);
  if (intel) enemyKnown = intel.Doctrine().Believed();
  string why;
  int ours, theirs;
  BasesHeld(cmd, ours, theirs);
  string pick = PickDoctrine(cmd.m_Doctrine.m_sKey, enemyKnown, won, basesLost, lost, kills, why, cmd.m_sHomeDoctrine, Ahead(ours, theirs));
  cmd.Say(string.Format("[ACR] AI commander %1 doctrine review: %2, bases +%3 -%4, lost %5 killed %6, enemy doctrine '%7', holds %8 to %9: ", cmd.m_sFaction, cmd.m_Doctrine.m_sName, won, basesLost, lost, kills, enemyKnown, ours, theirs) + why);
  string pending = cmd.m_sDoctrinePending;
  cmd.m_sDoctrinePending = "";
  if (pick.IsEmpty() || pick == cmd.m_Doctrine.m_sKey) return;
  float heldMin = -1;
  if (cmd.m_fDoctrineSince >= 0) heldMin = (now - cmd.m_fDoctrineSince) / 60000;
  string blocked = DoctrineChangeBlocked(pick, why, heldMin, cmd.m_sDoctrineLeft, cmd.m_sDoctrineLeftWhy);
  if (blocked.IsEmpty() && why == ACR_AICommander.WHY_HOME) blocked = HomeDriftBlocked(heldMin, HomeDriftMin(cmd));
  if (!blocked.IsEmpty())
  {
   cmd.Say(string.Format("[ACR] AI commander %1 doctrine: keeps %2 instead of %3 (%4)", cmd.m_sFaction, cmd.m_Doctrine.m_sName, ACR_AIDoctrine.NameOf(pick), blocked));
   return;
  }
  if (NeedsConfirming(why) && pending != pick)
  {
   cmd.m_sDoctrinePending = pick;
   cmd.Say(string.Format("[ACR] AI commander %1 doctrine: would switch to %2 (%3), if the next review agrees", cmd.m_sFaction, ACR_AIDoctrine.NameOf(pick), why));
   return;
  }
  ChangeDoctrine(cmd, ACR_AIDoctrine.Find(pick), why);
 }

 // Only losing ground and heavy losses change doctrine at once; anything else (getting nowhere, a
 // counter, going home) must be picked at two reviews in a row.
 static bool NeedsConfirming(string why)
 {
  return DoctrineReasonKind(why) != "going badly";
 }

 // Pure (tests): far enough ahead in bases held that a quiet spell or a believed enemy style
 // doesn't change how it fights.
 static bool Ahead(int ours, int theirs)
 {
  return ours >= theirs + ACR_AICommander.DOCTRINE_AHEAD_BY && ours >= theirs * 2;
 }

 // Bases this side holds, and those it knows a hostile side holds.
 static void BasesHeld(ACR_AICommander cmd, out int ours, out int theirs)
 {
  ours = 0;
  theirs = 0;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (!base || base.m_bRetired) continue;
   if (base.m_sOwner == cmd.m_sFaction) ours++;
   else if (cmd.HostileOwner(base) && cmd.Visible(base)) theirs++;
  }
 }

 // Reasons that lead to the same answer count as one: losing ground and heavy losses both pick
 // whichever of Fortress and Firepower the side isn't on; every counter is a counter.
 static string DoctrineReasonKind(string why)
 {
  if (why.StartsWith("countering")) return "counter";
  if (why == "losing ground" || why == "losses too heavy") return "going badly";
  return why;
 }

 // Why a picked change can't happen yet, "" when it can. heldMin < 0: never changed this match
 // (the starting doctrine has had the first review's wait).
 static string DoctrineChangeBlocked(string pick, string why, float heldMin, string left, string leftWhy)
 {
  if (heldMin >= 0 && heldMin < ACR_AICommander.DOCTRINE_HOLD_MIN)
   return string.Format("held %1 of %2 min", Math.Floor(heldMin), ACR_AICommander.DOCTRINE_HOLD_MIN);
  if (!left.IsEmpty() && pick == left && DoctrineReasonKind(why) == DoctrineReasonKind(leftWhy))
   return "just left it for the same reason: " + why;
  return "";
 }

 static float HomeDriftMin(ACR_AICommander cmd)
 {
  return ACR_AICommander.DOCTRINE_HOLD_MIN + ACR_Variety.Jitter(ACR_AICommander.HOME_DRIFT_EXTRA_MIN, 1, string.Format("home drift %1 %2", cmd.m_sFaction, cmd.m_iDoctrineChanges));
 }

 static string HomeDriftBlocked(float heldMin, float waitMin)
 {
  if (heldMin >= 0 && heldMin < waitMin)
   return string.Format("back to its home style after %1 min, held %2", Math.Floor(waitMin), Math.Floor(heldMin));
  return "";
 }

 // What a review decides over one review window: the doctrine key to change to, "" to keep. Away
 // from its home style ("" none) a side only goes back to it (see below).
 // 'ahead' (Ahead): well ahead in the war, it changes only for losing ground or heavy losses.
 static string PickDoctrine(string current, string enemyKnown, int won, int basesLost, int lost, int kills, out string why, string home = "", bool ahead = false)
 {
  bool losingGround = basesLost >= won + 2;
  bool bleeding = lost >= 12 && lost > kills * 1.6;
  // Bases gained count as winning only while not bleeding: early on they are mostly empty
  // sites (Everon night soak 26 Oct: "+3 -0, lost 20 killed 4: it is working").
  bool winning = (won >= basesLost + 2 && !bleeding) || (kills >= 12 && kills > lost * 1.6);
  bool stuck = won == 0 && basesLost == 0;
  if (winning)
  {
   why = "it is working";
   return "";
  }
  if (ahead && !losingGround && !bleeding)
  {
   why = "well ahead in the war, keeps what it has";
   return "";
  }
  string counter = ACR_AIDoctrine.Counter(enemyKnown);
  if (!counter.IsEmpty() && counter != current)
  {
   why = "countering the enemy's " + ACR_AIDoctrine.NameOf(enemyKnown);
   return counter;
  }
  // Already the answer to what the enemy does: only losing ground makes it think again.
  if (!counter.IsEmpty() && !losingGround)
  {
   why = "already countering the enemy's " + ACR_AIDoctrine.NameOf(enemyKnown);
   return "";
  }
  // Away from its home style (its company's, or the one it started with), a side keeps the change
  // while the trouble that made it lasts, and goes back home once that is over: trouble no longer
  // moves it on, or back and forth (#29). Only a known enemy's counter (above) still moves it to
  // another doctrine.
  if (!home.IsEmpty() && current != home)
  {
   if (losingGround || bleeding)
   {
    why = "still in trouble, keeps " + ACR_AIDoctrine.NameOf(current) + " before going back to " + ACR_AIDoctrine.NameOf(home);
    return "";
   }
   why = ACR_AICommander.WHY_HOME;
   return home;
  }
  if (losingGround)
  {
   why = "losing ground";
   if (current == "fortress") return "firepower";
   return "fortress";
  }
  if (bleeding)
  {
   why = "losses too heavy";
   if (current == "firepower") return "fortress";
   return "firepower";
  }
  if (stuck)
  {
   why = "getting nowhere";
   if (current == "blitz") return "raider";
   return "blitz";
  }
  why = "holding its own";
  return "";
 }

 static void ChangeDoctrine(ACR_AICommander cmd, ACR_AIDoctrine doctrine, string why)
 {
  if (!doctrine || doctrine == cmd.m_Doctrine) return;
  ACR_AIDoctrine was = cmd.m_Doctrine;
  cmd.m_Doctrine = doctrine;
  cmd.m_sDoctrineSource = "changed";
  cmd.m_iDoctrineChanges++;
  cmd.m_bDoctrineSettling = true;
  cmd.m_fDoctrineSince = GetGame().GetWorld().GetWorldTime();
  cmd.m_sDoctrineLeft = was.m_sKey;
  cmd.m_sDoctrineLeftWhy = why;
  cmd.Say(string.Format("[ACR] AI commander %1 doctrine: %2 -> %3 (%4; %5)", cmd.m_sFaction, was.m_sName, doctrine.m_sName, why, doctrine.m_sSummary));
  // Its own side hears it (players fighting under this commander); the enemy has to notice.
  string line = string.Format("Commander: switching to %1 (%2)", doctrine.m_sName, why);
  ACR_Intel own = cmd.m_Manager.IntelFor(cmd.m_sFaction);
  if (own) own.AddFeed(GetGame().GetWorld().GetWorldTime(), line);
  ACR_Report.Event(cmd.m_sFaction, string.Format("commander switched to %1 (%2)", doctrine.m_sName, why));
  cmd.m_Manager.NotifyFaction(cmd.m_sFaction, line);
  ACR_Intel enemy = cmd.m_Manager.IntelFor(ACR_Debug.EnemyOf(cmd.m_sFaction));
  if (enemy) enemy.Doctrine().OnEnemyChanged(was.m_sKey);
 }
}
