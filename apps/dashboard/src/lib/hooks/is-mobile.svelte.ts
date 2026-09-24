/*
 * Project Ambrose by Imjustchico
 * The is mobile.svelte hook from shadcn-svelte, copied in and owned: whether the viewport is phone width, read live.
 */

import { MediaQuery } from "svelte/reactivity";

const DEFAULT_MOBILE_BREAKPOINT = 768;

export class IsMobile extends MediaQuery {
    constructor(breakpoint: number = DEFAULT_MOBILE_BREAKPOINT) {
        super(`max-width: ${breakpoint - 1}px`);
    }
}
