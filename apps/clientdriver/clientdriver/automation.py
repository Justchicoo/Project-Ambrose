# Project Ambrose by Imjustchico
# Reads and presses the launcher window through UI Automation, the accessibility interface every Windows control and web view answers, so the driver reads what the window shows as the text a screen reader would hear and presses a control by its accessible name with its invoke pattern, with no cursor, no foreground and no global input; comtypes is imported only when a window is really read, so the unit tests hand the engine a fake in its place.
from .errors import StepFailed

UIA_INVOKE_PATTERN_ID = 10000
UIA_VALUE_PATTERN_ID = 10002
TREE_SCOPE_DESCENDANTS = 4


def _interface():
    import comtypes.client

    comtypes.client.GetModule("UIAutomationCore.dll")
    from comtypes.gen import UIAutomationClient

    return UIAutomationClient, comtypes.client.CreateObject(UIAutomationClient.CUIAutomation, interface=UIAutomationClient.IUIAutomation)


def _value_of(library, element):
    try:
        pattern = element.GetCurrentPattern(UIA_VALUE_PATTERN_ID)
        if pattern:
            return pattern.QueryInterface(library.IUIAutomationValuePattern).CurrentValue or ""
    except Exception:
        return ""
    return ""


class WindowAutomation:
    def _elements(self, handle):
        library, automation = _interface()
        root = automation.ElementFromHandle(handle)
        found = root.FindAll(TREE_SCOPE_DESCENDANTS, automation.CreateTrueCondition())
        elements = [root] + [found.GetElement(index) for index in range(found.Length)]
        return library, elements

    def texts(self, handle):
        library, elements = self._elements(handle)
        said = []
        for element in elements:
            for text in (element.CurrentName or "", _value_of(library, element)):
                text = text.strip()
                if text and text not in said:
                    said.append(text)
        return said

    def press(self, handle, control):
        library, elements = self._elements(handle)
        named = [element for element in elements if (element.CurrentName or "").strip() == control]
        if not named:
            raise StepFailed(f"the launcher window holds no control named {control!r}")
        for element in named:
            pattern = element.GetCurrentPattern(UIA_INVOKE_PATTERN_ID)
            if pattern:
                pattern.QueryInterface(library.IUIAutomationInvokePattern).Invoke()
                return f"invoked {control!r} through UI Automation"
        raise StepFailed(f"the launcher window's {control!r} cannot be invoked, so it is not a button")
