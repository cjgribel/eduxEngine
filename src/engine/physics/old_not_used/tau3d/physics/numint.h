//
//  numint.h
//  tau3d
//
//  Created by Carl Johan Gribel on 2015-04-08.
//
//

/*
 numerical integration
 
 time-step flow:
 1. integrate V from A=F/m using custom scheme
    schemes will call Feval(t+dt*[0,1]) at least once (RK4: 4 times)
 2. iterative constraint solver on V
 3. integrate X from V: implicit scheme
    - implicit scheme most stable, but any first-order scheme can be used, e.g. explicit
    - here, Feval() is not used; X is integrated from V
    - high-order schemes unfeasible since V, which depends on constraints,
    then needs to be evaluated more times
 
 -  note that effects of any high-order scheme, according to this flow,
    applies only to time-dependent forces, such as springs
    gravity is not effected, for instance
    greatest gain for e.g. stiff springs, systems of springs (spring-based soft-bodies) ...
    less gain in simple cases with e.g. wheel suspension (but could still be worthwhile)
    ideally, use high-order schemes only locally, where most needed
 
 -  impl:
    use fixed, preallocated array for body-states (flags for removal)
    state indices enough for forces and bodies
 
 -  impl, additions needed:
    body state struct
    state-dependent force classes (body-indices as class members, take state-indices as arg's)
    ...
 
 - key issue:
    in Feval -> V, where does mass come in?
    really, we integrate A->V
    and A=F/m ...
 
 - key issue:
    accumulated torques need to be considered as well during integration
    most forces apply both linear force and torque
    ? keep an array for accumulated torques as well
    ? put body::apply_force in state
    then masses are needed there as well
    bodies will still reach them easily through their state
    don't kill future additions of non-rigid types of bodies
 
 issue:
    if Feval, and all schemes, are always only acting on A=F/m -> V,
    might as well name functions and arguments accordingly, instead of general s,ds etc
 
 info:
    second order eom diff, x'' = f(t,x,x'), with analytical solution [24]
 
 */

#ifndef tau3d_numint_h
#define tau3d_numint_h

class body_t
{
    // rigidstate_t ?
    struct bodystate_t
    {
        // X, V
        // Q, W
        // F and T not needed
        // + aux; R, Ri
        
        // apply_force() etc ?
        // mass?
        
        void update()
        {
            // update aux
        }
    }
};

// hash body pointers, compliment to vector<body_t>
unordered_set<body_t*> body_set;

/**
 * 2-body force (spring)
 * dV = F(t,X,V)
 */
class force2_t
{
    friend class body_t;
    // body pointers as well?
    int bodyA, bodyB; // assume fixed, preallocated body array
 
    /**
     * compute forces acting on states s[bodyA] and s[bodyB]
     * add forces to ds[bodyA].dV and ds[bodyB].dV (impulses would be added to dX)
     */
    void apply_forces(float time, bodystate_t* s, dstate_t* ds) = 0;
};

class springdamper_t : public force2_t { };

/**
 * N-body force (wind, vortex)
 * dV = F(t,X,V)
 */
class forceN_t
{
    // acts on ALL bodies (naïvely)
    // thus attempt to iterate instead of dehashing
    
    friend class body_t;
    body_t* bodies;
    
    void apply_forces(float time, bodystate_t* s, dstate_t* ds) = 0;
    // last arg: array of nbr_bodies vec3 = forces, dV
    // but dV is stored within a state_derivs struct, which contains dX and dV
    // could create a new vec3 dV[], but this then needs copying into the state_derivs
    // ? split up state_derivs into: vec3 dX, dV
};
class gravity_t : forceN_t {};
class vortex_t : forceN_t {};

// given: all forces
vector<force_t> forces;

template<class T>
struct dstate_t { vec3<T> dX, dY; };

/**
 *F(t,X,V)
 *
 * s = initial state
 * hs = initial state advanced to ti+h
 * ds = state derivative
 * hds = state derivative at ti+h
 *
 */
