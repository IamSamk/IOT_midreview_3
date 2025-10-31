import { useEffect, useState } from 'react';
import { onValue, ref } from 'firebase/database';
import { db } from '../firebaseClient.js';

export function useRealtimeConnection() {
  const [connected, setConnected] = useState(false);

  useEffect(() => {
    const connectionRef = ref(db, '.info/connected');
    const unsubscribe = onValue(connectionRef, (snapshot) => {
      setConnected(Boolean(snapshot.val()));
    });

    return () => unsubscribe();
  }, []);

  useEffect(() => {
    if (connected) {
      console.info('Firebase connected');
    }
  }, [connected]);

  return connected;
}
