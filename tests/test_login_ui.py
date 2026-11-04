"""Exercise the actual embedded login dropdown with a Lua 5.1 compatibility VM."""
from pathlib import Path
import unittest
from lupa.lua51 import LuaRuntime

ROOT = Path(__file__).resolve().parents[1]
UI = (ROOT / "src/login/RecentNames.lua").read_text()
BOOT = r'''
frames, names = {}, {'ALPHA', 'BRAVO', 'CHARLIE', 'DELTA', 'ECHO', 'FOXTROT'}
savedName, passwordFocus, loginCalls, historyOK = 'ALPHA', 0, 0, 1
local methods = {}
function methods:SetWidth(n) self.width=n end
function methods:GetWidth() return self.width or 160 end
function methods:SetHeight(n) self.height=n end
function methods:SetPoint(...) self.point={...} end
function methods:ClearAllPoints() self.point=nil end
function methods:SetAllPoints(...) self.allPoints={...} end
function methods:SetFrameLevel(n) self.level=n end
function methods:GetFrameLevel() return self.level or 2 end
function methods:SetFrameStrata(n) self.strata=n end
function methods:SetNormalTexture(s) assert(type(s)=='string') end
function methods:SetPushedTexture(s) assert(type(s)=='string') end
function methods:SetHighlightTexture(s) assert(type(s)=='string') end
function methods:SetTextInsets(...) self.insets={...} end
function methods:SetBackdrop(...) end
function methods:SetBackdropColor(...) end
function methods:SetBackdropBorderColor(...) end
function methods:EnableMouse(...) end
function methods:EnableKeyboard(...) end
function methods:SetFont(...) end
function methods:SetJustifyH(...) end
function methods:SetTextColor(...) end
function methods:SetChecked(value) self.checked=value end
function methods:Hide() self.shown=false end
function methods:Show() self.shown=true end
function methods:IsShown() return self.shown end
function methods:Enable() self.enabled=true end
function methods:Disable() self.enabled=false end
function methods:SetText(text) self.text=text end
function methods:SetScript(name, fn) self.scripts[name]=fn end
function methods:CreateFontString() return CreateFrame('FontString',nil,self) end
function CreateFrame(kind,name,parent)
 local frame=setmetatable({kind=kind,name=name,parent=parent,scripts={},shown=true,enabled=true}, {__index=methods})
 table.insert(frames,frame)
 if name then _G[name]=frame end
 return frame
end
AccountLoginUI=CreateFrame('Frame')
AccountLoginAccountEdit=CreateFrame('EditBox',nil,AccountLoginUI)
AccountLoginAccountEdit:SetScript('OnEnterPressed',function() error('stock enter handler is preserved') end)
stockEnter=AccountLoginAccountEdit.scripts.OnEnterPressed
AccountLoginPasswordEdit={GetText=function() error('UI must never read passwords') end}
AccountLoginSaveAccountName=CreateFrame('CheckButton')
function AccountLogin_FocusPassword() passwordFocus=passwordFocus+1 end
function AccountLogin_Login() loginCalls=loginCalls+1 end
function SetSavedAccountName(name) savedName=name end
function zLoginHistory(command)
 if command=='clear' then names={}; return historyOK end
 if command=='list' then return table.concat(names,'\n'),historyOK end
 error('unexpected history command')
end
'''


