/*
 * Project Ambrose by Imjustchico
 * The app the console and log pages are looking at, shared so the choice follows the operator from page to page, and the one call that points it at an app and opens a page.
 */

export const focus = $state({ app: "gameserver" });

export function openFor(app: string, page: string) {
    focus.app = app;
    window.location.hash = `#${page}`;
}
