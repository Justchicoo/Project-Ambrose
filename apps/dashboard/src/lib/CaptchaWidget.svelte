<!-- Project Ambrose by Imjustchico: The sign-in captcha challenge. Renders the provider's widget once the script the panel named loads, hands the solved token to the page, and can be reset when an answer is refused. A provider the panel does not know, or a script that will not load, is said plainly instead of leaving an empty box. -->
<script lang="ts">
    import { captchaConfig, loadCaptchaScript, renderCaptcha, resetCaptcha } from "$lib/captcha.js";

    interface Props {
        provider: string;
        siteKey: string;
        scriptUrl: string;
        onToken: (token: string) => void;
        onExpired?: () => void;
    }

    let { provider, siteKey, scriptUrl, onToken, onExpired }: Props = $props();

    let container: HTMLDivElement | undefined = $state();
    let widgetId: number | null = $state(null);
    let failed = $state(false);

    $effect(() => {
        const config = captchaConfig(provider, scriptUrl);
        if (!config || !container) {
            failed = true;
            return;
        }
        let cancelled = false;
        const box = container;
        void (async () => {
            try {
                await loadCaptchaScript(config);
                if (cancelled) return;
                widgetId = renderCaptcha(
                    config,
                    box,
                    siteKey,
                    (token) => onToken(token),
                    () => {
                        widgetId = null;
                        onExpired?.();
                    },
                );
            } catch {
                if (!cancelled) failed = true;
            }
        })();
        return () => {
            cancelled = true;
        };
    });

    export function reset() {
        const config = captchaConfig(provider, scriptUrl);
        if (!config) return;
        resetCaptcha(config, widgetId);
        widgetId = null;
    }
</script>

<div bind:this={container} aria-label="Captcha challenge"></div>
{#if failed}
    <p class="text-sm text-destructive" role="alert">
        The captcha could not be loaded. Check the provider and site key in the panel settings.
    </p>
{/if}
