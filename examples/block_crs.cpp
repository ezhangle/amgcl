#include <iostream>
#include <fstream>
#include <iterator>
#include <algorithm>

#include <amgcl/amgcl.hpp>

#include <amgcl/backend/block_crs.hpp>
#include <amgcl/coarsening/aggregation.hpp>
#include <amgcl/relaxation/damped_jacobi.hpp>
#include <amgcl/solver/bicgstab.hpp>
#include <amgcl/profiler.hpp>

namespace amgcl {
    profiler<> prof("v2");
}

int main() {
    using amgcl::prof;

    typedef amgcl::amg<
        amgcl::backend::block_crs<double>,
        amgcl::coarsening::aggregation,
        amgcl::relaxation::damped_jacobi
        > AMG;

    amgcl::backend::crs<double, int> A;
    std::vector<double> rhs;

    prof.tic("read");
    {
        std::istream_iterator<int> iend;
        std::istream_iterator<double> dend;

        std::ifstream fptr("rows.txt");
        std::istream_iterator<int> iptr(fptr);

        std::ifstream fcol("cols.txt");
        std::istream_iterator<int> icol(fcol);

        std::ifstream fval("values.txt");
        std::istream_iterator<double> ival(fval);

        std::ifstream frhs("rhs.txt");
        std::istream_iterator<double> irhs(frhs);

        A.ptr.assign(iptr, iend);
        A.col.assign(icol, iend);
        A.val.assign(ival, dend);

        rhs.assign(irhs, dend);
    }
    int n = A.nrows = A.ncols = A.ptr.size() - 1;
    prof.toc("read");

    prof.tic("build");
    AMG::params prm;
    prm.backend.block_size = 4;
    AMG amg(A, prm);
    prof.toc("build");

    std::cout << amg << std::endl;

    std::vector<double> x(n, 0);

    amgcl::solver::bicgstab<AMG::backend_type> solve(n);

    prof.tic("solve");
    size_t iters;
    double resid;
    boost::tie(iters, resid) = solve(amg.top_matrix(), rhs, amg, x);
    prof.toc("solve");

    std::cout << "Iterations: " << iters << std::endl
              << "Error:      " << resid << std::endl
              << std::endl;

    std::cout << amgcl::prof << std::endl;
}