void F_eval(const state_t* s, const dstate_t* ds, const forces, t, h, dstate_t* hds)
{
    // state at t+h
    state_t hs[states.size()];
    
    // advance state to t+h
    for (int i=0; i<sc, i++)
    {
        hs.X = s[i].X + ds[i].dX * h;
        hs.V = s[i].V + ds[i].dV * h;
        
        hds[i].dX = hs.V;
        hds[i].dV = {0,0,0};
    }
    
    // * if force2_t and forceN_t heirs from the same baseclass (currently their apply_force match),
    // one loop would be enough
    // requires they are stored within the same array, as pointers.
    
    // apply 2-body forces for states at ti+h
    for (force2_t& f : forces2)
    {
        f.apply_force(t+h, hs, dhs);
    }
    
    // apply N-body forces for states at ti+h
    for (forceN_t& f : forcesN)
    {
        f.apply_force(t+h, hs, dhs);
    }
}

void numint_explicit(states, float t, float h)
{
    // eval forces at t = ti
    vec3f dX[states.size()];
    vec3f dY[states.size()];
    f_eval(states, t, 0, dX, dY);
//    state_derivs_t ds0[states.size()];
//    f_eval(states, t, 0, ds0);
    
    for (int i=0; i<states.size(), i++)
    {
        state[i].X += h * dX[i];
        state[i].V += h * dV[i];
//        state[i].X += h * ds0[i].dX;
//        state[i].V += h * ds0[i].dV;
    }
}

void numint_implicit(states, float t, float h)
{
    // eval forces at t+h
    state_derivs_t dsh[states.size()];
    f_eval(states, t, h, dsh);
    
    for (int i=0; i<states.size(), i++)
    {
        // vel: explicit
        state[i].V += h * dsh[i].dV;
        // pos: implicit
        state[i].X += h * dsh[i].dX;
    }
}

void numint_symplectic(states, float t, float h)
{
    // eval forces at t+h
    state_derivs_t ds0[states.size()];
    f_eval(states, t, h, ds0);
    
    for (int i=0; i<states.size(), i++)
    {
        // vel: explicit
        state[i].V += h * ds0[i].dV;
        // pos: implicit
        state[i].X += h * state[i].V;
    }
}

void numint_trapezoid(states, float t, float h)
{
    int stc = states.size();
    state_derivs_t ds[states.size()*2];
    state_derivs_t* ds0 = ds+0*stc;
    state_derivs_t* ds1 = ds+1*stc;
    
    // evaluate forces at t and t+h
    f_eval(states, t, 0, ds0);
    f_eval(states, t, h, dsh);
    
    for (int i=0; i<states.size(), i++)
    {
        state[i].X += h/2 * ( ds0[i].dX + ds1[i].dX );
        state[i].V += h/2 * ( ds0[i].dV + ds1[i].dV );
    }
}

class integrator_t
{
protected:
    bodystate_t* s;
    int sc;
    
    integrator_t(bodystate_t* s, int sc) : sc(sc), s(s) { }
    
    virtual void step(float t, float h) = 0;
};

/** RK4 [20,21]
 *
 * initial value formulation:
 * y(t0) = y0       initial state
 * y'(t) = f(t,y)   derivative
 * find y(t0+h)
 *
 * for equations of motion, vector form:
 * X(t0) = X0       initial states
 * V(t0) = V0
 * X'(t) = V(t,X)   derivatives
 * V'(t) = F(t,X,V)
 
 * RK4
 * y(t0+h) = y(t0) + h/6 (k1 +2k2 + 2k3 + k4)
 * k1 = f( t0,          y0 )
 * k2 = f( t0 + 0.5h,   y0 + 0.5h k1 )
 * k3 = f( t0 + 0.5h,   y0 + 0.5h k2 )
 * k4 = f( t0 + h,      y0 + h k3 )
 *
 * in this context:
 *
 * integrate X and V of state from ti->ti+h
 * abstain updating pos. instead: update vel -> solve constraints -> update pos using implicit scheme
 */
