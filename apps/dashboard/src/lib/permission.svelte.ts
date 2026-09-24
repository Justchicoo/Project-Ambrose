/*
 * Project Ambrose by Imjustchico
 * What the signed-in operator may do, read from the answer that signed them in rather than guessed from their role, so the page hides a control for the same reason the server would refuse it. A role's permissions hold everywhere; a grant holds on the one app it names, so asking about an app is a different question from asking in general, and asking about an app nobody granted anything on answers no, which is what the server says too by not admitting the app is there. A session carrying no panel user is one opened with the admin token, which the server treats as holding everything, so the page does too rather than hiding controls the caller would in fact be allowed to use.
 */

import { session } from "./api.svelte";

export function may(permission: string, app?: string): boolean {
    const user = session.user;
    if (!user) return session.state !== "signed-out";
    if (user.permissions.includes(permission)) return true;
    if (app === undefined) return false;
    return (user.grants[app] ?? []).includes(permission);
}

export function sees(app: string): boolean {
    const user = session.user;
    if (!user) return session.state !== "signed-out";
    return user.permissions.length > 0 || (user.grants[app] ?? []).length > 0;
}
