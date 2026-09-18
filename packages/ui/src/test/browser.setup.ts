/*
 * Project Ambrose by Imjustchico
 * What every browser test starts from: the one stylesheet and the dark theme on the root, so a story is measured against the real tokens rather than a bare page, with the browser's own ResizeObserver notice swallowed because it is not a fault in anything under test.
 */

import "../styles/app.css";

document.documentElement.setAttribute("data-theme", "dark");

const resizeNotice = "ResizeObserver loop";

window.addEventListener("error", (event: ErrorEvent) => {
    if (typeof event.message === "string" && event.message.includes(resizeNotice)) {
        event.stopImmediatePropagation();
        event.preventDefault();
    }
});

window.addEventListener("unhandledrejection", (event: PromiseRejectionEvent) => {
    const reason = event.reason as { message?: unknown };
    if (typeof reason?.message === "string" && reason.message.includes(resizeNotice)) {
        event.stopImmediatePropagation();
        event.preventDefault();
    }
});
