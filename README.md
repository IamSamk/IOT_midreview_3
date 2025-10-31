# IoT Crypto Alert Platform

Real-time cryptocurrency alert platform combining an ESP32 edge device with a Firebase-backed React dashboard.

## Architecture

- **ESP32 firmware** (`esp32/crypto_alert_esp32.ino`)
  - Fetches user coin subscriptions from Firebase Realtime Database
  - Polls CoinLayer `live` endpoint (single fetch per unique symbol) with caching
  - Compares live rates against user thresholds, triggers local hardware alerts, and logs events to Firebase
  - Updates `/coins/{symbol}` with the latest price for the web client to consume
- **Web dashboard** (`webapp/`)
  - Vite + React SPA with Firebase Authentication and Realtime Database SDKs
  - Users manage watchlists, configure thresholds, and inspect live charts & alert history
  - Coin search panel pulls from CoinLayer `list` API
- **Firebase configuration** (`firebase/database.rules.json`)
  - Security rules ensuring each user can only read/write their own subtree
  - Device access gated behind a custom token claim (`auth.token.device === true`)

## Prerequisites

1. Firebase project with Realtime Database & Authentication (Email/Password) enabled
2. CoinLayer API key (free tier supports limited requests)
3. ESP32 board + peripherals (LED, active buzzer, optional OLED)
4. Node.js 18+ for the frontend
5. Arduino IDE (or PlatformIO) with ESP32 board support and the following libraries:
   - `Firebase Arduino Client Library for ESP32`
   - `ArduinoJson`

## Setup Steps

### Firebase

1. Import the security rules: `firebase deploy --only database` after pointing Firebase CLI to `firebase/database.rules.json`
2. Create a custom service account or Custom Token flow for the ESP32 and set claim `{ "device": true }`
3. Grab the Realtime Database secret for legacy auth (placed in the sketch as `FIREBASE_DATABASE_SECRET`). Prefer signing custom tokens for production.
4. Configure Firebase Authentication providers (at least email/password)

### ESP32 Firmware

1. Open `esp32/crypto_alert_esp32.ino` in Arduino IDE
2. Replace Wi-Fi credentials, CoinLayer API key, and Firebase secrets with real values (consider `#define` values pulled from a separate header excluded from VCS)
3. Wire peripherals to match `LED_PIN` / `BUZZER_PIN`
4. Flash to your ESP32; monitor serial output to verify connections

### Web Dashboard

```pwsh
cd webapp
cp .env.example .env # fill Firebase + CoinLayer keys
npm install
npm run dev
```

> If your CoinLayer plan does not support HTTPS, set `VITE_COINLAYER_BASE_URL=http://api.coinlayer.com` in `.env`. Browsers will block HTTP calls when the dashboard is hosted over HTTPS, so deploy behind HTTPS only when your CoinLayer tier allows it.

Key scripts:
- `npm run dev` – start Vite dev server on port 5173
- `npm run build` – production bundle

### Device Claims Helper

If you prefer not to use database legacy secret:
1. Use Admin SDK (Cloud Function or custom backend) to mint a custom token for the ESP32 with claim `{ device: true }`
2. Replace `config.signer.tokens.legacy_token` with custom token login in the sketch

## Realtime Data Flow

1. User adds a coin; React app writes to `/users/{uid}/subscriptions/{symbol}`
2. ESP32 refreshes subscriptions every 60s, fetches prices, and updates `/coins/{symbol}` with the latest price
3. Threshold crossings push documents into `/alerts/{alertId}` consumed by the dashboard
4. Dashboard listens to both subscriptions and alerts, updating instantly via Firebase listeners

## Testing Checklist

- [ ] Firebase rules emulated via Firebase Emulator Suite
- [ ] ESP32 handles Wi-Fi drops and retries gracefully
- [ ] CoinLayer quota respected (adjust `PRICE_CACHE_TTL_MS` / `SUBSCRIPTION_REFRESH_MS` to meet plan limits)
- [ ] Web dashboard tested across desktop & mobile breakpoints
- [ ] Security review ensuring no secrets bundled in the React build

## Troubleshooting

| Issue | Resolution |
| --- | --- |
| ESP32 prints `Failed to fetch users` | Verify database secret / custom token validity and that the `users` path exists |
| CoinLayer HTTP 1015/429 | Implement exponential backoff and tighten refresh intervals |
| Web shows `Missing CoinLayer API key` | Ensure `.env` values are loaded; restart `npm run dev` |
| Alerts not appearing | Confirm thresholds create `lower`/`upper` values and ESP32 log outputs |
| Chart reads “CoinLayer plan denied access” | The free CoinLayer tier blocks `/timeframe`; upgrade the plan or disable the historical chart module |

## Roadmap Enhancements

- Migrate ESP32 to OAuth2 custom token authentication for improved security
- Implement Firebase Cloud Functions for anomaly detection or email/SMS notifications
- Add historical charting using CoinLayer `historical` endpoint cached in Firebase Storage/Firestore
- Support multi-factor authentication for dashboard users
