/*
 * Project Ambrose by Imjustchico
 * What the canary story inherits: the same stylesheet, the same themes and the same accessibility check, so the only difference from the real gallery is the story itself.
 */

import type { Preview } from "@storybook/svelte-vite";
import "../src/styles/app.css";

const preview: Preview = {
    parameters: {
        a11y: {
            test: "error",
            config: {
                rules: [{ id: "color-contrast", enabled: true }],
            },
        },
        controls: {
            matchers: {
                color: /(background|color)$/i,
            },
        },
        backgrounds: { disable: true },
    },
    globalTypes: {
        theme: {
            description: "Which Ambrose theme the story is drawn in",
            toolbar: {
                title: "Theme",
                items: [
                    { value: "dark", title: "Dark" },
                    { value: "light", title: "Light" },
                ],
                dynamicTitle: true,
            },
        },
    },
    initialGlobals: {
        theme: "dark",
    },
    decorators: [
        (story, context) => {
            const theme = (context.globals.theme as string) ?? "dark";
            document.documentElement.setAttribute("data-theme", theme);
            document.body.classList.add("bg-surface-page", "text-fg-body", "p-20");
            return story();
        },
    ],
};

export default preview;
