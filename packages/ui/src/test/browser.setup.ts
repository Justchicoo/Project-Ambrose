/*
 * Project Ambrose by Imjustchico
 * What every browser test starts from: the one stylesheet and the dark theme on the root, so a story is measured against the real tokens rather than a bare page.
 */

import "../styles/app.css";

document.documentElement.setAttribute("data-theme", "dark");
