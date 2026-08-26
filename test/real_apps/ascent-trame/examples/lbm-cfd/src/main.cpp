#include <iostream>
#include <iomanip>
#include <vector>
#include <cstdint>
#include <cstring>
#include <string>
#include <cstdio>
#include <cmath>
#include <fstream>
#include <sys/stat.h>

#ifdef ASCENT_ENABLED
#include <ascent.hpp>
#include <conduit_blueprint_mpi.hpp>
#endif

#include "lbmd2q9_mpi.hpp"

#ifdef ADIOS2_ENABLED
#include "adios2_writer.hpp"
#endif

void runLbmCfdSimulation(int rank, int num_ranks, uint32_t dim_x, uint32_t dim_y, uint32_t time_steps, void *ptr,
                         bool force_unstable, bool output_vts, const std::string &output_dir);
void exportVorticityToVTS2D(double *vorticity, bool *barrier, int nx, int ny, const char *filename);
#ifdef ASCENT_ENABLED
void updateAscentData(int rank, int num_ranks, int step, double time, bool is_stable, conduit::Node &mesh);
void runAscentInSituTasks(conduit::Node &mesh, ascent::Ascent *ascent_ptr, bool is_stable);
void repartitionCallback(conduit::Node &params, conduit::Node &output);
void steeringCallback(conduit::Node &params, conduit::Node &output);
void setTimeStepsCallback(conduit::Node &params, conduit::Node &output);
void checkStabilityCallback(conduit::Node &params, conduit::Node &output);
#endif
int32_t readFile(const char *filename, char** data_ptr);

// global vars for LBM and Barriers
std::vector<Barrier*> barriers;
LbmD2Q9 *lbm;

// global vars for rescue use case
uint32_t g_time_steps = 20000;
int g_checkpoint_step = -1;
bool g_is_stable = true;

// Vigil reason step: with --agent-rescue the automatic internal rescue is
// disabled and the simulation instead polls agent-written verdict flags each
// output step -- <flagbase>.rescue (revert to the last checkpoint + double the
// timesteps -> lower lattice speed -> restabilize) and <flagbase>.stop (halt).
// The AI agent writes these after inspecting the streamed chaos frames.
bool g_agent_rescue_mode = false;

#ifdef ADIOS2_ENABLED
// ADIOS2 streaming (SST) / file (BP5) output, selected on the command line.
bool g_use_adios2 = false;
std::string g_adios_output = "lbmcfd.bp";
std::string g_adios_config = "adios2.xml";
bool g_adios_derived = true;
#endif

int main(int argc, char **argv) {
    int rc, rank, num_ranks;
    rc = MPI_Init(&argc, &argv);
    rc |= MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    rc |= MPI_Comm_size(MPI_COMM_WORLD, &num_ranks);
    if (rc != 0)
    {
        std::cerr << "Error initializing MPI" << std::endl;
        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
    }

    uint32_t dim_x = 600;
    uint32_t dim_y = 240;
    bool force_unstable = false;
    bool output_vts = false;
    std::string output_dir = "paraview";

    // parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--force-unstable") == 0) {
            force_unstable = true;
        } else if (strcmp(argv[i], "--output-vts") == 0) {
            output_vts = true;
        } else if (strcmp(argv[i], "--output-dir") == 0 && i + 1 < argc) {
            output_dir = argv[++i];
        } else if (strcmp(argv[i], "--steps") == 0 && i + 1 < argc) {
            g_time_steps = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--agent-rescue") == 0) {
            g_agent_rescue_mode = true;
        }
#ifdef ADIOS2_ENABLED
        else if (strcmp(argv[i], "--adios2") == 0) {
            g_use_adios2 = true;
        } else if (strcmp(argv[i], "--output-bp") == 0 && i + 1 < argc) {
            g_use_adios2 = true;
            g_adios_output = argv[++i];
        } else if (strcmp(argv[i], "--adios-config") == 0 && i + 1 < argc) {
            g_adios_config = argv[++i];
        } else if (strcmp(argv[i], "--no-derived") == 0) {
            g_adios_derived = false;
        }
