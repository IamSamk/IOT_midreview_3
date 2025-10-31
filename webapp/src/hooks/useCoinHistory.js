export function useCoinHistory() {
  console.warn('useCoinHistory is deprecated. Historical charts have been removed.');
  return { history: [], loading: false, error: 'Historical charts disabled for current plan.' };
}
