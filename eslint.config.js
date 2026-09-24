/*
 * Project Ambrose by Imjustchico
 * What the linter enforces on every front-end file: typed rules, the Svelte template rules including its accessibility ones, and the rule that nothing in a shipped bundle may import the accessibility engine.
 */

import js from "@eslint/js";
import svelte from "eslint-plugin-svelte";
import globals from "globals";
import typescript from "typescript-eslint";
import svelteConfig from "./svelte.config.js";

export default typescript.config(
    {
        ignores: [
            "**/node_modules/**",
            "**/dist/**",
            "storybook-static/**",
            "packages/ui/src/tokens/tokens.ts",
            "packages/ui/src/icons/icons.ts",
        ],
    },
    js.configs.recommended,
    ...typescript.configs.recommended,
    ...svelte.configs.recommended,
    {
        languageOptions: {
            globals: { ...globals.browser, ...globals.node },
        },
        rules: {
            "no-restricted-imports": [
                "error",
                {
                    paths: [
                        {
                            name: "axe-core",
                            message:
                                "axe-core is MPL-2.0 and runs only in tests; importing it here would pull it into what the supervisor serves.",
                        },
                    ],
                },
            ],
            "@typescript-eslint/no-unused-vars": ["error", { argsIgnorePattern: "^_", varsIgnorePattern: "^_" }],
        },
    },
    {
        files: ["**/*.svelte", "**/*.svelte.ts"],
        languageOptions: {
            parserOptions: {
                parser: typescript.parser,
                svelteConfig,
            },
        },
    },
);
