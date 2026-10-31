// Dev login. The engine's #login reads passwordAdmin from a -config file, which our dev
// servers can't use (-config won't mount the unpacked mod). With -acrAdminPassword <pw> on the
// server, a player types "/login <pw>" in chat and becomes session admin, so every
// SCR_Global.IsAdmin check (ACA host tools, AiO's Admin page) passes. Off without the flag.
class ACR_DevAdmin
{
 // Client: (re)attach the chat command; safe to call repeatedly.
 static void Register()
 {
  SCR_ChatPanelManager chat = SCR_ChatPanelManager.GetInstance();
  if (!chat) return;
  ChatCommandInvoker command = chat.GetCommandInvoker("login");
  if (command)
  {
   command.Remove(OnLogin);
   command.Insert(OnLogin);
  }
  ACR_DebugCommand.Register();
 }

 static void OnLogin(SCR_ChatPanel panel, string data)
 {
  SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
  if (pc) pc.ACR_DevLogin(data.Trim());
 }

 static string Password()
 {
  string password;
  if (!System.GetCLIParam("acrAdminPassword", password)) return "";
  return password.Trim();
 }

 // Server.
 static string Login(int playerId, string attempt)
 {
  string password = Password();
  if (password == "") return "Login is off on this server";
  if (attempt != password)
  {
   PrintFormat("[ACR] DEV LOGIN refused player %1", playerId);
   return "Wrong password";
  }
  PlayerManager players = GetGame().GetPlayerManager();
  if (!SCR_Global.IsAdmin(playerId)) players.GivePlayerRole(playerId, EPlayerRole.SESSION_ADMINISTRATOR);
  if (!SCR_Global.IsAdmin(playerId)) players.GivePlayerRole(playerId, EPlayerRole.ADMINISTRATOR);
  bool admin = SCR_Global.IsAdmin(playerId);
  PrintFormat("[ACR] DEV LOGIN player %1 admin=%2", playerId, admin);
  if (!admin) return "Login failed";
  return "Logged in as admin";
 }
}

modded class SCR_PlayerController
{
 override void OnControlledEntityChanged(IEntity from, IEntity to)
 {
  super.OnControlledEntityChanged(from, to);
  if (this == GetGame().GetPlayerController()) ACR_DevAdmin.Register();
 }

 void ACR_DevLogin(string attempt)
 {
  Rpc(ACR_DevLoginServer, attempt);
 }

 // One try every 3 s per player, so the password can't be guessed by a script (audit 31 Oct).
 protected float m_fACRNextLogin;
 [RplRpc(RplChannel.Reliable, RplRcver.Server)]
 protected void ACR_DevLoginServer(string attempt)
 {
  float now = GetGame().GetWorld().GetWorldTime();
  if (now < m_fACRNextLogin)
  {
   Rpc(ACR_DevLoginOwner, "Wait a moment before trying again");
   return;
  }
  m_fACRNextLogin = now + 3000;
  Rpc(ACR_DevLoginOwner, ACR_DevAdmin.Login(GetPlayerId(), attempt));
 }

 [RplRpc(RplChannel.Reliable, RplRcver.Owner)]
 protected void ACR_DevLoginOwner(string message)
 {
  SCR_ChatPanelManager chat = SCR_ChatPanelManager.GetInstance();
  if (chat) chat.OnNewMessage("[Admin] " + message);
 }
}
