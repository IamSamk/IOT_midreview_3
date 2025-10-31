#!/usr/bin/env node

/**
 * Simulated price pumper for Firebase Realtime Database.
 *
 * coin1 flips every 3s; coin2 every 4s; coin3 every 10s.
 * Prices alternate across configured lower/upper thresholds to trigger alerts.
 */

const DATABASE_URL = process.env.FIREBASE_RTDB_URL || 'https://iotcie3-default-rtdb.asia-southeast1.firebasedatabase.app';
const DATABASE_SECRET = process.env.FIREBASE_RTDB_SECRET || '4ogjIrdOy635PgMK5sgcW1427fDReNvSrxgUg3yJ';
const DEMO_USER_ID = process.env.DEMO_USER_ID || 'demoUser1234';

if (!DATABASE_URL || !DATABASE_SECRET) {
  console.error('Missing Firebase credentials. Set FIREBASE_RTDB_URL and FIREBASE_RTDB_SECRET.');
  process.exit(1);
}

const coins = [
  {
    symbol: 'coin1',
    intervalMs: 3000,
    lowPrice: 27500,
    highPrice: 34500,
    thresholds: { lower: 28000, upper: 34000 }
  },
  {
    symbol: 'coin2',
    intervalMs: 4000,
    lowPrice: 1800,
    highPrice: 2250,
    thresholds: { lower: 1900, upper: 2200 }
  },
  {
    symbol: 'coin3',
    intervalMs: 10000,
    lowPrice: 0.45,
    highPrice: 0.85,
    thresholds: { lower: 0.5, upper: 0.8 }
  }
];

const latchState = new Map();
const timers = [];

function firebaseUrl(path) {
  const normalized = path.replace(/(^\/*)|(\/*$)/g, '');
  return `${DATABASE_URL.replace(/\/$/, '')}/${normalized}.json?auth=${DATABASE_SECRET}`;
}

async function patch(path, data) {
  const response = await fetch(firebaseUrl(path), {
    method: 'PATCH',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(data)
  });
  if (!response.ok) {
    const text = await response.text();
    throw new Error(`PATCH ${path} failed: ${response.status} ${response.statusText} -> ${text}`);
  }
  return response.json();
}

async function post(path, data) {
  const response = await fetch(firebaseUrl(path), {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(data)
  });
  if (!response.ok) {
    const text = await response.text();
    throw new Error(`POST ${path} failed: ${response.status} ${response.statusText} -> ${text}`);
  }
  return response.json();
}

async function simulateTick({ symbol, lowPrice, highPrice, thresholds }) {
  const latchKey = `${DEMO_USER_ID}:${symbol}`;
  const state = latchState.get(latchKey) || { toggle: false, lastDirection: null };
  const price = state.toggle ? highPrice : lowPrice;
  const now = Date.now();

  try {
    await patch(`coins/${symbol}`, {
      lastPrice: price,
      updatedAt: Math.floor(now / 1000),
      lastClientPrice: price,
      lastClientUpdatedAt: now
    });

    const direction = price >= thresholds.upper ? 'upper' : price <= thresholds.lower ? 'lower' : null;
    if (direction && state.lastDirection !== direction) {
      await post('alerts', {
        uid: DEMO_USER_ID,
        symbol,
        price,
        direction,
        triggeredAt: Math.floor(now / 1000)
      });
      state.lastDirection = direction;
      console.log(`[${new Date().toISOString()}] Alert pushed for ${symbol} ${direction} at ${price}`);
    } else if (!direction) {
      state.lastDirection = null;
    }

    console.log(`[${new Date().toISOString()}] Updated ${symbol} to ${price}`);
  } catch (error) {
    console.error(`Simulation error for ${symbol}:`, error.message);
  }

  state.toggle = !state.toggle;
  latchState.set(latchKey, state);
}

function start() {
  console.log('Starting Firebase price simulator...');
  coins.forEach((config) => {
    simulateTick(config); // immediate kick-off
    const timer = setInterval(() => simulateTick(config), config.intervalMs);
    timers.push(timer);
  });
}

function stop() {
  timers.forEach((timer) => clearInterval(timer));
  console.log('\nSimulation stopped.');
  process.exit(0);
}

process.on('SIGINT', stop);
process.on('SIGTERM', stop);

start();
