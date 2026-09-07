/**
 * TemuDriver3DS Chat API — Cloudflare Worker
 * Bindings needed:
 *   KV Namespace: USERS
 *   KV Namespace: MESSAGES
 * Deploy: wrangler deploy
 */

const CORS = {
  "Access-Control-Allow-Origin": "*",
  "Access-Control-Allow-Methods": "GET, POST, OPTIONS",
  "Access-Control-Allow-Headers": "Content-Type, Authorization",
};

function json(data, status = 200) {
  return new Response(JSON.stringify(data), {
    status,
    headers: { "Content-Type": "application/json", ...CORS },
  });
}

function bad(msg, status = 400) {
  return json({ ok: false, error: msg }, status);
}

async function sha256(text) {
  const data = new TextEncoder().encode(text);
  const hash = await crypto.subtle.digest("SHA-256", data);
  return [...new Uint8Array(hash)].map((b) => b.toString(16).padStart(2, "0")).join("");
}

function randomToken() {
  const a = new Uint8Array(24);
  crypto.getRandomValues(a);
  return [...a].map((b) => b.toString(16).padStart(2, "0")).join("");
}

async function authUser(request, env) {
  const h = request.headers.get("Authorization") || "";
  const token = h.startsWith("Bearer ") ? h.slice(7).trim() : "";
  if (!token) return null;
  const username = await env.USERS.get(`token:${token}`);
  if (!username) return null;
  return { username, token };
}

export default {
  async fetch(request, env) {
    if (request.method === "OPTIONS") {
      return new Response(null, { status: 204, headers: CORS });
    }

    const url = new URL(request.url);
    const path = url.pathname.replace(/\/+$/, "") || "/";

    try {
      if (path === "/" || path === "/health") {
        return json({ ok: true, service: "TemuDriver3DS Chat", version: 1 });
      }

      if (path === "/register" && request.method === "POST") {
        const body = await request.json().catch(() => ({}));
        const username = String(body.username || "").trim().toLowerCase();
        const password = String(body.password || "");
        if (!/^[a-z0-9_]{3,16}$/.test(username)) {
          return bad("username: 3-16 chars a-z 0-9 _");
        }
        if (password.length < 4 || password.length > 64) {
          return bad("password: 4-64 chars");
        }
        const existing = await env.USERS.get(`user:${username}`);
        if (existing) return bad("username taken", 409);

        const passHash = await sha256(password + ":" + username + ":temu");
        const token = randomToken();
        await env.USERS.put(
          `user:${username}`,
          JSON.stringify({ passHash, created: Date.now() })
        );
        await env.USERS.put(`token:${token}`, username, { expirationTtl: 60 * 60 * 24 * 30 });

        return json({ ok: true, username, token });
      }

      if (path === "/login" && request.method === "POST") {
        const body = await request.json().catch(() => ({}));
        const username = String(body.username || "").trim().toLowerCase();
        const password = String(body.password || "");
        const raw = await env.USERS.get(`user:${username}`);
        if (!raw) return bad("invalid login", 401);
        const user = JSON.parse(raw);
        const passHash = await sha256(password + ":" + username + ":temu");
        if (passHash !== user.passHash) return bad("invalid login", 401);

        const token = randomToken();
        await env.USERS.put(`token:${token}`, username, { expirationTtl: 60 * 60 * 24 * 30 });
        return json({ ok: true, username, token });
      }

      if (path === "/message" && request.method === "POST") {
        const user = await authUser(request, env);
        if (!user) return bad("unauthorized", 401);
        const body = await request.json().catch(() => ({}));
        const text = String(body.text || "").trim().slice(0, 200);
        if (!text) return bad("empty message");

        const id = `${Date.now()}-${Math.random().toString(36).slice(2, 8)}`;
        const msg = {
          id,
          from: user.username,
          text,
          at: Date.now(),
        };
        await env.MESSAGES.put(`msg:${id}`, JSON.stringify(msg), {
          expirationTtl: 60 * 60 * 24 * 14,
        });

        const listKey = "recent";
        let recent = [];
        try {
          recent = JSON.parse((await env.MESSAGES.get(listKey)) || "[]");
        } catch (_) {}
        recent.unshift(msg);
        recent = recent.slice(0, 50);
        await env.MESSAGES.put(listKey, JSON.stringify(recent));

        return json({ ok: true, message: msg });
      }

      if (path === "/messages" && request.method === "GET") {
        const user = await authUser(request, env);
        if (!user) return bad("unauthorized", 401);
        let recent = [];
        try {
          recent = JSON.parse((await env.MESSAGES.get("recent")) || "[]");
        } catch (_) {}
        return json({ ok: true, messages: recent, you: user.username });
      }

      if (path === "/me" && request.method === "GET") {
        const user = await authUser(request, env);
        if (!user) return bad("unauthorized", 401);
        return json({ ok: true, username: user.username });
      }

      return bad("not found", 404);
    } catch (e) {
      return bad(String(e.message || e), 500);
    }
  },
};
