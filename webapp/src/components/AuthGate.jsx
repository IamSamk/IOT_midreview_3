import { useState } from 'react';
import { signInWithEmailAndPassword, createUserWithEmailAndPassword } from 'firebase/auth';
import { ref, set } from 'firebase/database';
import { auth, db } from '../firebaseClient.js';

export default function AuthGate() {
  const [isRegister, setIsRegister] = useState(false);
  const [form, setForm] = useState({ email: '', password: '' });
  const [error, setError] = useState('');

  const handleChange = (event) => {
    const { name, value } = event.target;
    setForm((prev) => ({ ...prev, [name]: value }));
  };

  const handleSubmit = async (event) => {
    event.preventDefault();
    setError('');

    try {
      const { email, password } = form;
      if (isRegister) {
        const credential = await createUserWithEmailAndPassword(auth, email, password);
        await set(ref(db, `users/${credential.user.uid}`), {
          email,
          subscriptions: {}
        });
      } else {
        await signInWithEmailAndPassword(auth, email, password);
      }
    } catch (err) {
      setError(err.message);
    }
  };

  return (
    <div className="auth-container">
      <h1>{isRegister ? 'Create Account' : 'Sign In'}</h1>
      <form onSubmit={handleSubmit}>
        <div className="form-field">
          <label htmlFor="email">Email</label>
          <input id="email" name="email" type="email" required onChange={handleChange} value={form.email} />
        </div>
        <div className="form-field">
          <label htmlFor="password">Password</label>
          <input id="password" name="password" type="password" minLength={8} required onChange={handleChange} value={form.password} />
        </div>
        {error && <span style={{ color: '#ff7b7b' }}>{error}</span>}
        <button className="primary-button" type="submit">
          {isRegister ? 'Register' : 'Sign In'}
        </button>
      </form>
      <button
        style={{ marginTop: '1rem', background: 'transparent', color: '#7f5af0', border: 'none' }}
        type="button"
        onClick={() => setIsRegister((prev) => !prev)}
      >
        {isRegister ? 'Already have an account? Sign in' : "Don't have an account? Create one"}
      </button>
    </div>
  );
}
