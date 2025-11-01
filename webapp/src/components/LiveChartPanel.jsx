import { useCallback, useMemo, useState } from 'react';
import axios from 'axios';
import { Bar } from 'react-chartjs-2';
import {
  Chart as ChartJS,
  CategoryScale,
  LinearScale,
  BarElement,
  Tooltip,
  Legend
} from 'chart.js';
import { useCoinPriceMap } from '../hooks/useCoinPriceMap.js';
import { ref, update } from 'firebase/database';
import { db } from '../firebaseClient.js';
import { COIN_LAYER_API_KEY, COIN_LAYER_BASE_URL } from '../config.js';

ChartJS.register(CategoryScale, LinearScale, BarElement, Tooltip, Legend);

function formatTooltipLabel(context) {
  const value = context.raw;
  if (value === null || typeof value === 'undefined') return 'No price';
  return `$${Number(value).toFixed(2)}`;
}

export default function LiveChartPanel({ availableSymbols }) {
  const { entries, loading } = useCoinPriceMap(availableSymbols || []);
  const [refreshing, setRefreshing] = useState(false);
  const [refreshError, setRefreshError] = useState('');
  const [lastRefresh, setLastRefresh] = useState(null);

  const handleRefresh = useCallback(async () => {
    if (!availableSymbols?.length) {
      setRefreshError('Add coins to monitor before refreshing prices.');
      return;
    }
    if (!COIN_LAYER_API_KEY) {
      setRefreshError('CoinLayer API key missing. Set VITE_COINLAYER_API_KEY.');
      return;
    }

    setRefreshError('');
    setRefreshing(true);

    try {
      const request = async (baseUrl) =>
        axios.get(`${baseUrl.replace(/\/$/, '')}/live`, {
          params: {
            access_key: COIN_LAYER_API_KEY,
            symbols: availableSymbols.join(',')
          }
        });

      let response;
      try {
        response = await request(COIN_LAYER_BASE_URL);
      } catch (initialError) {
        const base = COIN_LAYER_BASE_URL || '';
        if (base.startsWith('https://')) {
          response = await request(`http://${base.replace(/^https?:\/\//, '')}`);
        } else {
          throw initialError;
        }
      }

      if (!response.data?.success) {
        const message = response.data?.error?.info || 'Failed to refresh prices from CoinLayer.';
        throw new Error(message);
      }

      const timestamp = response.data?.timestamp || Math.floor(Date.now() / 1000);
      const rates = response.data?.rates || {};
      const updates = {};

      availableSymbols.forEach((symbol) => {
        const price = rates[symbol];
        if (typeof price === 'number') {
          updates[`coins/${symbol}/price`] = price;
          updates[`coins/${symbol}/updatedAt`] = timestamp;
          updates[`coins/${symbol}/source`] = 'webapp';
        }
      });

      if (!Object.keys(updates).length) {
        throw new Error('CoinLayer returned no prices for the requested symbols.');
      }

      await update(ref(db), updates);
      setLastRefresh(Date.now());
    } catch (error) {
      setRefreshError(error.message || 'Unable to refresh prices.');
    } finally {
      setRefreshing(false);
    }
  }, [availableSymbols, setRefreshError]);

  const chartData = useMemo(() => {
    const labels = entries.map((entry) => entry.symbol);
    const data = entries.map((entry) => (entry.price !== null ? Number(entry.price.toFixed(2)) : null));
    const palette = ['rgba(255, 99, 132, 0.75)', 'rgba(54, 162, 235, 0.75)', 'rgba(75, 192, 192, 0.75)'];
    const borders = ['rgb(255, 99, 132)', 'rgb(54, 162, 235)', 'rgb(75, 192, 192)'];

    return {
      labels,
      datasets: [
        {
          label: 'Current price (USD)',
          data,
          backgroundColor: entries.map((_, index) => palette[index % palette.length]),
          borderColor: entries.map((_, index) => borders[index % borders.length]),
          borderWidth: 1,
          borderRadius: 6,
          barThickness: 34
        }
      ]
    };
  }, [entries]);

  const chartOptions = useMemo(
    () => ({
      responsive: true,
      maintainAspectRatio: false,
      plugins: {
        legend: { position: 'top', labels: { color: '#333', usePointStyle: true } },
        tooltip: {
          callbacks: {
            label: formatTooltipLabel
          }
        }
      },
      scales: {
        x: {
          ticks: { color: '#333' },
          grid: { display: false }
        },
        y: {
          ticks: {
            color: '#444',
            callback: (value) => `$${value}`
          },
          grid: { color: '#e8e8e8' }
        }
      }
    }),
    []
  );

  const emptyState = !availableSymbols?.length;
  const noData = !loading && entries.every((entry) => entry.price === null);
  const lastRefreshLabel = lastRefresh ? new Date(lastRefresh).toLocaleTimeString() : null;
  const statusText = loading ? 'Fetching latest quotes…' : 'Updates sync from ESP32 or CoinLayer cache';
  const statusWithRefresh = lastRefreshLabel ? `${statusText} • Last refresh ${lastRefreshLabel}` : statusText;

  return (
    <section className="card">
      <div className="card__header">
        <div>
          <h3 className="card__title">Portfolio snapshot</h3>
          <p className="card__meta">{statusWithRefresh}</p>
        </div>
        <button
          className="primary-button"
          type="button"
          onClick={handleRefresh}
          disabled={refreshing || !availableSymbols?.length}
        >
          {refreshing ? 'Refreshing…' : 'Refresh prices'}
        </button>
      </div>
      <div className="chart-area">
        {emptyState ? (
          <span className="muted-text">Add coins to your watchlist to preview price distribution.</span>
        ) : noData ? (
          <span className="muted-text">Waiting for the first price snapshot. Hold tight!</span>
        ) : (
          <Bar height={120} data={chartData} options={chartOptions} />
        )}
      </div>
      {refreshError && (
        <div className="form-error form-error--block" role="alert">
          {refreshError}
        </div>
      )}
    </section>
  );
}
