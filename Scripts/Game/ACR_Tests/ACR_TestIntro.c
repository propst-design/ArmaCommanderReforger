// -acrIntroTest 1 (server-capable, ~15 s): the mission intro (ACR_Intro) without a screen. A dedicated
// server has no player, camera or widgets, so this checks what decides the look and the sound: the 15 s
// cap and the music fade (Jacob, 10 Nov 2026), the zombies strike timeline (bolt, flash, ZOMBIES), the
// sound project, and that an intro comes once per match and a skip ends it for good.
class ACR_TestIntro
{
 static const int CHECKS = 7;

 static void Run(ACR_TestHarness h, int tick)
 {
  if (tick != 6) return;
  float total = ACR_Intro.ORBIT_S + ACR_Intro.ZOOM_S;
  float fade = ACR_Intro.MusicFadeFrom();
  ACR_TestRegress.Check("intro: 15 s at most; the music fades over the zoom and is stopped at the handover", total <= 15.0 && fade > ACR_Intro.ORBIT_S && fade < total, string.Format("total %1, fade from %2", total, fade));

  // Titles gone before the zoom; ZOMBIES only from the strike on.
  float s = ACR_Intro.STRIKE_S;
  bool titles = ACR_Intro.TitleOpacity(0.5) == 0 && ACR_Intro.TitleOpacity(5.0) == 1 && ACR_Intro.TitleOpacity(ACR_Intro.ORBIT_S) == 0 && ACR_Intro.ZombiesOpacity(ACR_Intro.ORBIT_S) == 0;
  bool word = ACR_Intro.ZombiesOpacity(s - 0.1) == 0 && ACR_Intro.ZombiesOpacity(s + 0.2) == 1;
  ACR_TestRegress.Check("intro: the title and ZOMBIES are gone before the zoom; ZOMBIES comes with the strike", titles && word, string.Format("titles %1, word %2", titles, word));

  // The bolt crosses the screen in the 0.3 s before the strike; the flash peaks on it and is gone 0.7 s on.
  bool bolt = ACR_Intro.StreakX(s - 0.3) < 0 && ACR_Intro.StreakX(s) > 1 && ACR_Intro.StreakOpacity(s - 0.15) == 1 && ACR_Intro.StreakOpacity(s - 0.5) == 0 && ACR_Intro.StreakOpacity(s + 0.3) == 0;
  float peak = ACR_Intro.FlashOpacity(s);
  bool flash = ACR_Intro.FlashOpacity(s - 0.2) == 0 && peak > 0.8 && ACR_Intro.FlashOpacity(s + 0.35) < peak && ACR_Intro.FlashOpacity(s + 0.75) == 0;
  ACR_TestRegress.Check("intro: zombies strike: the bolt sweeps across onto the beat, the flash peaks on it and clears", bolt && flash, string.Format("bolt %1, flash %2 (peak %3)", bolt, flash, peak));

  // Both tracks come from ACA's own sound project, and it loads.
  Resource acp = Resource.Load("{C2C5BC05AC9625CD}Sounds/Music/ACR_IntroMusic.acp");
  bool loads = acp && acp.IsValid();
  bool ours = ACR_Intro.DEFAULT_MUSIC.StartsWith("{C2C5BC05AC9625CD}") && ACR_Intro.ZOMBIE_MUSIC.StartsWith("{C2C5BC05AC9625CD}");
  ACR_TestRegress.Check("intro: Cliffhanger and Lost As Shit play from ACA's sound project, which loads", loads && ours, string.Format("loads %1, ours %2", loads, ours));

  // Once per match: a skip (any Stop) ends it, and a second intro sent later is ignored; a new match resets.
  ACR_Intro.Reset();
  ACR_Intro.Receive("0 0 0", "test", "", true);
  bool waiting = ACR_Intro.HasLocal() && !ACR_Intro.LocalDone();
  ACR_Intro.Stop("skipped");
  ACR_Intro.Receive("0 0 0", "test", "", false);
  bool once = !ACR_Intro.HasLocal() && ACR_Intro.LocalDone();
  ACR_TestRegress.Check("intro: a skip ends it for the match; a second intro is ignored", waiting && once, string.Format("waiting %1, once %2", waiting, once));
  ACR_Intro.Reset();
  bool fresh = !ACR_Intro.HasLocal() && !ACR_Intro.LocalDone();
  ACR_TestRegress.Check("intro: a new match lets it play again", fresh, "");
  // F10 mid-intro (review 0.5.13 #4): the RTS camera ends the intro before it saves the camera to go
  // back to (ACR_RTSCamera.InitCamera). A server can't open the RTS camera, so this checks the stop it
  // makes is final, and that the server keys "had one" by identity, not by player id (a reconnect).
  ACR_Intro.Receive("0 0 0", "test", "", false);
  ACR_Intro.Stop("rts camera");
  ACR_Intro.Receive("0 0 0", "test", "", false);
  bool rts = !ACR_Intro.HasLocal() && ACR_Intro.LocalDone();
  string key = ACR_SpawnPoints.PlayerKey(1);
  ACR_TestRegress.Check("intro: the RTS camera ends it for good; the server remembers players by identity", rts && key != "" && key != "1", string.Format("rts %1, key '%2'", rts, key));
  ACR_Intro.Reset();
  ACR_TestRegress.Complete("intro", CHECKS);
 }
}
