#include "scopi/params.hpp"

namespace scopi
{
    ScopiParams::ScopiParams()
    : output_frequency(1)
    , path(std::filesystem::current_path() / "Results")
    , filename("scopi_objects")
    , write_velocity(false)
    {
    }

    void ScopiParams::init_options(CLI::App& app)
    {
        auto opt = app.add_option_group("scopi options");
        opt->add_option("--path", path, "Path where to store the results")->capture_default_str();
        opt->add_option("--filename", filename, "Name of the outputs")->capture_default_str();
        opt->add_option("--freq", output_frequency, "Output frequency (in iterations)")->capture_default_str();
        opt->add_flag("--write-velocity", write_velocity, "Write the velocity of objects")->capture_default_str();
    }

}
