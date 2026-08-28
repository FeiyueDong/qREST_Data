#include "external_import.hpp"
#include "qrest_file.hpp"
#include "text_matrix.hpp"
#include "validation.hpp"

#include <CLI/CLI.hpp>

#include <cstdint>
#include <exception>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>

#include "metadata.hpp"

namespace {

using qrest_data::Metadata;
using qrest_data::tools::ExternalDataset;
using qrest_data::tools::load_hdf5_dataset;
using qrest_data::tools::load_mseed_dataset;
using qrest_data::tools::load_tdms_dataset;
using qrest_data::tools::MseedImportOptions;
using qrest_data::tools::print_validation_report;
using qrest_data::tools::read_qrest_file;
using qrest_data::tools::TdmsImportOptions;
using qrest_data::tools::validate_external_dataset_compatibility;
using qrest_data::tools::ValidationReport;
using qrest_data::tools::write_hdf5_dataset;
using qrest_data::tools::write_qrest_file;

std::string read_text_file(const std::string &path) {
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        throw std::runtime_error("Cannot open file: " + path);
    }
    return std::string((std::istreambuf_iterator<char>(input)),
                       std::istreambuf_iterator<char>());
}

void write_text_file(const std::string &path, const std::string &content) {
    std::ofstream output(path, std::ios::binary);
    if (!output.is_open()) {
        throw std::runtime_error("Cannot create file: " + path);
    }
    output.write(content.data(), static_cast<std::streamsize>(content.size()));
    if (!output) {
        throw std::runtime_error("Failed while writing file: " + path);
    }
}

struct PackCommand {
    std::string metadata_path;
    std::string data_path;
    std::string output_path;
    int source_id{1};
    int data_encoding{0};
};

struct ExtractCommand {
    std::string input_path;
    std::string metadata_path;
    std::string data_path;
    int precision{8};
};

struct InspectCommand {
    std::string input_path;
    bool show_channels{false};
};

struct ValidateQrestCommand {
    std::string input_path;
};

struct ValidateTextCommand {
    std::string metadata_path;
    std::string data_path;
};

struct ImportTdmsCommand {
    std::string input_path;
    std::string metadata_path;
    std::string output_path;
    int source_id{1};
    int data_encoding{0};
    std::string unit{"cm/s2"};
    std::string sensitivity_mode{"acquisition"};
    double explicit_sensitivity{};
    double sensitivity_storage_scale{100.0};
    double post_scale{1.0};
    bool output_counts{false};
    bool no_time_check{false};
};

struct ImportMseedCommand {
    std::string input_path;
    std::string metadata_path;
    std::string output_path;
    int source_id{1};
    int data_encoding{0};
    std::size_t group_index{};
    bool include_dimensionless{false};
    bool no_time_check{false};
};

struct ImportHdf5Command {
    std::string input_path;
    std::string output_path;
    int source_id{1};
    int data_encoding{0};
};

struct ExportHdf5Command {
    std::string input_path;
    std::string output_path;
};

struct ValidateTdmsCommand {
    std::string input_path;
    std::string unit{"cm/s2"};
    std::string sensitivity_mode{"acquisition"};
    double explicit_sensitivity{};
    double sensitivity_storage_scale{100.0};
    double post_scale{1.0};
    bool output_counts{false};
    bool no_time_check{false};
};

struct ValidateMseedCommand {
    std::string input_path;
    std::size_t group_index{};
    bool include_dimensionless{false};
    bool no_time_check{false};
};

struct ValidateHdf5Command {
    std::string input_path;
};

void configure_pack_command(CLI::App *command, PackCommand &values) {
    command
        ->add_option(
            "metadata", values.metadata_path, "Input qREST metadata JSON")
        ->required()
        ->check(CLI::ExistingFile);
    command
        ->add_option("data", values.data_path, "Input time-major text matrix")
        ->required()
        ->check(CLI::ExistingFile);
    command->add_option("output", values.output_path, "Output .qrest file")
        ->required();
    command
        ->add_option("--source-id", values.source_id, "qREST packet SourceID")
        ->default_val(values.source_id)
        ->check(CLI::Range(0, 65535));
    command
        ->add_option(
            "--encoding", values.data_encoding, "qREST packet DataEncodings")
        ->default_val(values.data_encoding)
        ->check(CLI::IsMember({0, 1, 10, 11}));
}

