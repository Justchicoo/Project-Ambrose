/*
 * Project Ambrose by Imjustchico
 * Where the panel starts: its own stylesheet, then the shell mounted on the one element the page holds.
 */

import "./app.css";
import { mount } from "svelte";
import App from "./App.svelte";

const target = document.getElementById("ambrose-panel");

if (!target) {
    throw new Error("the panel page has no mount point");
}

export default mount(App, { target });
