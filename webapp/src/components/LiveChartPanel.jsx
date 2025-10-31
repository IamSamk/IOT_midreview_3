import { useMemo } from 'react';
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

ChartJS.register(CategoryScale, LinearScale, BarElement, Tooltip, Legend);

function formatTooltipLabel(context) {
  const value = context.raw;
  if (value === null || typeof value === 'undefined') return 'No price';
  return `$${Number(value).toFixed(2)}`;
}

export default function LiveChartPanel({ availableSymbols }) {
  const { entries, loading } = useCoinPriceMap(availableSymbols || []);

  const chartData = useMemo(() => {
    const labels = entries.map((entry) => entry.symbol);
    const data = entries.map((entry) => (entry.price !== null ? Number(entry.price.toFixed(2)) : null));
    const background = entries.map((entry) =>
      entry.source === 'device' ? 'rgba(44, 177, 188, 0.7)' : 'rgba(127, 90, 240, 0.7)'
    );
    const border = background.map((color) => color.replace('0.7', '1'));

    return {
      labels,
      datasets: [
        {
          label: 'Current price (USD)',
          data,
          backgroundColor: background,
          borderColor: border,
          borderWidth: 1.5,
          borderRadius: 8,
          barThickness: 36
        }
      ]
    };
  }, [entries]);

  const chartOptions = useMemo(
    () => ({
      responsive: true,
      maintainAspectRatio: false,
      plugins: {
        legend: { position: 'top' },
        tooltip: {
          callbacks: {
            label: formatTooltipLabel
          }
        }
      },
      scales: {
        x: {
          ticks: { color: 'rgba(255,255,255,0.75)' },
          grid: { display: false }
        },
        y: {
          ticks: {
            color: 'rgba(255,255,255,0.65)',
            callback: (value) => `$${value}`
          },
          grid: { color: 'rgba(255,255,255,0.08)' }
        }
      }
    }),
    []
  );

  const emptyState = !availableSymbols?.length;
  const noData = !loading && entries.every((entry) => entry.price === null);

  return (
    <section className="card">
      <header style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
        <h3 style={{ margin: 0 }}>Portfolio snapshot</h3>
        <span style={{ opacity: 0.65, fontSize: '0.9rem' }}>
          {loading ? 'Fetching latest quotes…' : 'Updates sync from ESP32 or CoinLayer cache'}
        </span>
      </header>
      <div style={{ minHeight: '280px' }}>
        {emptyState ? (
          <span style={{ opacity: 0.6 }}>Add coins to your watchlist to preview price distribution.</span>
        ) : noData ? (
          <span style={{ opacity: 0.6 }}>Waiting for the first price snapshot. Hold tight!</span>
        ) : (
          <Bar height={120} data={chartData} options={chartOptions} />
        )}
      </div>
    </section>
  );
}