void configure_qrest_packet_options(CLI::App *command,
                                    int &source_id,
                                    int &data_encoding) {
    command->add_option("--source-id", source_id, "qREST packet SourceID")
        ->default_val(source_id)
        ->check(CLI::Range(0, 65535));
    command
        ->add_option("--encoding", data_encoding, "qREST packet DataEncodings")
        ->default_val(data_encoding)
        ->check(CLI::IsMember({0, 1, 10, 11}));
}

void configure_extract_command(CLI::App *command, ExtractCommand &values) {
    command->add_option("input", values.input_path, "Input .qrest file")
        ->required()
        ->check(CLI::ExistingFile);
    command
        ->add_option("metadata", values.metadata_path, "Output metadata JSON")
        ->required();
    command
        ->add_option("data", values.data_path, "Output time-major text matrix")
        ->required();
    command
        ->add_option("--precision", values.precision, "Text output precision")
        ->default_val(values.precision)
        ->check(CLI::Range(1, 17));
}

void configure_inspect_command(CLI::App *command, InspectCommand &values) {
    command->add_option("input", values.input_path, "Input .qrest file")
        ->required()
        ->check(CLI::ExistingFile);
    command
        ->add_flag("--channels",
                   values.show_channels,
                   "Print channel metadata summary")
        ->default_val(false);
}

void configure_validate_qrest_command(CLI::App *command,
                                      ValidateQrestCommand &values) {
    command->add_option("input", values.input_path, "Input .qrest file")
        ->required()
        ->check(CLI::ExistingFile);
}

void configure_validate_text_command(CLI::App *command,
                                     ValidateTextCommand &values) {
    command
        ->add_option(
            "metadata", values.metadata_path, "Input qREST metadata JSON")
        ->required()
        ->check(CLI::ExistingFile);
    command
        ->add_option("data", values.data_path, "Input time-major text matrix")
        ->required()
        ->check(CLI::ExistingFile);
}

void configure_tdms_options(CLI::App *command,
                            std::string &unit,
                            std::string &sensitivity_mode,
                            double &explicit_sensitivity,
                            double &sensitivity_storage_scale,
                            double &post_scale,
                            bool &output_counts,
                            bool &no_time_check) {
    command->add_option("--unit", unit, "Physical output unit: cm/s2 or m/s2")
        ->default_val(unit)
        ->check(CLI::IsMember({"cm/s2", "m/s2"}));
    command
        ->add_option(
            "--sensitivity-mode",
            sensitivity_mode,
            "Sensitivity selection: acquisition, first, last or explicit")
        ->default_val(sensitivity_mode)
        ->check(CLI::IsMember({"acquisition", "first", "last", "explicit"}));
    command
        ->add_option("--sensitivity",
                     explicit_sensitivity,
                     "Explicit TDMS raw sensitivity value")
        ->default_val(explicit_sensitivity);
    command
        ->add_option("--sensitivity-storage-scale",
                     sensitivity_storage_scale,
                     "Storage scale between raw and actual sensitivity")
        ->default_val(sensitivity_storage_scale);
    command
        ->add_option("--post-scale", post_scale, "Additional output multiplier")
        ->default_val(post_scale);
    command->add_flag("--counts", output_counts, "Use raw integer counts")
        ->default_val(false);
    command
        ->add_flag("--no-time-check",
                   no_time_check,
                   "Disable TDMS timestamp spacing verification")
        ->default_val(false);
}

void configure_mseed_options(CLI::App *command,
                             std::size_t &group_index,
                             bool &include_dimensionless,
                             bool &no_time_check) {
    command
        ->add_option("--group-index",
                     group_index,
                     "Synchronized MiniSEED channel group to import")
        ->default_val(group_index);
    command
        ->add_flag("--include-dimensionless",
                   include_dimensionless,
                   "Allow dimensionless MiniSEED channels")
        ->default_val(false);
    command
        ->add_flag("--no-time-check",
                   no_time_check,
                   "Disable MiniSEED record continuity verification")
        ->default_val(false);
}

