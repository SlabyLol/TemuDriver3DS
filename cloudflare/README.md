# TemuDriver3DS Chat (Cloudflare Worker)

Login + messages for the game. **Does not touch FoxWebChat.**

## Setup

```bash
cd cloudflare
npm i -g wrangler
wrangler login

wrangler kv namespace create USERS
wrangler kv namespace create MESSAGES
wrangler kv namespace create USERS --preview
wrangler kv namespace create MESSAGES --preview
```

Paste the IDs into `wrangler.toml`, then:

```bash
wrangler deploy
```

You get a URL like: `https://temu-driver-chat.<you>.workers.dev`

## API

| Method | Path | Auth | Body |
|--------|------|------|------|
| POST | `/register` | no | `{ "username", "password" }` → `{ token }` |
| POST | `/login` | no | `{ "username", "password" }` → `{ token }` |
| POST | `/message` | Bearer token | `{ "text": "hello" }` |
| GET | `/messages` | Bearer token | recent messages |
| GET | `/me` | Bearer token | current user |

Username: `a-z 0-9 _`, 3–16 chars.

## From 3DS later

HTTP POST/GET to this Worker URL from the game menu.
