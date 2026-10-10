/*
 * Project Ambrose by Imjustchico
 * The sign-in captcha widget's provider map: which script to load and which global renders the challenge for each provider the panel knows, with the script load shared so the page never fetches it twice. The three providers expose the same explicit-render shape, so one component drives them all.
 */

export type CaptchaProvider = "recaptcha" | "hcaptcha" | "turnstile";

export interface CaptchaWidgetApi {
    render(
        container: HTMLElement,
        options: {
            sitekey: string;
            callback: (token: string) => void;
            "expired-callback": () => void;
            "error-callback": () => void;
        },
    ): number;
    getResponse(widgetId?: number): string;
    reset(widgetId?: number): void;
}

export interface CaptchaConfig {
    provider: CaptchaProvider;
    scriptUrl: string;
    globalName: string;
}

export function captchaConfig(provider: string): CaptchaConfig | null {
    switch (provider) {
        case "recaptcha":
            return {
                provider,
                scriptUrl: "https://www.google.com/recaptcha/api.js?render=explicit",
                globalName: "grecaptcha",
            };
        case "hcaptcha":
            return {
                provider,
                scriptUrl: "https://js.hcaptcha.com/1/api.js?render=explicit",
                globalName: "hcaptcha",
            };
        case "turnstile":
            return {
                provider,
                scriptUrl: "https://challenges.cloudflare.com/turnstile/v0/api.js?render=explicit",
                globalName: "turnstile",
            };
        default:
            return null;
    }
}

const loading = new Map<string, Promise<void>>();

function globalApi(config: CaptchaConfig): CaptchaWidgetApi | undefined {
    const api = (window as unknown as Record<string, unknown>)[config.globalName];
    return typeof api === "object" && api !== null ? (api as CaptchaWidgetApi) : undefined;
}

export function loadCaptchaScript(config: CaptchaConfig): Promise<void> {
    const known = loading.get(config.scriptUrl);
    if (known) return known;
    const loaded = new Promise<void>((resolve, reject) => {
        if (globalApi(config)) {
            resolve();
            return;
        }
        const script = document.createElement("script");
        script.src = config.scriptUrl;
        script.async = true;
        script.defer = true;
        const timeout = window.setTimeout(() => {
            script.remove();
            loading.delete(config.scriptUrl);
            reject(new Error("The captcha script did not load"));
        }, 15000);
        script.onload = () => {
            const started = Date.now();
            const wait = () => {
                if (globalApi(config)) {
                    window.clearTimeout(timeout);
                    resolve();
                } else if (Date.now() - started > 10000) {
                    window.clearTimeout(timeout);
                    script.remove();
                    loading.delete(config.scriptUrl);
                    reject(new Error("The captcha script did not start"));
                } else {
                    window.setTimeout(wait, 100);
                }
            };
            wait();
        };
        script.onerror = () => {
            window.clearTimeout(timeout);
            script.remove();
            loading.delete(config.scriptUrl);
            reject(new Error("The captcha script could not be fetched"));
        };
        document.head.appendChild(script);
    });
    loading.set(config.scriptUrl, loaded);
    return loaded;
}

export function renderCaptcha(
    config: CaptchaConfig,
    container: HTMLElement,
    siteKey: string,
    onToken: (token: string) => void,
    onExpired: () => void,
): number {
    const api = globalApi(config);
    if (!api) throw new Error("The captcha script is not ready");
    return api.render(container, {
        sitekey: siteKey,
        callback: onToken,
        "expired-callback": onExpired,
        "error-callback": onExpired,
    });
}

export function resetCaptcha(config: CaptchaConfig, widgetId: number | null): void {
    const api = globalApi(config);
    if (!api) return;
    if (widgetId === null) api.reset();
    else api.reset(widgetId);
}
