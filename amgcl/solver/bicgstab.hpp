#ifndef AMGCL_SOLVERS_BICGSTAB_HPP
#define AMGCL_SOLVERS_BICGSTAB_HPP

/*
The MIT License

Copyright (c) 2012-2014 Denis Demidov <dennis.demidov@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

/**
 * \file   amgcl/solver/bicgstab.hpp
 * \author Denis Demidov <dennis.demidov@gmail.com>
 * \brief  BiCGStab iterative method.
 *
 * Implementation is based on \ref Templates_1994 "Barrett (1994)"
 */

#include <boost/tuple/tuple.hpp>
#include <amgcl/backend/interface.hpp>

namespace amgcl {
namespace solver {

template <class Backend>
class bicgstab {
    public:
        typedef typename Backend::vector                vector;
        typedef typename Backend::value_type            value_type;
        typedef typename Backend::params                backend_params;
        typedef typename math::scalar<value_type>::type scalar;

        bicgstab(
                size_t n, const backend_params &prm = backend_params(),
                size_t maxiter = 100, scalar tol = 1e-8
                )
            : n(n), maxiter(maxiter), tol(tol),
              r ( Backend::create_vector(n, prm) ),
              p ( Backend::create_vector(n, prm) ),
              v ( Backend::create_vector(n, prm) ),
              s ( Backend::create_vector(n, prm) ),
              t ( Backend::create_vector(n, prm) ),
              rh( Backend::create_vector(n, prm) ),
              ph( Backend::create_vector(n, prm) ),
              sh( Backend::create_vector(n, prm) )
        { }

        template <class Matrix, class Precond>
        boost::tuple<size_t, scalar> operator()(
                Matrix  const &A,
                vector  const &rhs,
                Precond const &P,
                vector        &x
                ) const
        {
            backend::residual(rhs, A, x, *r);
            backend::copy(*r, *rh);

            scalar rho1  = 0, rho2  = 0;
            scalar alpha = 0, omega = 0;

            scalar norm_of_rhs = backend::norm(rhs);

            size_t iter;
            scalar res = 2 * tol;

            for(iter = 0; res > tol && iter < maxiter; ++iter) {
                rho2 = rho1;
                rho1 = backend::inner_product(*r, *rh);

                if (fabs(rho1) < 1e-32)
                    throw std::logic_error("Zero rho in BiCGStab");

                if (iter) {
                    scalar beta = (rho1 * alpha) / (rho2 * omega);
                    backend::axpbypcz(1, *r, -beta * omega, *v, beta, *p);
                } else {
                    backend::copy(*r, *p);
                }

                backend::clear(*ph);
                P(*p, *ph);

                backend::spmv(1, A, *ph, 0, *v);

                alpha = rho1 / backend::inner_product(*rh, *v);

                backend::axpbypcz(1, *r, -alpha, *v, 0, *s);

                if ((res = backend::norm(*s) / norm_of_rhs) < tol) {
                    backend::axpby(alpha, *ph, 1, x);
                } else {
                    backend::clear(*sh);
                    P(*s, *sh);

                    backend::spmv(1, A, *sh, 0, *t);

                    omega = backend::inner_product(*t, *s)
                          / backend::inner_product(*t, *t);

                    if (fabs(omega) < 1e-32)
                        throw std::logic_error("Zero omega in BiCGStab");

                    backend::axpbypcz(alpha, *ph, omega, *sh, 1, x);
                    backend::axpbypcz(1, *s, -omega, *t, 0, *r);

                    res = backend::norm(*r) / norm_of_rhs;
                }
            }

            return boost::make_tuple(iter, res);
        }

    private:
        size_t n;
        size_t maxiter;
        scalar tol;

        boost::shared_ptr<vector> r;
        boost::shared_ptr<vector> p;
        boost::shared_ptr<vector> v;
        boost::shared_ptr<vector> s;
        boost::shared_ptr<vector> t;
        boost::shared_ptr<vector> rh;
        boost::shared_ptr<vector> ph;
        boost::shared_ptr<vector> sh;
};

} // namespace solver
} // namespace amgcl


#endif
