import { useEffect, useState } from 'react';
import classNames from 'classnames';
import { useCoinLivePrice } from '../hooks/useCoinLivePrice.js';

function formatUpdatedAt(timestamp) {
  if (!timestamp) return '';
  const diff = Date.now() - timestamp;
  if (diff < 60 * 1000) return 'just now';
  const minutes = Math.round(diff / (60 * 1000));
  if (minutes < 60) return `${minutes} minute${minutes === 1 ? '' : 's'} ago`;
  const hours = Math.round(minutes / 60);
  if (hours < 24) return `${hours} hour${hours === 1 ? '' : 's'} ago`;
  const days = Math.round(hours / 24);
  return `${days} day${days === 1 ? '' : 's'} ago`;
}

export default function ThresholdEditor({ symbol, lower, upper, onSave, onRemove }) {
  const [form, setForm] = useState({ lower, upper });
  const { price, loading, updatedAt, source } = useCoinLivePrice(symbol);

  useEffect(() => {
    setForm({ lower, upper });
  }, [lower, upper]);

  const handleChange = (event) => {
    const { name, value } = event.target;
    setForm((prev) => ({ ...prev, [name]: Number(value) }));
  };

  const handleSubmit = (event) => {
    event.preventDefault();
    onSave(form);
  };

  return (
    <article className="card threshold-card">
      <header className="threshold-card__header">
        <div>
          <h3 style={{ margin: 0 }}>{symbol}</h3>
          <p className="threshold-card__meta">
            <span>Last price:</span>
            {loading ? (
              <span>Loading…</span>
            ) : price !== null ? (
              <>
                <strong>${price.toFixed(2)}</strong>
                <span style={{ opacity: 0.7 }}>({source === 'device' ? 'ESP32' : 'CoinLayer client'})</span>
                {updatedAt && <span style={{ opacity: 0.6 }}>· {formatUpdatedAt(updatedAt)}</span>}
              </>
            ) : (
              <span style={{ opacity: 0.6 }}>No data yet</span>
            )}
          </p>
        </div>
        <button type="button" onClick={onRemove} className="threshold-card__remove">
          Remove
        </button>
      </header>
      <form onSubmit={handleSubmit} className="threshold-form">
        <label>
          Lower threshold
          <input
            name="lower"
            type="number"
            step="0.01"
            value={form.lower}
            onChange={handleChange}
            className={classNames('threshold-input')}
          />
        </label>
        <label>
          Upper threshold
          <input
            name="upper"
            type="number"
            step="0.01"
            value={form.upper}
            onChange={handleChange}
            className={classNames('threshold-input')}
          />
        </label>
        <button className="primary-button" type="submit">
          Save thresholds
        </button>
      </form>
    </article>
  );
}
