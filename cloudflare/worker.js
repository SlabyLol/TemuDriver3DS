/**
 * TemuDriver3DS SpotPass News API
 * - Players: GET /news (no login)
 * - You: POST /news with ADMIN_TOKEN to publish
 *
 * Cloudflare: bind KV namespace as NEWS
 * Secret: ADMIN_TOKEN (Workers → Settings → Variables → Encrypt)
 */

const CORS = {
  "Access-Control-Allow-Origin": "*",
  "Access-Control-Allow-Methods": "GET, POST, DELETE, OPTIONS",
  "Access-Control-Allow-Headers": "Content-Type, Authorization, X-Admin-Token",
};

function json(data, status = 200) {
  return new Response(JSON.stringify(data), {
    status,
    headers: { "Content-Type": "application/json; charset=utf-8", ...CORS },
  });
}

function bad(msg, status = 400) {
  return json({ ok: false, error: msg }, status);
}

function isAdmin(request, env) {
  const token = env.ADMIN_TOKEN || "";
  if (!token) return false;
  const h =
    request.headers.get("X-Admin-Token") ||
    request.headers.get("Authorization") ||
    "";
  const bare = h.startsWith("Bearer ") ? h.slice(7).trim() : h.trim();
  return bare === token;
}

export default {
  async fetch(request, env) {
    if (request.method === "OPTIONS") {
      return new Response(null, { status: 204, headers: CORS });
    }

    const url = new URL(request.url);
    const path = url.pathname.replace(/\/+$/, "") || "/";

    try {
      // Health
      if (path === "/" || path === "/health") {
        return json({
          ok: true,
          service: "TemuDriver3DS SpotPass News",
          version: 2,
        });
      }

      // GET /news — everyone (game + browser)
      if (path === "/news" && request.method === "GET") {
        let list = [];
        try {
          list = JSON.parse((await env.NEWS.get("list")) || "[]");
        } catch (_) {}
        return json({ ok: true, news: list, count: list.length });
      }

      // POST /news — only you (admin token)
      // Body: { "title": "Update", "body": "New cars!" }
      if (path === "/news" && request.method === "POST") {
        if (!isAdmin(request, env)) return bad("admin only", 403);
        const body = await request.json().catch(() => ({}));
        const title = String(body.title || "").trim().slice(0, 40);
        const text = String(body.body || body.text || "").trim().slice(0, 300);
        if (!title || !text) return bad("need title and body");

        const item = {
          id: `${Date.now()}`,
          title,
          body: text,
          at: Date.now(),
        };

        let list = [];
        try {
          list = JSON.parse((await env.NEWS.get("list")) || "[]");
        } catch (_) {}
        list.unshift(item);
        list = list.slice(0, 30);
        await env.NEWS.put("list", JSON.stringify(list));

        return json({ ok: true, news: item });
      }

      // DELETE /news — clear all (admin)
      if (path === "/news" && request.method === "DELETE") {
        if (!isAdmin(request, env)) return bad("admin only", 403);
        await env.NEWS.put("list", "[]");
        return json({ ok: true, cleared: true });
      }

      // Simple admin page (browser on iPad)
      if (path === "/admin" && request.method === "GET") {
        const html = `<!DOCTYPE html>
<html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Temu News Admin</title>
<style>
body{font-family:system-ui;max-width:420px;margin:24px auto;padding:0 16px}
input,textarea,button{width:100%;margin:6px 0;padding:12px;font-size:16px;box-sizing:border-box}
button{background:#c1121f;color:#fff;border:0;border-radius:8px}
pre{background:#f4f4f4;padding:12px;overflow:auto;font-size:12px}
</style></head><body>
<h1>Temu SpotPass News</h1>
<p>Mitteilungen an alle Spieler senden</p>
<input id="tok" type="password" placeholder="Admin-Token" />
<input id="title" placeholder="Titel z.B. News" />
<textarea id="body" rows="4" placeholder="Nachricht..."></textarea>
<button onclick="send()">Senden</button>
<button onclick="load()" style="background:#333">Aktuelle News laden</button>
<pre id="out"></pre>
<script>
const API = location.origin;
const out = (x) => document.getElementById("out").textContent =
  typeof x === "string" ? x : JSON.stringify(x, null, 2);
async function send() {
  const r = await fetch(API + "/news", {
    method: "POST",
    headers: {
      "Content-Type": "application/json",
      "X-Admin-Token": document.getElementById("tok").value
    },
    body: JSON.stringify({
      title: document.getElementById("title").value,
      body: document.getElementById("body").value
    })
  });
  out(await r.json());
}
async function load() {
  const r = await fetch(API + "/news");
  out(await r.json());
}
</script>
</body></html>`;
        return new Response(html, {
          headers: { "Content-Type": "text/html; charset=utf-8", ...CORS },
        });
      }

      return bad("not found", 404);
    } catch (e) {
      return bad(String(e.message || e), 500);
    }
  },
};
