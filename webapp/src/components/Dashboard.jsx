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
    <div className="dashboard">
      <header className="dashboard-header">
        <div className="dashboard-heading">
          <h1>Crypto Alerts</h1>
          <p className="muted-text">Signed in as {user?.email}</p>
        </div>
        <div className="dashboard-header__meta">
          <div className="status-group">
            <span className="chip">Subscriptions {subscriptionCount}</span>
            <span className="chip">Alerts stream</span>
            <span className={`chip chip--status ${firebaseConnected ? '' : 'chip--offline'}`}>
              <span className="chip__dot" aria-hidden /> Firebase {firebaseConnected ? 'online' : 'offline'}
            </span>
            <span className={`chip chip--status ${coinLayerConnected ? '' : 'chip--offline'}`}>
              <span className="chip__dot" aria-hidden /> CoinLayer {coinLayerConnected ? 'online' : 'offline'}
            </span>
          </div>
          <button className="primary-button" type="button" onClick={() => signOut(auth)}>
            Sign Out
          </button>
        </div>
      </header>
      <main className="dashboard-content">
        <section className="dashboard-column">
          <CoinSearch uid={user?.uid} onConnectionChange={setCoinLayerConnected} />
          <SubscriptionsGrid subscriptions={subscriptions} onUpdate={updateThreshold} onRemove={removeSymbol} />
        </section>
        <section className="dashboard-column">
          <LiveChartPanel availableSymbols={symbols} />
          <AlertsTable uid={user?.uid} />
        </section>
      </main>
    </div>
  );
}
