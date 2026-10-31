// Our actions in the game's Controls menu: an "AC: Actual" category (the
// world ping key ships unbound; players pick a key there).
modded class SCR_KeybindSetting
{
 override protected void InsertCategoriesToComboBox()
 {
  if (m_KeybindConfig && m_KeybindConfig.m_KeyBindingCategories)
  {
   bool present = false;
   foreach (SCR_KeyBindingCategory existing : m_KeybindConfig.m_KeyBindingCategories)
    if (existing && existing.m_sName == "ACR") present = true;
   if (!present)
   {
    SCR_KeyBindingCategory category = new SCR_KeyBindingCategory();
    category.m_sName = "ACR";
    category.m_sDisplayName = "AC: Actual";
    category.m_KeyBindingEntries = {};
    category.m_KeyBindingEntries.Insert(Entry("ACR_Ping", "Ping (tap: look, hold: pick)", "pressed"));
    category.m_KeyBindingEntries.Insert(Entry("ACR_Map", "Command map", "click"));
    category.m_KeyBindingEntries.Insert(Entry("ACR_Possess", "Take over soldier", "click"));
    category.m_KeyBindingEntries.Insert(Entry("ACR_Return", "Command unit", "click"));
    category.m_KeyBindingEntries.Insert(Entry("ACR_RTSCamera", "RTS 3D camera", "click"));
    category.m_KeyBindingEntries.Insert(Entry("ACR_RTSHideUI", "RTS camera: hide the UI", "click"));
    category.m_KeyBindingEntries.Insert(Entry("ACR_RTSFollow", "RTS camera: follow the selected squad", "click"));
    category.m_KeyBindingEntries.Insert(Entry("ACR_TwoICPanel", "2IC panel (map, with the 2IC on)", "click"));
    category.m_KeyBindingEntries.Insert(Entry("ACR_KeyHints", "Key reminders on/off", "click"));
    m_KeybindConfig.m_KeyBindingCategories.Insert(category);
   }
  }
  super.InsertCategoriesToComboBox();
 }

 // preset: the binding kind the Controls menu adds (vanilla names: "click", "pressed",
 // "hold"); without one its Add button stays grey. Our default keys carry the same
 // FilterPreset in ACR_Input.conf so the menu lists them.
 protected SCR_KeyBindingEntry Entry(string action, string name, string preset)
 {
  SCR_KeyBindingEntry entry = new SCR_KeyBindingEntry();
  entry.m_sActionName = action;
  entry.m_sDisplayName = name;
  entry.m_sPreset = preset;
  // Script-made entries skip the config defaults: the Controls menu reads the platform
  // list unchecked (a null one crashed it and hid our category).
  entry.m_aPlatforms = {};
  entry.m_ePrefixType = SCR_EActionPrefixType.NON_CONTINUOUS;
  return entry;
 }
}
