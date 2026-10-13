/*
 * Project Ambrose by Imjustchico
 * Tests the sign-in captcha provider map: each provider the panel knows keeps the script address the panel named and its render global, and anything else maps to nothing, an unknown provider or an empty script address, so the page says so instead of loading a stranger's script.
 */

import { describe, expect, it } from "vitest";
import { captchaConfig } from "./captcha";

describe("the sign-in captcha provider map", () => {
    it("keeps the panel's script and render global for every provider the panel knows", () => {
        expect(captchaConfig("recaptcha", "https://captcha.example/recaptcha/api.js")).toEqual({
            provider: "recaptcha",
            scriptUrl: "https://captcha.example/recaptcha/api.js",
            globalName: "grecaptcha",
        });
        expect(captchaConfig("hcaptcha", "https://captcha.example/hcaptcha/api.js")).toEqual({
            provider: "hcaptcha",
            scriptUrl: "https://captcha.example/hcaptcha/api.js",
            globalName: "hcaptcha",
        });
        expect(captchaConfig("turnstile", "https://captcha.example/turnstile/api.js")).toEqual({
            provider: "turnstile",
            scriptUrl: "https://captcha.example/turnstile/api.js",
            globalName: "turnstile",
        });
    });

    it("maps nothing for an unknown provider, for off, or for an empty script address", () => {
        expect(captchaConfig("off", "https://captcha.example/api.js")).toBeNull();
        expect(captchaConfig("", "https://captcha.example/api.js")).toBeNull();
        expect(captchaConfig("some-other-captcha", "https://captcha.example/api.js")).toBeNull();
        expect(captchaConfig("recaptcha", "")).toBeNull();
    });
});
