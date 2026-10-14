/*
 * Project Ambrose by Imjustchico
 * Where the Studio window starts: the same stylesheet the panel loads, then the Studio mounted on the one element the page holds.
 */

import "@ambrose/ui/styles.css";
import { mount } from "svelte";
import App from "./App.svelte";

const target = document.getElementById("ambrose-studio");

if (!target) {
    throw new Error("the Studio page has no mount point");
}

export default mount(App, { target });
