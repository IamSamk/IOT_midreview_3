import { useEffect, useState } from 'react';
import { onValue, query, ref, orderByChild, equalTo } from 'firebase/database';
import { db } from '../firebaseClient.js';

export default function AlertsTable({ uid }) {
  const [alerts, setAlerts] = useState([]);

  useEffect(() => {
    if (!uid) return undefined;

    const alertsQuery = query(ref(db, 'alerts'), orderByChild('uid'), equalTo(uid));
    const unsubscribe = onValue(alertsQuery, (snapshot) => {
      const value = snapshot.val() || {};
      const list = Object.entries(value)
        .map(([id, payload]) => ({ id, ...payload }))
        .sort((a, b) => (b.triggeredAt || 0) - (a.triggeredAt || 0));
      setAlerts(list);
    });

    return () => unsubscribe();
  }, [uid]);

  return (
    <section className="card">
      <div className="card__header">
        <h3 className="card__title">Alert history</h3>
      </div>
      <div className="scroll-area">
        <table className="alert-table">
          <thead>
            <tr>
              <th>Symbol</th>
              <th>Direction</th>
              <th>Price</th>
              <th>Triggered</th>
            </tr>
          </thead>
          <tbody>
            {alerts.map((alert) => (
              <tr key={alert.id}>
                <td>{alert.symbol}</td>
                <td>{alert.direction}</td>
                <td>${alert.price?.toFixed?.(2) ?? alert.price}</td>
                <td>{alert.triggeredAt ? new Date(alert.triggeredAt * 1000).toLocaleString() : 'N/A'}</td>
              </tr>
            ))}
            {!alerts.length && (
              <tr>
                <td colSpan={4} className="table-empty">No alerts yet. Threshold crossings will appear here instantly.</td>
              </tr>
            )}
          </tbody>
        </table>
      </div>
    </section>
  );
}
