/*
 * Project Ambrose by Imjustchico
 * Everything @ambrose/ui hands a surface: the components grouped as doc/COMPONENTS.md lists them, the generated tokens, the motion rules, the host bridge and the icon names.
 */

export { default as Heading } from "./components/Heading.svelte";
export { default as Label } from "./components/Label.svelte";
export { default as Mono } from "./components/Mono.svelte";
export { default as Icon } from "./components/Icon.svelte";
export { default as VisuallyHidden } from "./components/VisuallyHidden.svelte";

export { default as Button } from "./components/Button.svelte";
export { default as IconButton } from "./components/IconButton.svelte";
export { default as TextField } from "./components/TextField.svelte";
export { default as Select } from "./components/Select.svelte";
export { default as Checkbox } from "./components/Checkbox.svelte";
export { default as Switch } from "./components/Switch.svelte";

export { default as Card } from "./components/Card.svelte";
export { default as Dialog } from "./components/Dialog.svelte";
export { default as ConfirmDialog } from "./components/ConfirmDialog.svelte";
export { default as Tooltip } from "./components/Tooltip.svelte";
export { default as Tabs } from "./components/Tabs.svelte";
export { default as Menu } from "./components/Menu.svelte";

export { default as AppShell } from "./components/AppShell.svelte";
export { default as SideNav } from "./components/SideNav.svelte";

export { default as StateDot } from "./components/StateDot.svelte";
export { default as LiveDot } from "./components/LiveDot.svelte";
export { default as Badge } from "./components/Badge.svelte";
export { default as ProgressBar } from "./components/ProgressBar.svelte";
export { default as StepList } from "./components/StepList.svelte";
export { default as EmptyState } from "./components/EmptyState.svelte";
export { default as Toaster } from "./components/Toaster.svelte";

export { default as DataTable } from "./components/DataTable.svelte";
export { default as StatTile } from "./components/StatTile.svelte";
export { default as LogList } from "./components/LogList.svelte";
export { default as Sparkline } from "./components/Sparkline.svelte";
export { default as TimeSeries } from "./components/TimeSeries.svelte";

export { classes } from "./internal/classes";
export { motion, motionSettings, durationMs, easing } from "./motion/motion.svelte";
export type { MotionSetting, DurationName } from "./motion/motion.svelte";
export { createHost, hostKind } from "./bridge/bridge";
export type { Host, HostKind, HostRequest, HostReply } from "./bridge/bridge";
export { icons, iconNames } from "./icons/icons";
export type { IconName } from "./icons/icons";
export * from "./tokens/tokens";
