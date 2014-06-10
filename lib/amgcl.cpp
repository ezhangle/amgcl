#include <iostream>

#include <boost/type_traits.hpp>

#include <amgcl/amgcl.hpp>

#include <amgcl/backend/builtin.hpp>
#include <amgcl/backend/block_crs.hpp>
#include <amgcl/backend/crs_tuple.hpp>

#include <amgcl/coarsening/ruge_stuben.hpp>
#include <amgcl/coarsening/pointwise_aggregates.hpp>
#include <amgcl/coarsening/aggregation.hpp>
#include <amgcl/coarsening/smoothed_aggregation.hpp>
#include <amgcl/coarsening/smoothed_aggr_emin.hpp>

#include <amgcl/relaxation/gauss_seidel.hpp>
#include <amgcl/relaxation/ilu0.hpp>
#include <amgcl/relaxation/damped_jacobi.hpp>
#include <amgcl/relaxation/spai0.hpp>
#include <amgcl/relaxation/chebyshev.hpp>

#include <amgcl/solver/cg.hpp>
#include <amgcl/solver/bicgstab.hpp>
#include <amgcl/solver/gmres.hpp>

#include "amgcl.h"

template <typename V>
struct pointer_wrapper {
    typedef V value_type;

    size_t n;
    V *ptr;

    pointer_wrapper(size_t n, const V *ptr)
        : n(n), ptr(const_cast<V*>(ptr)) {}

    size_t size() const {
        return n;
    }

    const V& operator[](size_t i) const {
        return ptr[i];
    }

    V& operator[](size_t i) {
        return ptr[i];
    }

};

template <typename V>
pointer_wrapper<V> wrap(size_t n, V *ptr) {
    return pointer_wrapper<V>(n, ptr);
}

namespace amgcl {
    namespace backend {
        template <typename V>
        struct is_builtin_vector< pointer_wrapper<V> > : boost::true_type {};
    }
}

//---------------------------------------------------------------------------
template <
    class Backend,
    class Coarsening,
    template <class> class Relaxation,
    class Func
    >
void process(const Func &func)
{
    typedef amgcl::amg<Backend, Coarsening, Relaxation> AMG;
    func.template process<AMG>();
}

//---------------------------------------------------------------------------
template <
    class Backend,
    class Coarsening,
    class Func
    >
void process(amgclRelaxation relaxation, const Func &func)
{
    switch (relaxation) {
        case amgclRelaxationDampedJacobi:
            process<
                Backend,
                Coarsening,
                amgcl::relaxation::damped_jacobi
                >(func);
            break;
        case amgclRelaxationSPAI0:
            process<
                Backend,
                Coarsening,
                amgcl::relaxation::spai0
                >(func);
            break;
        case amgclRelaxationChebyshev:
            process<
                Backend,
                Coarsening,
                amgcl::relaxation::chebyshev
                >(func);
            break;
    }
}

//---------------------------------------------------------------------------
template <
    class Backend,
    class Func
    >
void process(
        amgclCoarsening coarsening,
        amgclRelaxation relaxation,
        const Func &func
        )
{
    switch (coarsening) {
        case amgclCoarseningRugeStuben:
            process<
                Backend,
                amgcl::coarsening::ruge_stuben
                >(relaxation, func);
            break;
        case amgclCoarseningAggregation:
            process<
                Backend,
                amgcl::coarsening::aggregation<
                    amgcl::coarsening::pointwise_aggregates
                    >
                >(relaxation, func);
            break;
        case amgclCoarseningSmoothedAggregation:
            process<
                Backend,
                amgcl::coarsening::smoothed_aggregation<
                    amgcl::coarsening::pointwise_aggregates
                    >
                >(relaxation, func);
            break;
        case amgclCoarseningSmoothedAggrEMin:
            process<
                Backend,
                amgcl::coarsening::smoothed_aggr_emin<
                    amgcl::coarsening::pointwise_aggregates
                    >
                >(relaxation, func);
            break;
    }
}

