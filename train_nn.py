#!/usr/bin/env python3
"""
Neural Network Training for Universe Simulator
Trains a trajectory prediction network using PyTorch
"""
import numpy as np
import struct
from pathlib import Path

try:
    import torch
    import torch.nn as nn
    import torch.optim as optim
    HAS_TORCH = True
except ImportError:
    HAS_TORCH = False
    print("PyTorch not available, using numpy fallback")


class ParticlePredictor(nn.Module if HAS_TORCH else object):
    def __init__(self, input_size=7, hidden_size=32, output_size=3):
        super().__init__() if HAS_TORCH else object.__init__(self)
        self.input_size = input_size
        self.hidden_size = hidden_size
        self.output_size = output_size

        if HAS_TORCH:
            self.net = nn.Sequential(
                nn.Linear(input_size, hidden_size),
                nn.Tanh(),
                nn.Linear(hidden_size, hidden_size),
                nn.Tanh(),
                nn.Linear(hidden_size, output_size),
                nn.Tanh()
            )

    def forward(self, x):
        return self.net(x) if HAS_TORCH else x


class DensityEstimator(nn.Module if HAS_TORCH else object):
    def __init__(self, input_size=3, hidden_size=64, output_size=1):
        super().__init__() if HAS_TORCH else object.__init__(self)
        if HAS_TORCH:
            self.net = nn.Sequential(
                nn.Linear(input_size, hidden_size),
                nn.ReLU(),
                nn.Linear(hidden_size, hidden_size),
                nn.ReLU(),
                nn.Linear(hidden_size, hidden_size),
                nn.ReLU(),
                nn.Linear(hidden_size, output_size),
                nn.Softplus()
            )

    def forward(self, x):
        return self.net(x) if HAS_TORCH else x


def generate_training_data(num_samples=10000, num_particles=100, seed=42):
    np.random.seed(seed)

    positions_prev = np.random.randn(num_samples, 3) * 1e10
    velocities = np.random.randn(num_samples, 3) * 1e5
    masses = np.abs(np.random.randn(num_samples)) * 1e25

    dt = 1e6 * 3.15576e7
    positions_curr = positions_prev + velocities * dt

    features = np.column_stack([
        positions_prev,
        velocities,
        masses
    ]).astype(np.float32)

    targets = positions_curr.astype(np.float32)

    return features, targets


def train_predictor(features, targets, hidden_size=32, epochs=1000, lr=0.001):
    if not HAS_TORCH:
        return train_predictor_numpy(features, targets)

    device = torch.device('cpu')
    X = torch.FloatTensor(features).to(device)
    y = torch.FloatTensor(targets).to(device)

    model = ParticlePredictor(
        input_size=features.shape[1],
        hidden_size=hidden_size,
        output_size=targets.shape[1]
    ).to(device)

    optimizer = optim.Adam(model.parameters(), lr=lr)
    scheduler = optim.lr_scheduler.ReduceLROnPlateau(optimizer, patience=50, factor=0.5)
    loss_fn = nn.MSELoss()

    for epoch in range(epochs):
        optimizer.zero_grad()
        output = model(X)
        loss = loss_fn(output, y)
        loss.backward()
        optimizer.step()
        scheduler.step(loss)

        if epoch % 200 == 0:
            print(f"  Epoch {epoch}/{epochs}, Loss: {loss.item():.6e}")

    return model


def train_predictor_numpy(features, targets):
    """Simple numpy fallback with one-layer training"""
    np.random.seed(42)
    n, d_in = features.shape
    _, d_out = targets.shape
    hidden = 32

    W1 = np.random.randn(d_in, hidden) * np.sqrt(2.0 / d_in)
    b1 = np.zeros(hidden)
    W2 = np.random.randn(hidden, d_out) * np.sqrt(2.0 / hidden)
    b2 = np.zeros(d_out)

    lr = 0.0001
    for epoch in range(1000):
        h = np.tanh(features @ W1 + b1)
        pred = np.tanh(h @ W2 + b2)
        loss = np.mean((pred - targets) ** 2)

        grad_pred = 2 * (pred - targets) / n
        dW2 = h.T @ grad_pred
        db2 = np.sum(grad_pred, axis=0)
        grad_h = grad_pred @ W2.T * (1 - h ** 2)
        dW1 = features.T @ grad_h
        db1 = np.sum(grad_h, axis=0)

        W2 -= lr * dW2
        b2 -= lr * db2
        W1 -= lr * dW1
        b1 -= lr * db1

        if epoch % 200 == 0:
            print(f"  Epoch {epoch}/1000, Loss: {loss:.6e}")

    return W1, b1, W2, b2