class LoginUITests(unittest.TestCase):
    def lua(self, extra=""):
        lua = LuaRuntime(unpack_returned_tuples=True)
        lua.execute(BOOT + extra)
        self.assertEqual(lua.execute(UI), 1)
        return lua

    def test_popup_shows_only_five_recent_usernames(self):
        lua = self.lua()
        lua.execute('zLoginHistoryUI.arrow.scripts.OnClick()')
        self.assertTrue(lua.eval('zLoginHistoryUI.dismiss:IsShown()'))
        self.assertEqual(lua.eval('zLoginHistoryUI.rows[1].label.text'), 'ALPHA')
        self.assertEqual(lua.eval('zLoginHistoryUI.rows[5].label.text'), 'ECHO')
        self.assertEqual(lua.eval('table.getn(zLoginHistoryUI.rows)'), 5)
        self.assertTrue(lua.eval('zLoginHistoryUI.clear:IsShown()'))

    def test_selection_fills_username_and_focuses_password_without_logging_in(self):
        lua = self.lua()
        lua.execute('zLoginHistoryUI.arrow.scripts.OnClick(); zLoginHistoryUI.rows[2].scripts.OnClick()')
        self.assertEqual(lua.eval('AccountLoginAccountEdit.text'), 'BRAVO')
        self.assertEqual(lua.eval('passwordFocus'), 1)
        self.assertEqual(lua.eval('loginCalls'), 0)
        self.assertEqual(lua.eval('AccountLoginSaveAccountName.checked'), 1)
        self.assertFalse(lua.eval('zLoginHistoryUI.dismiss:IsShown()'))
        self.assertTrue(lua.eval('stockEnter == AccountLoginAccountEdit.scripts.OnEnterPressed'))

    def test_clear_removes_names_and_stock_saved_account(self):
        lua = self.lua()
        lua.execute('zLoginHistoryUI.arrow.scripts.OnClick(); zLoginHistoryUI.clear.scripts.OnClick(); zLoginHistoryUI.arrow.scripts.OnClick()')
        self.assertEqual(lua.eval('table.getn(names)'), 0)
        self.assertEqual(lua.eval('savedName'), '')
        self.assertEqual(lua.eval('zLoginHistoryUI.rows[1].label.text'), 'No saved usernames')
        self.assertFalse(lua.eval('zLoginHistoryUI.rows[1].enabled'))

    def test_unavailable_history_leaves_login_and_manual_entry_usable(self):
        lua = self.lua('historyOK=0\n')
        lua.execute('zLoginHistoryUI.arrow.scripts.OnClick()')
        self.assertEqual(lua.eval('zLoginHistoryUI.rows[1].label.text'), 'History unavailable')
        lua.execute('AccountLoginAccountEdit:SetText("MANUAL"); AccountLogin_Login()')
        self.assertEqual(lua.eval('loginCalls'), 1)
        self.assertFalse(lua.eval('zLoginHistoryUI.dismiss:IsShown()'))

    def test_escape_and_outside_click_close_popup(self):
        lua = self.lua()
        lua.execute('zLoginHistoryUI.arrow.scripts.OnClick(); arg1="ESCAPE"; zLoginHistoryUI.dismiss.scripts.OnKeyDown()')
        self.assertFalse(lua.eval('zLoginHistoryUI.dismiss:IsShown()'))
        lua.execute('zLoginHistoryUI.arrow.scripts.OnClick(); zLoginHistoryUI.dismiss.scripts.OnClick()')
        self.assertFalse(lua.eval('zLoginHistoryUI.dismiss:IsShown()'))

    def test_initialization_is_idempotent(self):
        lua = self.lua()
        count = lua.eval('table.getn(frames)')
        self.assertEqual(lua.execute(UI), 1)
        self.assertEqual(lua.eval('table.getn(frames)'), count)

    def test_glue_native_constructor_fallback_and_late_frame_availability(self):
        lua = LuaRuntime(unpack_returned_tuples=True)
        lua.execute(BOOT + 'zLoginCreateFrame=CreateFrame; CreateFrame=nil; local old=zLoginCreateFrame; zLoginCreateFrame=function(...) CreateFrame=old; return old(unpack(arg)) end')
        self.assertEqual(lua.execute(UI), 1)
        lua = LuaRuntime(unpack_returned_tuples=True)
        lua.execute(BOOT + 'AccountLoginAccountEdit=nil')
        self.assertEqual(lua.execute(UI), 0)


if __name__ == '__main__':
    unittest.main()
