import { useEffect, useMemo, useState } from 'react';
import { onValue, ref } from 'firebase/database';
import { db } from '../firebaseClient.js';

function normalizeTimestamp(value) {
  if (!value) return null;
  return value > 1e12 ? value : value * 1000;
}

function buildEntry(symbol, payload) {
  const canonicalPrice = typeof payload?.price === 'number' ? payload.price : null;
  const devicePrice = typeof payload?.lastPrice === 'number' ? payload.lastPrice : null;
  const clientPrice = typeof payload?.lastClientPrice === 'number' ? payload.lastClientPrice : null;
  const price = canonicalPrice ?? devicePrice ?? clientPrice;
  return {
    symbol,
    price,
    source:
      canonicalPrice !== null
        ? payload?.source || 'device'
        : devicePrice !== null
        ? 'device'
        : clientPrice !== null
        ? 'client'
        : 'unknown',
    updatedAt: normalizeTimestamp(payload?.updatedAt || payload?.lastClientUpdatedAt)
  };
}

export function useCoinPriceMap(symbols = []) {
  const [priceMap, setPriceMap] = useState({});
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    if (!symbols.length) {
      setPriceMap({});
      setLoading(false);
      return undefined;
    }

    const coinsRef = ref(db, 'coins');
    const handleValue = (snapshot) => {
      const value = snapshot.val() || {};
      const next = {};
      symbols.forEach((symbol) => {
        next[symbol] = buildEntry(symbol, value[symbol]);
      });
      setPriceMap(next);
      setLoading(false);
    };

    const unsubscribe = onValue(coinsRef, handleValue, {
      onlyOnce: false
    });

    return () => unsubscribe();
  }, [symbols.join('|')]);

  const entries = useMemo(
    () =>
      symbols.map((symbol) => {
        const entry = priceMap[symbol];
        if (entry) return entry;
        return { symbol, price: null, source: 'unknown', updatedAt: null };
      }),
    [priceMap, symbols]
  );

  return { entries, loading };
}