void configure_import_tdms_command(CLI::App *command,
                                   ImportTdmsCommand &values) {
    command->add_option("input", values.input_path, "Input TDMS file")
        ->required()
        ->check(CLI::ExistingFile);
    command
        ->add_option(
            "metadata", values.metadata_path, "Input qREST metadata JSON")
        ->required()
        ->check(CLI::ExistingFile);
    command->add_option("output", values.output_path, "Output .qrest file")
        ->required();
    configure_qrest_packet_options(
        command, values.source_id, values.data_encoding);
    configure_tdms_options(command,
                           values.unit,
                           values.sensitivity_mode,
                           values.explicit_sensitivity,
                           values.sensitivity_storage_scale,
                           values.post_scale,
                           values.output_counts,
                           values.no_time_check);
}

void configure_import_mseed_command(CLI::App *command,
                                    ImportMseedCommand &values) {
    command
        ->add_option("input", values.input_path, "Input modified MiniSEED file")
        ->required()
        ->check(CLI::ExistingFile);
    command
        ->add_option(
            "metadata", values.metadata_path, "Input qREST metadata JSON")
        ->required()
        ->check(CLI::ExistingFile);
    command->add_option("output", values.output_path, "Output .qrest file")
        ->required();
    configure_qrest_packet_options(
        command, values.source_id, values.data_encoding);
    configure_mseed_options(command,
                            values.group_index,
                            values.include_dimensionless,
                            values.no_time_check);
}

void configure_import_hdf5_command(CLI::App *command,
                                   ImportHdf5Command &values) {
    command->add_option("input", values.input_path, "Input qREST HDF5 file")
        ->required()
        ->check(CLI::ExistingFile);
    command->add_option("output", values.output_path, "Output .qrest file")
        ->required();
    configure_qrest_packet_options(
        command, values.source_id, values.data_encoding);
}

void configure_export_hdf5_command(CLI::App *command,
                                   ExportHdf5Command &values) {
    command->add_option("input", values.input_path, "Input .qrest file")
        ->required()
        ->check(CLI::ExistingFile);
    command->add_option("output", values.output_path, "Output qREST HDF5 file")
        ->required();
}

void configure_validate_tdms_command(CLI::App *command,
                                     ValidateTdmsCommand &values) {
    command->add_option("input", values.input_path, "Input TDMS file")
        ->required()
        ->check(CLI::ExistingFile);
    configure_tdms_options(command,
                           values.unit,
                           values.sensitivity_mode,
                           values.explicit_sensitivity,
                           values.sensitivity_storage_scale,
                           values.post_scale,
                           values.output_counts,
                           values.no_time_check);
}

void configure_validate_mseed_command(CLI::App *command,
                                      ValidateMseedCommand &values) {
    command
        ->add_option("input", values.input_path, "Input modified MiniSEED file")
        ->required()
        ->check(CLI::ExistingFile);
    configure_mseed_options(command,
                            values.group_index,
                            values.include_dimensionless,
                            values.no_time_check);
}

void configure_validate_hdf5_command(CLI::App *command,
                                     ValidateHdf5Command &values) {
    command->add_option("input", values.input_path, "Input qREST HDF5 file")
        ->required()
        ->check(CLI::ExistingFile);
}

