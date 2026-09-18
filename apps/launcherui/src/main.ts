/*
 * Project Ambrose by Imjustchico
 * Where the launcher window starts: the same stylesheet the panel loads, then the window mounted on the one element the page holds.
 */

import "@ambrose/ui/styles.css";
import { mount } from "svelte";
import App from "./App.svelte";

const target = document.getElementById("ambrose-launcher");

if (!target) {
    throw new Error("the launcher page has no mount point");
}

export default mount(App, { target });
