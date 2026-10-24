// IMPORTANT: For AI agents, ALWAYS update the documentation in docs/llm-generated/database.md whenever any changes are made.

import { createClient, type SupabaseClient } from "@supabase/supabase-js";
import type { Database } from "./types/database";

/**
 * Reads the two build-time values on first use rather than at module load.
 *
 * The guard still fails loudly when the app actually builds a client, but it no
 * longer prevents pure helpers from being imported by tests that do not talk to
 * Supabase.
 */
function readConfig(): { url: string; publishableKey: string } {
  const envUrl = import.meta.env.VITE_SUPABASE_URL;
  const envPublishableKey = import.meta.env.VITE_SUPABASE_PUBLISHABLE_KEY;

  if (!envUrl || !envPublishableKey) {
    throw new Error(
      "Missing Supabase configuration: VITE_SUPABASE_URL and VITE_SUPABASE_PUBLISHABLE_KEY are required.",
    );
  }

  return { url: envUrl, publishableKey: envPublishableKey };
}

/**
 * Custom header carrying the session token issued by `device_data.authenticate`.
 *
 * Row level security resolves this header to a device_id, so every read is
 * scoped to the device the token belongs to. It is deliberately not an
 * `Authorization` value, because PostgREST only accepts JWTs there.
 */
export const SESSION_HEADER = "x-device-session";

// SupabaseClient's generic list is positional: <Database, clientOptions, schema,
// schemaShape>. createClient infers the last two through conditionals that stay
// unresolved when the schema is named through an alias, so they are spelled out
// here to give queries real row types.
export type DeviceDataClient = SupabaseClient<Database, "device_data", "device_data", Database["device_data"]>;

function buildClient(sessionToken: string | null): DeviceDataClient {
  // readConfig returns plain strings, so the values passed to createClient do
  // not carry `string | undefined` and the generic inference stays concrete.
  const { url, publishableKey } = readConfig();
  return createClient<Database, "device_data">(url, publishableKey, {
    // Default every query to the device_data namespace; the old public schema
    // is still present in the database but is no longer used by this app.
    db: { schema: "device_data" },
    global: {
      headers: sessionToken ? { [SESSION_HEADER]: sessionToken } : {},
    },
  });
}

let cachedToken: string | null | undefined;
let cachedClient: DeviceDataClient | null = null;

/**
 * Returns a client bound to a device session.
 *
 * PostgREST copies the client's headers once at construction, so a session
 * change requires a new client rather than a mutation. The previous client is
 * reused while the token is unchanged, which keeps identities stable across
 * ordinary re-renders.
 */
export function getSupabase(sessionToken: string | null): DeviceDataClient {
  if (cachedClient && cachedToken === sessionToken) return cachedClient;

  cachedToken = sessionToken;
  cachedClient = buildClient(sessionToken);
  return cachedClient;
}
