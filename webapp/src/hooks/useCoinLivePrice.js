import { useEffect, useMemo, useState } from 'react';
import { onValue, ref, update } from 'firebase/database';
import axios from 'axios';
import { db } from '../firebaseClient.js';
import { COIN_LAYER_API_KEY, COIN_LAYER_BASE_URL } from '../config.js';

function normalizeTimestamp(value) {
  if (!value) return null;
  // Firebase.getCurrentTime() returns seconds; convert to ms when needed.
  return value > 1e12 ? value : value * 1000;
}

export function useCoinLivePrice(symbol) {
  const [price, setPrice] = useState(null);
  const [updatedAt, setUpdatedAt] = useState(null);
  const [source, setSource] = useState('device');
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    if (!symbol) return undefined;

    const coinRef = ref(db, `coins/${symbol}`);
    const unsubscribe = onValue(coinRef, (snapshot) => {
      const data = snapshot.val();
      if (!data) {
        return;
      }

      const devicePrice = typeof data.lastPrice === 'number' ? data.lastPrice : null;
      const clientPrice = typeof data.lastClientPrice === 'number' ? data.lastClientPrice : null;
      const nextPrice = devicePrice ?? clientPrice;

      if (nextPrice !== null) {
        setPrice(nextPrice);
        setSource(devicePrice !== null ? 'device' : 'client');
        const timestamp = normalizeTimestamp(data.updatedAt || data.lastClientUpdatedAt);
        setUpdatedAt(timestamp);
        setLoading(false);
      }
    });

    return () => unsubscribe();
  }, [symbol]);

  useEffect(() => {
    if (!symbol || price !== null) {
      if (loading) {
        setLoading(false);
      }
      return undefined;
    }

    const key = COIN_LAYER_API_KEY;
    if (!key) {
      setLoading(false);
      return undefined;
    }

    const controller = new AbortController();
    async function fetchFallback() {
      try {
        const request = async (baseUrl) =>
          axios.get(`${baseUrl.replace(/\/$/, '')}/live`, {
            params: { access_key: key, symbols: symbol },
            signal: controller.signal
          });
        let payload;
        try {
          const response = await request(COIN_LAYER_BASE_URL);
          payload = response.data;
        } catch (initialError) {
          if (COIN_LAYER_BASE_URL.startsWith('https://')) {
            const fallbackUrl = `http://${COIN_LAYER_BASE_URL.replace(/^https?:\/\//, '')}`;
            const response = await request(fallbackUrl);
            payload = response.data;
          } else {
            throw initialError;
          }
        }

        if (payload.success && payload.rates?.[symbol]) {
          const fetchedPrice = Number(payload.rates[symbol]);
          const fetchedAt = Date.now();
          setPrice(fetchedPrice);
          setUpdatedAt(fetchedAt);
          setSource('client');
          try {
            await update(ref(db, `coins/${symbol}`), {
              lastClientPrice: fetchedPrice,
              lastClientUpdatedAt: fetchedAt
            });
          } catch (dbError) {
            console.warn('Failed to persist client price snapshot', dbError);
          }
        }
      } catch (error) {
        console.warn('CoinLayer fallback fetch failed', error);
      } finally {
        setLoading(false);
      }
    }

    fetchFallback();
    return () => controller.abort();
  }, [symbol, price, loading]);

  const metadata = useMemo(
    () => ({ price, updatedAt, source, loading }),
    [price, updatedAt, source, loading]
  );

  return metadata;
}
