/*
 * Project Ambrose by Imjustchico
 * Generated from design/icons.json: every icon a surface may use, compiled to inline markup at build time and named by the type the panel and the launcher share.
 */

import type { Component } from "svelte";

import IconActivity from "~icons/ambrose/activity";
import IconArchive from "~icons/ambrose/archive";
import IconArrowLeft from "~icons/ambrose/arrow-left";
import IconArrowRight from "~icons/ambrose/arrow-right";
import IconBell from "~icons/ambrose/bell";
import IconCheck from "~icons/ambrose/check";
import IconChevronDown from "~icons/ambrose/chevron-down";
import IconChevronRight from "~icons/ambrose/chevron-right";
import IconCircleAlert from "~icons/ambrose/circle-alert";
import IconCircleCheck from "~icons/ambrose/circle-check";
import IconCircleHelp from "~icons/ambrose/circle-help";
import IconClock from "~icons/ambrose/clock";
import IconCopy from "~icons/ambrose/copy";
import IconDatabase from "~icons/ambrose/database";
import IconDownload from "~icons/ambrose/download";
import IconFileText from "~icons/ambrose/file-text";
import IconFolder from "~icons/ambrose/folder";
import IconGauge from "~icons/ambrose/gauge";
import IconHardDrive from "~icons/ambrose/hard-drive";
import IconHouse from "~icons/ambrose/house";
import IconInfo from "~icons/ambrose/info";
import IconKey from "~icons/ambrose/key";
import IconList from "~icons/ambrose/list";
import IconLock from "~icons/ambrose/lock";
import IconLogOut from "~icons/ambrose/log-out";
import IconMenu from "~icons/ambrose/menu";
import IconMinus from "~icons/ambrose/minus";
import IconPause from "~icons/ambrose/pause";
import IconPlay from "~icons/ambrose/play";
import IconPlus from "~icons/ambrose/plus";
import IconPower from "~icons/ambrose/power";
import IconRefreshCw from "~icons/ambrose/refresh-cw";
import IconRotateCcw from "~icons/ambrose/rotate-ccw";
import IconSearch from "~icons/ambrose/search";
import IconServer from "~icons/ambrose/server";
import IconSettings from "~icons/ambrose/settings";
import IconShield from "~icons/ambrose/shield";
import IconSquare from "~icons/ambrose/square";
import IconTerminal from "~icons/ambrose/terminal";
import IconTrash2 from "~icons/ambrose/trash-2";
import IconTriangleAlert from "~icons/ambrose/triangle-alert";
import IconUpload from "~icons/ambrose/upload";
import IconUser from "~icons/ambrose/user";
import IconUsers from "~icons/ambrose/users";
import IconWifi from "~icons/ambrose/wifi";
import IconX from "~icons/ambrose/x";
import IconZap from "~icons/ambrose/zap";

export const icons = {
    "activity": IconActivity,
    "archive": IconArchive,
    "arrow-left": IconArrowLeft,
    "arrow-right": IconArrowRight,
    "bell": IconBell,
    "check": IconCheck,
    "chevron-down": IconChevronDown,
    "chevron-right": IconChevronRight,
    "circle-alert": IconCircleAlert,
    "circle-check": IconCircleCheck,
    "circle-help": IconCircleHelp,
    "clock": IconClock,
    "copy": IconCopy,
    "database": IconDatabase,
    "download": IconDownload,
    "file-text": IconFileText,
    "folder": IconFolder,
    "gauge": IconGauge,
    "hard-drive": IconHardDrive,
    "house": IconHouse,
    "info": IconInfo,
    "key": IconKey,
    "list": IconList,
    "lock": IconLock,
    "log-out": IconLogOut,
    "menu": IconMenu,
    "minus": IconMinus,
    "pause": IconPause,
    "play": IconPlay,
    "plus": IconPlus,
    "power": IconPower,
    "refresh-cw": IconRefreshCw,
    "rotate-ccw": IconRotateCcw,
    "search": IconSearch,
    "server": IconServer,
    "settings": IconSettings,
    "shield": IconShield,
    "square": IconSquare,
    "terminal": IconTerminal,
    "trash-2": IconTrash2,
    "triangle-alert": IconTriangleAlert,
    "upload": IconUpload,
    "user": IconUser,
    "users": IconUsers,
    "wifi": IconWifi,
    "x": IconX,
    "zap": IconZap,
} as const satisfies Record<string, Component>;

export type IconName = keyof typeof icons;

export const iconNames = Object.keys(icons) as IconName[];
