/*
 * Project Ambrose by Imjustchico
 * The small rules the two-factor pages share with the server: a setup secret written in groups of four so a person can type it into an authenticator app, a typed code read as six digits once its spaces and dashes are gone, a typed recovery code read the way the server reads it, in any case, with or without its dash and with O taken as zero and I or L as one, and the text a set of recovery codes is saved as.
 */

export const CodeDigits = 6;
export const RecoveryCodeLength = 10;
const Crockford = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";

export function groupSecret(secret: string, size = 4): string {
    const bare = secret.replace(/[\s=]/g, "").toUpperCase();
    const groups: string[] = [];
    for (let at = 0; at < bare.length; at += size) groups.push(bare.slice(at, at + size));
    return groups.join(" ");
}

export function readCode(typed: string): string | null {
    const digits = typed.replace(/[\s-]/g, "");
    return new RegExp(`^\\d{${CodeDigits}}$`).test(digits) ? digits : null;
}

export function readRecoveryCode(typed: string): string | null {
    let read = "";
    for (const character of typed.toUpperCase()) {
        if (character === "-" || /\s/.test(character)) continue;
        const folded = character === "O" ? "0" : character === "I" || character === "L" ? "1" : character;
        if (!Crockford.includes(folded)) return null;
        read += folded;
    }
    return read.length === RecoveryCodeLength ? read : null;
}

export function groupRecoveryCode(code: string): string {
    const read = readRecoveryCode(code) ?? code;
    return read.length === RecoveryCodeLength ? `${read.slice(0, 5)}-${read.slice(5)}` : read;
}

export function recoveryCodesText(codes: readonly string[], account: string, issuer: string, issued: Date): string {
    return [
        `Recovery codes for ${account} on ${issuer}`,
        `Issued ${issued.toISOString()}`,
        "Each code signs you in once when your authenticator app is not to hand. Keep them somewhere only you can reach.",
        "",
        ...codes.map(groupRecoveryCode),
        "",
    ].join("\n");
}
