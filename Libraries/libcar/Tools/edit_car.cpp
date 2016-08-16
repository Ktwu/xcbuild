/**
 Copyright (c) 2015-present, Facebook, Inc.
 All rights reserved.

 This source code is licensed under the BSD-style license found in the
 LICENSE file in the root directory of this source tree. An additional grant
 of patent rights can be found in the PATENTS file in the same directory.
 */

#include <bom/bom.h>
#include <bom/bom_format.h>
#include <car/car_format.h>
#include <car/Facet.h>
#include <car/Reader.h>
#include <car/Rendition.h>
#include <car/Writer.h>
#include <libutil/Options.h>

#include <regex>
#include <string>
#include <vector>

namespace {
class Options {
    ext::optional<bool>        _version;
    ext::optional<bool>        _help;
    std::vector<std::string>   _removeAssets;
    ext::optional<std::string> _input;
    ext::optional<std::string> _output;

public:
    Options();

public:
    bool version() const
    { return _version.value_or(false); }

    bool help() const
    { return _help.value_or(false); }

    ext::optional<std::string> const &input() const
    { return _input; }

    ext::optional<std::string> const &output() const
    { return _output; }

    std::vector<std::string> const &removeAssets() const
    { return _removeAssets; }

private:
    friend class libutil::Options;
    std::pair<bool, std::string>
    parseArgument(std::vector<std::string> const &args, std::vector<std::string>::const_iterator *it);
};
}

Options::
Options()
{
}

std::pair<bool, std::string> Options::
parseArgument(std::vector<std::string> const &args,
    std::vector<std::string>::const_iterator *it)
{
    std::string const &arg = **it;

    if (arg == "--version") {
        return libutil::Options::Current<bool>(&_version, arg);
    } else if (arg == "--help") {
        return libutil::Options::Current<bool>(&_help, arg);
    } else if (arg == "--input") {
        return libutil::Options::Next<std::string>(&_input, args, it);
    } else if (arg == "--output") {
        return libutil::Options::Next<std::string>(&_output, args, it);
    } else if (arg == "--remove-asset") {
        return libutil::Options::AppendNext<std::string>(&_removeAssets, args, it);
    } else {
        return std::make_pair(false, "unknown argument " + arg);
    }
}

static void
_print_help(char *name)
{
    fprintf(stderr,
        "Usage: %s --input <filename> --output <filename> [--remove-asset <regex>]\n", name);
}

int
main(int argc, char **argv)
{
    auto args = std::vector<std::string>(argv + 1, argv + argc);

    Options options;
    std::pair<bool, std::string> optionsResult = libutil::Options::Parse<Options>(&options, args);

    if (options.help()) {
        _print_help(argv[0]);
        return EXIT_SUCCESS;
    }

    if (!options.input() || !options.output()) {
        fprintf(stderr, "error: bad arguments\n");
        _print_help(argv[0]);
        return EXIT_FAILURE;
    }

    // Reader
    struct bom_context_memory memory_reader = bom_context_memory_file(options.input()->c_str(), false, 0);
    auto bom_reader = std::unique_ptr<struct bom_context, decltype(&bom_free)>(bom_alloc_load(memory_reader), bom_free);
    if (bom_reader == nullptr) {
        fprintf(stderr, "error: unable to load BOM for reading\n");
        return EXIT_FAILURE;
    }

    ext::optional<car::Reader> reader = car::Reader::Load(std::move(bom_reader));

    /*
     * A fast CAR file write has pre-allocated space for BOM indexes
     * A baseline of 6 indexes are required:
     *   - CAR Header (1)
     *   - Key Format (1)
     *   - FACET tree (2)
     *   - RENDITION tree (2)
     * Each tree entry (facet or rendition) requires 2:
     *   - key index (1)
     *   - value index (1)
     */
    uint32_t index_count = 6 +
        reader->facetCount() * 2 +
        reader->renditionCount() * 2;

    // Writer
    struct bom_context_memory memory_writer = bom_context_memory_file(options.output()->c_str(), true, sizeof(struct bom_header));
    auto bom_writer = std::unique_ptr<struct bom_context, decltype(&bom_free)>(bom_alloc_empty2(memory_writer, index_count), bom_free);
    if (bom_writer == nullptr) {
        fprintf(stderr, "error: unable to load BOM for writing, using %s\n", argv[2]);
        return EXIT_FAILURE;
    }
    ext::optional<car::Writer> writer = car::Writer::Create(std::move(bom_writer));

    std::vector<std::regex> filters;
    for (const auto &removeAsset : options.removeAssets()) {
        filters.push_back(std::regex(removeAsset));
    }

    // Get the offset of the identifier in the rendition key
    struct car_key_format *keyfmt = reader->keyfmt();
    if (!keyfmt) {
        fprintf(stderr, "error: No key format in input\n");
        exit(EXIT_FAILURE);
    }
    writer->keyfmt() = keyfmt;
    size_t identifier_index = 0;

    // Scan the key format for the facet identifier index
    for (size_t i = 0; i < keyfmt->num_identifiers; i++) {
        if (keyfmt->identifier_list[i] == car_attribute_identifier_identifier) {
            identifier_index = i;
            break;
        }
    }

    reader->facetIterate([&reader, &writer, &filters, identifier_index, keyfmt](car::Facet const &facet) {
        for (auto const &filter : filters) {
            if (std::regex_match(facet.name(), filter)) {
                return;
            }
        }

        ext::optional<uint16_t> id_lookup = facet.attributes().get(car_attribute_identifier_identifier);
        if (!id_lookup) {
            return;
        }
        uint16_t facet_identifier = *id_lookup;

        writer->addFacet(std::move(facet));

        reader->renditionFastIterate([facet_identifier, &writer, identifier_index, keyfmt](void *key, size_t key_len, void *value, size_t value_len) {
            uint16_t *raw_attributes = (uint16_t *)key;
            if (raw_attributes[identifier_index] == facet_identifier) {
                writer->addRendition(key, key_len, value, value_len);
            }
        });
    });

    writer->write();
    return EXIT_SUCCESS;
}
