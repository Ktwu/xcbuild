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
    std::vector<std::string>   _removeScales;
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

    std::vector<std::string> const &removeScales() const
    { return _removeScales; }

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
    } else if (arg == "--remove-scale") {
        return libutil::Options::AppendNext<std::string>(&_removeScales, args, it);
    } else {
        return std::make_pair(false, "unknown argument " + arg);
    }
}

static void
_print_help(char *name)
{
    fprintf(stderr,
        "Usage: %s --input <filename> --output <filename> [--remove-asset <regex>] [--remove-scale <integer>]\n", name);
}

static size_t
_attribute_index(struct car_key_format *keyfmt, uint16_t attribute_identifier)
{
    for (size_t i = 0; i < keyfmt->num_identifiers; i++) {
        if (keyfmt->identifier_list[i] == attribute_identifier) {
            return i;
        }
    }
    return SIZE_MAX;
}

typedef struct {
    void *key;
    size_t keyLength;
    void *value;
    size_t valueLength;
} KeyValuePair;

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

    std::vector<std::regex> facetFilters;
    for (const auto &removeAsset : options.removeAssets()) {
        facetFilters.push_back(std::regex(removeAsset));
    }

    std::vector<uint16_t> scaleFilters;
    for (const auto &scale : options.removeScales()) {
        scaleFilters.push_back(std::stoi(scale));
    }

    // Get the offset of the identifier in the rendition key
    struct car_key_format *keyfmt = reader->keyfmt();
    if (!keyfmt) {
        fprintf(stderr, "error: No key format in input\n");
        exit(EXIT_FAILURE);
    }
    writer->keyfmt() = keyfmt;

    // Scan the key format for the facet identifier and scale index
    size_t identifier_index = _attribute_index(keyfmt, car_attribute_identifier_identifier);
    if (identifier_index == SIZE_MAX) {
        // There are no imagesets
        writer->write();
        return EXIT_SUCCESS;
    }
    size_t scale_index = _attribute_index(keyfmt, car_attribute_identifier_scale);
    if (scale_index == SIZE_MAX && options.removeScales().size() > 0) {
        fprintf(stderr, "error: Could not find scale in key format\n");
        exit(EXIT_FAILURE);
    }

    reader->facetIterate([&reader, &writer, &facetFilters, &scaleFilters, identifier_index, scale_index](car::Facet const &facet) {
        for (auto const &filter : facetFilters) {
            if (std::regex_match(facet.name(), filter)) {
                return;
            }
        }

        // Collect a list of the renditions in raw key/value form
        // This is _much_ faster
        std::vector<KeyValuePair> renditions = {};

        ext::optional<uint16_t> id_lookup = facet.attributes().get(car_attribute_identifier_identifier);
        if (!id_lookup) {
            return;
        }
        uint16_t facet_identifier = *id_lookup;

        reader->renditionFastIterate([facet_identifier, identifier_index, &renditions](void *key, size_t keyLength, void *value, size_t valueLength) {
            uint16_t *raw_attributes = static_cast<uint16_t *>(key);
            if (raw_attributes[identifier_index] != facet_identifier) {
                // Skip unrelated renditions
                return;
            }
            renditions.push_back({key, keyLength, value, valueLength});
        });

        // Filter intelligently based on scale here
        // We only want to remove a scale if we have an alternate rendition
        for (const auto scale : scaleFilters) {
            for (int i = 0; i < renditions.size() && renditions.size() >= 2; ) {
                uint16_t *raw_attributes = static_cast<uint16_t *>(renditions[i].key);
                if (raw_attributes[scale_index] == scale) {
                    renditions.erase(renditions.begin() + i);
                } else {
                    i++;
                }
            }
        }

        if (renditions.size() == 0) {
            // Either no Renditions found for Facets, or all have been filtered
            return;
        }

        // At this point we have at least one rendition left
        writer->addFacet(std::move(car::Facet(facet)));
        for (const auto &kv : renditions) {
            writer->addRendition(kv.key, kv.keyLength, kv.value, kv.valueLength);
        }
    });

    writer->write();
    return EXIT_SUCCESS;
}
