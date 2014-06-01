#ifndef AMGCL_BACKEND_VEXCL_HPP
#define AMGCL_BACKEND_VEXCL_HPP

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
 * \file   amgcl/backend/vexcl.hpp
 * \author Denis Demidov <dennis.demidov@gmail.com>
 * \brief  VexCL backend.
 */

#include <vexcl/vexcl.hpp>

namespace amgcl {
namespace backend {

template <typename V, typename C, typename P>
struct value_type< vex::SpMat<V, C, P> > {
    typedef V type;
};

template < typename V, typename C, typename P >
struct rows_impl< vex::SpMat<V, C, P> > {
    static size_t get(const vex::SpMat<V, C, P> &A) {
        return A.rows();
    }
};

template < typename V, typename C, typename P >
struct cols_impl< vex::SpMat<V, C, P> > {
    static size_t get(const vex::SpMat<V, C, P> &A) {
        return A.cols();
    }
};

template < typename V, typename C, typename P >
struct nonzeros_impl< vex::SpMat<V, C, P> > {
    static size_t get(const vex::SpMat<V, C, P> &A) {
        return A.nonzeros();
    }
};

//---------------------------------------------------------------------------
// VexCL backend definition
//---------------------------------------------------------------------------
template <typename real>
struct vexcl {
    typedef real value_type;
    typedef long index_type;

    typedef vex::SpMat<value_type, index_type, index_type> matrix;
    typedef vex::vector<value_type>                        vector;

    struct params {
        std::vector< vex::backend::command_queue > q;
    };

    static boost::shared_ptr<matrix>
    copy_matrix(boost::shared_ptr< typename builtin<real>::matrix > A, const params &prm)
    {
        return boost::make_shared<matrix>(prm.q, rows(*A), cols(*A),
                A->ptr.data(), A->col.data(), A->val.data()
                );
    }

    static boost::shared_ptr<vector>
    copy_vector(boost::shared_ptr< typename builtin<real>::vector > x, const params &prm)
    {
        return boost::make_shared<vector>(prm.q, *x);
    }

    static boost::shared_ptr<vector>
    copy_vector(typename builtin<real>::vector const &x, const params &prm)
    {
        return boost::make_shared<vector>(prm.q, x);
    }

    static boost::shared_ptr<vector>
    create_vector(size_t size, const params &prm)
    {
        return boost::make_shared<vector>(prm.q, size);
    }
};

template < typename V, typename C, typename P >
struct spmv_impl< vex::SpMat<V, C, P>, vex::vector<V> >
{
    typedef vex::SpMat<V, C, P> matrix;
    typedef vex::vector<V>      vector;

    static void apply(V alpha, const matrix &A, const vector &x,
            V beta, vector &y)
    {
        if (beta)
            y = alpha * (A * x) + beta * y;
        else
            y = alpha * (A * x);
    }
};

template < typename V, typename C, typename P >
struct residual_impl< vex::SpMat<V, C, P>, vex::vector<V> >
{
    typedef vex::SpMat<V, C, P> matrix;
    typedef vex::vector<V>      vector;

    static void apply(const vector &rhs, const matrix &A, const vector &x,
            vector &r)
    {
        r = rhs - A * x;
    }
};

template < typename V >
struct clear_impl< vex::vector<V> >
{
    static void apply(vex::vector<V> &x)
    {
        x = 0;
    }
};

template < typename V >
struct inner_product_impl< vex::vector<V> >
{
    static V get(const vex::vector<V> &x, const vex::vector<V> &y)
    {
        vex::Reductor<V, vex::SUM> sum( x.queue_list() );
        return sum(x * y);
    }
};

template < typename V >
struct axpby_impl< vex::vector<V> > {
    static void apply(V a, const vex::vector<V> &x, V b, vex::vector<V> &y)
    {
        if (b)
            y = a * x + b * y;
        else
            y = a * x;
    }
};

template < typename V >
struct vmul_impl< vex::vector<V> > {
    static void apply(V a, const vex::vector<V> &x, const vex::vector<V> &y,
            V b, vex::vector<V> &z)
    {
        if (b)
            z = a * x * y + b * z;
        else
            z = a * x * y;
    }
};

template < typename V >
struct copy_impl< vex::vector<V> > {
    static void apply(const vex::vector<V> &x, vex::vector<V> &y)
    {
        y = x;
    }
};


} // namespace backend
} // namespace amgcl

#endif
