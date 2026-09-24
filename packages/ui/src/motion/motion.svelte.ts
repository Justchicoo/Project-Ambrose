/*
 * Project Ambrose by Imjustchico
 * The one motion module: the four durations, the reduced-motion flag that combines the media query with the viewer's own setting, and the rule that every duration collapses to zero when it is set.
 */

import { durationMs, easing, type DurationName } from "../tokens/tokens";

export type MotionSetting = "system" | "on" | "off";

export const motionSettings: readonly MotionSetting[] = ["system", "on", "off"] as const;

const QUERY = "(prefers-reduced-motion: reduce)";

class Motion {
    systemAsksForLess = $state(false);
    setting = $state<MotionSetting>("system");

    constructor() {
        if (typeof window === "undefined" || typeof window.matchMedia !== "function") {
            return;
        }
        const media = window.matchMedia(QUERY);
        this.systemAsksForLess = media.matches;
        media.addEventListener("change", (event) => {
            this.systemAsksForLess = event.matches;
        });
    }

    get off(): boolean {
        if (this.setting === "off") {
            return true;
        }
        if (this.setting === "on") {
            return false;
        }
        return this.systemAsksForLess;
    }

    duration(name: DurationName): number {
        return this.off ? 0 : durationMs[name];
    }

    durations(): Record<DurationName, number> {
        const out = {} as Record<DurationName, number>;
        for (const name of Object.keys(durationMs) as DurationName[]) {
            out[name] = this.duration(name);
        }
        return out;
    }

    choose(setting: MotionSetting): void {
        this.setting = setting;
        if (typeof document !== "undefined") {
            if (setting === "system") {
                document.documentElement.removeAttribute("data-motion");
            } else {
                document.documentElement.setAttribute("data-motion", setting);
            }
        }
    }
}

export const motion = new Motion();

export { durationMs, easing };
export type { DurationName };
