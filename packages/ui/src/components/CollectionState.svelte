<!-- Project Ambrose by Imjustchico: What a collection shows instead of its rows while it is loading, when it holds nothing, when a filter matched nothing, and when it could not be loaded. -->
<script lang="ts">
    import type { Snippet } from "svelte";
    import EmptyState from "./EmptyState.svelte";
    import type { IconName } from "../icons/icons";

    type Status = "loading" | "empty" | "no-results" | "error";

    type Props = {
        status: Status;
        label: string;
        empty?: string;
        noResults?: string;
        error?: string;
        detail?: string;
        class?: string;
        action?: Snippet;
    };

    let {
        status,
        label,
        empty = "Nothing here yet",
        noResults = "Nothing matched",
        error = "Could not be loaded",
        detail,
        class: extra,
        action,
    }: Props = $props();

    const icons: Record<Status, IconName> = {
        loading: "clock",
        empty: "archive",
        "no-results": "search",
        error: "triangle-alert",
    };

    let title = $derived(
        status === "loading" ? `Loading ${label}` : status === "empty" ? empty : status === "no-results" ? noResults : error,
    );

    let hint = $derived(
        detail ?? (status === "loading" ? "This is still running" : status === "no-results" ? "Change the filter to see more" : undefined),
    );
</script>

<div role="status" aria-live="polite" aria-busy={status === "loading"} data-collection-state={status} class={extra}>
    <EmptyState {title} detail={hint} icon={icons[status]} {action} />
</div>
