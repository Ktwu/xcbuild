/**
 Copyright (c) 2015-present, Facebook, Inc.
 All rights reserved.

 This source code is licensed under the BSD-style license found in the
 LICENSE file in the root directory of this source tree. An additional grant
 of patent rights can be found in the PATENTS file in the same directory.
 */

#include <bom/bom.h>
#include <car/Facet.h>
#include <car/Reader.h>
#include <car/Rendition.h>
#include <car/Writer.h>
#include <libutil/DefaultFilesystem.h>
#include <libutil/Filesystem.h>
#include <libutil/Options.h>
#include <plist/Format/JSON.h>
#include <plist/Objects.h>
#include <process/DefaultContext.h>
#include <process/DefaultLauncher.h>
#include <process/DefaultUser.h>
#include <xcassets/Slot/Idiom.h>

#include <iterator>
#include <sstream>
#include <string>

using libutil::DefaultFilesystem;
using libutil::Filesystem;

using plist::Format::JSON;
using plist::String;
using plist::Integer;
using plist::Real;
using plist::Dictionary;
using plist::Array;

class Options {
private:
    ext::optional<bool>        _help;
    ext::optional<bool>        _version;

private:
    ext::optional<std::string> _idiom;
    ext::optional<int>         _scale;

private:
    ext::optional<bool>        _info;

private:
    ext::optional<std::string> _inputFile;
    ext::optional<std::string> _outputFile;

public:
    Options() {}
    ~Options() {}

public:
    bool help() const
    { return _help.value_or(false); }
    bool version() const
    { return _version.value_or(false); }

public:
    ext::optional<std::string> idiom() const
    { return _idiom; }
    ext::optional<int> scale() const
    { return _scale; }


public:
    bool infoAction() const
    { return _info.value_or(false); }

public:
    ext::optional<std::string> inputFile() const
    { return _inputFile; }
    ext::optional<std::string> outputFile() const
    { return _outputFile; }

private:
    friend class libutil::Options;
    std::pair<bool, std::string>
    parseArgument(std::vector<std::string> const &args, std::vector<std::string>::const_iterator *it);
};

std::pair<bool, std::string> Options::
parseArgument(std::vector<std::string> const &args, std::vector<std::string>::const_iterator *it)
{
    std::string const &arg = **it;

    if (arg == "-h" || arg == "--help" || arg == "-help") {
        return libutil::Options::Current<bool>(&_help, arg);

    } else if (arg == "--version" || arg == "-V") {
        return libutil::Options::Current<bool>(&_version, arg);

    } else if (arg == "--idiom" || arg == "-I") {
        return libutil::Options::Next<std::string>(&_idiom, args, it);

    } else if (arg == "--scale" || arg == "-s") {
        auto scale = libutil::Options::Next<int>(&_scale, args, it);
        if (_scale && *_scale < 1) {
          return std::make_pair(false, "scale must be >=1, given " + arg);
        }
        return scale;

    } else if (arg == "--info" || arg == "-I") {
        return libutil::Options::Current<bool>(&_info, arg);

    } else if (arg == "--output" || arg == "-o") {
        return libutil::Options::Next<std::string>(&_outputFile, args, it);
    }

    if (!arg.empty() && arg[0] != '-') {
        if (!_inputFile) {
            _inputFile = arg;
            return std::make_pair(true, std::string());
        }
    }

    return std::make_pair(false, "unknown argument " + arg);
}

static bool
shouldIgnoreFacet(std::string const &idiom, uint16_t scale, Options const &options)
{
    if (options.idiom() && idiom != *options.idiom()) {
        return true;
    }

    if (options.scale() && scale != *options.scale()) {
        return true;
    }

    return false;
}

static ext::optional<std::unique_ptr<Dictionary>>
getFacetJSON(car::Reader const &reader, car::Facet const &facet, Options const &options)
{
    uint rendition_count = 0;
    uint16_t scale = 0;
    std::string idiom;

    auto sizes = Array::New();
    auto renditions = reader.lookupRenditions(facet);
    for (auto const &rendition : renditions) {
        scale = *(rendition.attributes().get(car_attribute_identifier_scale));
        idiom = xcassets::Slot::Idioms::String(
            (car_attribute_identifier_idiom_value)*(
                rendition.attributes().get(car_attribute_identifier_idiom)
            )
        );

        std::stringstream stream;
        stream << rendition.width() << "x" << rendition.height() << " ";
        stream << "index:" << rendition_count << " ";
        stream << "idiom:" << idiom;
        sizes->append(String::New(stream.str()));

        rendition_count++;
    }

    if (shouldIgnoreFacet(idiom, scale, options)) {
        return ext::nullopt;
    }

    auto dictionary = Dictionary::New();
    dictionary->set("AssetType", String::New("MultiSized Image"));
    dictionary->set("Name", String::New(facet.name()));
    dictionary->set("Idiom", String::New(idiom));
    dictionary->set("Scale", Integer::New(scale));
    dictionary->set("Sizes", std::move(sizes));

    return std::move(dictionary);
}