TdmsImportOptions make_tdms_import_options(const std::string &unit,
                                           const std::string &selection,
                                           double explicit_sensitivity,
                                           double sensitivity_storage_scale,
                                           double post_scale,
                                           bool output_counts,
                                           bool no_time_check) {
    TdmsImportOptions options;
    if (unit == "m/s2") {
        options.output_unit = TdmsImportOptions::Unit::MeterPerSecondSquared;
    } else {
        options.output_unit =
            TdmsImportOptions::Unit::CentimeterPerSecondSquared;
    }

    if (selection == "first") {
        options.sensitivity_selection =
            TdmsImportOptions::SensitivitySelection::First;
    } else if (selection == "last") {
        options.sensitivity_selection =
            TdmsImportOptions::SensitivitySelection::Last;
    } else if (selection == "explicit") {
        options.sensitivity_selection =
            TdmsImportOptions::SensitivitySelection::Explicit;
    } else {
        options.sensitivity_selection =
            TdmsImportOptions::SensitivitySelection::Acquisition;
    }
    options.explicit_sensitivity = explicit_sensitivity;
    options.sensitivity_storage_scale = sensitivity_storage_scale;
    options.post_scale = post_scale;
    options.output_counts = output_counts;
    options.verify_time_axis = !no_time_check;
    return options;
}

MseedImportOptions make_mseed_import_options(std::size_t group_index,
                                             bool include_dimensionless,
                                             bool no_time_check) {
    MseedImportOptions options;
    options.group_index = group_index;
    options.include_dimensionless = include_dimensionless;
    options.verify_time_continuity = !no_time_check;
    return options;
}

void print_dataset_summary(const ExternalDataset &dataset) {
    std::cout << "  source_format : " << dataset.source_format << '\n'
              << "  channels      : " << dataset.channel_count << '\n'
              << "  samples       : " << dataset.sample_count << '\n'
              << "  sample_rate   : " << dataset.sample_rate_hz << " Hz\n";
    if (!dataset.channel_labels.empty()) {
        std::cout << "  channel_labels:";
        for (const auto &label : dataset.channel_labels) {
            std::cout << ' ' << label;
        }
        std::cout << '\n';
    }
}

void require_or_print_compatibility(const ExternalDataset &dataset,
                                    const Metadata &metadata) {
    const auto report =
        validate_external_dataset_compatibility(dataset, metadata);
    for (const auto &warning : report.warnings) {
        std::cerr << "warning: " << warning << '\n';
    }
    if (!report.ok()) {
        std::ostringstream oss;
        oss << "External dataset is not compatible with metadata:";
        for (const auto &error : report.errors) {
            oss << "\n  - " << error;
        }
        throw std::runtime_error(oss.str());
    }
}

int run_pack_command(const PackCommand &command) {
    const std::string metadata_json = read_text_file(command.metadata_path);
    const Metadata metadata = Metadata::from_bytes(metadata_json);
    const auto data = qrest_data::tools::read_time_major_text_matrix(
        command.data_path,
        static_cast<std::size_t>(metadata.InstrumentInfo.ChannelNum),
        static_cast<std::size_t>(metadata.DataInfo.NPTS));

    qrest_data::tools::write_qrest_file(
        command.output_path,
        metadata_json,
        data,
        static_cast<std::uint16_t>(command.source_id),
        static_cast<std::uint16_t>(command.data_encoding));

    std::cout << "Wrote qREST file: " << command.output_path << '\n'
              << "  channels : " << metadata.InstrumentInfo.ChannelNum << '\n'
              << "  samples  : " << metadata.DataInfo.NPTS << '\n'
              << "  encoding : " << command.data_encoding << '\n';
    return 0;
}

int run_extract_command(const ExtractCommand &command) {
    const auto file = qrest_data::tools::read_qrest_file(command.input_path);
    write_text_file(command.metadata_path, file.metadata_json);
    qrest_data::tools::write_time_major_text_matrix(
        command.data_path,
        file.channel_sequential_data,
        static_cast<std::size_t>(file.metadata.InstrumentInfo.ChannelNum),
        static_cast<std::size_t>(file.metadata.DataInfo.NPTS),
        command.precision);

    std::cout << "Extracted qREST file: " << command.input_path << '\n'
              << "  metadata : " << command.metadata_path << '\n'
              << "  data     : " << command.data_path << '\n'
              << "  channels : " << file.metadata.InstrumentInfo.ChannelNum
              << '\n'
              << "  samples  : " << file.metadata.DataInfo.NPTS << '\n';
    return 0;
}