def export_weights_pytorch(model, filepath="nn_weights.bin"):
    """Export PyTorch model weights to binary format"""
    if not HAS_TORCH:
        W1, b1, W2, b2 = model
        num_layers = 3
        layer_sizes = [7, 32, 3]
        weights_list = [W1.T, W2.T]
        biases_list = [b1, b2]
    else:
        layers = [m for m in model.net if isinstance(m, nn.Linear)]
        num_layers = len(layers) + 1
        layer_sizes = [model.input_size]
        weights_list = []
        biases_list = []
        for l in layers:
            layer_sizes.append(l.weight.shape[0])
            weights_list.append(l.weight.detach().numpy())
            biases_list.append(l.bias.detach().numpy())

    with open(filepath, 'wb') as f:
        f.write(struct.pack('i', num_layers))
        for s in layer_sizes:
            f.write(struct.pack('i', s))
        for w, b in zip(weights_list, biases_list):
            w_flat = np.ascontiguousarray(w.T).flatten()
            w_flat.astype(np.float64).tofile(f)
            b.astype(np.float64).tofile(f)

    print(f"Weights exported to {filepath}")
    for i, (w, b) in enumerate(zip(weights_list, biases_list)):
        print(f"  Layer {i}: weights {w.shape}, biases {b.shape}")


def train_density_estimator():
    """Train a network that estimates particle density at any 3D point"""
    np.random.seed(42)

    centres = np.random.randn(10, 3) * 1e15
    widths = np.abs(np.random.randn(10)) * 1e13 + 1e12
    densities = np.abs(np.random.randn(10)) * 1e10 + 1e5

    X = np.random.randn(50000, 3) * 5e15
    y = np.zeros(50000)
    for i in range(50000):
        for c, w, d in zip(centres, widths, densities):
            r2 = np.sum((X[i] - c) ** 2)
            y[i] += d * np.exp(-r2 / (2 * w ** 2))

    y = y.astype(np.float32)
    X = X.astype(np.float32)

    if HAS_TORCH:
        X_t = torch.FloatTensor(X)
        y_t = torch.FloatTensor(y).reshape(-1, 1)

        model = DensityEstimator()
        optimizer = optim.Adam(model.parameters(), lr=0.001)
        loss_fn = nn.MSELoss()

        for epoch in range(500):
            optimizer.zero_grad()
            pred = model(X_t)
            loss = loss_fn(pred, y_t)
            loss.backward()
            optimizer.step()
            if epoch % 100 == 0:
                print(f"  Density Epoch {epoch}, Loss: {loss.item():.3e}")

        torch.save(model.state_dict(), "density_estimator.pt")
        print("Density estimator saved")
        return model

    return None


if __name__ == "__main__":
    print("=" * 60)
    print("Neural Network Training for Universe Simulator")
    print("=" * 60)

    print("\n1. Training Particle Trajectory Predictor...")
    features, targets = generate_training_data(num_samples=20000)
    print(f"   Data: {features.shape[0]} samples, {features.shape[1]} features")

    model = train_predictor(features, targets, hidden_size=64, epochs=500)
    export_weights(model, "nn_weights.bin")

    print("\n2. Training Density Estimator...")
    train_density_estimator()

    print("\n3. Creating Interpolation Weights...")
    interp_weights = np.array([7, 64, 64, 3], dtype=np.int32)
    with open("nn_interp.bin", 'wb') as f:
        interp_weights.tofile(f)
    print("   Interpolation config saved")

    print("\nTraining complete!")
