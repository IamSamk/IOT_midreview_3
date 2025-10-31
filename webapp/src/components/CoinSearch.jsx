import { useEffect, useMemo, useState } from 'react';
import axios from 'axios';
import { ref, set, update } from 'firebase/database';
import { db } from '../firebaseClient.js';
import { COIN_LAYER_API_KEY, COIN_LAYER_BASE_URL } from '../config.js';

const HTTPS_ERROR_SNIPPET = 'HTTPS Encryption';

export default function CoinSearch({ uid, onConnectionChange = () => {} }) {
  const [query, setQuery] = useState('');
  const [coins, setCoins] = useState([]);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState('');
  const [coinLayerConnected, setCoinLayerConnected] = useState(false);

  useEffect(() => {
    const key = COIN_LAYER_API_KEY;
    if (!key) {
      setError('Missing CoinLayer API key. Fill VITE_COINLAYER_API_KEY in your .env file.');
      setCoinLayerConnected(false);
      return;
    }

    const controller = new AbortController();

    async function fetchCoins() {
      setLoading(true);
      try {
        const request = async (baseUrl) =>
          axios.get(`${baseUrl.replace(/\/$/, '')}/list`, {
          params: { access_key: key },
          signal: controller.signal
        });
        let data;
        try {
          const response = await request(COIN_LAYER_BASE_URL);
          data = response.data;
        } catch (initialError) {
          if (COIN_LAYER_BASE_URL.startsWith('https://')) {
            try {
              const response = await request(`http://${COIN_LAYER_BASE_URL.replace(/^https?:\/\//, '')}`);
              data = response.data;
              console.info('CoinLayer fallback to HTTP succeeded');
            } catch (retryError) {
              throw retryError;
            }
          } else {
            throw initialError;
          }
        }

        if (!data.success) {
          const info = data.error?.info || 'Failed to load CoinLayer catalog.';
          setError(info);
          if (info.includes(HTTPS_ERROR_SNIPPET)) {
            console.warn('CoinLayer plan does not allow HTTPS. Consider setting VITE_COINLAYER_BASE_URL=http://api.coinlayer.com');
          }
          setCoinLayerConnected(false);
          onConnectionChange(false);
          return;
        }

        const flattened = Object.entries(data.crypto || {})
          .map(([symbol, details]) => ({
            symbol,
            name: details?.fullname || symbol,
            supply: Number(details?.supply) || 0
          }))
          .filter((coin) => Boolean(coin.symbol))
          .sort((a, b) => a.symbol.localeCompare(b.symbol));
        setCoins(flattened);
        setError('');
        setCoinLayerConnected(true);
        onConnectionChange(true);
        console.info('CoinLayer connected');
      } catch (err) {
        if (!axios.isCancel(err)) {
          setError(err.message);
        }
        setCoinLayerConnected(false);
        onConnectionChange(false);
      } finally {
        setLoading(false);
      }
    }

    fetchCoins();

    return () => controller.abort();
  }, [onConnectionChange]);

  useEffect(() => {
    onConnectionChange(coinLayerConnected);
  }, [coinLayerConnected, onConnectionChange]);

  const filtered = useMemo(() => {
    const trimmed = query.trim().toLowerCase();
    const baseList = trimmed
      ? coins.filter((coin) => {
          const symbol = coin.symbol?.toLowerCase?.() || '';
          const name = coin.name?.toLowerCase?.() || '';
          return symbol.includes(trimmed) || name.includes(trimmed);
        })
      : coins;
    return baseList.slice(0, trimmed ? 120 : 400);
  }, [coins, query]);

  const handleSubscribe = async (symbol) => {
    if (!uid) return;
    await update(ref(db, `coins/${symbol}`), {
      updatedAt: Date.now()
    });
    await set(ref(db, `users/${uid}/subscriptions/${symbol}`), {
      lower: 0,
      upper: 0
    });
  };

  return (
    <div className="card">
      <h3>Add coins</h3>
      <p style={{ opacity: 0.6 }}>Search coins and add base thresholds. ESP32 will use these entries to trigger alerts.</p>
      <div style={{ display: 'flex', gap: '0.75rem', flexWrap: 'wrap', marginBottom: '0.75rem' }}>
        <span
          className="chip"
          style={{
            background: coinLayerConnected ? 'rgba(44, 177, 188, 0.16)' : 'rgba(255, 123, 123, 0.16)'
          }}
        >
          CoinLayer {coinLayerConnected ? 'connected' : 'offline'}
        </span>
        <span style={{ opacity: 0.6 }}>Showing {filtered.length} of {coins.length} coins</span>
      </div>
      <input
        placeholder="Search symbol or name"
        value={query}
        onChange={(event) => setQuery(event.target.value)}
        style={{
          width: '100%',
          borderRadius: '0.75rem',
          border: '1px solid rgba(255,255,255,0.08)',
          background: 'rgba(255,255,255,0.03)',
          color: 'inherit',
          padding: '0.75rem 1rem'
        }}
      />
      {loading && <span>Loading coins...</span>}
      {error && <span style={{ color: '#ff7b7b' }}>{error}</span>}
      <div style={{ display: 'grid', gap: '0.75rem', maxHeight: '260px', overflowY: 'auto' }}>
        {filtered.map((coin) => (
          <div key={coin.symbol} style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
            <div>
              <strong>{coin.symbol}</strong>
              <div style={{ opacity: 0.6, fontSize: '0.85rem' }}>{coin.name || coin.symbol}</div>
            </div>
            <button className="primary-button" type="button" onClick={() => handleSubscribe(coin.symbol)}>
              Add
            </button>
          </div>
        ))}
        {!loading && !filtered.length && !error && (
          <div style={{ opacity: 0.6 }}>No matches. Try a different symbol or name.</div>
        )}
      </div>
    </div>
  );
}
