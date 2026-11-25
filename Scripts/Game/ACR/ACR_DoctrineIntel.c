// What one side has worked out about the enemy AI commander's doctrine (ACR_AIDoctrine pass 2,
// Jacob 24 Oct: doctrines can be discovered). Intel points come only from what the side saw:
// each enemy squad reported in contact (more for vehicles, armour and aircraft, at most
// SQUAD_MAX from one squad), an enemy FOB found, a base taken from the enemy (what was left
// behind there) and a base lost to them (how they came). At HINT points the side learns two
// doctrines it could be (the real one and its look-alike), at CONFIRM the doctrine itself:
// feed line, notice, report timeline, and a line in the map panel's Report.
// When the enemy commander changes doctrine the picture goes out of date: the side keeps
// believing what it had (its Report and its AI commander alike) until CHANGE_NOTICE points of
// new intel show it the change, then works the new doctrine out from scratch.
// The AI commander reads the same picture (Believed(), for ReviewDoctrine), never the truth.
// Server only; one per ACR_Intel.
class ACR_DoctrineIntel : Managed
{
 static const float HINT = 30;
 static const float CONFIRM = 60;
 static const float CHANGE_NOTICE = 10;
 static const float SQUAD_MAX = 8;
 static const float FOB_FOUND = 20;
 static const float BASE_TAKEN = 25;
 static const float BASE_LOST = 10;

 protected ACR_Intel m_Intel;
 float m_fPoints;
 int m_iLevel;                // 0 nothing yet, 1 hint, 2 confirmed
 // The picture from before the enemy's last change, kept until the change is noticed.
 protected string m_sOldKey;
 protected int m_iOldLevel;
 protected float m_fOldPoints;
 bool m_bStale;               // the enemy changed doctrine since the old picture
 bool m_bChangeNoticed;
 protected ref map<string, float> m_FromSquad = new map<string, float>();
 ACR_AIDoctrine m_EnemyForTest; // tests: the enemy doctrine without an enemy commander
 float m_fHeadStart;          // points given once an enemy AI commander is in command (Raider's starting kit, ACR_DoctrineKits)

 void ACR_DoctrineIntel(ACR_Intel intel)
 {
  m_Intel = intel;
 }

 protected string Side() { return m_Intel.m_sFaction; }
 protected string EnemySide() { return ACR_Debug.EnemyOf(Side()); }

 // The enemy commander's doctrine now, or null while no AI commands them (a player does).
 ACR_AIDoctrine Enemy()
 {
  if (m_EnemyForTest) return m_EnemyForTest;
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return null;
  ACR_AICommander ai = manager.AICommanderFor(EnemySide());
  if (!ai || !ai.m_bActive) return null;
  return ai.Doctrine();
 }

 // The enemy doctrine this side believes is confirmed ("" if none): the old one while a change
 // has gone unnoticed.
 string Believed()
 {
  if (m_bStale && !m_bChangeNoticed)
  {
   if (m_iOldLevel >= 2) return m_sOldKey;
   return "";
  }
  ACR_AIDoctrine enemy = Enemy();
  if (!enemy || m_iLevel < 2) return "";
  return enemy.m_sKey;
 }

 // "Blitz or Raider": the doctrine and its look-alike, in a fixed order that gives nothing away.
 static string Hint(string key)
 {
  string other = ACR_AIDoctrine.LookAlike(key);
  array<ref ACR_AIDoctrine> all = ACR_AIDoctrine.All();
  foreach (ACR_AIDoctrine doctrine : all)
  {
   if (doctrine.m_sKey == key) return ACR_AIDoctrine.NameOf(key) + " or " + ACR_AIDoctrine.NameOf(other);
   if (doctrine.m_sKey == other) return ACR_AIDoctrine.NameOf(other) + " or " + ACR_AIDoctrine.NameOf(key);
  }
  return ACR_AIDoctrine.NameOf(key);
 }

 // Every second (ACR_Intel.Tick): a head start is given once there is an enemy doctrine to learn.
 void Tick()
 {
  if (m_fHeadStart <= 0 || !Enemy()) return;
  float points = m_fHeadStart;
  m_fHeadStart = 0;
  Add(points, "recon teams' head start");
 }

 void Add(float points, string why)
 {
  ACR_AIDoctrine enemy = Enemy();
  if (!enemy || points <= 0 || m_iLevel >= 2) return;
  m_fPoints += points;
  if (m_bStale && !m_bChangeNoticed && m_fPoints >= CHANGE_NOTICE)
  {
   m_bChangeNoticed = true;
   // Only a confirmed doctrine is named: a hint ("Firepower or Fortress") may cover the new
   // one too (Everon night soak 26 Oct: "no longer Firepower or Fortress" as it went Fortress).
   string line = "Intel: the enemy commander has changed tactics";
   if (m_iOldLevel >= 2) line += string.Format(" (no longer %1)", ACR_AIDoctrine.NameOf(m_sOldKey));
   Announce(line, "enemy changed tactics");
  }
  if (m_iLevel < 1 && m_fPoints >= HINT)
  {
   m_iLevel = 1;
   Announce(string.Format("Intel: the enemy commander fights like %1", Hint(enemy.m_sKey)), "enemy doctrine " + Hint(enemy.m_sKey) + "?");
  }
  if (m_fPoints >= CONFIRM)
  {
   m_iLevel = 2;
   m_bStale = false;
   Announce(string.Format("Intel confirms the enemy doctrine: %1 (%2)", enemy.m_sName, enemy.m_sSummary), "enemy doctrine confirmed: " + enemy.m_sName);
  }
  PrintFormat("[ACR] DOCTRINE INTEL %1: +%2 (%3) = %4, level %5", Side(), points, why, m_fPoints, m_iLevel);
 }