int run_inspect_command(const InspectCommand &command) {
    const auto file = qrest_data::tools::read_qrest_file(command.input_path);

    std::cout << "qREST file: " << command.input_path << '\n'
              << "  file_size     : " << file.file.file_size << " bytes\n"
              << "  metadata_size : " << file.file.metadata_size << " bytes\n"
              << "  data_size     : " << file.file.data_size << " bytes\n"
              << "  project       : " << file.metadata.BuildingInfo.ProjectName
              << '\n'
              << "  event         : " << file.metadata.DataInfo.EventName
              << '\n'
              << "  start_time    : " << file.metadata.DataInfo.StartTime
              << '\n'
              << "  channels      : " << file.metadata.InstrumentInfo.ChannelNum
              << '\n'
              << "  samples       : " << file.metadata.DataInfo.NPTS << '\n'
              << "  dt            : " << file.metadata.DataInfo.DT << '\n'
              << "  packet_source : " << file.packet.source_id << '\n'
              << "  packet_rate   : " << file.packet.sampling_rate << " Hz\n"
              << "  packet_encoding: " << file.packet.data_encoding << '\n'
              << "  packet_body   : " << file.packet.body_size << " bytes\n"
              << "  packet_crc32  : 0x" << std::hex << file.packet.checksum
              << std::dec << '\n';

    if (command.show_channels) {
        std::cout << "  channel_list:\n";
        for (const auto &channel : file.metadata.InstrumentInfo.Channels) {
            std::cout << "    " << channel.ChannelNo << ": "
                      << channel.ChannelID << ", " << channel.Measurand
                      << ", azimuth=" << channel.Azimuth << ", location=["
                      << channel.LocationXYZ[0] << ", "
                      << channel.LocationXYZ[1] << ", "
                      << channel.LocationXYZ[2] << "]\n";
        }
    }

    return 0;
}

int run_validate_qrest_command(const ValidateQrestCommand &command) {
    const auto report =
        qrest_data::tools::validate_qrest_file(command.input_path);
    qrest_data::tools::print_validation_report(
        std::cout, command.input_path, report);
    return report.ok() ? 0 : 1;
}

int run_validate_text_command(const ValidateTextCommand &command) {
    const auto report = qrest_data::tools::validate_text_dataset(
        command.metadata_path, command.data_path);
    qrest_data::tools::print_validation_report(
        std::cout, command.metadata_path + " + " + command.data_path, report);
    return report.ok() ? 0 : 1;
}

int run_import_tdms_command(const ImportTdmsCommand &command) {
    const std::string metadata_json = read_text_file(command.metadata_path);
    const Metadata metadata = Metadata::from_bytes(metadata_json);
    const auto dataset = load_tdms_dataset(
        command.input_path,
        make_tdms_import_options(command.unit,
                                 command.sensitivity_mode,
                                 command.explicit_sensitivity,
                                 command.sensitivity_storage_scale,
                                 command.post_scale,
                                 command.output_counts,
                                 command.no_time_check));
    require_or_print_compatibility(dataset, metadata);
    write_qrest_file(command.output_path,
                     metadata_json,
                     dataset.channel_sequential_data,
                     static_cast<std::uint16_t>(command.source_id),
                     static_cast<std::uint16_t>(command.data_encoding));

    std::cout << "Imported TDMS to qREST: " << command.output_path << '\n';
    print_dataset_summary(dataset);
    return 0;
}

int run_import_mseed_command(const ImportMseedCommand &command) {
    const std::string metadata_json = read_text_file(command.metadata_path);
    const Metadata metadata = Metadata::from_bytes(metadata_json);
    const auto dataset = load_mseed_dataset(
        command.input_path,
        make_mseed_import_options(command.group_index,
                                  command.include_dimensionless,
                                  command.no_time_check));
    require_or_print_compatibility(dataset, metadata);
    write_qrest_file(command.output_path,
                     metadata_json,
                     dataset.channel_sequential_data,
                     static_cast<std::uint16_t>(command.source_id),
                     static_cast<std::uint16_t>(command.data_encoding));

    std::cout << "Imported MiniSEED to qREST: " << command.output_path << '\n';
    print_dataset_summary(dataset);
    return 0;
}

