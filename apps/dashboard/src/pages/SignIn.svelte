<!-- Project Ambrose by Imjustchico: The page a signed-out browser sees, which asks for what the host it came from signs people in with: a panel with no operator yet asks for the token from the one-time link the supervisor printed, which the page took out of the address and the browser's history as soon as it loaded so it lands on the overview, and the name and password to make the owner with, a password link asks for the new password and signs in with it, a panel that has an operator asks for a name and password and then, for an operator with two-factor sign-in, for a code from their authenticator app or one recovery code, opening straight on that step when a sign-in link asked for it, with the password dropped from the page as soon as it has been checked and a sign-in that ran out sent back to the start, and an app's own listener asks for its admin token, which is traded once for a session so nothing is kept in the browser; a field's own problem sits beside it, any other refusal sits above the form with its request id, a link that did not sign in says why above the form, and a session that has just ended says so. After repeated failed sign-ins with a captcha configured, the page says a captcha is being asked for, was refused or could not be checked, rather than calling it a wrong password. -->
<script lang="ts">
    import * as Card from "$lib/components/ui/card/index.js";
    import { Button } from "$lib/components/ui/button/index.js";
    import { Input } from "$lib/components/ui/input/index.js";
    import { Label } from "$lib/components/ui/label/index.js";
    import { ApiError, answerSecondFactor, claimOwner, session, setPassword, signIn, signInAsUser } from "$lib/api.svelte.js";
    import { arrivedWith } from "$lib/links.js";
    import { readCode, readRecoveryCode } from "$lib/twofactor.js";
    import CircleAlertIcon from "@lucide/svelte/icons/circle-alert";
    import LogInIcon from "@lucide/svelte/icons/log-in";

    type Fields = Record<string, string>;

    const arrived = arrivedWith.link;
    if (arrived !== null && arrived.page !== "link") arrivedWith.link = null;

    let token = $state(arrived !== null && arrived.page === "claim" ? arrived.token : "");
    let resetToken = $state(arrived !== null && arrived.page === "password" ? arrived.token : "");
    let username = $state("");
    let password = $state("");
    let again = $state("");
    let busy = $state(false);
    let fields = $state<Fields>({});
    let problem = $state<ApiError | null>(null);
    let requestId = $state("");
    let step = $state<"password" | "second-factor">(session.secondFactorPending ? "second-factor" : "password");
    let code = $state("");
    let recovery = $state(false);
    let notice = $state("");
    session.secondFactorPending = false;

    const claiming = $derived(session.panel && session.needsOwner);
    const asUser = $derived(session.panel && !session.needsOwner);
    const secondStep = $derived(asUser && step === "second-factor");
    const resetting = $derived(session.panel && resetToken !== "" && !secondStep);
    const linkProblem = $derived(session.linkProblem);
    const mine = new Set(["token", "username", "password", "again", "code", "recovery_code"]);
    const others = $derived(Object.entries(fields).filter(([field]) => !mine.has(field)));

    function describe(error: ApiError): string {
        if (error.code === "wrong_token") return "That is not this app's admin token.";
        if (error.code === "sign_in_refused") return "That name and password do not sign in.";
        if (error.code === "not_this_machine") return "The first operator is made from the machine the supervisor runs on.";
        if (error.code === "link_refused") return "That is not the token the supervisor printed.";
        if (error.code === "link_expired") return "That link has been used or has run out. Restart the supervisor for another.";
        if (error.code === "already_claimed") return "This panel already has an operator. Sign in with a name and password.";
        if (error.code === "second_factor_refused") return "That code does not sign you in.";
        if (error.code === "captcha_required") return "Too many failed sign-ins for this name. Wait a few minutes and try again.";
        if (error.code === "captcha_invalid") return "The captcha answer was refused. Try again.";
        if (error.code === "captcha_unreachable") return "The captcha could not be checked, so the sign-in was refused. Try again later.";
        if (error.status === 429) return "Too many attempts. Wait a moment and try again.";
        if (error.status === 403)
            return "The server refused a sign-in from this page's address. Open the panel from the address it prints.";
        if (error.status === 0) return "The panel could not reach its server. Check that it is still running.";
        return error.message;
    }

    function describeLink(error: ApiError): string {
        if (error.code === "link_expired") return "That link has been used or has run out. Ask for another.";
        if (error.code === "link_refused") return "That link does not sign anyone in.";
        if (error.code === "not_this_machine") return "That link opens the panel only on the machine it runs on.";
        if (error.code === "plain_http_remote") return "This panel serves plain HTTP beyond its own machine, so it takes no pairing link.";
        if (error.status === 429) return "Too many links were tried from here. Wait a minute and try again.";
        if (error.status === 0) return "The panel could not reach its server. Check that it is still running.";
        return error.message;
    }

    function required(): Fields {
        const missing: Fields = {};
        if (secondStep) {
            if (recovery && readRecoveryCode(code) === null) missing.recovery_code = "Enter one recovery code, ten characters.";
            if (!recovery && readCode(code) === null) missing.code = "Enter the six digits your authenticator app shows.";
            return missing;
        }
        if (resetting) {
            if (password === "") missing.password = "Choose a password of at least twelve characters.";
            else if (again !== password) missing.again = "Type the same password again.";
            return missing;
        }
        if (claiming && token.trim() === "") missing.token = "Paste the token from the link the supervisor printed.";
        if (asUser || claiming) {
            if (username.trim() === "") missing.username = claiming ? "Choose the name you will sign in with." : "Enter your name.";
            if (password === "") missing.password = claiming ? "Choose a password of at least twelve characters." : "Enter your password.";
        } else if (token.trim() === "") {
            missing.token = "Enter this app's admin token.";
        }
        return missing;
    }

    function startAgain(message: string) {
        step = "password";
        code = "";
        recovery = false;
        notice = message;
    }

    function switchFactor() {
        recovery = !recovery;
        code = "";
        fields = {};
    }

    function toCodeStep() {
        step = "second-factor";
        recovery = false;
        code = "";
    }

    async function submit(event: SubmitEvent) {
        event.preventDefault();
        problem = null;
        requestId = "";
        notice = "";
        session.linkProblem = null;
        fields = required();
        if (Object.keys(fields).length > 0) return;
        busy = true;
        try {
            if (secondStep) {
                await answerSecondFactor(recovery ? { recovery_code: readRecoveryCode(code) ?? code } : { code: readCode(code) ?? code });
                code = "";
            } else if (resetting) {
                const next = await setPassword(resetToken, password);
                resetToken = "";
                if (next === "second-factor") toCodeStep();
            } else if (claiming) await claimOwner(token.trim(), username.trim(), password);
            else if (asUser) {
                if ((await signInAsUser(username.trim(), password)) === "second-factor") toCodeStep();
            } else await signIn(token.trim());
            token = "";
            password = "";
            again = "";
        } catch (failure) {
            if (failure instanceof ApiError && secondStep && failure.code === "challenge_expired") {
                startAgain("That sign-in ran out. Enter your name and password again.");
                requestId = failure.requestId;
            } else if (failure instanceof ApiError) {
                fields = failure.fields;
                problem = Object.keys(failure.fields).length === 0 ? failure : null;
                requestId = failure.requestId;
                if (resetting && failure.status === 410) {
                    session.linkProblem = failure;
                    problem = null;
                    requestId = "";
                    resetToken = "";
                    password = "";
                    again = "";
                }
            } else {
                problem = new ApiError(0, "failed", "Signing in failed for a reason the panel could not read.", "");
            }
        } finally {
            busy = false;
        }
    }
