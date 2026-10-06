/*
 * Project Ambrose by Imjustchico
 * Where the panel program's screens start: the stylesheet every surface loads, then the program mounted on the one element the page holds.
 */

import "@ambrose/ui/styles.css";
import { mount } from "svelte";
import App from "./App.svelte";

const target = document.getElementById("ambrose-panel-program");

if (!target) {
    throw new Error("the panel program's page has no mount point");
}

export default mount(App, { target });
