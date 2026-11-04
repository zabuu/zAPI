-- Embedded by zAPI. Runs in the login Lua state; no password value is read.
if zLoginHistoryUI then return 1 end
local edit = AccountLoginAccountEdit
local parent = AccountLoginUI
local create = CreateFrame or zLoginCreateFrame
if not edit or not parent or not create or not zLoginHistory then return 0 end

local arrow = create("Button", "zLoginHistoryArrow", parent)
arrow:SetWidth(22); arrow:SetHeight(22)
arrow:SetPoint("RIGHT", edit, "RIGHT", -5, 0)
arrow:SetFrameLevel(edit:GetFrameLevel() + 3)
arrow:SetNormalTexture("Interface\\Buttons\\UI-ScrollBar-ScrollDownButton-Up")
arrow:SetPushedTexture("Interface\\Buttons\\UI-ScrollBar-ScrollDownButton-Down")
arrow:SetHighlightTexture("Interface\\Buttons\\UI-ScrollBar-ScrollDownButton-Highlight")
edit:SetTextInsets(8, 28, 0, 0)

local dismiss = create("Button", "zLoginHistoryDismiss", parent)
dismiss:SetAllPoints(parent)
dismiss:SetFrameStrata("DIALOG")
dismiss:SetFrameLevel(edit:GetFrameLevel() + 10)
dismiss:Hide()
local menu = create("Frame", "zLoginHistoryMenu", dismiss)
menu:SetWidth(edit:GetWidth()); menu:SetHeight(40)
menu:SetPoint("TOPRIGHT", edit, "BOTTOMRIGHT", 0, 3)
menu:SetFrameLevel(dismiss:GetFrameLevel() + 1)
menu:SetBackdrop({bgFile="Interface\\Tooltips\\UI-Tooltip-Background",
  edgeFile="Interface\\Tooltips\\UI-Tooltip-Border", tile=true, tileSize=16,
  edgeSize=12, insets={left=3, right=3, top=3, bottom=3}})
menu:SetBackdropColor(0.06, 0.04, 0.03, 0.98)
menu:SetBackdropBorderColor(0.65, 0.50, 0.25, 1)
menu:EnableMouse(true)

local rows = {}
local function MakeRow(index)
  local row = create("Button", nil, menu)
  row:SetWidth(menu:GetWidth() - 12); row:SetHeight(22)
  row:SetPoint("TOPLEFT", menu, "TOPLEFT", 6, -6 - (index - 1) * 22)
  local label = row:CreateFontString(nil, "OVERLAY")
  label:SetFont("Fonts\\FRIZQT__.TTF", 12)
  label:SetPoint("LEFT", row, "LEFT", 5, 0)
  label:SetWidth(row:GetWidth() - 10); label:SetJustifyH("LEFT")
  label:SetTextColor(1, 0.85, 0.50)
  row.label = label
  row:SetHighlightTexture("Interface\\QuestFrame\\UI-QuestTitleHighlight")
  row:SetScript("OnClick", function()
    edit:SetText(row.username)
    if AccountLoginSaveAccountName then AccountLoginSaveAccountName:SetChecked(1) end
    dismiss:Hide()
    AccountLogin_FocusPassword()
  end)
  return row
end
for i = 1, 5 do rows[i] = MakeRow(i) end
local clear = MakeRow(6)
clear.label:SetText("Clear saved usernames")
clear.label:SetFont("Fonts\\FRIZQT__.TTF", 10)
clear:SetScript("OnClick", function()
  if zLoginHistory("clear") == 1 then
    SetSavedAccountName("")
    if AccountLoginSaveAccountName then AccountLoginSaveAccountName:SetChecked(nil) end
    dismiss:Hide()
  end
end)

local function Refresh()
  local text, ok = zLoginHistory("list")
  local names = {}
  if ok == 1 and type(text) == "string" then
    for name in string.gfind(text, "[^\n]+") do
      if table.getn(names) < 5 then table.insert(names, name) end
    end
  end
  local count = table.getn(names)
  for i = 1, 5 do
    rows[i]:Hide()
    if names[i] then
      rows[i].username = names[i]
      rows[i].label:SetText(names[i]); rows[i]:Enable(); rows[i]:Show()
    end
  end
  clear:Hide()
  if count == 0 then
    rows[1].username = nil
    rows[1].label:SetText(ok == 1 and "No saved usernames" or "History unavailable")
    rows[1]:Disable(); rows[1]:Show()
    menu:SetHeight(34)
  else
    clear:ClearAllPoints()
    clear:SetPoint("TOPLEFT", menu, "TOPLEFT", 6, -8 - count * 22)
    clear:Show()
    menu:SetHeight(16 + (count + 1) * 22)
  end
end
arrow:SetScript("OnClick", function()
  if dismiss:IsShown() then dismiss:Hide() else Refresh(); dismiss:Show() end
end)
dismiss:SetScript("OnClick", function() dismiss:Hide() end)
dismiss:EnableKeyboard(true)
dismiss:SetScript("OnKeyDown", function() if arg1 == "ESCAPE" then dismiss:Hide() end end)
local oldLogin = AccountLogin_Login
if type(oldLogin) == "function" then
  AccountLogin_Login = function() dismiss:Hide(); return oldLogin() end
end
zLoginHistoryUI = {arrow=arrow, menu=menu, dismiss=dismiss, rows=rows, clear=clear}
return 1
