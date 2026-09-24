/*
 * Project Ambrose by Imjustchico
 * The panel's light and dark choice: system, light or dark, remembered in this browser, resolved against the system setting as it changes, and written to the page root so the tokens and every component's dark styles follow it.
 */

export type ThemeChoice = "system" | "light" | "dark";

const storageKey = "ambrose.panel.theme";
const systemLight = window.matchMedia("(prefers-color-scheme: light)");

function remembered(): ThemeChoice {
    try {
        const saved = window.localStorage.getItem(storageKey);
        if (saved === "system" || saved === "light" || saved === "dark") return saved;
    } catch {
        return "system";
    }
    return "system";
}

export const theme = $state({ choice: remembered(), resolved: "dark" as "light" | "dark" });

function apply() {
    theme.resolved = theme.choice === "system" ? (systemLight.matches ? "light" : "dark") : theme.choice;
    document.documentElement.dataset.theme = theme.resolved;
}

export function chooseTheme(choice: ThemeChoice) {
    theme.choice = choice;
    apply();
    try {
        window.localStorage.setItem(storageKey, choice);
    } catch {
        return;
    }
}

systemLight.addEventListener("change", apply);
apply();