static std::unique_ptr<Dictionary>
getRenditionJSON(car::Rendition const &rendition)
{
    // TODO what about PackedImages
    auto dictionary = Dictionary::New();

    auto filename = rendition.fileName();
    const std::string icon_prefix = "Icon-";
    std::string asset_type = "Image";
    if (filename.compare(0, icon_prefix.length(), icon_prefix) == 0) {
      asset_type = "Icon Image";
    }

    car_attribute_identifier_idiom_value idiom = (car_attribute_identifier_idiom_value)*(rendition.attributes().get(car_attribute_identifier_idiom));

    dictionary->set("RenditionName", String::New(filename));
    dictionary->set("Idiom", String::New(xcassets::Slot::Idioms::String(idiom)));
    dictionary->set("AssetType", String::New(asset_type));
    dictionary->set("PixelHeight", Integer::New(rendition.height()));
    dictionary->set("PixelWidth", Integer::New(rendition.width()));

    return std::move(dictionary);
}

static ext::optional<car::Writer>
createCarWriter(std::string const &path)
{
    struct bom_context_memory memory = bom_context_memory_file(path.c_str(), true, 0);
    if (memory.data == NULL) {
        return ext::nullopt;
    }

    auto bom = car::Writer::unique_ptr_bom(bom_alloc_empty(memory), bom_free);
    if (bom == nullptr) {
        return ext::nullopt;
    }

    return car::Writer::Create(std::move(bom));
}

static int
Version()
{
    printf("assetutil version 1 (xcbuild)\n");
    return 0;
}

static int
Help(std::string const &error = std::string())
{
    if (!error.empty()) {
        fprintf(stderr, "error: %s\n", error.c_str());
        fprintf(stderr, "\n");
    }

    fprintf(stderr, "Usage: assetutil [options] [input file]\n\n");
    fprintf(stderr, "Find and execute developer tools.\n\n");

#define INDENT "  "
    fprintf(stderr, "Thinning parameters:\n");
    fprintf(stderr, INDENT "-i, --idiom [universal/phone/pad/tv/car/watch/marketing]\n");
    fprintf(stderr, INDENT "-s, --scale [int >= 1]\n");
    fprintf(stderr, "\n");

    fprintf(stderr, "Output:\n");
    fprintf(stderr, INDENT "-I, --info (dumps a JSON file describing the input file)\n");
    fprintf(stderr, "\n");

    fprintf(stderr, "General:\n");
    fprintf(stderr, INDENT "--version, -V\n");
    fprintf(stderr, INDENT "--output, -o [path]\n");
    fprintf(stderr, "\n");
#undef INDENT

    return (error.empty() ? 0 : -1);
}

static int
InfoAction(car::Reader const &car, Options const &options) {
  auto plistOutput = Array::New();

  car.facetIterate([&car, &plistOutput, &options](car::Facet const &facet) {
      auto facetJSON = getFacetJSON(car, facet, options);
      if (!facetJSON) {
          return;
      }

      plistOutput->append(std::move(*facetJSON));
      auto renditions = car.lookupRenditions(facet);
      for (auto const &rendition : renditions) {
          auto renditionJSON = getRenditionJSON(rendition);
          plistOutput->append(std::move(renditionJSON));
      }
  });

  auto serialize = JSON::Serialize(plistOutput.get(), JSON::Create());
  auto bytes = *serialize.first;
  printf(std::string(bytes.begin(), bytes.end()).c_str());

  return 0;
}

static int
ThinningAction(car::Reader const &car, Options const &options) {
    if (!options.outputFile())  {
        fprintf(stderr, "error: unable to thin without output file specified\n");
        return 1;
    }

    auto writer = createCarWriter(*options.outputFile());
    if (!writer) {
      fprintf(stderr, "error: failed to create writer for thinned .car\n");
      return 1;
    }

    car.facetIterate([&car, &writer, &options](car::Facet const &facet) {
        auto facetJSON = getFacetJSON(car, facet, options);
        if (!facetJSON) {
            return;
        }

        // Use the presence of the JSON metadata blob as signal whether the facet
        // passes the given filter.
        writer->addFacet(facet);
        auto renditions = car.lookupRenditions(facet);
        for (auto const &rendition : renditions) {
            writer->addRendition(rendition);
        }
    });

    writer->write();
    return 0;
}

static int Run(Filesystem *filesystem, process::User const *user, process::Context const *processContext, process::Launcher *processLauncher)
{
    /*
     * Parse out the options, or print help & exit.
     */
    auto command_line_arguments = processContext->commandLineArguments();
    Options options;
    std::pair<bool, std::string> result = libutil::Options::Parse<Options>(&options, command_line_arguments);
    if (!result.first) {
        return Help(result.second);
    }

    /*
     * Handle the basic options.
     */
    if (options.help()) {
        return Help();
    } else if (options.version()) {
        return Version();
    }

    if (!options.inputFile()) {
      return Help("Not given an input file.");
    }

    struct bom_context_memory memory = bom_context_memory_file(options.inputFile()->c_str(), false, 0);
    auto bom = std::unique_ptr<struct bom_context, decltype(&bom_free)>(bom_alloc_load(memory), bom_free);
    if (bom == nullptr) {
        fprintf(stderr, "error: unable to load BOM\n");
        return 1;
    }

    ext::optional<car::Reader> car = car::Reader::Load(std::move(bom));
    if (!car) {
        fprintf(stderr, "error: unable to load car archive\n");
        return 1;
    }

    if (options.infoAction()) {
      return InfoAction(*car, options);
    }

    return ThinningAction(*car, options);
}

int
main(int argc, char **argv)
{
    DefaultFilesystem filesystem = DefaultFilesystem();
    process::DefaultContext processContext = process::DefaultContext();
    process::DefaultLauncher processLauncher = process::DefaultLauncher();
    process::DefaultUser user = process::DefaultUser();
    return Run(&filesystem, &user, &processContext, &processLauncher);
}
