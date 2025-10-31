import { signOut } from 'firebase/auth';
import { useMemo, useState } from 'react';
import { auth } from '../firebaseClient.js';
import CoinSearch from './CoinSearch.jsx';
import SubscriptionsGrid from './SubscriptionsGrid.jsx';
import AlertsTable from './AlertsTable.jsx';
import LiveChartPanel from './LiveChartPanel.jsx';
import { useAuthContext } from './AuthProvider.jsx';
import { useSubscriptions } from '../hooks/useSubscriptions.js';
import { useRealtimeConnection } from '../hooks/useRealtimeConnection.js';

export default function Dashboard() {
  const { user } = useAuthContext();
  const { subscriptions, updateThreshold, removeSymbol } = useSubscriptions(user?.uid);
  const firebaseConnected = useRealtimeConnection();
  const [coinLayerConnected, setCoinLayerConnected] = useState(false);

  const subscriptionCount = useMemo(() => Object.keys(subscriptions).length, [subscriptions]);
  const symbols = useMemo(() => Object.keys(subscriptions), [subscriptions]);

  return (
    <div className="dashboard-shell">
      <aside className="sidebar">
        <div>
          <h2>Crypto Alerts</h2>
          <p style={{ opacity: 0.6 }}>Signed in as {user?.email}</p>
        </div>
        <div className="card">
          <span className="chip">Subscriptions: {subscriptionCount}</span>
          <span className="chip">Alerts logged in realtime</span>
          <span
            className="chip"
            style={{
              background: firebaseConnected ? 'rgba(44, 177, 188, 0.16)' : 'rgba(255, 123, 123, 0.16)'
            }}
          >
            Firebase {firebaseConnected ? 'connected' : 'offline'}
          </span>
          <span
            className="chip"
            style={{
              background: coinLayerConnected ? 'rgba(44, 177, 188, 0.16)' : 'rgba(255, 123, 123, 0.16)'
            }}
          >
            CoinLayer {coinLayerConnected ? 'connected' : 'offline'}
          </span>
        </div>
        <button className="primary-button" type="button" onClick={() => signOut(auth)}>
          Sign Out
        </button>
      </aside>
      <main className="main-panel">
        <CoinSearch uid={user?.uid} onConnectionChange={setCoinLayerConnected} />
        <LiveChartPanel availableSymbols={symbols} />
        <SubscriptionsGrid subscriptions={subscriptions} onUpdate={updateThreshold} onRemove={removeSymbol} />
        <AlertsTable uid={user?.uid} />
      </main>
    </div>
  );
}
