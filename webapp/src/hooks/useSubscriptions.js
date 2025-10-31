import { useCallback, useEffect, useState } from 'react';
import { onValue, ref, update, remove } from 'firebase/database';
import { db } from '../firebaseClient.js';

export function useSubscriptions(uid) {
  const [subscriptions, setSubscriptions] = useState({});

  useEffect(() => {
    if (!uid) return undefined;

    const subscriptionsRef = ref(db, `users/${uid}/subscriptions`);
    const unsubscribe = onValue(subscriptionsRef, (snapshot) => {
      setSubscriptions(snapshot.val() || {});
    });

    return () => unsubscribe();
  }, [uid]);

  const updateThreshold = useCallback(
    (symbol, payload) => {
      if (!uid) return;
      return update(ref(db, `users/${uid}/subscriptions/${symbol}`), payload);
    },
    [uid]
  );

  const removeSymbol = useCallback(
    (symbol) => {
      if (!uid) return;
      return remove(ref(db, `users/${uid}/subscriptions/${symbol}`));
    },
    [uid]
  );

  return { subscriptions, updateThreshold, removeSymbol };
}
