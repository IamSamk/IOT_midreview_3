import ThresholdEditor from './ThresholdEditor.jsx';

export default function SubscriptionsGrid({ subscriptions, onUpdate, onRemove }) {
  const entries = Object.entries(subscriptions || {});

  if (!entries.length) {
    return (
      <div className="card">
        <h3>No coins subscribed yet</h3>
        <p style={{ opacity: 0.7 }}>Use the search panel to add coins and configure price thresholds.</p>
      </div>
    );
  }

  return (
    <section>
      <h2 style={{ marginBottom: '1rem' }}>Your watchlist</h2>
      <div className="cards-grid">
        {entries.map(([symbol, thresholds]) => (
          <ThresholdEditor
            key={symbol}
            symbol={symbol}
            lower={thresholds.lower}
            upper={thresholds.upper}
            onSave={(payload) => onUpdate(symbol, payload)}
            onRemove={() => onRemove(symbol)}
          />
        ))}
      </div>
    </section>
  );
}
