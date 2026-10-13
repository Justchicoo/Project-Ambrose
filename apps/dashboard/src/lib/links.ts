/*
 * Project Ambrose by Imjustchico
 * The one-time links a page can be opened from: the owner claim link the supervisor prints, a password link and a sign-in link a desktop program opens, each carrying its token, and a pairing line its certificate pin, in the address fragment the browser never sends to a server; the page, token and pin are read from an address by a pure function, and the link the page arrived with is read once as the page loads and taken out of the address and the browser's history at once, so the page lands on the overview and nothing keeps the token. An invite link carries the token of a one-time invitation instead, and the claim page reads the invitation before asking for a name and password.
 */

export type LinkPage = "claim" | "password" | "link" | "invite";

export type Arrival = { page: LinkPage; token: string; pin: string };

const pages: ReadonlySet<string> = new Set<LinkPage>(["claim", "password", "link", "invite"]);

export function readLink(hash: string): Arrival | null {
    const fragment = hash.replace(/^#\/?/, "");
    const question = fragment.indexOf("?");
    const page = question < 0 ? fragment : fragment.slice(0, question);
    if (!pages.has(page) || question < 0) return null;
    const query = new URLSearchParams(fragment.slice(question + 1));
    const token = query.get("token") ?? "";
    if (token === "") return null;
    return { page: page as LinkPage, token, pin: page === "link" ? (query.get("sha256") ?? "") : "" };
}

export function takeArrival(): Arrival | null {
    if (typeof window === "undefined") return null;
    const found = readLink(window.location.hash);
    if (found === null) return null;
    history.replaceState(history.state, "", `${window.location.pathname}${window.location.search}#overview`);
    window.dispatchEvent(new HashChangeEvent("hashchange"));
    return found;
}

export const arrivedWith: { link: Arrival | null } = { link: takeArrival() };