 protected void Announce(string line, string timeline)
 {
  m_Intel.AddFeed(GetGame().GetWorld().GetWorldTime(), line);
  ACR_Report.Event(Side(), timeline);
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (manager) manager.NotifyFaction(Side(), line);
  ACR_RadioAudio.NotifyFaction(Side(), ACR_ERadioCue.INTEL_FOUND);
 }

 // The enemy commander changed doctrine (ACR_AICommander.ChangeDoctrine).
 void OnEnemyChanged(string oldKey)
 {
  if (!m_bStale || m_bChangeNoticed)
  {
   // A second change before the first was noticed keeps the first old picture.
   m_bStale = m_iLevel > 0;
   m_sOldKey = oldKey;
   m_iOldLevel = m_iLevel;
   m_fOldPoints = m_fPoints;
  }
  m_bChangeNoticed = false;
  m_fPoints = 0;
  m_iLevel = 0;
  m_FromSquad.Clear();
 }

 // A fresh contact report (ACR_Intel.Report): a squad of the enemy commander.
 void OnContact(ACR_Contact contact)
 {
  if (!contact || !contact.m_Squad || !contact.m_Squad.GetFaction() || contact.m_Squad.GetFaction().GetFactionKey() != EnemySide()) return;
  float points = 2;
  if (contact.m_iKind == ACR_Intel.KIND_VEHICLE || contact.m_iKind == ACR_Intel.KIND_ARMED) points = 3;
  else if (contact.m_iKind == ACR_Intel.KIND_ARMOUR || contact.m_iKind == ACR_Intel.KIND_AIR) points = 4;
  float had;
  m_FromSquad.Find(contact.m_sKey, had);
  points = Math.Min(points, SQUAD_MAX - had);
  if (points <= 0) return;
  m_FromSquad.Set(contact.m_sKey, had + points);
  Add(points, "contact " + contact.m_sKey);
 }

 // A base changed hands (ACR_BaseSystem.Capture): both sides learn something.
 static void OnCapture(ACR_Manager manager, string by, string from)
 {
  if (!manager || by.IsEmpty() || from.IsEmpty()) return;
  ACR_Intel winner = manager.IntelFor(by);
  if (winner && ACR_Debug.EnemyOf(by) == from) winner.Doctrine().Add(BASE_TAKEN, "base taken");
  ACR_Intel loser = manager.IntelFor(from);
  if (loser && ACR_Debug.EnemyOf(from) == by) loser.Doctrine().Add(BASE_LOST, "base lost");
 }

 // The Report panel's lines: what our commander fights to (players under an AI commander
 // know it) and what we know of the enemy's.
 static string ReportLines(string key)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return "";
  string text;
  ACR_AICommander own = manager.AICommanderFor(key);
  if (own && own.m_bActive && own.Doctrine()) text = "Our commander: " + own.Doctrine().m_sName + " (" + own.Doctrine().m_sSummary + ")";
  ACR_Intel intel = manager.IntelFor(key);
  if (!intel) return text;
  string enemyLine = intel.Doctrine().Text();
  if (enemyLine.IsEmpty()) return text;
  if (!text.IsEmpty()) text += "\n";
  return text + enemyLine;
 }

 string Text()
 {
  if (m_bStale && !m_bChangeNoticed) return Describe(m_sOldKey, m_iOldLevel, m_fOldPoints);
  ACR_AIDoctrine enemy = Enemy();
  if (!enemy) return "";
  string line = Describe(enemy.m_sKey, m_iLevel, m_fPoints);
  if (m_bStale)
  {
   if (m_iOldLevel >= 2) line += "\nTactics changed: no longer " + ACR_AIDoctrine.NameOf(m_sOldKey);
   else line += "\nTactics changed";
  }
  return line;
 }

 protected static string Describe(string key, int level, float points)
 {
  int percent = (int)Math.Min(100, Math.Round(points * 100 / CONFIRM));
  string intel = " (intel " + percent.ToString() + "%)";
  if (level >= 2) return "Enemy doctrine: " + ACR_AIDoctrine.NameOf(key) + " (confirmed)";
  if (level == 1) return "Enemy doctrine: " + Hint(key) + "?" + intel;
  return "Enemy doctrine: unknown" + intel;
 }
}
