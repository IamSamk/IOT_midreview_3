import AuthGate from './components/AuthGate.jsx';
import Dashboard from './components/Dashboard.jsx';
import { AuthProvider, useAuthContext } from './components/AuthProvider.jsx';

function Shell() {
  const { user, loading } = useAuthContext();

  if (loading) {
    return (
      <div className="app-loading">
        <h2>Loading...</h2>
      </div>
    );
  }

  return user ? <Dashboard /> : <AuthGate />;
}

export default function App() {
  return (
    <AuthProvider>
      <Shell />
    </AuthProvider>
  );
}
