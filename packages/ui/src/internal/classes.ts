/*
 * Project Ambrose by Imjustchico
 * Joins class names, dropping anything falsy, so a component can build its class list without a dependency.
 */

export type ClassValue = string | false | null | undefined;

export function classes(...values: ClassValue[]): string {
    return values.filter((value): value is string => typeof value === "string" && value.length > 0).join(" ");
}