int run_import_hdf5_command(const ImportHdf5Command &command) {
    Metadata metadata;
    const auto dataset = load_hdf5_dataset(command.input_path, &metadata);
    require_or_print_compatibility(dataset, metadata);
    write_qrest_file(command.output_path,
                     metadata,
                     dataset.channel_sequential_data,
                     static_cast<std::uint16_t>(command.source_id),
                     static_cast<std::uint16_t>(command.data_encoding));

    std::cout << "Imported HDF5 to qREST: " << command.output_path << '\n';
    print_dataset_summary(dataset);
    return 0;
}

int run_export_hdf5_command(const ExportHdf5Command &command) {
    const auto file = read_qrest_file(command.input_path);
    write_hdf5_dataset(
        command.output_path, file.metadata, file.channel_sequential_data);

    std::cout << "Exported qREST to HDF5: " << command.output_path << '\n'
              << "  channels : " << file.metadata.InstrumentInfo.ChannelNum
              << '\n'
              << "  samples  : " << file.metadata.DataInfo.NPTS << '\n';
    return 0;
}

int run_validate_tdms_command(const ValidateTdmsCommand &command) {
    ValidationReport report;
    ExternalDataset dataset;
    try {
        dataset = load_tdms_dataset(
            command.input_path,
            make_tdms_import_options(command.unit,
                                     command.sensitivity_mode,
                                     command.explicit_sensitivity,
                                     command.sensitivity_storage_scale,
                                     command.post_scale,
                                     command.output_counts,
                                     command.no_time_check));
    } catch (const std::exception &e) {
        report.errors.push_back(e.what());
        print_validation_report(std::cout, command.input_path, report);
        return 1;
    }
    print_validation_report(std::cout, command.input_path, report);
    print_dataset_summary(dataset);
    return 0;
}

int run_validate_mseed_command(const ValidateMseedCommand &command) {
    ValidationReport report;
    ExternalDataset dataset;
    try {
        dataset = load_mseed_dataset(
            command.input_path,
            make_mseed_import_options(command.group_index,
                                      command.include_dimensionless,
                                      command.no_time_check));
    } catch (const std::exception &e) {
        report.errors.push_back(e.what());
        print_validation_report(std::cout, command.input_path, report);
        return 1;
    }
    print_validation_report(std::cout, command.input_path, report);
    print_dataset_summary(dataset);
    return 0;
}

int run_validate_hdf5_command(const ValidateHdf5Command &command) {
    ValidationReport report;
    ExternalDataset dataset;
    Metadata metadata;
    try {
        dataset = load_hdf5_dataset(command.input_path, &metadata);
        report = validate_external_dataset_compatibility(dataset, metadata);
    } catch (const std::exception &e) {
        report.errors.push_back(e.what());
    }
    print_validation_report(std::cout, command.input_path, report);
    if (report.ok()) {
        print_dataset_summary(dataset);
        return 0;
    }
    return 1;
}

} // namespace

