<!-- Project Ambrose by Imjustchico: The page an operator the panel's two-factor requirement covers sees in place of the whole panel until they turn it on: why they are here, the setup that turns it on and shows their recovery codes once, and signing out; nothing else is asked of the server while it shows, since every other route would refuse them. -->
<script lang="ts">
    import { Button } from "$lib/components/ui/button/index.js";
    import { session, signOut } from "$lib/api.svelte.js";
    import LogOutIcon from "@lucide/svelte/icons/log-out";
    import ShieldAlertIcon from "@lucide/svelte/icons/shield-alert";
    import { toast } from "svelte-sonner";
    import TwoFactorEnroll from "../components/TwoFactorEnroll.svelte";

    async function leave() {
        try {
            await signOut();
        } catch {
            toast.error("Signing out failed; the session ends by itself when it expires");
        }
    }
</script>

<main class="flex min-h-svh items-center justify-center bg-background p-4">
    <div class="w-full max-w-lg space-y-4">
        <div class="space-y-2">
            <div class="flex size-10 items-center justify-center rounded-lg bg-primary text-primary-foreground">
                <ShieldAlertIcon class="size-5" />
            </div>
            <h1 class="font-serif text-2xl font-semibold">Turn on two-factor sign-in</h1>
            <p class="text-sm text-muted-foreground">
                This panel asks {session.user ? session.user.display_name : "your account"} to sign in with a code from an authenticator app as
                well as a password. Set it up here to carry on; the rest of the panel opens as soon as it is on.
            </p>
        </div>
        <TwoFactorEnroll />
        <div class="flex justify-end">
            <Button variant="ghost" onclick={() => void leave()}><LogOutIcon />Sign out</Button>
        </div>
    </div>
</main>
