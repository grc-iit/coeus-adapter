#ifndef LBM_ADIOS2_WRITER_HPP
#define LBM_ADIOS2_WRITER_HPP

// ADIOS2 output for the 2D LBM-CFD simulation.
//
// Streams the vorticity field as one global row-major (y-slow, x-fast) array
// over an ADIOS2 engine selected in the XML config (SST for live streaming,
// BP5 for a file). Only vorticity is shipped as a *raw* field; the density /
// instability trigger statistics are produced in situ via ADIOS2
// derived-variable operators (variance / add on vorticity) when `derived` is
// enabled -- the same pattern gray-scott uses (derive/VarV). A global variance
// is pooled across writer blocks on the reader/trigger side (add supplies the
// block sums for the pooled-variance combine).

#include <algorithm>
#include <string>
#include <vector>

#include <adios2.h>

#include "lbmd2q9_mpi.hpp"

class Lbm2DAdiosWriter
{
public:
    Lbm2DAdiosWriter(adios2::IO io, LbmD2Q9 *lbm, bool derived)
        : io_(io), lbm_(lbm), derived_(derived)
    {
        const uint32_t total_x = lbm_->getTotalDimX();
        const uint32_t total_y = lbm_->getTotalDimY();
        const uint32_t num_x = lbm_->getSizeX();   // interior width (no ghosts)
        const uint32_t num_y = lbm_->getSizeY();   // interior height (no ghosts)
        const uint32_t off_x = lbm_->getOffsetX(); // global start of interior
        const uint32_t off_y = lbm_->getOffsetY();

        // Global 2D array, row-major to match the LBM memory layout
        // (y is the slow axis, x the fast axis).
        var_vort_ = io_.DefineVariable<double>(
            "vorticity", {total_y, total_x}, {off_y, off_x}, {num_y, num_x});

        // Per-step scalars (written identically by every rank).
        var_step_ = io_.DefineVariable<int>("step");
        var_time_ = io_.DefineVariable<double>("time");
        var_stable_ = io_.DefineVariable<int>("stable");

        // Fides uniform-2D data model so a ParaView / Fides reader can consume
        // the stream directly (vorticity as cell data).
        io_.DefineAttribute<std::string>("Fides_Data_Model", "uniform");
        const double origin[3] = {0.0, 0.0, 0.0};
        io_.DefineAttribute<double>("Fides_Origin", origin, 3);
        const double spacing[3] = {1.0, 1.0, 1.0};
        io_.DefineAttribute<double>("Fides_Spacing", spacing, 3);
        io_.DefineAttribute<std::string>("Fides_Dimension_Variable", "vorticity");
        const std::vector<std::string> vlist = {"vorticity"};
        const std::vector<std::string> alist = {"cells"};
        io_.DefineAttribute<std::string>("Fides_Variable_List", vlist.data(),
                                         vlist.size());
        io_.DefineAttribute<std::string>("Fides_Variable_Associations",
                                         alist.data(), alist.size());

        if (derived_)
        {
            // Instability trigger signal. As the D2Q9 scheme goes unstable the
            // vorticity field's heterogeneity explodes (and eventually goes
            // non-finite), so variance(vorticity) spikes sharply -- the same
            // rise/collapse detector gray-scott keys on. Each reduces the local
            // block to a single value; the exact global variance is pooled from
            // derive/VarVort + derive/AddVort across blocks on the trigger side.
            io_.DefineDerivedVariable("derive/VarVort",
                                      "x = vorticity \n"
                                      "variance(x)",
                                      adios2::DerivedVarType::StoreData);
            io_.DefineDerivedVariable("derive/AddVort",
                                      "x = vorticity \n"
                                      "add(x)",
                                      adios2::DerivedVarType::StoreData);
        }
    }

    void open(const std::string &name)
    {
        writer_ = io_.Open(name, adios2::Mode::Write);
    }

    // Pack this rank's interior (num_y x num_x, skipping ghost cells) out of the
    // ghosted local vorticity array and Put it. Collective across ranks.
    void write(int step, double time, bool stable)
    {
        lbm_->computeVorticity();

        const uint32_t num_x = lbm_->getSizeX();
        const uint32_t num_y = lbm_->getSizeY();
        const uint32_t start_x = lbm_->getStartX(); // interior offset in ghosted
        const uint32_t start_y = lbm_->getStartY();
        const uint32_t dim_x = lbm_->getDimX();     // ghosted row stride
        const double *vort = lbm_->getVorticity();

        buf_.resize(static_cast<size_t>(num_x) * num_y);
        for (uint32_t jj = 0; jj < num_y; ++jj)
        {
            const double *src =
                vort + static_cast<size_t>(start_y + jj) * dim_x + start_x;
            std::copy(src, src + num_x, buf_.data() + static_cast<size_t>(jj) * num_x);
        }

        const int stable_i = stable ? 1 : 0;

        writer_.BeginStep();
        writer_.Put<double>(var_vort_, buf_.data());
        writer_.Put<int>(var_step_, &step);
        writer_.Put<double>(var_time_, &time);
        writer_.Put<int>(var_stable_, &stable_i);
        writer_.EndStep();
    }

    void close()
    {
        if (writer_)
        {
            writer_.Close();
        }
    }

private:
    adios2::IO io_;
    LbmD2Q9 *lbm_;
    bool derived_;
    adios2::Engine writer_;
    adios2::Variable<double> var_vort_;
    adios2::Variable<int> var_step_;
    adios2::Variable<double> var_time_;
    adios2::Variable<int> var_stable_;
    std::vector<double> buf_;
};

#endif // LBM_ADIOS2_WRITER_HPP
