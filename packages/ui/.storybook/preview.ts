/*
 * Project Ambrose by Imjustchico
 * What every story inherits: the one stylesheet, the dark and light themes as a toolbar choice, and the accessibility check set to fail the run on an error.
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
