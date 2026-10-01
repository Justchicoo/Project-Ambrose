<!-- Project Ambrose by Imjustchico: The launcher's own hero picture, drawn here rather than taken from anywhere: a night sky that falls smoothly from the page's own ground overhead to a lighter horizon, with no band or edge anywhere in it, scattered stars, a crescent moon drawn as one shape so it sits cleanly on any part of that sky, and a skyline of spires and towers in two layers of silhouette darker than the horizon behind them so they read as silhouettes, every colour a design token read as a CSS variable so it follows the theme, and nothing in it a word, a logo or anything from the game. The stars are placed by a small fixed sequence, so the picture is the same on every opening and in every test. It is decoration and says nothing, so it is hidden from assistive technology, it fills whatever box it is given by covering it from the horizon up, and its one movement, the stars fading in once when the window opens, is not played at all when the viewer asks for reduced motion through the system or the panel's own setting. -->
<script lang="ts">
    type Star = { x: number; y: number; r: number; tone: "bright" | "soft" | "faint" };

    function stars(count: number): Star[] {
        let seed = 0x2f6b9a13;
        const next = () => {
            seed = (seed * 1664525 + 1013904223) >>> 0;
            return seed / 4294967296;
        };
        const made: Star[] = [];
        for (let index = 0; index < count; index++) {
            const roll = next();
            made.push({
                x: Math.round(next() * 1440),
                y: Math.round(next() * 380),
                r: roll > 0.9 ? 1.8 : roll > 0.6 ? 1.2 : 0.8,
                tone: roll > 0.8 ? "bright" : roll > 0.45 ? "soft" : "faint",
            });
        }
        return made;
    }

    const field = stars(90);

    const far =
        "M0 520 L0 452 L70 452 L70 430 L96 430 L96 446 L150 446 L150 418 L162 404 L174 418 L174 440 L240 440 L240 426 L292 426 " +
        "L292 448 L360 448 L360 412 L372 380 L384 412 L384 444 L470 444 L470 430 L520 430 L520 450 L610 450 L610 422 L640 422 " +
        "L640 438 L720 438 L720 414 L732 396 L744 414 L744 446 L830 446 L830 428 L880 428 L880 452 L960 452 L960 420 L990 420 " +
        "L990 440 L1060 440 L1060 408 L1070 386 L1080 408 L1080 446 L1170 446 L1170 432 L1230 432 L1230 450 L1310 450 " +
        "L1310 424 L1350 424 L1350 444 L1440 444 L1440 520 Z";

    const near =
        "M0 520 L0 478 L48 478 L48 462 L88 462 L88 484 L128 484 L128 440 L140 440 L140 400 L152 352 L164 400 L164 440 L176 440 " +
        "L176 486 L236 486 L236 470 L262 456 L288 470 L288 490 L352 490 L352 472 L400 472 L400 458 L412 458 L412 430 L422 392 " +
        "L432 430 L432 458 L444 458 L444 480 L520 480 L520 492 L600 492 L600 468 L646 468 L646 486 L1010 486 L1010 470 " +
        "L1052 470 L1052 448 L1064 448 L1064 404 L1078 334 L1092 404 L1092 448 L1104 448 L1104 476 L1150 476 L1150 488 " +
        "L1218 488 L1218 466 L1244 452 L1270 466 L1270 484 L1330 484 L1330 456 L1342 456 L1342 428 L1352 398 L1362 428 " +
        "L1362 456 L1374 456 L1374 482 L1440 482 L1440 520 Z";

    const lit = [
        { x: 146, y: 412 },
        { x: 146, y: 426 },
        { x: 418, y: 440 },
        { x: 1070, y: 418 },
        { x: 1084, y: 418 },
        { x: 1070, y: 434 },
        { x: 1348, y: 438 },
        { x: 258, y: 474 },
    ];
</script>

<svg class="sky" viewBox="0 0 1440 520" preserveAspectRatio="xMidYMax slice" aria-hidden="true" focusable="false">
    <defs>
        <linearGradient id="ambrose-night-sky-fall" x1="0" y1="0" x2="0" y2="1">
            <stop class="zenith" offset="0" />
            <stop class="middle" offset="0.55" />
            <stop class="horizon" offset="0.9" />
        </linearGradient>
    </defs>
    <rect x="0" y="0" width="1440" height="520" fill="url(#ambrose-night-sky-fall)" />
    <g class="stars">
        {#each field as star, index (index)}
            <circle class={star.tone} cx={star.x} cy={star.y} r={star.r} />
        {/each}
    </g>
    <path class="moon" d="M 1214.09 128.79 A 38 38 0 1 1 1173.43 74.57 A 34 34 0 0 0 1214.09 128.79 Z" />
    <path class="far" d={far} />
    <path class="near" d={near} />
    <g class="windows">
        {#each lit as window, index (index)}
            <rect x={window.x} y={window.y} width="4" height="6" rx="1" />
        {/each}
    </g>
</svg>

<style>
    .sky {
        position: absolute;
        inset: 0;
        width: 100%;
        height: 100%;
        pointer-events: none;
    }

    .zenith {
        stop-color: var(--color-surface-page);
    }

    .middle {
        stop-color: var(--color-surface-card);
    }

    .horizon {
        stop-color: var(--color-edge-strong);
    }

    .bright {
        fill: var(--color-fg-body);
    }

    .soft {
        fill: var(--color-fg-muted);
        opacity: 0.8;
    }

    .faint {
        fill: var(--color-fg-faint);
        opacity: 0.6;
    }

    .moon {
        fill: var(--color-fg-muted);
    }

    .far {
        fill: var(--color-surface-page);
    }

    .near {
        fill: var(--color-surface-chrome);
    }

    .windows rect {
        fill: var(--color-fg-faint);
        opacity: 0.55;
    }

    .stars {
        animation: arrive var(--ambrose-duration-screen, 320ms) ease-out both;
    }

    @keyframes arrive {
        from {
            opacity: 0;
        }

        to {
            opacity: 1;
        }
    }

    @media (prefers-reduced-motion: reduce) {
        :global(:root:not([data-motion="on"])) .stars {
            animation: none;
        }
    }

    :global(:root[data-motion="off"]) .stars {
        animation: none;
    }
</style>
