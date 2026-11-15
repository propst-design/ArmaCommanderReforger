// Character templates carry their kit data so casualties reinforce with the same role.
enum ACR_EInfantryKit { NONE, RU, UA, WESTERN, RU_SF, UA_SF, USMC, MARSOC }

class ACR_InfantryKitComponentClass : ScriptComponentClass {}
class ACR_InfantryKitComponent : ScriptComponent
{
 [Attribute("0")] ACR_EInfantryKit m_eKit;
 [Attribute("0")] ACR_EUARole m_eRole;
 bool m_bEquipped;
 bool m_bReady;
 int m_iSupplyAttempts;
 bool m_bAppearanceReady;
 ResourceName m_sHelmet;
 ResourceName m_sBoots;
 bool IsSF() { return m_eKit == ACR_EInfantryKit.RU_SF || m_eKit == ACR_EInfantryKit.UA_SF || m_eKit == ACR_EInfantryKit.MARSOC; }
 bool IsUA() { return m_eKit == ACR_EInfantryKit.UA || m_eKit == ACR_EInfantryKit.WESTERN || m_eKit == ACR_EInfantryKit.UA_SF; }
 bool IsUSMC() { return m_eKit == ACR_EInfantryKit.USMC || m_eKit == ACR_EInfantryKit.MARSOC; }
 static ACR_InfantryKitComponent Of(IEntity body)
 {
  if (!body) return null;
  return ACR_InfantryKitComponent.Cast(body.FindComponent(ACR_InfantryKitComponent));
 }
}

