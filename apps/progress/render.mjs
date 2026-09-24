/*
 * Project Ambrose by Imjustchico
 * Renders the generated progress card to a PNG with the same browser the front-end tests use, so Discord and any other surface that cannot draw SVG still shows the card.
 */
import { chromium } from "playwright";
import { readFileSync } from "node:fs";

const source = process.argv[2] ?? "doc/progress/progress.svg";
const target = process.argv[3] ?? "progress.png";
const svg = readFileSync(source, "utf8");
const width = Number(/width="(\d+)"/.exec(svg)?.[1] ?? 880);
const height = Number(/height="(\d+)"/.exec(svg)?.[1] ?? 560);

const browser = await chromium.launch();
const page = await browser.newPage({ viewport: { width, height }, deviceScaleFactor: 2 });
await page.setContent(`<body style="margin:0">${svg}</body>`);
await page.screenshot({ path: target });
await browser.close();
console.log(`${target}: ${width}x${height} at 2x`);
