# Navier-Stokes Math Notes

This note exists to keep the code comments short and the math references explicit.

## Governing Equations

For the intended 2D incompressible solver:

```text
du/dt + (u · grad)u = -(1/rho) grad p + nu laplacian(u) + f
div(u) = 0
```

Where:

- `u` is velocity
- `p` is pressure
- `rho` is density
- `nu` is kinematic viscosity
- `f` is external forcing

## Operator Split Used In Real-Time Solvers

The common real-time split is:

1. Add external forces and source terms.
2. Advect velocity.
3. Diffuse velocity.
4. Compute divergence of the intermediate velocity `u*`.
5. Solve the Poisson pressure equation.
6. Project the velocity to make it divergence free.
7. Advect scalar fields such as dye or smoke density.

This is the split used by the skeleton in `FluidFrameSystem`.

## Discrete Equations Used By The Skeleton

### Divergence on a MAC grid

With `u` stored on vertical faces and `v` on horizontal faces:

```text
div(u*)[i,j] =
    (u[i+1/2,j] - u[i-1/2,j]
   + v[i,j+1/2] - v[i,j-1/2]) / h
```

### Pressure Poisson equation

```text
laplacian(p) = (rho / dt) div(u*)
```

In the code sketch we absorb `rho` into the chosen units and use a Jacobi solve.

### Jacobi update

```text
p_new[i,j] = (pL + pR + pB + pT - h^2 rhs[i,j]) / 4
```

### Projection

```text
u^{n+1} = u* - (dt / rho) grad(p)
```

Again, `rho` can be absorbed into units for a sandbox solver.

### Semi-Lagrangian scalar advection

```text
phi^{n+1}(x) = phi^n(x - dt * u(x))
```

This is stable and easy to implement, but numerically dissipative.

## Why These Choices

For a game-like interactive fluid:

- semi-Lagrangian advection is stable
- Jacobi is simple and parallel-friendly
- MAC grids reduce pressure/velocity checkerboarding

The main downside is dissipation, which can later be reduced with:

- MacCormack / BFECC advection
- vorticity confinement
- better pressure solvers

## References

These are the main references I would keep next to the code:

1. Jos Stam, "Stable Fluids", SIGGRAPH 1999.
2. Robert Bridson, *Fluid Simulation for Computer Graphics*, 2nd ed.
3. Mark J. Harris, "Fast Fluid Dynamics Simulation on the GPU", GPU Gems, Chapter 38.

## Practical Reading Order

If you want a compact progression:

1. Stam for the overall split and why it is stable.
2. GPU Gems 38 for the pass-by-pass implementation mindset.
3. Bridson for the cleaner numerical framing and boundary thinking.
