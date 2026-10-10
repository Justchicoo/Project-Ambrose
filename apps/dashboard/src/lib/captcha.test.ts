/*
 * Project Ambrose by Imjustchico
 * Tests the sign-in captcha provider map: each provider the panel knows names its script and its render global, and anything else maps to nothing so the page says so instead of loading a stranger's script.
 */

import { describe, expect, it } from "vitest";
import { captchaConfig } from "./captcha";

describe("the sign-in captcha provider map", () => {
    it("names the script and render global for every provider the panel knows", () => {
        expect(captchaConfig("recaptcha")).toEqual({
            provider: "recaptcha",
            scriptUrl: "https://www.google.com/recaptcha/api.js?render=explicit",
            globalName: "grecaptcha",
        });
        expect(captchaConfig("hcaptcha")).toEqual({
            provider: "hcaptcha",
            scriptUrl: "https://js.hcaptcha.com/1/api.js?render=explicit",
            globalName: "hcaptcha",
        });
        expect(captchaConfig("turnstile")).toEqual({
            provider: "turnstile",
            scriptUrl: "https://challenges.cloudflare.com/turnstile/v0/api.js?render=explicit",
            globalName: "turnstile",
        });
    });

    it("maps nothing for an unknown provider or for off", () => {
        expect(captchaConfig("off")).toBeNull();
        expect(captchaConfig("")).toBeNull();
        expect(captchaConfig("some-other-captcha")).toBeNull();
    });
});