#endif
    }

    if (rank == 0) std::cout << "LBM-CFD> running with " << num_ranks << " processes" << std::endl;
    if (rank == 0) std::cout << "LBM-CFD> resolution=" << dim_x << "x" << dim_y << ", time steps=" << g_time_steps << std::endl;
    if (rank == 0 && force_unstable) std::cout << "LBM-CFD> *** FORCE UNSTABLE MODE ***" << std::endl;
    if (rank == 0 && output_vts) std::cout << "LBM-CFD> VTS output to: " << output_dir << "/" << std::endl;

    void *ascent_ptr = NULL;

#ifdef ASCENT_ENABLED
    if (rank == 0) std::cout << "LBM-CFD> Ascent in situ: ENABLED" << std::endl;

    // Copy MPI Communicator to use with Ascent
    MPI_Comm comm;
    MPI_Comm_dup(MPI_COMM_WORLD, &comm);

    // Create Ascent object
    ascent::Ascent ascent;

    // Set Ascent options
    conduit::Node ascent_opts;
    ascent_opts["mpi_comm"] = MPI_Comm_c2f(comm);
    ascent.open(ascent_opts);

    // Register callbacks
    ascent::register_callback("repartitionCallback", repartitionCallback);
    ascent::register_callback("steeringCallback", steeringCallback);
    ascent::register_callback("setTimeSteps", setTimeStepsCallback);
    ascent::register_callback("checkStability", checkStabilityCallback);

    ascent_ptr = &ascent;
#endif

    // Run simulation
    runLbmCfdSimulation(rank, num_ranks, dim_x, dim_y, g_time_steps, ascent_ptr,
                        force_unstable, output_vts, output_dir);

#ifdef ASCENT_ENABLED
    ascent.close();
#endif

    MPI_Finalize();

    return 0;
}

