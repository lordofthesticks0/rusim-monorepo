// IMPORTANT: For AI agents, ALWAYS update the documentation in docs/llm-generated/database.md whenever any changes are made.

import { createClient, type SupabaseClient } from "@supabase/supabase-js";
import type { Database } from "./types/database";

const envUrl = import.meta.env.VITE_SUPABASE_URL;
const envPublishableKey = import.meta.env.VITE_SUPABASE_PUBLISHABLE_KEY;

if (!envUrl || !envPublishableKey) {
  throw new Error(
    "Missing Supabase configuration: VITE_SUPABASE_URL and VITE_SUPABASE_PUBLISHABLE_KEY are required.",
  );
}

// Bound to plain strings after the guard above. Vite types env reads as
// `string | undefined`, and letting that leak into createClient suppresses its
// generic inference, which degrades every query to an unresolved conditional.
const url: string = envUrl;
const publishableKey: string = envPublishableKey;

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