class ACR_InfantryTemplates
{
 static ResourceName Character(ACR_EInfantryKit kit, ACR_EUARole role)
 {
  array<ResourceName> men = {};
  switch (kit)
  {
   case 1:
    men = {"{AC230926A0010040}Prefabs/ACR/Infantry/RU_Rifleman.et", "{AC230926A0010041}Prefabs/ACR/Infantry/RU_Leader.et", "{AC230926A0010042}Prefabs/ACR/Infantry/RU_MG.et", "{AC230926A0010043}Prefabs/ACR/Infantry/RU_AR.et", "{AC230926A0010044}Prefabs/ACR/Infantry/RU_GL.et", "{AC230926A0010045}Prefabs/ACR/Infantry/RU_AT.et", "{AC230926A0010046}Prefabs/ACR/Infantry/RU_Marksman.et", "", "", "", "{AC230926A0010047}Prefabs/ACR/Infantry/RU_MGAssistant.et", "{AC230926A0010048}Prefabs/ACR/Infantry/RU_ATAssistant.et", "{AC230926A0010049}Prefabs/ACR/Infantry/RU_Medic.et", "{AC230926A001004A}Prefabs/ACR/Infantry/RU_Engineer.et", "{AC230926A001004B}Prefabs/ACR/Infantry/RU_LightAT.et"};
    break;
   case 2:
    men = {"{AC230926A001004C}Prefabs/ACR/Infantry/UA_Rifleman.et", "{AC230926A001004D}Prefabs/ACR/Infantry/UA_Leader.et", "{AC230926A001004E}Prefabs/ACR/Infantry/UA_MG.et", "{AC230926A001004F}Prefabs/ACR/Infantry/UA_AR.et", "{AC230926A0010050}Prefabs/ACR/Infantry/UA_GL.et", "{AC230926A0010051}Prefabs/ACR/Infantry/UA_AT.et", "{AC230926A0010052}Prefabs/ACR/Infantry/UA_Marksman.et", "", "", "", "{AC230926A0010053}Prefabs/ACR/Infantry/UA_MGAssistant.et", "{AC230926A0010054}Prefabs/ACR/Infantry/UA_ATAssistant.et", "{AC230926A0010055}Prefabs/ACR/Infantry/UA_Medic.et", "{AC230926A0010056}Prefabs/ACR/Infantry/UA_Engineer.et", "{AC230926A0010057}Prefabs/ACR/Infantry/UA_LightAT.et"};
    break;
   case 3:
    men = {"{AC230926A0010058}Prefabs/ACR/Infantry/Western_Rifleman.et", "{AC230926A0010059}Prefabs/ACR/Infantry/Western_Leader.et", "{AC230926A001005A}Prefabs/ACR/Infantry/Western_MG.et", "{AC230926A001005B}Prefabs/ACR/Infantry/Western_AR.et", "{AC230926A001005C}Prefabs/ACR/Infantry/Western_GL.et", "{AC230926A001005D}Prefabs/ACR/Infantry/Western_AT.et", "{AC230926A001005E}Prefabs/ACR/Infantry/Western_Marksman.et", "", "", "", "{AC230926A001005F}Prefabs/ACR/Infantry/Western_MGAssistant.et", "{AC230926A0010088}Prefabs/ACR/Infantry/Western_ATAssistant.et", "{AC230926A0010087}Prefabs/ACR/Infantry/Western_Medic.et", "{AC230926A0010062}Prefabs/ACR/Infantry/Western_Engineer.et", "{AC230926A0010063}Prefabs/ACR/Infantry/Western_LightAT.et"};
    break;
   case 4:
    men = {"{AC230926A0010064}Prefabs/ACR/Infantry/RUSF_Rifleman.et", "{AC230926A0010065}Prefabs/ACR/Infantry/RUSF_Leader.et", "", "", "{AC230926A0010066}Prefabs/ACR/Infantry/RUSF_GL.et", "", "{AC230926A0010067}Prefabs/ACR/Infantry/RUSF_Marksman.et", "", "", "", "", "", "{AC230926A0010068}Prefabs/ACR/Infantry/RUSF_Medic.et", "", "{AC230926A0010069}Prefabs/ACR/Infantry/RUSF_LightAT.et"};
    break;
   case 5:
    men = {"{AC230926A001006A}Prefabs/ACR/Infantry/UASF_Rifleman.et", "{AC230926A001006B}Prefabs/ACR/Infantry/UASF_Leader.et", "", "{AC230926A001006C}Prefabs/ACR/Infantry/UASF_AR.et", "", "", "{AC230926A001006D}Prefabs/ACR/Infantry/UASF_Marksman.et", "", "", "", "", "", "{AC230926A001006E}Prefabs/ACR/Infantry/UASF_Medic.et", "", "{AC230926A001006F}Prefabs/ACR/Infantry/UASF_LightAT.et"};
    break;
   case 6:
    men = {"{AC230926A0010070}Prefabs/ACR/Infantry/USMC_Rifleman.et", "{AC230926A0010071}Prefabs/ACR/Infantry/USMC_Leader.et", "{AC230926A0010072}Prefabs/ACR/Infantry/USMC_MG.et", "{AC230926A0010073}Prefabs/ACR/Infantry/USMC_AR.et", "{AC230926A0010074}Prefabs/ACR/Infantry/USMC_GL.et", "", "{AC230926A0010075}Prefabs/ACR/Infantry/USMC_Marksman.et", "", "", "", "{AC230926A0010076}Prefabs/ACR/Infantry/USMC_MGAssistant.et", "", "{AC230926A0010077}Prefabs/ACR/Infantry/USMC_Medic.et", "{AC230926A0010078}Prefabs/ACR/Infantry/USMC_Engineer.et", "{AC230926A0010079}Prefabs/ACR/Infantry/USMC_LightAT.et", "{AC230926A001007A}Prefabs/ACR/Infantry/USMC_Sniper.et", "{AC230926A001007B}Prefabs/ACR/Infantry/USMC_Spotter.et"};
    break;
   case 7:
    men = {"{AC230926A001007C}Prefabs/ACR/Infantry/MARSOC_Rifleman.et", "{AC230926A0010082}Prefabs/ACR/Infantry/MARSOC_Leader.et", "", "{AC230926A001007E}Prefabs/ACR/Infantry/MARSOC_AR.et", "", "", "{AC230926A001007F}Prefabs/ACR/Infantry/MARSOC_Marksman.et", "", "", "", "", "", "{AC230926A0010080}Prefabs/ACR/Infantry/MARSOC_Medic.et", "", "{AC230926A0010081}Prefabs/ACR/Infantry/MARSOC_LightAT.et", "", ""};
    break;
  }
  if (role < 0 || role >= men.Count()) return "";
  return men[role];
 }
}
