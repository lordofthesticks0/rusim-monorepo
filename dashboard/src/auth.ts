import { useCallback, useEffect, useState } from "react";
import { authenticate, signOut as revokeSession } from "./data";
import type { DeviceSession } from "./types/domain";

const STORAGE_KEY = "rusim.device_session";

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
      const next = await authenticate(token);
      writeStoredSession(next);
      setSession(next);
      return true;
    } catch (err) {
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
