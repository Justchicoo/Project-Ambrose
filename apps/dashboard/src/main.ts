/*
 * Project Ambrose by Imjustchico
 * Where the panel starts: the shared stylesheet, then the shell mounted on the one element the page holds.
 */

import "@ambrose/ui/styles.css";
import { mount } from "svelte";
import App from "./App.svelte";

const target = document.getElementById("ambrose-panel");

if (!target) {
    throw new Error("the panel page has no mount point");
}

export default mount(App, { target });
