#include <iostream>
#include <cstdlib>

#include <amgcl/amgcl.hpp>
#include <amgcl/interp_smoothed_aggr.hpp>
#include <amgcl/aggr_plain.hpp>
#include <amgcl/level_viennacl.hpp>
#include <amgcl/operations_viennacl.hpp>
#include <amgcl/profiler.hpp>

#ifdef VIENNACL_WITH_OPENCL
#  include <vexcl/devlist.hpp>
#endif

#if defined(VIENNACL_WITH_OPENCL) || defined(VIENNACL_WITH_CUDA)
#  include <viennacl/hyb_matrix.hpp>
#else
#  include <viennacl/compressed_matrix.hpp>
#endif

#include <viennacl/vector.hpp>
#include <viennacl/linalg/cg.hpp>

#include "read.hpp"

typedef double real;

namespace amgcl {
    profiler<> prof("viennacl");
}
using amgcl::prof;

int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <problem.dat>" << std::endl;
        return 1;
    }

#ifdef VIENNACL_WITH_OPENCL
    // There is no easy way to select compute device in ViennaCL, so just use
    // VexCL for that.
    vex::Context ctx(
            vex::Filter::Env &&
            vex::Filter::DoublePrecision &&
            vex::Filter::Count(1)
            );
    std::vector<cl_device_id> dev_id(1, ctx.queue(0).getInfo<CL_QUEUE_DEVICE>()());
    std::vector<cl_command_queue> queue_id(1, ctx.queue(0)());
    viennacl::ocl::setup_context(0, ctx.context(0)(), dev_id, queue_id);
    std::cout << ctx << std::endl;
#endif

    // Read matrix and rhs from a binary file.
    std::vector<int>  row;
    std::vector<int>  col;
    std::vector<real> val;
    std::vector<real> rhs;
    int n = read_problem(argv[1], row, col, val, rhs);

    // Wrap the matrix into amgcl::sparse::map:
    amgcl::sparse::matrix_map<real, int> A(
            n, n, row.data(), col.data(), val.data()
            );

    // Build the preconditioner.
    typedef amgcl::solver<
        real, int,
        amgcl::interp::smoothed_aggregation<amgcl::aggr::plain>,
#if defined(VIENNACL_WITH_OPENCL) || defined(VIENNACL_WITH_CUDA)
        amgcl::level::viennacl<amgcl::GPU_MATRIX_HYB, amgcl::relax::spai0>
#else
        amgcl::level::viennacl<amgcl::GPU_MATRIX_CRS, amgcl::relax::spai0>
#endif
        > AMG;

    // Use K-Cycle on each level to improve convergence:
    AMG::params prm;
    prm.level.kcycle = 1;

    prof.tic("setup");
    AMG amg(A, prm);
    prof.toc("setup");

    std::cout << amg << std::endl;

    // Copy matrix and rhs to GPU(s).
#if defined(VIENNACL_WITH_OPENCL) || defined(VIENNACL_WITH_CUDA)
    viennacl::hyb_matrix<real> Agpu;
#else
    viennacl::compressed_matrix<real> Agpu;
#endif
    viennacl::copy(amgcl::sparse::viennacl_map(A), Agpu);

    viennacl::vector<real> f(n);
    viennacl::fast_copy(rhs, f);

    // Solve the problem with CG method from ViennaCL. Use AMG as a
    // preconditioner:
    prof.tic("solve");
    viennacl::linalg::cg_tag tag(1e-8, 100);
    viennacl::vector<real> x = viennacl::linalg::solve(Agpu, f, tag,
            amgcl::make_viennacl_precond< viennacl::vector<real> >(amg));
    prof.toc("solve");

    std::cout << "Iterations: " << tag.iters() << std::endl
              << "Error:      " << tag.error() << std::endl;

    std::cout << prof;

#ifdef VIENNACL_WITH_OPENCL
    // Prevent ViennaCL from segfaulting:
    exit(0);
#endif
}