void runLbmCfdSimulation(int rank, int num_ranks, uint32_t dim_x, uint32_t dim_y, uint32_t time_steps, void *ptr,
                         bool force_unstable, bool output_vts, const std::string &output_dir)
{
    // simulate corn syrup at 25 C in a 2 m pipe, moving 0.75 m/s for 8 sec
    double physical_density = 1380.0;     // kg/m^3
    double physical_speed = 0.75;         // m/s
    double physical_length = 2.0;         // m
    double physical_viscosity = 1.3806;   // Pa s
    double physical_time = 8.0;           // s
    double physical_freq = 0.25;          // s
    double reynolds_number = (physical_density * physical_speed * physical_length) / physical_viscosity;

    if (force_unstable) {
        // Use fewer timesteps → larger dt → higher lattice speed → instability
        g_time_steps = 3200;
        time_steps = g_time_steps;
        if (rank == 0) std::cout << "LBM-CFD> [UNSTABLE] Overriding time steps to " << time_steps << std::endl;
    }

    // convert physical properties into simulation properties
    double simulation_dx = physical_length / (double)dim_y;
    double simulation_dt = physical_time / (double)time_steps;
    double simulation_speed_scale = simulation_dt / simulation_dx;
    double simulation_speed = simulation_speed_scale * physical_speed;
    double simulation_viscosity = simulation_dt / (simulation_dx * simulation_dx * reynolds_number);

    // output simulation properties
    if (rank == 0)
    {
        std::cout << std::fixed << std::setprecision(6) << "LBM-CFD> speed: " << simulation_speed << ", viscosity: " <<
                     simulation_viscosity << ", reynolds: " << reynolds_number << "\n" << std::endl;
    }

    // Create output directory for VTS files
    if (output_vts && rank == 0) {
        mkdir(output_dir.c_str(), 0755);
    }

    // create LBM object
    lbm = new LbmD2Q9(dim_x, dim_y, simulation_speed_scale, rank, num_ranks);

    // initialize simulation
    // barrier: center-gap
    barriers.push_back(new BarrierVertical( 8 * dim_y / 27 + 1, 12 * dim_y / 27 - 1, dim_x / 8));
    barriers.push_back(new BarrierVertical( 8 * dim_y / 27 + 1, 12 * dim_y / 27 - 1, dim_x / 8 + 1));
    barriers.push_back(new BarrierVertical(13 * dim_y / 27 + 1, 17 * dim_y / 27 - 1, dim_x / 8));
    barriers.push_back(new BarrierVertical(13 * dim_y / 27 + 1, 17 * dim_y / 27 - 1, dim_x / 8 + 1));
    lbm->initBarrier(barriers);
    lbm->initFluid(physical_speed);

#ifdef ADIOS2_ENABLED
    // Set up ADIOS2 output (engine chosen in the XML: SST to stream, BP5 to
    // file). Only vorticity is a raw field; density / instability trigger
    // statistics are computed in situ via derived variables (see adios2_writer).
    adios2::ADIOS *adios = NULL;
    Lbm2DAdiosWriter *adios_writer = NULL;
    if (g_use_adios2)
    {
        adios = new adios2::ADIOS(g_adios_config, MPI_COMM_WORLD);
        adios2::IO adios_io = adios->DeclareIO("SimulationOutput");
        adios_writer = new Lbm2DAdiosWriter(adios_io, lbm, g_adios_derived);
        if (rank == 0)
            std::cout << "LBM-CFD> ADIOS2 output ENABLED: stream='" << g_adios_output
                      << "', config='" << g_adios_config << "', derived="
                      << (g_adios_derived ? "on" : "off") << std::endl;
        adios_writer->open(g_adios_output); // may block on SST reader rendezvous
        if (rank == 0)
            std::cout << "LBM-CFD> ADIOS2 stream open" << std::endl;
    }
#endif

    // sync all processes
    MPI_Barrier(MPI_COMM_WORLD);

    // run simulation
    int t;
    double time;
    int output_count = 0;
    double next_output_time = 0.0;
    uint8_t stable, all_stable;
    int checkpoint_interval = 500; // save checkpoint every N steps
    int rescue_attempts = 0;
    int max_rescue_attempts = 5;

    // save initial checkpoint
    lbm->saveCheckpoint();
    g_checkpoint_step = 0;
    if (rank == 0) std::cout << "LBM-CFD> Initial checkpoint saved" << std::endl;

    // Agent verdict flags (Vigil reason step). Clear any stale flags at startup.
    std::string flag_base =
#ifdef ADIOS2_ENABLED
        g_use_adios2 ? g_adios_output :
#endif
        std::string("lbmcfd");
    const std::string stopFlag = flag_base + ".stop";
    const std::string rescueFlag = flag_base + ".rescue";
    if (g_agent_rescue_mode)
    {
        if (rank == 0)
        {
            std::remove(stopFlag.c_str());
            std::remove(rescueFlag.c_str());
            std::cout << "LBM-CFD> Agent-rescue mode: automatic rescue disabled; "
                      << "polling '" << rescueFlag << "' / '" << stopFlag << "'"
                      << std::endl;
        }
        MPI_Barrier(MPI_COMM_WORLD);
    }

    int vts_interval = force_unstable ? 10 : 500;
    int max_step = force_unstable ? 400 : (int)g_time_steps;

    for (t = 0; t < (int)g_time_steps; t++)
    {
        // perform one iteration of the simulation
        lbm->collide(simulation_viscosity);
        lbm->stream();
        lbm->bounceBackStream();

        // Export VTS files (decoupled from time-based output)
        if (output_vts && ((t + 1) % vts_interval == 0 || t == 0)) {
            lbm->computeVorticity();
            lbm->gatherDataOnRank0(LbmD2Q9::Vorticity);
            if (rank == 0) {
                char vts_filename[256];
                snprintf(vts_filename, sizeof(vts_filename), "%s/simulation_state_t%05d.vts",
                         output_dir.c_str(), t + 1);
                exportVorticityToVTS2D(lbm->getGatheredData(), lbm->getBarrier(),
                                      lbm->getTotalDimX(), lbm->getTotalDimY(), vts_filename);
            }
        }

        // Stop at max_step in force-unstable mode
        if (force_unstable && (t + 1) >= max_step) {
            if (rank == 0)
                std::cout << "LBM-CFD> [UNSTABLE] Stopping at step " << (t + 1) << std::endl;
            break;
        }

        // save periodic checkpoints for rescue (only when stable)
        if (!force_unstable && t > 0 && t % checkpoint_interval == 0 && g_is_stable)
        {
            lbm->saveCheckpoint();
            g_checkpoint_step = t;
            if (rank == 0)
            {
                std::cout << "LBM-CFD> Checkpoint saved at step " << t << std::endl;
            }
        }

        // output data at frequency equivalent to `physical_freq` time
        simulation_dt = physical_time / (double)g_time_steps;
        time = (t + 1) * simulation_dt;
        if (time >= next_output_time)
        {
            if (rank == 0)
            {
                std::cout << std::fixed << std::setprecision(3) << "LBM-CFD> time: " << time << " / " <<
                             physical_time << " , time step: " << (t + 1) << " / " << g_time_steps << std::endl;
            }

            // check stability across all ranks
            stable = lbm->checkStability() ? 0 : 1;
            MPI_Allreduce(MPI_IN_PLACE, &stable, 1, MPI_UNSIGNED_CHAR, MPI_MAX, MPI_COMM_WORLD);
            g_is_stable = (stable == 0);

            // compute and print density statistics across all ranks
            {
                double *rho = lbm->getDensity();
                uint32_t lx = lbm->getDimX();
                uint32_t ly = lbm->getDimY();
                double local_min = 1e30, local_max = -1e30, local_sum = 0.0;
                int local_count = 0;
                for (uint32_t jj = 1; jj < ly - 1; jj++) {
                    for (uint32_t ii = 1; ii < lx - 1; ii++) {
                        double val = rho[jj * lx + ii];
                        if (val < local_min) local_min = val;
                        if (val > local_max) local_max = val;
                        local_sum += val;
                        local_count++;
                    }
                }
                double global_min, global_max, global_sum;
                int global_count;
                MPI_Reduce(&local_min, &global_min, 1, MPI_DOUBLE, MPI_MIN, 0, MPI_COMM_WORLD);
                MPI_Reduce(&local_max, &global_max, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
                MPI_Reduce(&local_sum, &global_sum, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
                MPI_Reduce(&local_count, &global_count, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
                if (rank == 0) {
                    double global_avg = global_sum / global_count;
                    std::cout << std::fixed << std::setprecision(6)
                              << "LBM-CFD> density: min=" << global_min
                              << ", max=" << global_max
                              << ", avg=" << global_avg << std::endl;
                }
            }

            if (!g_is_stable && rank == 0)
            {
                std::cerr << "LBM-CFD> WARNING: simulation has become UNSTABLE at step " << (t + 1) << std::endl;
            }

#ifdef ASCENT_ENABLED
            ascent::Ascent *ascent_ptr = static_cast<ascent::Ascent*>(ptr);
            conduit::Node mesh;
            updateAscentData(rank, num_ranks, t + 1, time, g_is_stable, mesh);
            runAscentInSituTasks(mesh, ascent_ptr, g_is_stable);
#endif

#ifdef ADIOS2_ENABLED
            if (adios_writer)
            {
                adios_writer->write(t + 1, time, g_is_stable);
            }
#endif

            // Decide whether to rescue this output step. In agent-rescue mode
            // the AI agent drives it via flags (the automatic path is off); in
            // normal mode the simulation self-heals on detected instability.
            bool do_rescue = false;
            if (g_agent_rescue_mode)
            {
                int local2[2] = {std::ifstream(stopFlag).good() ? 1 : 0,
                                 std::ifstream(rescueFlag).good() ? 1 : 0};
                int global2[2] = {0, 0};
                MPI_Allreduce(local2, global2, 2, MPI_INT, MPI_LOR, MPI_COMM_WORLD);
                if (global2[0])  // STOP verdict
                {
                    if (rank == 0)
                    {
                        std::cout << "LBM-CFD> Agent STOP verdict at step " << (t + 1)
                                  << " (" << stopFlag << ") - halting early" << std::endl;
                        std::remove(stopFlag.c_str());
                    }
                    break;
                }
                if (global2[1])  // RESCUE verdict
                {
                    if (rank == 0)
                    {
                        std::cout << "LBM-CFD> Agent RESCUE verdict at step " << (t + 1)
                                  << " (" << rescueFlag << ")" << std::endl;
                        std::remove(rescueFlag.c_str());
                    }
                    do_rescue = true;
                }
            }
            else if (!g_is_stable && !force_unstable)
            {
                do_rescue = true;  // automatic self-healing rescue
            }

            // rescue: revert to checkpoint and increase timesteps
            if (do_rescue)
            {
                rescue_attempts++;
                if (g_checkpoint_step >= 0 && rescue_attempts <= max_rescue_attempts)
                {
                    uint32_t new_time_steps = g_time_steps * 2;
                    if (rank == 0)
                    {
                        std::cerr << "LBM-CFD> RESCUE (attempt " << rescue_attempts << "/" << max_rescue_attempts
                                  << "): reverting to checkpoint at step " << g_checkpoint_step << std::endl;
                        std::cerr << "LBM-CFD> RESCUE: increasing timesteps from " << g_time_steps << " to " << new_time_steps << std::endl;
                    }
                    const double old_speed_scale = lbm->getSpeedScale();
                    lbm->loadCheckpoint();
                    g_time_steps = new_time_steps;

                    // recalculate simulation parameters with new timestep count
                    simulation_dt = physical_time / (double)g_time_steps;
                    simulation_speed_scale = simulation_dt / simulation_dx;
                    simulation_viscosity = simulation_dt / (simulation_dx * simulation_dx * reynolds_number);

                    // Convert the restored flow to the new lattice units and
                    // re-apply the inlet. The checkpoint's distributions encode
                    // the OLD lattice speed; without the rescale the interior
                    // keeps moving at the unstable speed and the run re-diverges
                    // no matter how many times it is rescued.
                    lbm->setSpeedScale(simulation_speed_scale);
                    lbm->rescaleVelocity(simulation_speed_scale / old_speed_scale);
                    lbm->updateFluid(physical_speed);

                    // revert loop counter to checkpoint
                    t = g_checkpoint_step - 1; // -1 because for loop increments
                    output_count = (int)(g_checkpoint_step * simulation_dt / physical_freq);
                    next_output_time = output_count * physical_freq;
                    g_is_stable = true;

                    if (rank == 0)
                    {
                        std::cout << std::fixed << std::setprecision(6) << "LBM-CFD> RESCUE: new speed: " << simulation_speed_scale * physical_speed
                                  << ", new viscosity: " << simulation_viscosity << std::endl;
                    }
                    continue;
                }
                else
                {
                    if (rank == 0)
                    {
                        if (rescue_attempts > max_rescue_attempts)
                            std::cerr << "LBM-CFD> RESCUE: max attempts (" << max_rescue_attempts << ") reached, giving up" << std::endl;
                        else
                            std::cerr << "LBM-CFD> RESCUE: no checkpoint available, cannot recover" << std::endl;
                    }
                    break;
                }
            }
            else
            {
                rescue_attempts = 0; // reset on stable output
            }

            output_count++;
            next_output_time = output_count * physical_freq;
        }
    }

    // Clean up
#ifdef ADIOS2_ENABLED
    if (adios_writer)
    {
        adios_writer->close();
        delete adios_writer;
    }
    delete adios;
#endif
    delete lbm;
}

#ifdef ASCENT_ENABLED
void updateAscentData(int rank, int num_ranks, int step, double time, bool is_stable, conduit::Node &mesh)
{
    // Gather data on rank 0
    lbm->computeVorticity();

    uint32_t dim_x = lbm->getDimX();
    uint32_t dim_y = lbm->getDimY();
    uint32_t offset_x = lbm->getOffsetX();
    uint32_t offset_y = lbm->getOffsetY();
    uint32_t prop_size = dim_x * dim_y;

    int *barrier_data = new int[barriers.size() * 4];
    int i;
    for (i = 0; i < (int)barriers.size(); i++)
    {
        barrier_data[4 * i + 0] = barriers[i]->getX1();
        barrier_data[4 * i + 1] = barriers[i]->getY1();
        barrier_data[4 * i + 2] = barriers[i]->getX2();
        barrier_data[4 * i + 3] = barriers[i]->getY2();
    }

    mesh["state/domain_id"] = rank;
    mesh["state/num_domains"] = num_ranks;
    mesh["state/cycle"] = step;
    mesh["state/time"] = time;
    mesh["state/stable"] = is_stable ? (int64_t)1 : (int64_t)0;
    mesh["state/timesteps"] = (int64_t)g_time_steps;
    mesh["state/checkpoint_step"] = (int64_t)g_checkpoint_step;
    mesh["state/coords/start/x"] = lbm->getStartX();
    mesh["state/coords/start/y"] = lbm->getStartY();
    mesh["state/coords/size/x"] = lbm->getSizeX();
    mesh["state/coords/size/y"] = lbm->getSizeY();
    mesh["state/num_barriers"] = (int)barriers.size();
    mesh["state/barriers"].set(barrier_data, (int)barriers.size() * 4);

    mesh["coordsets/coords/type"] = "uniform";
    mesh["coordsets/coords/dims/i"] = dim_x + 1;
    mesh["coordsets/coords/dims/j"] = dim_y + 1;

    mesh["coordsets/coords/origin/x"] = offset_x - (int)lbm->getStartX();
    mesh["coordsets/coords/origin/y"] = offset_y - (int)lbm->getStartY();
    mesh["coordsets/coords/spacing/dx"] = 1;
    mesh["coordsets/coords/spacing/dy"] = 1;

    mesh["topologies/topo/type"] = "uniform";
    mesh["topologies/topo/coordset"] = "coords";

    mesh["fields/vorticity/association"] = "element";
    mesh["fields/vorticity/topology"] = "topo";
    mesh["fields/vorticity/values"].set_external(lbm->getVorticity(), prop_size);

    delete[] barrier_data;
}

void runAscentInSituTasks(conduit::Node &mesh, ascent::Ascent *ascent_ptr, bool is_stable)
{
    ascent_ptr->publish(mesh);

    conduit::Node actions;
    conduit::Node &add_extracts = actions.append();
    add_extracts["action"] = "add_extracts";
    conduit::Node &extracts = add_extracts["extracts"];

    if (is_stable)
    {
        // stable path: run the normal Trame bridge for interactive visualization
        char *py_script;
        if (readFile("ascent/ascent_trame_bridge.py", &py_script) >= 0)
        {
            extracts["e1/type"] = "python";
            extracts["e1/params/source"] = py_script;
            free(py_script);
        }
    }
    else
    {
        // unstable path: run rescue notification script
        char *py_script;
        if (readFile("ascent/ascent_rescue.py", &py_script) >= 0)
        {
            extracts["rescue/type"] = "python";
            extracts["rescue/params/source"] = py_script;
            free(py_script);
        }
    }

    ascent_ptr->execute(actions);
}

void repartitionCallback(conduit::Node &params, conduit::Node &output)
{
    int num_ranks = (int)params["state/num_domains"].as_int32();
    uint32_t layout[4] = {params["state/coords/start/x"].as_uint32(), params["state/coords/start/y"].as_uint32(),
                          params["state/coords/size/x"].as_uint32(), params["state/coords/size/y"].as_uint32()};
    uint32_t *layout_all = new uint32_t[4 * num_ranks];
    MPI_Allgather(layout, 4, MPI_UNSIGNED, layout_all, 4, MPI_UNSIGNED, MPI_COMM_WORLD);

    int i;
    conduit::Node options, selections;
    for (i = 0; i < num_ranks; i++)
    {
        uint32_t rank_start_x = layout_all[4 * i];
        uint32_t rank_start_y = layout_all[4 * i + 1];
        uint32_t rank_size_x = layout_all[4 * i + 2];
        uint32_t rank_size_y = layout_all[4 * i + 3];
        conduit::Node &selection = selections.append();
        selection["type"] = "logical";
        selection["domain_id"] = i;
        selection["start"] = {rank_start_x, rank_start_y, 0u};
        selection["end"] = {rank_start_x + rank_size_x - 1u, rank_start_y + rank_size_y - 1u, 0u};
    }
    options["target"] = 1;
    options["fields"] = {"vorticity"};
    options["selections"] = selections;
    options["mapping"] = 0;

    conduit::blueprint::mpi::mesh::partition(params, options, output, MPI_COMM_WORLD);

    delete[] layout_all;
}

void steeringCallback(conduit::Node &params, conduit::Node &output)
{
    if (params.has_path("task_id") && params.has_path("flow_speed") && params.has_path("num_barriers") && params.has_path("barriers"))
    {
        int rank = (int)params["task_id"].as_int64();
        double flow_speed = params["flow_speed"].as_float64();
        int num_barriers = (int)params["num_barriers"].as_int64();
        int32_t *new_barriers = params["barriers"].as_int32_ptr();

        int i;
        barriers.clear();
        for (i = 0; i < num_barriers; i++)
        {
            int x1 = new_barriers[4 * i + 0];
            int y1 = new_barriers[4 * i + 1];
            int x2 = new_barriers[4 * i + 2];
            int y2 = new_barriers[4 * i + 3];
            if (x1 == x2)
            {
                barriers.push_back(new BarrierVertical(std::min(y1, y2), std::max(y1, y2), x1));
            }
            else if (y1 == y2)
            {
                barriers.push_back(new BarrierHorizontal(std::min(x1, x2), std::max(x1, x2), y1));
            }
        }
        lbm->initBarrier(barriers);
        lbm->updateFluid(flow_speed);
    }
}

// Rescue callback: allows user to manually set timesteps via Ascent
// Usage: params["timesteps"] = 3200; execute_callback("setTimeSteps", params, output)
void setTimeStepsCallback(conduit::Node &params, conduit::Node &output)
{
    if (params.has_path("timesteps"))
    {
        int64_t new_timesteps = params["timesteps"].to_int64();
        g_time_steps = (uint32_t)new_timesteps;
        output["status"] = "ok";
        output["timesteps"] = (int64_t)g_time_steps;
        std::cout << "LBM-CFD> setTimeSteps callback: timesteps set to " << g_time_steps << std::endl;
    }
}

// Boolean stability callback: Ascent can query this to check if the simulation is stable
void checkStabilityCallback(conduit::Node &params, conduit::Node &output)
{
    output["stable"] = g_is_stable ? (int64_t)1 : (int64_t)0;
    output["checkpoint_step"] = (int64_t)g_checkpoint_step;
    output["timesteps"] = (int64_t)g_time_steps;
}
#endif

void exportVorticityToVTS2D(double *vorticity, bool *barrier, int nx, int ny, const char *filename)
{
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        fprintf(stderr, "Error: could not open %s for writing\n", filename);
        return;
    }

    fprintf(fp, "<?xml version=\"1.0\"?>\n");
    fprintf(fp, "<VTKFile type=\"StructuredGrid\" version=\"0.1\" byte_order=\"LittleEndian\">\n");
    fprintf(fp, "  <StructuredGrid WholeExtent=\"0 %d 0 %d 0 0\">\n", nx - 1, ny - 1);
    fprintf(fp, "    <Piece Extent=\"0 %d 0 %d 0 0\">\n", nx - 1, ny - 1);

    // Points (2D grid in XY plane)
    fprintf(fp, "      <Points>\n");
    fprintf(fp, "        <DataArray type=\"Float64\" NumberOfComponents=\"3\" format=\"ascii\">\n");
    for (int j = 0; j < ny; j++) {
        for (int i = 0; i < nx; i++) {
            fprintf(fp, "          %d %d 0\n", i, j);
        }
    }
    fprintf(fp, "        </DataArray>\n");
    fprintf(fp, "      </Points>\n");

    // Cell data (vorticity) — clamp to [-0.3, 0.3] for stable color mapping
    double vort_clamp = 0.3;
    fprintf(fp, "      <CellData>\n");
    fprintf(fp, "        <DataArray type=\"Float64\" Name=\"vorticity\" format=\"ascii\">\n");
    for (int j = 0; j < ny - 1; j++) {
        for (int i = 0; i < nx - 1; i++) {
            int idx = j * nx + i;
            double val = (vorticity != NULL) ? vorticity[idx] : 0.0;
            if (std::isnan(val) || std::isinf(val)) val = 0.0;
            if (val > vort_clamp) val = vort_clamp;
            if (val < -vort_clamp) val = -vort_clamp;
            fprintf(fp, "          %.8e\n", val);
        }
    }
    fprintf(fp, "        </DataArray>\n");

    // Raw vorticity (unclamped) for reference
    fprintf(fp, "        <DataArray type=\"Float64\" Name=\"vorticity_raw\" format=\"ascii\">\n");
    for (int j = 0; j < ny - 1; j++) {
        for (int i = 0; i < nx - 1; i++) {
            int idx = j * nx + i;
            double val = (vorticity != NULL) ? vorticity[idx] : 0.0;
            if (std::isnan(val) || std::isinf(val)) val = 0.0;
            fprintf(fp, "          %.8e\n", val);
        }
    }
    fprintf(fp, "        </DataArray>\n");

    // Barrier mask
    if (barrier) {
        fprintf(fp, "        <DataArray type=\"Int32\" Name=\"barrier\" format=\"ascii\">\n");
        for (int j = 0; j < ny - 1; j++) {
            for (int i = 0; i < nx - 1; i++) {
                int idx = j * nx + i;
                fprintf(fp, "          %d\n", barrier[idx] ? 1 : 0);
            }
        }
        fprintf(fp, "        </DataArray>\n");
    }

    fprintf(fp, "      </CellData>\n");
    fprintf(fp, "    </Piece>\n");
    fprintf(fp, "  </StructuredGrid>\n");
    fprintf(fp, "</VTKFile>\n");

    fclose(fp);
    printf("VTS export: %s\n", filename);
}

int32_t readFile(const char *filename, char** data_ptr)
{
    FILE *fp;
    int err = 0;
#ifdef _WIN32
    err = fopen_s(&fp, filename, "rb");
#else
    fp = fopen(filename, "rb");
#endif
    if (err != 0 || fp == NULL)
    {
        std::cerr << "Error: cannot open " << filename << std::endl;
        return -1;
    }

    fseek(fp, 0, SEEK_END);
    int32_t fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    *data_ptr = (char*)malloc(fsize + 1);
    size_t read = fread(*data_ptr, fsize, 1, fp);
    if (read != 1)
    {
        std::cerr << "Error: cannot read " << filename <<std::endl;
        return -1;
    }
    (*data_ptr)[fsize] = '\0';

    fclose(fp);

    return fsize;
}