int main(int argc, char **argv) {
    CLI::App app{"qREST data command-line tools"};
    app.require_subcommand(1);

    PackCommand pack_values;
    auto *pack = app.add_subcommand(
        "pack",
        "Build a .qrest file from metadata JSON and a time-major text matrix");
    configure_pack_command(pack, pack_values);

    PackCommand generate_values;
    auto *generate = app.add_subcommand("generate", "Legacy alias of pack");
    configure_pack_command(generate, generate_values);

    ExtractCommand extract_values;
    auto *extract = app.add_subcommand(
        "extract", "Extract metadata JSON and text matrix from .qrest");
    configure_extract_command(extract, extract_values);

    ExtractCommand load_values;
    auto *load = app.add_subcommand("load", "Legacy alias of extract");
    configure_extract_command(load, load_values);

    InspectCommand inspect_values;
    auto *inspect = app.add_subcommand("inspect", "Print a qREST file summary");
    configure_inspect_command(inspect, inspect_values);

    auto *import_app =
        app.add_subcommand("import", "Import external data into .qrest");
    import_app->require_subcommand(1);

    ImportTdmsCommand import_tdms_values;
    auto *import_tdms =
        import_app->add_subcommand("tdms", "Import TDMS plus metadata JSON");
    configure_import_tdms_command(import_tdms, import_tdms_values);

    ImportMseedCommand import_mseed_values;
    auto *import_mseed = import_app->add_subcommand(
        "mseed", "Import modified MiniSEED plus metadata JSON");
    configure_import_mseed_command(import_mseed, import_mseed_values);

    ImportHdf5Command import_hdf5_values;
    auto *import_hdf5 =
        import_app->add_subcommand("hdf5", "Import qREST HDF5 into .qrest");
    configure_import_hdf5_command(import_hdf5, import_hdf5_values);

    auto *export_app =
        app.add_subcommand("export", "Export .qrest into another data format");
    export_app->require_subcommand(1);

    ExportHdf5Command export_hdf5_values;
    auto *export_hdf5 =
        export_app->add_subcommand("hdf5", "Export .qrest to qREST HDF5");
    configure_export_hdf5_command(export_hdf5, export_hdf5_values);

    auto *validate = app.add_subcommand(
        "validate", "Validate qREST files and input datasets");
    validate->require_subcommand(1);

    ValidateQrestCommand validate_qrest_values;
    auto *validate_qrest =
        validate->add_subcommand("qrest", "Validate a .qrest file");
    configure_validate_qrest_command(validate_qrest, validate_qrest_values);

    ValidateTextCommand validate_text_values;
    auto *validate_text = validate->add_subcommand(
        "text", "Validate metadata JSON plus a time-major text matrix");
    configure_validate_text_command(validate_text, validate_text_values);

    ValidateTdmsCommand validate_tdms_values;
    auto *validate_tdms =
        validate->add_subcommand("tdms", "Validate a TDMS input file");
    configure_validate_tdms_command(validate_tdms, validate_tdms_values);

    ValidateMseedCommand validate_mseed_values;
    auto *validate_mseed = validate->add_subcommand(
        "mseed", "Validate a modified MiniSEED input file");
    configure_validate_mseed_command(validate_mseed, validate_mseed_values);

    ValidateHdf5Command validate_hdf5_values;
    auto *validate_hdf5 =
        validate->add_subcommand("hdf5", "Validate a qREST HDF5 file");
    configure_validate_hdf5_command(validate_hdf5, validate_hdf5_values);

    try {
        app.parse(argc, argv);
        if (pack->parsed()) {
            return run_pack_command(pack_values);
        }
        if (generate->parsed()) {
            return run_pack_command(generate_values);
        }
        if (extract->parsed()) {
            return run_extract_command(extract_values);
        }
        if (load->parsed()) {
            return run_extract_command(load_values);
        }
        if (inspect->parsed()) {
            return run_inspect_command(inspect_values);
        }
        if (import_tdms->parsed()) {
            return run_import_tdms_command(import_tdms_values);
        }
        if (import_mseed->parsed()) {
            return run_import_mseed_command(import_mseed_values);
        }
        if (import_hdf5->parsed()) {
            return run_import_hdf5_command(import_hdf5_values);
        }
        if (export_hdf5->parsed()) {
            return run_export_hdf5_command(export_hdf5_values);
        }
        if (validate_qrest->parsed()) {
            return run_validate_qrest_command(validate_qrest_values);
        }
        if (validate_text->parsed()) {
            return run_validate_text_command(validate_text_values);
        }
        if (validate_tdms->parsed()) {
            return run_validate_tdms_command(validate_tdms_values);
        }
        if (validate_mseed->parsed()) {
            return run_validate_mseed_command(validate_mseed_values);
        }
        if (validate_hdf5->parsed()) {
            return run_validate_hdf5_command(validate_hdf5_values);
        }

        return 2;
    } catch (const CLI::ParseError &e) {
        return app.exit(e);
    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }
}