class integrator_RK4_t : public integrator_t
{
    dstate_t *ds;
    dstate_t *ds1, *ds2, *ds3, *ds4; // k1-k4
public:
    
    integrator_RK4_t(bodystate_t* s, int sc) : integrator_t(s, sc)
    {
        ds = new dstate_t[sc*4];
        ds1 = ds+0*sc; // k1
        ds2 = ds+1*sc; // k2
        ds3 = ds+2*sc; // k3
        ds4 = ds+3*sc; // k4
    }
    
    ~integrator_RK4_t()
    {
        delete[] ds;
    }
    
    void step(float t, float h)
    {
        F_eval(s, {0,0},t, 0,   ds1);
        F_eval(s, ds1,  t, h/2, ds2);
        F_eval(s, ds2,  t, h/2, ds3);
        F_eval(s, ds3,  t, h,   ds4);
        
        for (int i=0; i<sc, i++)
        {
            // [optional] update pos
            s[i].X += h/6 * (ds1[i].dX + 2*(ds2[i].dX + ds3[i].dX) + ds4[i].dX);
            // update vel
            s[i].V += h/6 * (ds1[i].dV + 2*(ds2[i].dV + ds3[i].dV) + ds4[i].dV);
        }
        
        // k1 = f(ti, X[ti], V[ti]) (explicit step)
        //
        // for all forces
        //    created copies of the states of the bodies associated with the force: state1A, state1B
        //    * advance X = X[ti] + 0*V[ti] (unchanged)
        //    * set V = V[ti] (unchanged)
        //    apply_force(ti+0, state1A, state1B, slot=0)
        //    forces will be computed and added to slot 0 of the f-accum of each respective body
        // * the set V constitute change in X: apply? (0 here)
        // * the accumulated forces in stateA/B constitute change in V: apply to V state?
        // - it seems these (V,F)=(dX,dV) correspond to k1, wrt pos and vel ->
        //
        // k2 = f(ti+h/2, X[ti+h/2], V[ti] + h/2*k1) <-- X?
        //
        // for all forces
        //    created copies of the states of the bodies associated with the force: state2A, state2B
        //    * advance X = X[ti] + h/2*V[ti]
        //    * set V = V[ti] + h/2*k1 (accumulated force in slot 0 on respective body)
        //    apply_force(ti+h/2, state2A, state2B):
        // ...
        // combine accumulated forces to form V[ti+h]
    }
}

void numint_PredCorr(/* bodies? states? */, float h)
{
    // predictor
    // (explicit step, equivalent of k1 for RK4)
    //
    // compute am intermediate ~V
    // – how is this done effectively? fetch V from
    
    // corrector
    // for all forces
    //    call apply_force with ~V and slot 1 argument
    //
    // combine accum forces from slot 0-1 to form V[ti+h]
}


/*
 generalized integration
 
 overall goal:
 ability to integrate a general entity P[ti]->P[ti+h] from its derivative dP/dt using any scheme
 
 find an entity P[ti+h] from h and;
 dPdt[ti] (explicit)
 dPdt[ti+h] (implicit)
 other intermediate points between dPdt[ti] and dPdt[ti+h] (midpoint, pred-corr, RK4)
 
 motion equations (2nd order differential):
 { P, dPdt, ddP/ddt } <-> { X, V, F } <-> { Q, W, T }
 semi-implicit: explicit V (dPdt), and then implicit X (P)
 for non-explicit V/W, need to *evaluate* F/T at arbitrary t
 
 general, non-explicit case: needs ability to *evaluate* dPdt at arbitrary t
 e.g. F/T when integrating V/W
 
 velocity
 linear: V from F
 angular: W from T
 
 position:
 linear: X from V.
 angular: Q/R from W
 
 Verlet: no input velocity, just P at two times
 matrix-based integrator [14]
 */



#endif