</script>

<main class="flex min-h-svh items-center justify-center bg-background p-4">
    <Card.Root class="w-full max-w-sm shadow-xs">
        <Card.Header>
            <div
                class="mb-2 flex size-10 items-center justify-center rounded-lg bg-primary font-serif text-xl font-bold text-primary-foreground"
            >
                A
            </div>
            <Card.Title class="font-serif text-2xl">
                <h1>
                    {resetting
                        ? "Set your password"
                        : claiming
                          ? "Make the first operator"
                          : secondStep
                            ? "Enter your code"
                            : "Sign in to Ambrose"}
                </h1>
            </Card.Title>
            <Card.Description>
                {#if secondStep}
                    {recovery
                        ? "Enter one of the recovery codes you saved when you turned on two-factor sign-in. Each works once."
                        : "Enter the six-digit code your authenticator app shows for this panel."}
                {:else if resetting}
                    Choose the password you will sign in with from now on. The link works once, and signing in with it ends every other
                    session you had.
                {:else if claiming}
                    This panel has no operator yet. Paste the token from the link the supervisor printed and choose the name and password
                    you will sign in with.
                {:else if asUser}
                    Sign in with your panel account.
                {:else}
                    Enter the admin token this app keeps. It is traded once for a session in this browser and is never stored here.
                {/if}
            </Card.Description>
        </Card.Header>
        <Card.Content>
            <form class="space-y-4" onsubmit={submit} novalidate>
                {#if notice}
                    <p class="rounded-md border px-3 py-2 text-sm text-muted-foreground" role="status">{notice}</p>
                {:else if session.ended && !secondStep}
                    <p class="rounded-md border px-3 py-2 text-sm text-muted-foreground" role="status">
                        Your session ended. Sign in again to carry on.
                    </p>
                {/if}
                {#if linkProblem && !problem}
                    <div class="flex gap-2 rounded-md border border-destructive/30 bg-destructive/5 px-3 py-2 text-sm" role="alert">
                        <CircleAlertIcon class="mt-0.5 size-4 shrink-0 text-destructive" />
                        <div class="space-y-1">
                            <p>{describeLink(linkProblem)}</p>
                            {#if linkProblem.requestId}
                                <p class="text-xs text-muted-foreground">
                                    Request <span class="font-mono select-all">{linkProblem.requestId}</span>
                                </p>
                            {/if}
                        </div>
                    </div>
                {/if}
                {#if problem || others.length > 0}
                    <div class="flex gap-2 rounded-md border border-destructive/30 bg-destructive/5 px-3 py-2 text-sm" role="alert">
                        <CircleAlertIcon class="mt-0.5 size-4 shrink-0 text-destructive" />
                        <div class="space-y-1">
                            {#if problem}<p>{describe(problem)}</p>{/if}
                            {#each others as [field, text] (field)}
                                <p><span class="font-mono">{field}</span>: {text}</p>
                            {/each}
                            {#if requestId}
                                <p class="text-xs text-muted-foreground">Request <span class="font-mono select-all">{requestId}</span></p>
                            {/if}
                        </div>
                    </div>
                {:else if requestId}
                    <p class="text-xs text-muted-foreground">Request <span class="font-mono select-all">{requestId}</span></p>
                {/if}
                {#if secondStep}
                    {#if recovery}
                        <div class="space-y-2">
                            <Label for="sign-in-recovery-code">Recovery code</Label>
                            <Input
                                id="sign-in-recovery-code"
                                autocomplete="one-time-code"
                                spellcheck="false"
                                bind:value={code}
                                aria-invalid={fields.recovery_code !== undefined}
                                aria-describedby={fields.recovery_code !== undefined ? "sign-in-recovery-code-problem" : undefined}
                                class="font-mono"
                            />
                            {#if fields.recovery_code}<p id="sign-in-recovery-code-problem" class="text-sm text-destructive">
                                    {fields.recovery_code}
                                </p>{/if}
                        </div>
                    {:else}
                        <div class="space-y-2">
                            <Label for="sign-in-code">Code</Label>
                            <Input
                                id="sign-in-code"
                                autocomplete="one-time-code"
                                inputmode="numeric"
                                spellcheck="false"
                                bind:value={code}
                                aria-invalid={fields.code !== undefined}
                                aria-describedby={fields.code !== undefined ? "sign-in-code-problem" : undefined}
                                class="font-mono"
                            />
                            {#if fields.code}<p id="sign-in-code-problem" class="text-sm text-destructive">{fields.code}</p>{/if}
                        </div>
                    {/if}
                    <Button variant="link" class="h-auto px-0" onclick={switchFactor}
                        >{recovery ? "Use a code from the app instead" : "Use a recovery code instead"}</Button
                    >
                {:else if resetting}
                    <div class="space-y-2">
                        <Label for="sign-in-new-password">New password</Label>
                        <Input
                            id="sign-in-new-password"
                            type="password"
                            autocomplete="new-password"
                            bind:value={password}
                            aria-invalid={fields.password !== undefined}
                            aria-describedby={fields.password !== undefined ? "sign-in-new-password-problem" : undefined}
                        />
                        {#if fields.password}<p id="sign-in-new-password-problem" class="text-sm text-destructive">
                                {fields.password}
                            </p>{/if}
                    </div>
                    <div class="space-y-2">
                        <Label for="sign-in-new-password-again">Type it again</Label>
                        <Input
                            id="sign-in-new-password-again"
                            type="password"
                            autocomplete="new-password"
                            bind:value={again}
                            aria-invalid={fields.again !== undefined}
                            aria-describedby={fields.again !== undefined ? "sign-in-new-password-again-problem" : undefined}
                        />
                        {#if fields.again}<p id="sign-in-new-password-again-problem" class="text-sm text-destructive">
                                {fields.again}
                            </p>{/if}
                    </div>
                {/if}
                {#if !secondStep && !resetting && (claiming || !asUser)}
                    <div class="space-y-2">
                        <Label for="sign-in-token">{claiming ? "Link token" : "Admin token"}</Label>
                        <Input
                            id="sign-in-token"
                            type="password"
                            autocomplete={claiming ? "one-time-code" : "current-password"}
                            spellcheck="false"
                            bind:value={token}
                            aria-invalid={fields.token !== undefined}
                            aria-describedby={fields.token !== undefined ? "sign-in-token-problem" : undefined}
                            class="font-mono"
                        />
                        {#if fields.token}<p id="sign-in-token-problem" class="text-sm text-destructive">{fields.token}</p>{/if}
                    </div>
                {/if}
                {#if !secondStep && !resetting && (claiming || asUser)}
                    <div class="space-y-2">
                        <Label for="sign-in-username">Name</Label>
                        <Input
                            id="sign-in-username"
                            autocomplete="username"
                            spellcheck="false"
                            bind:value={username}
                            aria-invalid={fields.username !== undefined}
                            aria-describedby={fields.username !== undefined ? "sign-in-username-problem" : undefined}
                        />
                        {#if fields.username}<p id="sign-in-username-problem" class="text-sm text-destructive">{fields.username}</p>{/if}
                    </div>
                    <div class="space-y-2">
                        <Label for="sign-in-password">Password</Label>
                        <Input
                            id="sign-in-password"
                            type="password"
                            autocomplete={claiming ? "new-password" : "current-password"}
                            bind:value={password}
                            aria-invalid={fields.password !== undefined}
                            aria-describedby={fields.password !== undefined ? "sign-in-password-problem" : undefined}
                        />
                        {#if fields.password}<p id="sign-in-password-problem" class="text-sm text-destructive">{fields.password}</p>{/if}
                    </div>
                {/if}
                <Button type="submit" class="w-full" disabled={busy}>
                    <LogInIcon />{busy
                        ? "Signing in"
                        : resetting
                          ? "Set my password"
                          : claiming
                            ? "Make me the owner"
                            : secondStep
                              ? "Confirm"
                              : "Sign in"}
                </Button>
                {#if secondStep}
                    <Button variant="ghost" class="w-full" onclick={() => startAgain("")}>Sign in as someone else</Button>
                {/if}
            </form>
        </Card.Content>
        <Card.Footer>
            <p class="text-xs text-muted-foreground">
                {#if resetting}
                    The link came from an operator or the supervisor's console with <span class="font-mono">panel user reset-password</span
                    >, and it works once.
                {:else if claiming}
                    The link is printed once, to the supervisor's console and log, and works only from this machine.
                {:else if secondStep}
                    Lost your authenticator and your recovery codes? An operator can turn two-factor sign-in off for you from the
                    supervisor's console with <span class="font-mono">panel user reset-two-factor</span>.
                {:else if asUser}
                    Lost your password? An operator can issue a reset from the supervisor's console with
                    <span class="font-mono">panel user reset-password</span>.
                {:else}
                    The token is <span class="font-mono">Admin.Token</span> in the app's config, or the token file the app names in its log when
                    it generates one.
                {/if}
            </p>
            {#if session.panel && asUser && !secondStep}
                <p class="mt-3 flex gap-4 text-sm">
                    <a class="text-primary underline underline-offset-4" href="#player/register">Create a player account</a>
                    <a class="text-primary underline underline-offset-4" href="#player/recover">Forgot player password?</a>
                </p>
            {/if}
        </Card.Footer>
    </Card.Root>
</main>