//---------------------------------------------------------------------------
template <class Func>
void process(
        amgclBackend    backend,
        amgclCoarsening coarsening,
        amgclRelaxation relaxation,
        const Func &func
        )
{
    switch (backend) {
        case amgclBackendBuiltin:
            process< amgcl::backend::builtin<double> >(
                    coarsening, relaxation, func);
            break;
        case amgclBackendBlockCRS:
            process< amgcl::backend::block_crs<double> >(
                    coarsening, relaxation, func);
            break;
    }
}

//---------------------------------------------------------------------------
struct Handle {
    amgclBackend    backend;
    amgclCoarsening coarsening;
    amgclRelaxation relaxation;

    void *amg;
};

//---------------------------------------------------------------------------
struct do_create {
    size_t  n;

    const long   *ptr;
    const long   *col;
    const double *val;

    mutable void *handle;

    do_create(
            size_t  n,
            const long   *ptr,
            const long   *col,
            const double *val
          )
        : n(n), ptr(ptr), col(col), val(val), handle(0)
    {}

    template <class AMG>
    void process() const {
        AMG *amg = new AMG(
                boost::make_tuple(
                    n, n,
                    boost::make_iterator_range(val, val + ptr[n]),
                    boost::make_iterator_range(col, col + ptr[n]),
                    boost::make_iterator_range(ptr, ptr + n + 1)
                    )
                );

        std::cout << *amg << std::endl;

        handle = static_cast<void*>(amg);
    }
};

//---------------------------------------------------------------------------
amgclHandle amgcl_create(
        amgclBackend    backend,
        amgclCoarsening coarsening,
        amgclRelaxation relaxation,
        size_t n,
        const long   *ptr,
        const long   *col,
        const double *val
        )
{
    do_create c(n, ptr, col, val);
    process(backend, coarsening, relaxation, c);

    Handle *h = new Handle();

    h->backend    = backend;
    h->coarsening = coarsening;
    h->relaxation = relaxation;
    h->amg        = c.handle;

    return static_cast<amgclHandle>(h);
}

//---------------------------------------------------------------------------
struct do_destroy {
    void *handle;

    do_destroy(void *handle) : handle(handle) {}

    template <class AMG>
    void process() const {
        delete static_cast<AMG*>(handle);
    }
};

//---------------------------------------------------------------------------
void amgcl_destroy(amgclHandle handle) {
    Handle *h = static_cast<Handle*>(handle);

    process(h->backend, h->coarsening, h->relaxation, do_destroy(h->amg));
}

//---------------------------------------------------------------------------
struct do_solve {
    void *handle;
    amgclSolver solver;
    const double *rhs;
    double *x;

    do_solve(void *handle, amgclSolver solver,
        const double *rhs, double *x
        ) : handle(handle), solver(solver), rhs(rhs), x(x)
    {}

    template <class Solver, class AMG>
    void solve(const AMG &amg) const {
        const size_t n = amgcl::backend::rows( amg.top_matrix() );

        Solver s(n);
        size_t iters;
        double resid;

        pointer_wrapper<double> w_rhs(n, rhs);
        pointer_wrapper<double> w_x  (n, x);

        boost::tie(iters, resid) = s(amg, w_rhs, w_x);

        std::cout
            << "Iterations: " << iters << std::endl
            << "Error:      " << resid << std::endl
            << std::endl;

    }

    template <class AMG>
    void process() const {
        typedef typename AMG::backend_type Backend;

        AMG *amg = static_cast<AMG*>(handle);

        switch (solver) {
            case amgclSolverCG:
                solve< amgcl::solver::cg<Backend> >(*amg);
                break;
            case amgclSolverBiCGStab:
                solve< amgcl::solver::bicgstab<Backend> >(*amg);
                break;
            case amgclSolverGMRES:
                solve< amgcl::solver::gmres<Backend> >(*amg);
                break;
        }
    }
};

//---------------------------------------------------------------------------
void amgcl_solve(
        amgclSolver solver,
        amgclHandle handle,
        const double *rhs,
        double *x
        )
{
    Handle *h = static_cast<Handle*>(handle);

    process(h->backend, h->coarsening, h->relaxation,
            do_solve(h->amg, solver, rhs, x));
}