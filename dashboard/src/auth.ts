import { useCallback, useEffect, useState } from "react";
import { authenticate, signOut as revokeSession } from "./data";
import type { DeviceSession } from "./types/domain";

const STORAGE_KEY = "rusim.device_session";

// Client-side login backoff state (module scope so it survives re-renders).
// The server enforces the real throttle; this only spaces out accidental
// rapid retries instead of hammering the RPC.
let consecutiveFailures = 0;
let lastAttemptAt = 0;

type StoredSession = {
  sessionToken: string;
  device: DeviceSession["device"];
  expiresAt: string;
};

function readStoredSession(): DeviceSession | null {
  const raw = localStorage.getItem(STORAGE_KEY);
  if (!raw) return null;

  try {
    const stored = JSON.parse(raw) as StoredSession;
    if (typeof stored.sessionToken !== "string") return null;
    if (Date.parse(stored.expiresAt) <= Date.now()) return null;
    return {
      sessionToken: stored.sessionToken,
      device: stored.device,
      expiresAt: stored.expiresAt,
    };
  } catch {
    return null;
  }
}

function writeStoredSession(session: DeviceSession | null): void {
  if (!session) {
    localStorage.removeItem(STORAGE_KEY);
    return;
  }
  localStorage.setItem(STORAGE_KEY, JSON.stringify({
    sessionToken: session.sessionToken,
    device: session.device,
    expiresAt: session.expiresAt,
  }));
}

export type AuthState = {
  session: DeviceSession | null;
  signingIn: boolean;
  error: string;
  signIn: (token: string) => Promise<boolean>;
  signOut: () => Promise<void>;
};

export function useAuth(): AuthState {
  const [session, setSession] = useState<DeviceSession | null>(() => readStoredSession());
  const [signingIn, setSigningIn] = useState(false);
  const [error, setError] = useState("");

  useEffect(() => {
    if (!session) return;
    const remaining = Date.parse(session.expiresAt) - Date.now();
    if (!Number.isFinite(remaining) || remaining <= 0) return;

    const timer = window.setTimeout(() => {
      writeStoredSession(null);
      setSession(null);
      setError("Your session expired. Sign in again to continue.");
    }, remaining);
    return () => window.clearTimeout(timer);
  }, [session]);

  const signIn = useCallback(async (token: string) => {
    setSigningIn(true);
    setError("");
    try {
      // Client-side backoff complements the server throttle in
      // device_data.authenticate (30 failures / 5 min globally): rapid
      // repeat submits wait briefly instead of hammering the RPC.
      const waited = Date.now() - lastAttemptAt;
      const backoffMs = Math.min(1000 * 2 ** Math.max(consecutiveFailures - 1, 0), 5000);
      if (consecutiveFailures >= 2 && waited < backoffMs) {
        await new Promise((resolve) => setTimeout(resolve, backoffMs - waited));
      }
      lastAttemptAt = Date.now();
      const next = await authenticate(token);
      consecutiveFailures = 0;
      writeStoredSession(next);
      setSession(next);
      return true;
    } catch (err) {
      consecutiveFailures += 1;
      setError(err instanceof Error ? err.message : "Unable to sign in with that token.");
      return false;
    } finally {
      setSigningIn(false);
    }
  }, []);

  const signOut = useCallback(async () => {
    if (session) await revokeSession(session.sessionToken);
    writeStoredSession(null);
    setSession(null);
    setError("");
  }, [session]);

  return { session, signingIn, error, signIn, signOut };
}
