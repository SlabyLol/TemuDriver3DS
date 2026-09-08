# TemuDriver3DS SpotPass News

**Kein Chat.** Du schreibst Mitteilungen (News), das Spiel holt sie ab.

## iPad Setup (Cloudflare Dashboard)

1. [dash.cloudflare.com](https://dash.cloudflare.com) → **Workers & Pages** → **Create Worker**
2. Name: `temu-driver-news` → Deploy → **Edit code**
3. Code aus `worker.js` einfügen → **Save and deploy**
4. **Settings → Variables** → Secret hinzufügen:
   - Name: `ADMIN_TOKEN`
   - Wert: irgendein langes Passwort (nur für dich)
5. **Settings → Bindings** → KV Namespace:
   - Variable name: `NEWS`
   - Namespace neu erstellen

## Nutzen

- News lesen (Spiel / Browser): `GET https://DEINE-URL.workers.dev/news`
- News schreiben (nur du): Admin-Seite öffnen:
  `https://DEINE-URL.workers.dev/admin`
  → Admin-Token + Titel + Text → Senden

## API

| Wer | Methode | Pfad |
|-----|---------|------|
| Alle | GET | `/news` |
| Du | POST | `/news` + Header `X-Admin-Token` |
| Du | Browser | `/admin` |
