/*
 * Project Ambrose by Imjustchico
 * The power buttons' answer while the panel runs on sample data: a toast naming what would have been sent to which app, and saying plainly that nothing was.
 */

import { toast } from "svelte-sonner";

export type PowerAction = "start" | "restart" | "stop";

const verbs: Record<PowerAction, string> = { start: "Start", restart: "Restart", stop: "Stop" };

export function requestPower(action: PowerAction, target: string) {
    toast(`${verbs[action]} ${target}`, { description: "Sample data: no server received this." });
}
